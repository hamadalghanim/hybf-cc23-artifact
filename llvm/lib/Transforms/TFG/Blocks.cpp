#include "llvm/Transforms/TFG/Blocks.h"

#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"

#include "llvm/IR/Dominators.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/llvm-config.h"

#include "MinHash.h"
#include "TileReorder.h"
#include "Tiles.h"
#include "globals.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <vector>

using namespace llvm;

namespace llvm {
// Define the command line option with enum values

void printTiles(const std::map<Function *, FunctionInfo *> &info) {
  for (const auto &[F, finfo] : info) {
    if (!finfo || finfo->blocks.empty())
      continue;
    finfo->print(errs(), F);
  }
}
void writeFunctionDiff(const Function &F, const std::string &original) {
  // Get current function string
  // Check if function name is
  std::string currStr;
  raw_string_ostream currStream(currStr);
  F.print(currStream);
  currStream.flush();

  // Only write diff if they're different
  if (original == currStr) {
    return;
  }

  std::error_code EC;
  // Create output directory if it doesn't exist
  std::filesystem::create_directories("output");

  std::string filePath = "output/" + F.getName().str() + ".diff";
  raw_fd_ostream diffFile(filePath, EC);
  if (!EC) {
    diffFile << "--- a/" << F.getName() << ".ll (original)\n";
    diffFile << "+++ b/" << F.getName() << ".ll (current)\n";
    diffFile << "@@ -1,1 +1,1 @@\n";

    SmallVector<StringRef, 16> originalLines;
    StringRef(original).split(originalLines, '\n');
    for (auto line : originalLines) {
      diffFile << "- " << line << "\n";
    }

    // Current function with '+' prefix

    SmallVector<StringRef, 16> currLines;
    StringRef(currStr).split(currLines, '\n');
    for (auto line : currLines) {
      diffFile << "+ " << line << "\n";
    }

    diffFile.close();
    errs() << "VERIFICATION FAILED for function: " << F.getName()
           << " - diff written to output/" << F.getName() << ".diff\n";
  } else {
    errs() << "Error creating diff file: " << EC.message() << "\n";
  }
}

PreservedAnalyses BasicBlocksPass::run(Module &M, AnalysisManager<Module> &AM) {
  auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  // TODO: to be removed if debug is not needed
  std::map<Function *, std::string> OriginalFunctions;
  std::map<Function *, FunctionInfo *> functionInfo;
  size_t inst_cnt = 0;
  std::vector<PostDominatorTree *> trees;
  auto start = std::chrono::high_resolution_clock::now();

  for (Function &F : M.functions()) {
    // TODO: to be removed if debug is not needed
    std::string FuncStr;
    raw_string_ostream Stream(FuncStr);
    F.print(Stream);
    Stream.flush();
    OriginalFunctions[&F] = std::move(FuncStr);
    // Skip declarations/empty functions
    if (F.empty()) {
      errs() << "Function " << F.getName() << " is empty, skipping\n";
      continue;
    }

    FunctionInfo *fi = functionInfo[&F]; // get the tiled blocks for function
    if (!fi) {
      fi = new FunctionInfo();
    }

    for (BasicBlock &B : F) {
      if (B.empty()) {
        // errs() << "Basic block is empty, skipping\n";
        continue;
      }

      TiledBlock *tblock = generateTiledBlock(&B);
      if (tblock == nullptr) {
        // errs() << "generateTiledBlock returned null for block " <<
        // B.getName() << "\n";
        return PreservedAnalyses::all();
      }

      if (tblock->tiles.empty()) {
        // errs() << "Warning: TiledBlock has no tiles\n";
        fi->blocks.push_back(tblock);
        continue;
      }

      for (size_t i = 0; i < tblock->tiles.size(); i++) { // counting
        if (tblock->tiles[i] == nullptr) {
          // errs() << "Warning: Tile " << i << " is null\n";
          continue;
        }
        inst_cnt += tblock->tiles[i]->insts.size();
      }
      fi->blocks.push_back(tblock);
    }
    // fi->PDT = &FAM.getResult<PostDominatorTreeAnalysis>(F);
    // fi->DT = &FAM.getResult<DominatorTreeAnalysis>(F); // Dominator tree
    functionInfo[&F] = fi;
  }

  auto end1 =
      std::chrono::high_resolution_clock::now(); // Tagging and building done

  std::map<Function *, size_t> functionInstructionsMoved;
  std::map<Function *, size_t> functionInstructionsTotalMoved;
  std::map<Function *, size_t> functionBlocksWithMoves;
  auto end2 = std::chrono::high_resolution_clock::now(); // Hashing done

  for (const auto &[F, finfo] : functionInfo) {
    AAResults &AA = FAM.getResult<AAManager>(*F);
    size_t functionMoveCount = 0;
    size_t functionTotalMoveCount = 0;

    for (TiledBlock *block : finfo->blocks) {
      BasicBlock *bb = block->basedOn;

      // Store original instruction order
      std::vector<Instruction *> originalOrder;
      for (Instruction &I : *bb)
        originalOrder.push_back(&I);

      functionMoveCount += reorderBasicBlockByTiles(block, &AA);

      size_t totalMovedCount = 0;
      std::set<Instruction *> seenInCorrectPosition;

      for (size_t i = 0; i < originalOrder.size(); i++) {
        Instruction *current = nullptr;
        size_t currentIdx = 0;
        for (Instruction &I : *bb) {
          if (currentIdx == i) {
            current = &I;
            break;
          }
          currentIdx++;
        }

        if (current != originalOrder[i])
          totalMovedCount++;
      }
      functionTotalMoveCount += totalMovedCount;
      if (totalMovedCount)
        functionBlocksWithMoves[F]++;
    }
    functionInstructionsMoved[F] = functionMoveCount;
    functionInstructionsTotalMoved[F] = functionTotalMoveCount;
  }
  if (!functionInstructionsMoved.empty()) {
    // Write to CSV file
    std::error_code EC;
    std::string moduleName = M.getName().str();
    if (moduleName.empty())
      moduleName = "module";
    // Sanitize moduleName to replace slashes with underscores
    std::string sanitizedName = moduleName;
    std::replace(sanitizedName.begin(), sanitizedName.end(), '/', '_');

    std::string filePath = "output/instruction_moves_" + sanitizedName + ".csv";

    // Create the output directory
    std::filesystem::create_directories("output", EC);
    if (EC) {
      errs() << "Error creating output directory: " << EC.message() << "\n";
    }

    raw_fd_ostream csvFile(filePath, EC);
    if (!EC) {
      // Header
      csvFile << "Module Name,Function Name,Total Instructions,Block "
                 "Count,Instructions Moved,Total Instructions Relocated,Blocks "
                 "Moved\n";

      for (const auto &[F, count] : functionInstructionsMoved) {
        size_t blockCount = functionInfo[F]->blocks.size();
        size_t totalMoved = functionInstructionsTotalMoved[F];
        size_t blocksWithMovements = functionBlocksWithMoves[F];

        csvFile << moduleName << "," << F->getName() << ","
                << F->getInstructionCount() << "," << blockCount << "," << count
                << "," << totalMoved << "," << blocksWithMovements << "\n";
      }

      csvFile.close();
      errs() << "Results written to " << filePath << "\n";
    } else {
      errs() << "Error writing CSV file: " << EC.message() << " (" << filePath
             << ")\n";
    }
  }
  auto end3 = std::chrono::high_resolution_clock::now(); // Branch Hoisting done
  errs() << "TFG: Processed " << inst_cnt << " instructions\n";
  if (inst_cnt != 0) {
    // errs() << "\tFused " << fuse_cnt << " time(s) out of " << fuse_run << "
    // runs\n";

    errs() << "\n";

    auto build_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start)
            .count();
    errs() << "TFG: Building time:\t\t" << build_time << "us\t\t"
           << (build_time / inst_cnt) << "us/inst\n";

    auto hash_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - end1)
            .count();
    errs() << "TFG: Hashing time:\t\t" << hash_time << "us\t\t"
           << (hash_time / inst_cnt) << "us/inst\n";

