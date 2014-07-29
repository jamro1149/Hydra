#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "hydra/Support/FunAlgorithms.h"

using namespace llvm;
using namespace hydra;

namespace {
  class TestModule2 : public ModulePass {
  public:
    static char ID;
    TestModule2() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M) override;
  };
}

char TestModule2::ID{ 0 };

bool TestModule2::runOnModule(Module &M) {
  LLVMContext &c{ M.getContext() };
  Constant *main{ M.getOrInsertFunction("main", Type::getInt32Ty(c), nullptr) };
  Constant *fPar{ M.getOrInsertFunction("fSpawn", Type::getInt32Ty(c),
                                        nullptr) };
  Constant *gLeave{ M.getOrInsertFunction("gLeave", Type::getInt32Ty(c),
                                          nullptr) };
  Constant *hPar{ M.getOrInsertFunction("hSpawn", Type::getInt32Ty(c),
                                        nullptr) };
  Constant *iLeave{ M.getOrInsertFunction("iLeave", Type::getInt32Ty(c),
                                          nullptr) };
  Constant *jLeave{ M.getOrInsertFunction("jLeave", Type::getInt32Ty(c),
                                          nullptr) };

  addInstructions(0u, *cast<Function>(main));
  addInstructions(10000u, *cast<Function>(fPar));
  addInstructions(10u, *cast<Function>(gLeave));
  addInstructions(10000u, *cast<Function>(hPar));
  addInstructions(10000u, *cast<Function>(iLeave));
  addInstructions(10u, *cast<Function>(jLeave));


  auto iter = cast<Function>(main)->getEntryBlock().begin();
  std::vector<Value *> args;
  CallInst::Create(fPar, args, "", &*iter);
  CallInst::Create(gLeave, args, "", &*iter);
  CallInst::Create(hPar, args, "", &*iter);
  CallInst::Create(iLeave, args, "", &*iter);
  CallInst::Create(jLeave, args, "", &*iter);

  return true;
}

static RegisterPass<TestModule2> X("test-module-profitability",
                                   "Generate Test Module 2", false, false);
