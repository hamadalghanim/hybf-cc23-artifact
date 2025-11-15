//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//
#include "llvm/Transforms/Scalar/HybridBranchFusion.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/CFMelder/CFMelder.h"
#include "llvm/Transforms/IPO/FunctionMerging.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/BranchFusion.h"
#include "llvm/Transforms/TFG/Blocks.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;

#define DEBUG_TYPE "hybrid-brfusion"

static cl::opt<bool> RunCFMOnly("run-cfm-only", cl::init(false), cl::Hidden,
                                cl::desc("Only run control-flow melding"));

static cl::opt<bool> RunBFOnly("run-brfusion-only", cl::init(false), cl::Hidden,
                               cl::desc("Only run branch fusion"));

static cl::opt<bool> EnableTFG("enable-tfg", cl::init(false), cl::Hidden,
                               cl::desc("enable tfg"));

namespace {

class HybridBranchFusionLegacyPass : public ModulePass {

public:
  static char ID;

  HybridBranchFusionLegacyPass() : ModulePass(ID) {
    initializeHybridBranchFusionLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnModule(Module &M) override;
};

} // namespace

/*
static int computeCodeSize(Function *F, TargetTransformInfo &TTI) {
  float CodeSize = 0;
  for (Instruction &I : instructions(*F)) {
    switch(I.getOpcode()) {
    case Instruction::PHI:
      CodeSize += 0.2;
      break;
    default:
      CodeSize += TTI.getInstructionCost(
                       &I, TargetTransformInfo::TargetCostKind::TCK_CodeSize)
                    .getValue()
                    .getValue();
    }
  }
  return CodeSize;
}
*/

class ClonedFunctionInfo {
public:
  Function *Original{nullptr};
  Function *Cloned{nullptr};
  DominatorTree DT;
  PostDominatorTree PDT;
  ValueToValueMapTy VMap;
  bool NeedPDT{false};

  ClonedFunctionInfo(Function *F, bool NeedPDT)
      : Original(F), NeedPDT(NeedPDT) {};

  ~ClonedFunctionInfo() { Invalidate(); }

  void Reinitialize() {
    if (Cloned == nullptr) {
      VMap.clear();
      Cloned = llvm::CloneFunction(Original, VMap);
      DT = DominatorTree(*Cloned);
      if (NeedPDT)
        PDT = PostDominatorTree(*Cloned);
    }
  }

  void Invalidate() {
    if (Cloned != nullptr) {
      Cloned->eraseFromParent();
      Cloned = nullptr;
    }
  }

  BasicBlock *getClonedBB(BasicBlock *BB) {
    return llvm::dyn_cast<BasicBlock>(VMap[BB]);
  }

