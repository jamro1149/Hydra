#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "hydra/Support/FunAlgorithms.h"

using namespace llvm;
using namespace hydra;

namespace {
  class TestModule5 : public ModulePass {
  public:
    static char ID;
    TestModule5() : ModulePass{ ID } {}
    virtual bool runOnModule(Module &M) override;
  };
}

char TestModule5::ID{ 0 };

bool TestModule5::runOnModule(Module &M) {
  LLVMContext &c{ M.getContext() };
  Type *const intTy{ Type::getInt32Ty(c) };

  Function *main{ cast<Function>(
      M.getOrInsertFunction("main", intTy, nullptr)) };
  Function *spawnMe{ cast<Function>(
      M.getOrInsertFunction("spawn_me", intTy, nullptr)) };
  Function *work{ cast<Function>(
      M.getOrInsertFunction("do_work", intTy, nullptr)) };

  // synthesise main's entry block
  auto *mainEntry = BasicBlock::Create(c, "mainEntry", main);
  CallInst::Create(spawnMe, "", mainEntry);
  CallInst::Create(work, "", mainEntry);
  ReturnInst::Create(c, ConstantInt::get(intTy, 0u), mainEntry);

  // synthesise spawnMe's entry block
  auto *spawnMeEntry = BasicBlock::Create(c, "spawnMeEntry", spawnMe);
  auto *workRes = CallInst::Create(work, "", spawnMeEntry);
  ReturnInst::Create(c, workRes, spawnMeEntry);

  // add all of work's blocks
  auto *workEntry = BasicBlock::Create(c, "workEntry", work);
  auto *workLoopHeader = BasicBlock::Create(c, "workLoopHeader", work);
  auto *workLoopBody = BasicBlock::Create(c, "workLoopBody", work);
  auto *workExit = BasicBlock::Create(c, "workExit", work);

  // synthesise work's entry block
  BranchInst::Create(workLoopHeader, workEntry);

  // synthesise work's loop header block
  auto *indPhy = PHINode::Create(intTy, 2u, "i", workLoopHeader);
  auto *retPhy = PHINode::Create(intTy, 2u, "ret", workLoopHeader);
  auto *cmp =
      CmpInst::Create(BinaryOperator::ICmp, CmpInst::ICMP_ULT, indPhy,
                      ConstantInt::get(intTy, 1000u), "cmp", workLoopHeader);
  BranchInst::Create(workLoopBody, workExit, cmp, workLoopHeader);

  // synthesise work's loop body block
  auto *newRet = BinaryOperator::Create(BinaryOperator::Add, retPhy, indPhy,
                                        "newRet", workLoopBody);
  auto *newInd = BinaryOperator::Create(BinaryOperator::Add, indPhy,
                                        ConstantInt::get(intTy, 1u), "newInd",
                                        workLoopBody);
  BranchInst::Create(workLoopHeader, workLoopBody);

  // handle the PHINodes in the loop header
  indPhy->addIncoming(ConstantInt::get(intTy, 0u), workEntry);
  indPhy->addIncoming(newInd, workLoopBody);
  retPhy->addIncoming(ConstantInt::get(intTy, 0u), workEntry);
  retPhy->addIncoming(newRet, workLoopBody);

  // synthesise work's exit block
  ReturnInst::Create(c, retPhy, workExit);

  return true;
}

static RegisterPass<TestModule5> X("test-module-makespawnable",
                                   "Generate Test Module 5", false, false);
