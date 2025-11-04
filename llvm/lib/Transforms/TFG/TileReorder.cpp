#include "TileReorder.h"
using namespace llvm;

// Check if two instructions have a memory dependency that prevents reordering
static bool hasMemoryDependency(Instruction *I1, Instruction *I2,
                                AAResults *AA) {
  if (!AA)
    return false; // Conservative: assume no dependency if no AA

  auto getMemLoc = [](Instruction *I) -> std::optional<MemoryLocation> {
    if (auto *LI = dyn_cast<LoadInst>(I))
      return MemoryLocation::get(LI);
    if (auto *SI = dyn_cast<StoreInst>(I))
      return MemoryLocation::get(SI);
    return std::nullopt;
  };

  auto Loc1 = getMemLoc(I1);
  auto Loc2 = getMemLoc(I2);

  if (!Loc1 || !Loc2)
    return false;

  // Check if they may alias
  AliasResult AR = AA->alias(*Loc1, *Loc2);
  return AR != AliasResult::NoAlias;
}

// Check if reordering tile1 before tile2 is safe
static bool
canSafelyReorderTiles(const std::vector<Instruction *> &tile1,
                      const std::vector<Instruction *> &tile2, AAResults *AA,
                      const std::vector<Instruction *> &originalOrder) {
  // Build position map for checking original order
  std::map<Instruction *, int> posMap;
  for (size_t i = 0; i < originalOrder.size(); i++) {
    posMap[originalOrder[i]] = i;
  }

  // Check if tile1 originally comes after tile2
  bool wouldReorder = false;
  for (Instruction *I1 : tile1) {
    for (Instruction *I2 : tile2) {
      if (posMap[I1] > posMap[I2]) {
        wouldReorder = true;
        break;
      }
    }
    if (wouldReorder)
      break;
  }

  if (!wouldReorder)
    return true; // No reordering happening

  // Check for memory dependencies between tiles
  for (Instruction *I1 : tile1) {
    if (!isa<LoadInst>(I1) && !isa<StoreInst>(I1))
      continue;

    for (Instruction *I2 : tile2) {
      if (!isa<LoadInst>(I2) && !isa<StoreInst>(I2))
        continue;

      // Check for RAW (Read After Write), WAR (Write After Read),
      // or WAW (Write After Write) hazards
      bool I1IsStore = isa<StoreInst>(I1);
      bool I2IsStore = isa<StoreInst>(I2);

      // Only need to check if there's at least one store
      if (!I1IsStore && !I2IsStore)
        continue;

      // RAW: I2 is store, I1 is load, moving I1 before I2
      // WAR: I2 is load, I1 is store, moving I1 before I2
      // WAW: Both are stores
      if (hasMemoryDependency(I1, I2, AA)) {
        if (posMap[I1] > posMap[I2]) {
          // We're trying to move I1 (from tile1) before I2 (from tile2)
          // but they have a memory dependency
          return false;
        }
      }
    }
  }

  return true;
}

