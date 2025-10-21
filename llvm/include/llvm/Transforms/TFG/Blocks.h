#ifndef LLVM_TRANSFORMS_TFG_H
#define LLVM_TRANSFORMS_TFG_H
#include "llvm/IR/PassManager.h"
namespace llvm {

// Function pass for control-flow melding
struct BasicBlocksPass : public PassInfoMixin<BasicBlocksPass> {

  PreservedAnalyses run(Module &M, AnalysisManager<Module> &AM);
};
} // namespace llvm
#endif