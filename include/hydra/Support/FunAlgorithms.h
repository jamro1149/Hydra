#ifndef HYDRA_FUN_ALGORITHMS_H
#define HYDRA_FUN_ALGORITHMS_H

#include <algorithm>
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/InstIterator.h"

namespace hydra {
inline unsigned numInstructions(const llvm::Function &F) {
  return std::distance(inst_begin(F), inst_end(F));
}

inline bool isEmittingInst(const llvm::Instruction &I) {
  return !(llvm::isa<llvm::BitCastInst>(I) || llvm::isa<llvm::PHINode>(I));
}

inline unsigned numEmittingInsts(const llvm::Function &F) {
  return std::count_if(
      inst_begin(F), inst_end(F),
      [](const llvm::Instruction &I) { return isEmittingInst(I); });
}

inline bool isMemoryAccess(const llvm::Instruction &I) {
  return llvm::isa<llvm::AllocaInst>(I) || llvm::isa<llvm::LoadInst>(I) ||
         llvm::isa<llvm::StoreInst>(I) ||
         llvm::isa<llvm::AtomicCmpXchgInst>(I) ||
         llvm::isa<llvm::AtomicRMWInst>(I);
}

inline unsigned numMemAccesses(const llvm::Function &F) {
  return std::count_if(
      inst_begin(F), inst_end(F),
      [](const llvm::Instruction &I) { return isMemoryAccess(I); });
}

inline unsigned numCallInsts(const llvm::Function &F) {
  return std::count_if(
      inst_begin(F), inst_end(F),
      [](const llvm::Instruction &I) { return llvm::isa<llvm::CallInst>(I); });
}

inline void addInstructions(const unsigned num, llvm::Function &F) {
  llvm::LLVMContext &c{ F.getContext() };
  auto BB = llvm::BasicBlock::Create(c, "", &F);
  for (unsigned i{ 0u }; i != num; ++i) {
    llvm::BinaryOperator::Create(llvm::BinaryOperator::And,
                                 llvm::ConstantInt::getTrue(c),
                                 llvm::ConstantInt::getFalse(c), "", BB);
  }
  llvm::ReturnInst::Create(
      c, llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(c), 0), BB);
}

inline bool inOrder(const llvm::Instruction *il, const llvm::Instruction *ir) {
  auto *block = il->getParent();
  assert(block == ir->getParent() && "Instructions are in a different block!");
  for (auto &inst : *block) {
    if (&inst == il) {
      return true;
    }
    if (&inst == ir) {
      return false;
    }
  }
  llvm_unreachable("Neither il or ir found in block!");
}
} // namespace hydra

#endif
