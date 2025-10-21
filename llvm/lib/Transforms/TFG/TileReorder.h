#ifndef __REORDERTILES_H__
#define __REORDERTILES_H__
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "Tiles.h"
#include <vector>
#include <set>
#include <map>
void reorderBasicBlockByTiles(llvm::TiledBlock *block);
#endif