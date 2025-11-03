#include "Tiles.h"

#include <map>
#include <set>

using namespace std;
namespace llvm {
void Tile::print(raw_ostream &OS) const {
  OS << "\t\tTile:\n\t\tHash: ";
  if (!hash.empty()) {
    for (size_t i = 0; i < hash.size(); ++i)
      OS << (i ? ", " : "") << hash[i];
  } else {
    OS << "(empty)";
  }
  OS << "\n\t\t------------\n";

  for (Instruction *inst : insts) {
    unsigned idx = 0;
    bool found = false;
    if (block && block->basedOn) { // now OK because TiledBlock is defined
      for (Instruction &BI : *block->basedOn) {
        if (&BI == inst) {
          found = true;
          break;
        }
        ++idx;
      }
    }
    OS << "\t\t\t[" << (found ? std::to_string(idx) : "not-in-block") << "] ";
    inst->print(OS);
    OS << "\n";
  }
  OS << "\n";
}

bool isControlFlowInst(Instruction *inst) {
  return isa<BranchInst>(inst) || isa<ReturnInst>(inst) ||
         isa<CallInst>(inst) || isa<InvokeInst>(inst) ||
         isa<SwitchInst>(inst) || isa<IndirectBrInst>(inst);
}
bool isMemoryBarrier(Instruction *inst) {
  return isa<LoadInst>(inst) || isa<StoreInst>(inst) ||
         inst->mayReadOrWriteMemory();
}

bool wouldCrossBoundary(Instruction *i1, Instruction *i2,
                        std::set<Instruction *> tileBarriers,
                        BasicBlock *basedOn) {
  // Check if there's a control flow instruction between i1 and i2
  bool foundFirst = false;
  for (Instruction &I : *basedOn) {
    if (&I == i1 || &I == i2) {
      if (foundFirst)
        return false; // Found both without barrier
      foundFirst = true;
    } else if (foundFirst && tileBarriers.count(&I)) {
      return true; // Found barrier between them
    }
  }
  return false;
};

TiledBlock *generateTiledBlock(BasicBlock *basedOn) {
  std::map<Instruction *, int> tile_map;

  TiledBlock *block = new TiledBlock();
  block->basedOn = basedOn; // assign relation

  std::set<Instruction *> tileBarriers{};
  for (Instruction &I : *basedOn) {
    if (isControlFlowInst(&I) || isMemoryBarrier(&I)) {
      tileBarriers.insert(&I);
    }
  }

  int nxtId = 0;
  for (auto it = basedOn->rbegin(); it != basedOn->rend(); it++) {
    Instruction *inst = &*it;

    // Control flow instructions force a new tile for everything before them
    if (isControlFlowInst(inst) || isMemoryBarrier(inst)) {
      nxtId++; // Start a new tile ID for instructions before this control flow
      continue;
    }

    if (tile_map.count(inst) <= 0) {
      tile_map[inst] = nxtId;
      nxtId++;
    }

    for (Use &U : inst->operands()) { // use-def chain
      Value *v = U.get();
      if (auto *i = dyn_cast<Instruction>(v)) {
        // Don't propagate tile IDs across control flow boundaries
        if (isControlFlowInst(i) || isMemoryBarrier(i))
          continue;

        if (tile_map.count(i) == 0) {
          if (!wouldCrossBoundary(i, inst, tileBarriers, basedOn)) {
            tile_map[i] = tile_map[inst];
          }
          // else: leave unassigned, will get its own tile later in the
          // iteration
        } else if (tile_map.count(i) == 1 and tile_map[i] != tile_map[inst]) {
          // Don't merge tiles if they're separated by control flow
          if (wouldCrossBoundary(i, inst, tileBarriers, basedOn)) {
            continue; // Keep tiles separate
          }

          // We need to merge the tiles
          int idUse = nxtId;
          nxtId++;

          int oId1 = tile_map[i];
          int oId2 = tile_map[inst];

          for (auto it2 = tile_map.begin(); it2 != tile_map.end(); it2++) {
            if (it2->second == oId1 || it2->second == oId2) {
              tile_map[it2->first] = idUse;
            }
          }
        } else if (tile_map.count(i) > 1 and tile_map[i] != tile_map[inst]) {
          errs() << "\t\t\t\t\tError???\n";
          return nullptr;
        }
      }
    }
  }

  // All instructions will be placed into a tile based on tag id -> tile indx
  std::map<int, int> id_to_indx;
  for (Instruction &I : *basedOn) {
    // Skip control flow instructions when creating tiles
    if (isControlFlowInst(&I) || isMemoryBarrier(&I))
      continue;

    if (tile_map.count(&I) <= 0) {
      errs() << "\t\t Not all instructions tagged??\n";
      return nullptr;
    }

    if (id_to_indx.count(tile_map[&I]) <= 0) {
      Tile *t = new Tile();
      block->tiles.push_back(t);
      t->block = block; // assign relation

      id_to_indx[tile_map[&I]] = block->tiles.size() - 1; // assign id -> idx
    }

    int indx = id_to_indx[tile_map[&I]];
    block->tiles[indx]->insts.push_back(&I);
  }

  // TODO: We need to reorder tiles in the blocks tile list so that all br or
  // jmp are last tile, not happening rn
  return block;
}

}; // namespace llvm
