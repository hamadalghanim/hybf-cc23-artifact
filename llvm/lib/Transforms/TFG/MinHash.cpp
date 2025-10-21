#include "MinHash.h"

using namespace llvm;

// For a Tile
/* deprecated
size_t hashInstructions(Tile *tile) {
    size_t hash = 0;
    for (int i = 0; i < tile->insts.size(); i++) {
        Instruction *I = tile->insts[i];
        hash ^= instToInt(I) + (hash << 6) + (hash >> 2);
    }

    return hash;
} // */

// TODO: Far-future, turn this into Signature object which can replaced with embedding

// TODO: Hamad, non-vector funcs use vector and then this
size_t collapseHashes(std::vector<size_t> hashes)
{
    size_t realHash = 0;
    for (int h = 0; h < hashes.size(); h++)
        realHash ^= hashes[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

size_t factorOperands(Instruction *I)
{
    // I->print(errs()); errs() << "\n";
    size_t instHash = 1;
    for (size_t i = 0; i < I->getNumOperands(); i++)
    {
        size_t id = (size_t)I->getOperand(i);
        // errs() << "inst: " << instHash << "\tID: " << id;
        // I->getOperand(i)->print(errs());
        // errs() << "\n";
        instHash += (id + 1);
    }
    // errs() << "\n";
    return instHash;
}

// FOR TILE if cant shingle due to <=1 instructions
size_t minhashInstructions(Tile *tile, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tile->insts.size(); i++)
    {
        Instruction *I = tile->insts[i];
        size_t instHash = instToInt(I);

        if (i == 0)
            instHash ^= factorOperands(I);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = instHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = collapseHashes(minValues);

    return realHash;
}

// FOR TILE shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleInstructions(Tile *tile, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tile->insts.size() < shingleSize)
        return minhashInstructions(tile, numHashes);

    // for each shingle
    for (int i = 0; i <= tile->insts.size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        if (i == 0)
            shingleHash = factorOperands(tile->insts[0]);

        for (int j = 0; j < shingleSize; j++)
        {
            Instruction *I = tile->insts[i + j];
            shingleHash ^= instToInt(I) + (shingleHash << 6) + (shingleHash >> 2);
        }
        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = 0;
    for (int h = 0; h < numHashes; h++)
        realHash ^= minValues[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

// For a Block
/* deprecated
size_t hashTiles(TiledBlock *tblock) {
    size_t hash = 0;

    for (int i = 0; i < tblock->tiles.size(); i++) {
        size_t thash = tblock->tiles[i]->hash;
        hash ^= thash + (hash << 6) + (hash >> 2);
    }

    return hash;
} // */

// FOR BLOCK if cant shingle due to <=1 instructions
size_t minhashTiles(TiledBlock *tblock, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tblock->tiles.size(); i++)
    {
        size_t thash = collapseHashes(tblock->tiles[i]->hash);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = thash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = 0;
    for (int h = 0; h < numHashes; h++)
        realHash ^= minValues[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

// FOR BLOCK shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleTiles(TiledBlock *tblock, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tblock->tiles.size() < shingleSize)
        return minhashTiles(tblock, numHashes);

    // for each shingle
    for (int i = 0; i <= tblock->tiles.size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        for (int j = 0; j < shingleSize; j++)
        {
            // kind of guessing with this modeled after the one for tiles
            size_t hh = collapseHashes(tblock->tiles[i + j]->hash);
            shingleHash ^= hh + (shingleHash << 6) + (shingleHash >> 2);
        }

        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = 0;
    for (int h = 0; h < numHashes; h++)
        realHash ^= minValues[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

// For a Function
/* deprecated
size_t hashBlocks(std::vector<TiledBlock *> *tblocks) {
    size_t hash = 0;

    for (int i = 0; i < tblocks->size(); i++) {
        size_t tbhash = tblocks->at(i)->hash;
        hash ^= tbhash + (hash << 6) + (hash >> 2);
    }

    return hash;
} // */

// FOR FUNCTION if cant shingle due to <=1 instructions
size_t minhashBlocks(std::vector<TiledBlock *> *tblocks, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tblocks->size(); i++)
    {
        size_t tbhash = collapseHashes(tblocks->at(i)->hash);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = tbhash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = 0;
    for (int h = 0; h < numHashes; h++)
        realHash ^= minValues[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

// FOR FUNCTIONS shingle version, falls back on the non shingle version if we cant shingle
size_t minhashShingleBlocks(std::vector<TiledBlock *> *tblocks, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tblocks->size() < shingleSize)
        return minhashBlocks(tblocks, numHashes);

    // for each shingle in the function?(idk how to word this man)
    for (int i = 0; i <= tblocks->size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        for (int j = 0; j < shingleSize; j++)
        {
            size_t hh = collapseHashes(tblocks->at(i + j)->hash);
            shingleHash ^= hh + (shingleHash << 6) + (shingleHash >> 2);
        }

        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine
    size_t realHash = 0;
    for (int h = 0; h < numHashes; h++)
        realHash ^= minValues[h] + (realHash << 6) + (realHash >> 2);

    return realHash;
}

std::vector<size_t> minhashShingleTilesVector(TiledBlock *tblock, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tblock->tiles.size() < shingleSize)
        return minhashTilesVector(tblock, numHashes);

    // for each shingle
    for (int i = 0; i <= tblock->tiles.size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        for (int j = 0; j < shingleSize; j++)
        {
            // kind of guessing with this modeled after the one for tiles
            size_t hh = collapseHashes(tblock->tiles[i + j]->hash);
            shingleHash ^= hh + (shingleHash << 6) + (shingleHash >> 2);
        }

        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    return minValues;
}
// FOR BLOCK if cant shingle due to <=1 instructions
std::vector<size_t> minhashTilesVector(TiledBlock *tblock, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tblock->tiles.size(); i++)
    {
        size_t thash = collapseHashes(tblock->tiles[i]->hash);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = thash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    return minValues;
}

// FOR TILE if cant shingle due to <=1 instructions
std::vector<size_t> minhashInstructionsVector(Tile *tile, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tile->insts.size(); i++)
    {
        Instruction *I = tile->insts[i];
        size_t instHash = instToInt(I);

        if (i == 0)
            instHash ^= factorOperands(I);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = instHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    // combine

    return minValues;
}

// FOR TILE shingle version, falls back on the non shingle version if we cant shingle
std::vector<size_t> minhashShingleInstructionsVector(Tile *tile, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tile->insts.size() < shingleSize)
        return minhashInstructionsVector(tile, numHashes);

    // for each shingle
    for (int i = 0; i <= tile->insts.size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        if (i == 0)
            shingleHash = factorOperands(tile->insts[0]);

        for (int j = 0; j < shingleSize; j++)
        {
            Instruction *I = tile->insts[i + j];
            shingleHash ^= instToInt(I) + (shingleHash << 6) + (shingleHash >> 2);
        }
        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    return minValues;
}

// FOR FUNCTION if cant shingle due to <=1 instructions
std::vector<size_t> minhashBlocksVector(std::vector<TiledBlock *> *tblocks, int numHashes)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    for (int i = 0; i < tblocks->size(); i++)
    {
        size_t tbhash = collapseHashes(tblocks->at(i)->hash);

        // numHashes number of hash functions!!!! lets try 100
        for (int h = 0; h < numHashes; h++)
        {
            // awesome hash function
            size_t hashValue = tbhash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    return minValues;
}
// FOR FUNCTIONS shingle version, falls back on the non shingle version if we cant shingle
std::vector<size_t> minhashShingleBlocksVector(std::vector<TiledBlock *> *tblocks, int numHashes, int shingleSize)
{
    std::vector<size_t> minValues(numHashes, ULONG_MAX);
    // can shingle check
    if (tblocks->size() < shingleSize)
        return minhashBlocksVector(tblocks, numHashes);

    // for each shingle in the function?(idk how to word this man)
    for (int i = 0; i <= tblocks->size() - shingleSize; i++)
    {
        // this is making shingles, i think we're just gonna do pairs
        size_t shingleHash = 0;
        for (int j = 0; j < shingleSize; j++)
        {
            size_t hh = collapseHashes(tblocks->at(i + j)->hash);
            shingleHash ^= hh + (shingleHash << 6) + (shingleHash >> 2);
        }

        // numhashes number of hashes
        for (int h = 0; h < numHashes; h++)
        {
            size_t hashValue = shingleHash ^ (h * 0x45d9f3b);
            hashValue = hashValue * 0x45d9f3b + (hashValue >> 16);
            minValues[h] = std::min(minValues[h], hashValue);
        }
    }
    return minValues;
}
