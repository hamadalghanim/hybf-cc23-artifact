#ifndef __TILES_H__
#define __TILES_H__

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/PostDominators.h"

#include <stdint.h>
#include <vector>

namespace llvm
{

	struct TiledBlock;

	struct Tile
	{
		std::vector<size_t> hash;
		std::vector<Instruction *> insts;
		TiledBlock *block;
		void print(raw_ostream &OS) const;
	};

	struct TiledBlock
	{
		std::vector<size_t> hash;
		std::vector<Tile *> tiles;
		BasicBlock *basedOn;
		void print(raw_ostream &OS) const
		{
			OS << "\tBasic Block: '" << basedOn->getName() << "'\n";
			OS << "\tHash: ";
			if (!hash.empty())
			{
				for (size_t i = 0; i < hash.size(); ++i)
					OS << (i ? ", " : "") << hash[i];
			}
			else
			{
				OS << "(empty)";
			}
			OS << "\n\t--------------------\n";

			for (const Tile *tile : tiles)
				tile->print(OS);
		}
	};
	struct FunctionInfo
	{
		std::vector<TiledBlock *> blocks;
		PostDominatorTree *PDT;
		DominatorTree *DT;
		std::vector<size_t> hashes;
		void print(raw_ostream &OS, Function *F) const
		{
			OS << "\nFunction: " << F->getName() << "\nHash: ";
			if (!hashes.empty())
			{
				for (size_t i = 0; i < hashes.size(); ++i)
					OS << (i ? ", " : "") << hashes[i];
			}
			else
			{
				OS << "(empty)";
			}
			OS << "\n---------------------------\n";

			for (const TiledBlock *block : blocks)
				block->print(OS);
		}
	};
	TiledBlock *generateTiledBlock(BasicBlock *basedOn);

	bool isControlFlowInst(Instruction *inst);
}; /* namespace llvm */

#endif
