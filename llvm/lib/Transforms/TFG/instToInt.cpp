#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"

using namespace llvm;

// instToInt implemented in llvm-next-function-merging at lines 2650–2920 ([raw.githubusercontent.com](https://raw.githubusercontent.com/kateinoigakukun/llvm-next-function-merging/main/lib/Transforms/IPO/FunctionMerging.cpp))

unsigned instToInt(Instruction *I)
{
  bool deterministic = true;
  uint32_t value = 0;
  static uint32_t pseudorand_value = 100;

  if (pseudorand_value > 10000)
    pseudorand_value = 100;

  // std::ofstream myfile;
  // std::string newPath = "/home/sean/similarityChecker.txt";

  // Opcodes must be equivalent for instructions to match -- use opcode value as
  // base
  value = I->getOpcode();

  // Number of operands must be equivalent -- except in the case where the
  // instruction is a return instruction -- +1 to stop being zero
  uint32_t operands = (I->getOpcode() == Instruction::Ret) ? 1 : I->getNumOperands();
  value = value * (operands + 1);

  // Instruction type must be equivalent, pairwise operand types must be
  // equivalent -- use typeID casted to int -- This may not be perfect as my
  // understanding of this is limited
  auto instTypeID = static_cast<uint32_t>(I->getType()->getTypeID());
  value = value * (instTypeID + 1);
  auto *ITypePtr = I->getType();
  if (ITypePtr && !deterministic)
  {
    value = value * (reinterpret_cast<std::uintptr_t>(ITypePtr) + 1);
  }

  for (size_t i = 0; i < I->getNumOperands(); i++)
  {
    auto operTypeID = static_cast<uint32_t>(I->getOperand(i)->getType()->getTypeID());
    value = value * (operTypeID + 1);

    auto *IOperTypePtr = I->getOperand(i)->getType();

    if (IOperTypePtr && !deterministic)
    {
      value =
          value *
          (reinterpret_cast<std::uintptr_t>(I->getOperand(i)->getType()) + 1);
    }

    value = value * (i + 1);
  }
  return value;
}