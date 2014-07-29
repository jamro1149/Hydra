#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "hydra/Support/FunAlgorithms.h"

using namespace llvm;
using namespace hydra;

namespace {
  class TestModule4 : public ModulePass {
  public:
    static char ID;
    TestModule4() : ModulePass{ ID } {}
    virtual bool runOnModule(Module &M) override;
  };
}

char TestModule4::ID{ 0 };

bool TestModule4::runOnModule(Module &M) {
  LLVMContext &c{ M.getContext() };
  Type *const intTy{ Type::getInt32Ty(c) };
  Function *main{ cast<Function>(
      M.getOrInsertFunction("main", intTy, nullptr)) };
  Function *spawnMe{ cast<Function>(
      M.getOrInsertFunction("spawn_me", intTy, nullptr)) };
  Function *work{ cast<Function>(
      M.getOrInsertFunction("do_work", intTy, nullptr)) };

  // synthesise do_work
  addInstructions(2000u, *work);

  // synthesise spawn_me
  auto *sBB = BasicBlock::Create(c, "spawn_meEntry", spawnMe);
  std::vector<Value *> args{};
  CallInst::Create(work, args, "ret", sBB);
  ReturnInst::Create(c, ConstantInt::get(intTy, 42u), sBB);

  // synthesise main's entry block
  auto *mainEntry = BasicBlock::Create(c, "mainEntry", main);
  auto *spawnMeResult = CallInst::Create(spawnMe, args, "res", mainEntry);
  auto *workResult = CallInst::Create(work, args, "", mainEntry);
  auto *cmp =
      CmpInst::Create(BinaryOperator::ICmp, CmpInst::ICMP_NE, workResult,
                      ConstantInt::get(intTy, 23u), "cmp", mainEntry);
  auto *mainLeft = BasicBlock::Create(c, "useRes", main);
  auto *mainRight = BasicBlock::Create(c, "dontUseRes", main);
  BranchInst::Create(mainLeft, mainRight, cmp, mainEntry);

  // synthesise main's left block
  BinaryOperator::Create(BinaryOperator::Add, spawnMeResult, spawnMeResult,
                         "useRes", mainLeft);
  auto *mainExtra = BasicBlock::Create(c, "extra", main);
  BranchInst::Create(mainExtra, mainLeft);

  // synthesise main's right block
  BranchInst::Create(mainExtra, mainRight);

  // synthesise main's extra block
  BinaryOperator::Create(BinaryOperator::Sub, spawnMeResult,
                         ConstantInt::get(intTy, 4u), "useRes", mainExtra);
  auto *mainExit = BasicBlock::Create(c, "exit", main);
  BranchInst::Create(mainExit, mainExtra);

  // synthesise main's exit block
  BinaryOperator::Create(BinaryOperator::Sub, spawnMeResult,
                         ConstantInt::get(intTy, 4u), "useRes", mainExit);
  ReturnInst::Create(c, ConstantInt::get(intTy, 0u), mainExit);
  return true;
}

static RegisterPass<TestModule4> X("test-module-parallelisecalls",
                                   "Generate Test Module 6", false, false);
