#ifndef __REORDERTILES_H__
#define __REORDERTILES_H__
#include "Tiles.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include <map>
#include <optional>
#include <set>
#include <vector>
void reorderBasicBlockByTiles(llvm::TiledBlock *block, llvm::AAResults *AA);
#endif