  BranchInst *getClonedBI(BranchInst *BI) {
    return llvm::dyn_cast<BranchInst>(VMap[BI]);
  }
};
static bool runImpl(Function *F, DominatorTree &DT, PostDominatorTree &PDT,
                    LoopInfo &LI, TargetTransformInfo &TTI, AAManager &AA) {
  errs() << "Procesing function : " << F->getName() << "\n";
  int CFMCount = 0, BFCount = 0, TFGCount = 0;
  bool LocalChange = false, Changed = false;

  int OrigCodeSize = EstimateFunctionSize(F, TTI);

  std::set<BasicBlock *> VisitedBBs;

  do {
    LocalChange = false;
    int BeforeSize = EstimateFunctionSize(F, TTI);
    ClonedFunctionInfo CFMFunc(F, true);
    ClonedFunctionInfo BFFunc(F, false);
    ClonedFunctionInfo CFMTFGFunc(F, true); // CFM with TFG
    ClonedFunctionInfo BFTFGFunc(F, false); // BF with TFG

    for (BasicBlock *BB : post_order(&F->getEntryBlock())) {
      if (VisitedBBs.count(BB))
        continue;
      VisitedBBs.insert(BB);

      BranchInst *BI = dyn_cast<BranchInst>(BB->getTerminator());
      if (BI && BI->isConditional()) {

        int CFMProfit = 0;
        int CFMTFGProfit = 0;
        SmallVector<unsigned> ProfitableIdxs;
        SmallVector<unsigned> ProfitableTFGIdxs;

        if (RunCFMOnly || !RunBFOnly) {
          // 1. CFM without TFG
          CFMFunc.Reinitialize();
          SmallVector<unsigned> EmptyIdxs;
          ProfitableIdxs = runCFM(CFMFunc.getClonedBB(BB), CFMFunc.DT,
                                  CFMFunc.PDT, TTI, EmptyIdxs);
          bool CFMSuccess = ProfitableIdxs.size() > 0;
          CFMProfit = CFMSuccess ? BeforeSize -
                                       EstimateFunctionSize(CFMFunc.Cloned, TTI)
                                 : 0;
          errs() << "CFM code reduction : " << CFMProfit << "\n";
          if (CFMSuccess)
            CFMFunc.Invalidate();

          // 2. TFG + CFM (if TFG enabled)
          if (EnableTFG) {
            CFMTFGFunc.Reinitialize();
            BasicBlock *ClonedBB = CFMTFGFunc.getClonedBB(BB);

            // Run TFG first
            bool TFGSuccess = runTFG(ClonedBB, AA);
            if (TFGSuccess) {
              // Then run CFM on the TFG-optimized code
              SmallVector<unsigned> EmptyIdxs2;
              ProfitableTFGIdxs = runCFM(ClonedBB, CFMTFGFunc.DT,
                                         CFMTFGFunc.PDT, TTI, EmptyIdxs2);
              bool CFMTFGSuccess = ProfitableTFGIdxs.size() > 0;
              CFMTFGProfit = CFMTFGSuccess
                                 ? BeforeSize - EstimateFunctionSize(
                                                    CFMTFGFunc.Cloned, TTI)
                                 : 0;
              errs() << "TFG+CFM code reduction : " << CFMTFGProfit << "\n";
              if (CFMTFGSuccess)
                CFMTFGFunc.Invalidate();
            }
          }
        }

        int BFProfit = 0;
        int BFTFGProfit = 0;

        if (!RunCFMOnly || RunBFOnly) {
          // 3. BF without TFG
          BFFunc.Reinitialize();
          bool BFSuccess = MergeBranchRegions(
              *(BFFunc.Cloned), BFFunc.getClonedBI(BI), BFFunc.DT, TTI, false);
          BFProfit = BFSuccess
                         ? BeforeSize - EstimateFunctionSize(BFFunc.Cloned, TTI)
                         : 0;
          errs() << "Branch fusion code reduction : " << BFProfit << "\n";
          if (BFSuccess)
            BFFunc.Invalidate();

          // 4. TFG + BF (if TFG enabled)
          if (EnableTFG) {
            BFTFGFunc.Reinitialize();
            BasicBlock *ClonedBB = BFTFGFunc.getClonedBB(BB);
            BranchInst *ClonedBI = BFTFGFunc.getClonedBI(BI);

            // Run TFG first
            bool TFGSuccess = runTFG(ClonedBB, AA);
            if (TFGSuccess) {
              // Then run BF on the TFG-optimized code
              bool BFTFGSuccess = MergeBranchRegions(
                  *(BFTFGFunc.Cloned), ClonedBI, BFTFGFunc.DT, TTI, false);
              BFTFGProfit =
                  BFTFGSuccess
                      ? BeforeSize - EstimateFunctionSize(BFTFGFunc.Cloned, TTI)
                      : 0;
              errs() << "TFG+BF code reduction : " << BFTFGProfit << "\n";
              if (BFTFGSuccess)
                BFTFGFunc.Invalidate();
            }
          }
        }

        // Pick the best combination out of all 4 options
        int MaxProfit =
            std::max({BFProfit, CFMProfit, BFTFGProfit, CFMTFGProfit});

        if (MaxProfit > 0) {
          if (MaxProfit == BFTFGProfit && EnableTFG) {
            errs() << "Profitable: TFG+BF " << BB->getName().str() << ": ";
            BI->dump();
            runTFG(BB, AA);
            MergeBranchRegions(*F, BI, DT, TTI, true);
            TFGCount++;
            BFCount++;
          } else if (MaxProfit == BFProfit) {
            errs() << "Profitable Branch Fusion: SEME-brfusion "
                   << BB->getName().str() << ": ";
            BI->dump();
            MergeBranchRegions(*F, BI, DT, TTI, true);
            BFCount++;
          } else if (MaxProfit == CFMTFGProfit && EnableTFG) {
            errs() << "Profitable: TFG+CFM " << BB->getName().str() << ": ";
            BI->dump();
            runTFG(BB, AA);
            runCFM(BB, DT, PDT, TTI, ProfitableTFGIdxs);
            TFGCount++;
            CFMCount++;
          } else if (MaxProfit == CFMProfit) {
            errs() << "Profitable Branch Fusion: CFMelder "
                   << BB->getName().str() << ": ";
            BI->dump();
            runCFM(BB, DT, PDT, TTI, ProfitableIdxs);
            CFMCount++;
          }
          LocalChange = true;

          DT.recalculate(*F);
          PDT.recalculate(*F);
          break;
        }
      }
    }
    Changed |= LocalChange;
  } while (LocalChange);

  if (Changed) {
    int FinalCodeSize = EstimateFunctionSize(F, TTI);
    double PercentReduction =
        (OrigCodeSize - FinalCodeSize) * 100 / (double)OrigCodeSize;
    errs() << "Size reduction for function " << F->getName() << ": "
           << OrigCodeSize << " to  " << FinalCodeSize << " ("
           << PercentReduction << "%)"
           << "\n";
    errs() << "Brach fusion applied " << BFCount << " times, CFM applied "
           << CFMCount << " times, and TFG applied " << TFGCount << " times\n";
  }

  return Changed;
}

bool HybridBranchFusionLegacyPass::runOnModule(Module &M) {
  errs() << "HybridBranchFusionLegacyPass not implemented\n";
  return false;
}

void HybridBranchFusionLegacyPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<TargetTransformInfoWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
}

PreservedAnalyses
HybridBranchFusionModulePass::run(Module &M, ModuleAnalysisManager &MAM) {
  auto &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  bool Changed = false;
  SmallVector<Function *, 64> Funcs;

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    Funcs.push_back(&F);
  }

  for (Function *F : Funcs) {
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(*F);
    auto &PDT = FAM.getResult<PostDominatorTreeAnalysis>(*F);
    auto &TTI = FAM.getResult<TargetIRAnalysis>(*F);
    auto &LI = FAM.getResult<LoopAnalysis>(*F);
    auto &AA = FAM.getResult<AAManager>(*F);
    Changed |= runImpl(F, DT, PDT, LI, TTI, AA);
  }
  if (!Changed)
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  return PA;
}

char HybridBranchFusionLegacyPass::ID = 0;

INITIALIZE_PASS_BEGIN(HybridBranchFusionLegacyPass, "hybrid-brfusion",
                      "Hybrid branch fusion for code size", false, false)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_END(HybridBranchFusionLegacyPass, "hybrid-brfusion",
                    "Hybrid branch fusion for code size", false, false)

// Initialization Routines
void llvm::initializeHybridBranchFusion(PassRegistry &Registry) {
  initializeHybridBranchFusionLegacyPassPass(Registry);
}

ModulePass *llvm::createHybridBranchFusionModulePass() {
  return new HybridBranchFusionLegacyPass();
}