// Reorder instructions in a basic block to group tiles together
// while preserving control flow instruction positions and memory dependencies
void reorderBasicBlockByTiles(TiledBlock *block, AAResults *AA) {
  if (!block || block->tiles.empty()) {
    errs() << "Basic Block has empty tiles\n";
    return;
  }

  BasicBlock *BB = block->basedOn;

  // Get original instruction order for reference
  std::vector<Instruction *> originalOrder;
  for (Instruction &I : *BB) {
    originalOrder.push_back(&I);
  }

  std::map<Instruction *, int> instToTile;
  std::vector<Instruction *> newOrder;
  std::set<Instruction *> processed;
  std::vector<Instruction *> barrierInsts;
  std::vector<Instruction *> phiNodes;

  for (size_t i = 0; i < block->tiles.size(); i++) {
    for (Instruction *inst : block->tiles[i]->insts) {
      instToTile[inst] = i;
    }
  }

  for (Instruction &I : *BB) {
    if (isControlFlowInst(&I)) {
      barrierInsts.push_back(&I);
    }
    if (isa<PHINode>(&I)) {
      phiNodes.push_back(&I);
    }
  }

  // Helper to find position of instruction in original order
  auto getOriginalPos = [&](Instruction *inst) -> int {
    for (size_t i = 0; i < originalOrder.size(); i++) {
      if (originalOrder[i] == inst)
        return i;
    }
    return -1;
  };

  // Ensure phinodes are at the top of the block
  for (Instruction *phiInst : phiNodes) {
    newOrder.push_back(phiInst);
    processed.insert(phiInst);
  }

  // Process each region (between control flow boundaries)
  int regionStart = 0;
  for (Instruction *barrierInst : barrierInsts) {
    int barrierPos = getOriginalPos(barrierInst);

    // Collect all instructions in this region that belong to tiles
    std::map<int, std::vector<Instruction *>> tilesInRegion;
    for (int i = regionStart; i < barrierPos; i++) {
      Instruction *inst = originalOrder[i];
      if (instToTile.count(inst) > 0) {
        int tileIdx = instToTile[inst];
        tilesInRegion[tileIdx].push_back(inst);
      }
    }

    // Check memory dependencies before reordering
    std::vector<int> tileOrder;
    for (auto &pair : tilesInRegion) {
      tileOrder.push_back(pair.first);
    }

    // Verify all pairs of tiles can be safely reordered
    bool canReorder = true;
    for (size_t i = 0; i < tileOrder.size() && canReorder; i++) {
      for (size_t j = i + 1; j < tileOrder.size() && canReorder; j++) {
        int tile1Idx = tileOrder[i];
        int tile2Idx = tileOrder[j];

        if (!canSafelyReorderTiles(tilesInRegion[tile1Idx],
                                   tilesInRegion[tile2Idx], AA,
                                   originalOrder)) {
          canReorder = false;
          errs() << "Warning: Cannot safely reorder tiles " << tile1Idx
                 << " and " << tile2Idx << " due to memory dependencies\n";
        }
      }
    }

    if (!canReorder) {
      // Fall back to original order for this region
      errs() << "Skipping reordering for region before barrier at position "
             << barrierPos << "\n";
      for (int i = regionStart; i < barrierPos; i++) {
        Instruction *inst = originalOrder[i];
        if (processed.count(inst) == 0) {
          newOrder.push_back(inst);
          processed.insert(inst);
        }
      }
    } else {
      // Add tiles in order, preserving instruction order within each tile
      for (auto &pair : tilesInRegion) {
        // Sort instructions within tile by original position
        std::sort(pair.second.begin(), pair.second.end(),
                  [&](Instruction *a, Instruction *b) {
                    return getOriginalPos(a) < getOriginalPos(b);
                  });

        for (Instruction *inst : pair.second) {
          if (processed.count(inst) == 0) {
            newOrder.push_back(inst);
            processed.insert(inst);
          }
        }
      }
    }

    newOrder.push_back(barrierInst);
    processed.insert(barrierInst);
    regionStart = barrierPos + 1;
  }

  // Handle remaining instructions after last barrier
  std::map<int, std::vector<Instruction *>> remainingTiles;
  for (size_t i = regionStart; i < originalOrder.size(); i++) {
    Instruction *inst = originalOrder[i];
    if (processed.count(inst) == 0) {
      if (instToTile.count(inst) > 0) {
        int tileIdx = instToTile[inst];
        remainingTiles[tileIdx].push_back(inst);
      }
    }
  }

  // Check safety for remaining tiles
  std::vector<int> remainingTileOrder;
  for (auto &pair : remainingTiles) {
    remainingTileOrder.push_back(pair.first);
  }

  bool canReorderRemaining = true;
  for (size_t i = 0; i < remainingTileOrder.size() && canReorderRemaining;
       i++) {
    for (size_t j = i + 1; j < remainingTileOrder.size() && canReorderRemaining;
         j++) {
      int tile1Idx = remainingTileOrder[i];
      int tile2Idx = remainingTileOrder[j];

      if (!canSafelyReorderTiles(remainingTiles[tile1Idx],
                                 remainingTiles[tile2Idx], AA, originalOrder)) {
        canReorderRemaining = false;
        errs() << "Warning: Cannot safely reorder remaining tiles " << tile1Idx
               << " and " << tile2Idx << " due to memory dependencies\n";
      }
    }
  }

  if (!canReorderRemaining) {
    // Fall back to original order
    for (size_t i = regionStart; i < originalOrder.size(); i++) {
      Instruction *inst = originalOrder[i];
      if (processed.count(inst) == 0) {
        newOrder.push_back(inst);
        processed.insert(inst);
      }
    }
  } else {
    for (auto &pair : remainingTiles) {
      std::sort(pair.second.begin(), pair.second.end(),
                [&](Instruction *a, Instruction *b) {
                  return getOriginalPos(a) < getOriginalPos(b);
                });

      for (Instruction *inst : pair.second) {
        newOrder.push_back(inst);
        processed.insert(inst);
      }
    }
  }

  // Count how many instructions will actually be reordered
  int reorderedCount = 0;
  for (size_t i = 0; i < newOrder.size(); i++) {
    if (i < originalOrder.size() && newOrder[i] != originalOrder[i]) {
      reorderedCount++;
    }
  }
  if (reorderedCount > 0) {
    errs() << "Reordering " << reorderedCount << " out of " << BB->size()
           << " instructions in block '" << BB->getName() << "' function name '"
           << BB->getParent()->getName() << "'\n";
  }
  // Verify all instructions are accounted for
  if (newOrder.size() != BB->size()) {
    errs() << "Warning: instruction count mismatch during reordering\n";
    errs() << "  Expected: " << BB->size() << ", Got: " << newOrder.size()
           << "\n";
    return;
  }

  // Remove all instructions from the basic block
  std::vector<Instruction *> toRemove;
  for (Instruction &I : *BB) {
    toRemove.push_back(&I);
  }
  for (Instruction *I : toRemove) {
    I->removeFromParent();
  }

  // Re-insert in new order
  for (Instruction *I : newOrder) {
    BB->getInstList().push_back(I);
  }
}