#ifndef __MINHASH_H__
#define __MINHASH_H__

#include "Tiles.h"

#include <vector>
#include <map>
#include <climits>

extern unsigned instToInt(llvm::Instruction *I);

// FOR TILE if cant shingle due to <=1 instructions
size_t minhashInstructions(llvm::Tile *tile, int numHashes);

// FOR TILE shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleInstructions(llvm::Tile *tile, int numHashes, int shingleSize);

// FOR BLOCK if cant shingle due to <=1 instructions
size_t minhashTiles(llvm::TiledBlock *tblock, int numHashes);

// FOR BLOCK shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleTiles(llvm::TiledBlock *tblock, int numHashes, int shingleSize);

// FOR FUNCTION if cant shingle due to <=1 instructions
size_t minhashBlocks(std::vector<llvm::TiledBlock *> *tblocks, int numHashes);

// FOR FUNCTIONS shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleBlocks(std::vector<llvm::TiledBlock *> *tblocks, int numHashes, int shingleSize);

// vectorized versions of the same function
// FOR TILE if cant shingle due to <=1 instructions
std::vector<size_t> minhashInstructionsVector(llvm::Tile *tile, int numHashes);
// FOR BLOCK if cant shingle due to <=1 instructions
std::vector<size_t> minhashTilesVector(llvm::TiledBlock *tblock, int numHashes);
// FOR TILE shingle version, falls back on the non shingle version if we cant shingle
std::vector<size_t> minhashShingleInstructionsVector(llvm::Tile *tile, int numHashes, int shingleSize);
// FOR BLOCK shingle version, falls back on the non shingle version if we cant shingle
std::vector<size_t> minhashShingleTilesVector(llvm::TiledBlock *tblock, int numHashes, int shingleSize);
// FOR FUNCTION if cant shingle due to <=1 instructions
std::vector<size_t> minhashBlocksVector(std::vector<llvm::TiledBlock *> *tblocks, int numHashes);
// FOR FUNCTIONS shingle version, falls back on the non shingle version if we cant shingle
std::vector<size_t> minhashShingleBlocksVector(std::vector<llvm::TiledBlock *> *tblocks, int numHashes, int shingleSize);

#endif