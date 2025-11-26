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

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
using namespace llvm;

#define DEBUG_TYPE "hybrid-brfusion"

static cl::opt<bool> RunCFMOnly("run-cfm-only", cl::init(false), cl::Hidden,
                                cl::desc("Only run control-flow melding"));

static cl::opt<bool> RunBFOnly("run-brfusion-only", cl::init(false), cl::Hidden,
                               cl::desc("Only run branch fusion"));

static cl::opt<bool> EnableTFG("enable-tfg", cl::init(false), cl::Hidden,
                               cl::desc("enable tfg"));

namespace {

struct ProfitInformation {
  Function *F;
  std::string technique;
  int profit;
};
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
                    LoopInfo &LI, TargetTransformInfo &TTI, AAResults &AA,
                    std::vector<ProfitInformation> profits) {
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
    ClonedFunctionInfo BFTFGFunc(F, false);

    // Initialize and apply TFG to the TFG clones ONCE at the beginning
    if (EnableTFG) {
      BFTFGFunc.Reinitialize();
      runTFGOnFunction(BFTFGFunc.Cloned, AA);
    }

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
          if (CFMSuccess) {
            CFMProfit = BeforeSize - EstimateFunctionSize(CFMFunc.Cloned, TTI);
            errs() << "CFM code reduction : " << CFMProfit << "\n";
            CFMFunc.Invalidate();
          }

          if (EnableTFG) {
            CFMFunc.Reinitialize();
            BasicBlock *ClonedBB = CFMFunc.getClonedBB(BB);
            runTFG(ClonedBB, AA);
            SmallVector<unsigned> EmptyIdxs2;
            ProfitableTFGIdxs =
                runCFM(ClonedBB, CFMFunc.DT, CFMFunc.PDT, TTI, EmptyIdxs2);
            bool CFMTFGSuccess = ProfitableTFGIdxs.size() > 0;
            if (CFMTFGSuccess) {
              CFMTFGProfit =
                  BeforeSize - EstimateFunctionSize(CFMFunc.Cloned, TTI);
              errs() << "TFG+CFM code reduction : " << CFMTFGProfit << "\n";
              CFMFunc.Invalidate();
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
          if (BFSuccess) {
            BFProfit = BeforeSize - EstimateFunctionSize(BFFunc.Cloned, TTI);

            BFFunc.Invalidate();
            errs() << "Branch fusion code reduction : " << BFProfit << "\n";
          }

          // 4. TFG + BF (TFG already applied to BFTFGFunc at the beginning)
          if (EnableTFG) {
            BranchInst *ClonedBI = BFTFGFunc.getClonedBI(BI);
            bool BFTFGSuccess = MergeBranchRegions(
                *(BFTFGFunc.Cloned), ClonedBI, BFTFGFunc.DT, TTI, false);
            if (BFTFGSuccess) {
              BFTFGProfit =
                  BeforeSize - EstimateFunctionSize(BFTFGFunc.Cloned, TTI);

              errs() << "TFG+BF code reduction : " << BFTFGProfit << "\n";
            }
          }
        }

        profits.push_back(ProfitInformation{F, "BFProfit", BFProfit});
        profits.push_back(ProfitInformation{F, "CFMProfit", CFMProfit});
        profits.push_back(ProfitInformation{F, "BFTFGProfit", BFTFGProfit});
        profits.push_back(ProfitInformation{F, "CFMTFGProfit", CFMTFGProfit});
        // Pick the best combination out of all 4 options
        int MaxProfit =
            std::max({BFProfit, CFMProfit, BFTFGProfit, CFMTFGProfit});

        if (MaxProfit > 0) {
          if (MaxProfit == BFTFGProfit && EnableTFG) {
            errs() << "Profitable: TFG+BF " << BB->getName().str() << ": ";
            BI->dump();
            runTFGOnFunction(F, AA); // Apply TFG to original function
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
    errs() << "Branch fusion applied " << BFCount << " times, CFM applied "
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

void outputCSVFile(Module &M, std::vector<ProfitInformation> profits) {
  // Write to CSV file
  std::error_code EC;
  std::string moduleName = M.getName().str();
  if (moduleName.empty())
    moduleName = "module";
  // Sanitize moduleName to replace slashes with underscores
  std::string sanitizedName = moduleName;
  std::replace(sanitizedName.begin(), sanitizedName.end(), '/', '_');

  if (EnableTFG) {
    sanitizedName = "TFG_" + sanitizedName;
  }

  std::string filePath = "output/HyBF_" + sanitizedName + ".csv";

  // Create the output directory
  EC = llvm::sys::fs::create_directories("output");
  if (EC) {
    llvm::errs() << "Failed to create directory: " << EC.message() << "\n";
    return;
  }

  raw_fd_ostream csvFile(filePath, EC);
  if (!EC) {
    // Header
    csvFile << "Module Name,Function Name,Technique Name,Profit";

    for (const auto &prof : profits) {
      csvFile << moduleName << "," << prof.F->getName() << "," << prof.technique
              << "," << prof.profit << "\n";
    }

    csvFile.close();
    errs() << "Results written to " << filePath << "\n";
  } else {
    errs() << "Error writing CSV file: " << EC.message() << " (" << filePath
           << ")\n";
  }
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
  std::vector<ProfitInformation> profits;
  for (Function *F : Funcs) {
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(*F);
    auto &PDT = FAM.getResult<PostDominatorTreeAnalysis>(*F);
    auto &TTI = FAM.getResult<TargetIRAnalysis>(*F);
    auto &LI = FAM.getResult<LoopAnalysis>(*F);
    auto &AA = FAM.getResult<AAManager>(*F);
    Changed |= runImpl(F, DT, PDT, LI, TTI, AA, profits);
  }
  outputCSVFile(M, profits);
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
