#include "llvm/Transforms/TFG/Blocks.h"

#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"

#include "MinHash.h"
#include "TileReorder.h"
#include "Tiles.h"
#include "globals.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include <chrono>
#include <filesystem>
#include <map>
#include <vector>

using namespace llvm;
static cl::opt<MyHashMode>
    HashMode("tilehash", cl::desc("Choose the hashing method for your pass"),
             cl::values(clEnumValN(HASH_NORMAL, "normal",
                                   "Normal non-vectorized hashing function"),
                        clEnumValN(HASH_VECTOR, "vector",
                                   "Vectorized hashing function")),
             cl::init(HASH_NORMAL));
namespace llvm {
// Define the command line option with enum values

auto const FUNC_TO_DEBUG = "Parse";
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
  hashmode = HashMode;
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
        errs() << "Basic block is empty, skipping\n";
        continue;
      }

      TiledBlock *tblock = generateTiledBlock(&B);
      if (tblock == nullptr) {
        errs() << "generateTiledBlock returned null for block " << B.getName()
               << "\n";
        return PreservedAnalyses::all();
      }

      if (tblock->tiles.empty()) {
        errs() << "Warning: TiledBlock has no tiles\n";
        fi->blocks.push_back(tblock);
        continue;
      }

      for (size_t i = 0; i < tblock->tiles.size(); i++) { // counting
        if (tblock->tiles[i] == nullptr) {
          errs() << "Warning: Tile " << i << " is null\n";
          continue;
        }
        inst_cnt += tblock->tiles[i]->insts.size();
      }
      fi->blocks.push_back(tblock);
    }
    fi->PDT = &FAM.getResult<PostDominatorTreeAnalysis>(F);
    fi->DT = &FAM.getResult<DominatorTreeAnalysis>(F); // Dominator tree
    functionInfo[&F] = fi;
  }

  auto end1 =
      std::chrono::high_resolution_clock::now(); // Tagging and building done

  // // set hash functions and shingle size
  // for (auto it = functionInfo.begin(); it != functionInfo.end();
  //      it++) { // foreach function
  //   FunctionInfo *fi = it->second;
  //   if (!fi) {
  //     errs() << "Warning: Function " << it->first->getName()
  //            << " has null FunctionInfo\n";
  //     continue;
  //   }

  //   std::vector<TiledBlock *> *blocks = &fi->blocks;

  //   if (blocks->empty()) {
  //     errs() << "Function " << it->first->getName()
  //            << "has no blocks, skipping hashing\n";
  //     continue;
  //   }

  //   for (size_t i = 0; i < blocks->size(); i++) { // foreach block
  //     TiledBlock *block = blocks->at(i);

  //     for (size_t j = 0; j < block->tiles.size(); j++) { // foreach tile
  //       Tile *tile = block->tiles[j];
  //       if (HashMode == HASH_NORMAL)
  //         tile->hash = {minhashShingleInstructions(tile, N_HASHES,
  //         N_SHINGLES)};
  //       else
  //         tile->hash =
  //             minhashShingleInstructionsVector(tile, N_HASHES, N_SHINGLES);
  //     }

  //     if (HashMode == HASH_NORMAL)
  //       block->hash = {minhashShingleTiles(block, N_HASHES, N_SHINGLES)};
  //     else
  //       block->hash = minhashShingleTilesVector(block, N_HASHES, N_SHINGLES);
  //   }

  //   if (HashMode == HASH_NORMAL)
  //     fi->hashes = {minhashShingleBlocks(blocks, N_HASHES, N_SHINGLES)};
  //   else
  //     fi->hashes = minhashShingleBlocksVector(blocks, N_HASHES, N_SHINGLES);
  // }

  auto end2 = std::chrono::high_resolution_clock::now(); // Hashing done

  /*errs() << "Function Similarity Check: \n";
    int bit_compare = 4; //bits to compare between
    int band_count = (sizeof(size_t) * 8) / bit_compare;
    for (Function &F: M.functions()) {
    if (functionHashes.count(&F) <= 0) continue; //skip unavailable

    for (Function &F2: M.functions()) {
    if (functionHashes.count(&F2) <= 0) continue; //skip unavailable
    if (&F == &F2) continue; //skip same

    int distance = LSH_distance(functionHashes[&F], functionHashes[&F2],
    band_count); errs() << F.getName() << " vs " << F2.getName() << ": " <<
    distance << " / " << band_count << "\n"; if (distance == 0) errs() <<
    "found functions that should be merged\n";
    }
    }*/

  // errs() << "\n";
  // printTiles(functionInfo, functionHashes);
  // errs() << "\n\t\t\t Printed successfully, returning now\n";

  // NOTE: we dont fuse now
  // int fuse_run = 1;
  // bool fuse = FuseBranches(functionInfo);
  // int fuse_cnt = (int)fuse;
  // while (fuse)
  // { // fixed point alg
  // 	fuse = FuseBranches(functionInfo);
  // 	if (fuse)
  // 		fuse_cnt++;
  // 	fuse_run++;
  // }

  // printTiles(functionInfo);

  for (const auto &[F, finfo] : functionInfo) {
    AAResults &AA = FAM.getResult<AAManager>(*F);
    for (TiledBlock *block : finfo->blocks) {
      reorderBasicBlockByTiles(block, &AA);
    }
  }
  auto end3 = std::chrono::high_resolution_clock::now(); // Branch Hoisting done
  /*for (Function &f: M.functions()) {
    for (BasicBlock &B : f) {
    errs() << "Basic Block: " << B.getName() << "\n";
    for (Instruction &I : B) {
    I.print(errs());
    errs() << "\n";
    }
    }
    } */
  // printTiles(functionInfo, functionHashes);
  errs() << "Processed " << inst_cnt << " instructions\n";
  if (inst_cnt != 0) {
    // errs() << "\tFused " << fuse_cnt << " time(s) out of " << fuse_run << "
    // runs\n";

    errs() << "\n";

    auto build_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start)
            .count();
    errs() << "Building time:\t\t" << build_time << "us\t\t"
           << (build_time / inst_cnt) << "us/inst\n";

    auto hash_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - end1)
            .count();
    errs() << "Hashing time:\t\t" << hash_time << "us\t\t"
           << (hash_time / inst_cnt) << "us/inst\n";

    auto csr_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end3 - end2)
            .count();
    errs() << "Reduce time:\t\t" << csr_time << "us\t\t"
           << (csr_time / inst_cnt) << "us/inst\n";

    auto total_time =
        std::chrono::duration_cast<std::chrono::microseconds>(end3 - start)
            .count();
    errs() << "Total Time:\t\t" << total_time << "us\t\t"
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
    return false;
  }

  // Reorder the basic block
  reorderBasicBlockByTiles(tblock, &AA);

  // Clean up if needed
  delete tblock;

  return true;
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