    auto csr_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end3 - end2)
            .count();
    errs() << "TFG: Reorder time:\t\t" << csr_time << "us\t\t"
           << (csr_time / inst_cnt) << "us/inst\n";

    auto total_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end3 - start)
            .count();
    errs() << "TFG: Total Time:\t\t" << total_time << "us\t\t"
           << (total_time / inst_cnt) << "us/inst\n";
    errs() << "\n";
  }

  // At the end of your BasicBlocksPass::run() method, before return:
  errs() << "Verifying IR after BasicBlocksPass\n";
  bool failed = false;
  for (Function &F : M) {
    if (verifyFunction(F, &errs())) {
      errs() << "VERIFICATION FAILED for function: " << F.getName() << "\n";
      writeFunctionDiff(F, OriginalFunctions[&F]);
      failed = true;
    }
    // if (strcmp(FUNC_TO_DEBUG, F.getName().str().c_str()) == 0) {
    // writeFunctionDiff(F, OriginalFunctions[&F]); // NOTE: DEBUG ONLY
    // }
  }
  if (failed) {
    exit(1);
    return PreservedAnalyses::all(); // Don't continue with broken IR
  }
  errs() << "IR verification passed\n";
  return PreservedAnalyses::none(); // all()
};

bool runTFG(BasicBlock *BB, AAResults &AA) {
  // Generate the tiled block
  TiledBlock *tblock = generateTiledBlock(BB);
  if (!tblock || tblock->tiles.empty()) {
    if (tblock)
      delete tblock;
    return false;
  }

  // Reorder the basic block
  reorderBasicBlockByTiles(tblock, &AA);

  // Clean up if needed
  delete tblock;

  return true;
}
bool runTFGOnFunction(Function *F, AAResults &AA) {
  bool Changed = false;

  for (BasicBlock &BB : *F) {
    if (BB.empty())
      continue;

    // Generate tiled block
    TiledBlock *tblock = generateTiledBlock(&BB);
    if (!tblock || tblock->tiles.empty()) {
      if (tblock)
        delete tblock;
      continue;
    }

    // Reorder the basic block
    reorderBasicBlockByTiles(tblock, &AA);

    // Clean up
    delete tblock;
    Changed = true;
  }

  return Changed;
}

} // namespace llvm

// extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
// llvmGetPassPluginInfo() {
//   return {.APIVersion = LLVM_PLUGIN_API_VERSION,
//           .PluginName = "Tile Flow Graph Reducers",
//           .PluginVersion = "v0.1",
//           .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
//             // Register for automatic execution
//             PB.registerPipelineStartEPCallback(
//                 [](ModulePassManager &MPM, OptimizationLevel Level) {
//                   MPM.addPass(BasicBlocksPass());
//                   return true;
//                 });

//             // Register for manual execution by name
//             PB.registerPipelineParsingCallback(
//                 [](StringRef Name, ModulePassManager &MPM,
//                    ArrayRef<PassBuilder::PipelineElement>) {
//                   if (Name == "tfg") {
//                     MPM.addPass(BasicBlocksPass());
//                     return true;
//                   }
//                   return false;
//                 });
//           }};
// }
