#include "TileReorder.h"
using namespace llvm;

// Reorder instructions in a basic block to group tiles together
// while preserving control flow instruction positions
void reorderBasicBlockByTiles(TiledBlock *block) {
  if (!block || block->tiles.empty()) {
    errs() << "Basic Block has empty tiles";
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
  std::vector<Instruction *> controlFlowInsts;
  std::vector<Instruction *> phiNodes;
  for (size_t i = 0; i < block->tiles.size(); i++) {
    for (Instruction *inst : block->tiles[i]->insts) {
      instToTile[inst] = i;
    }
  }

  for (Instruction &I : *BB) {
    if (isControlFlowInst(&I)) {
      controlFlowInsts.push_back(&I);
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
  for (Instruction *cfInst : controlFlowInsts) {
    int cfPos = getOriginalPos(cfInst);

    // Collect all instructions in this region that belong to tiles
    std::map<int, std::vector<Instruction *>> tilesInRegion;
    for (int i = regionStart; i < cfPos; i++) {
      Instruction *inst = originalOrder[i];
      if (instToTile.count(inst) > 0) {
        int tileIdx = instToTile[inst];
        tilesInRegion[tileIdx].push_back(inst);
      }
    }

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

    // Add the control flow instruction
    newOrder.push_back(cfInst);
    processed.insert(cfInst);

    regionStart = cfPos + 1;
  }

  // Handle remaining instructions after last control flow
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