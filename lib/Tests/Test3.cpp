#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "hydra/Support/FunAlgorithms.h"

using namespace llvm;
using namespace hydra;

namespace {
  class TestModule3 : public ModulePass {
  public:
    static char ID;
    TestModule3() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M) override;
  };
}

char TestModule3::ID{ 0 };

bool TestModule3::runOnModule(Module &M) {
  LLVMContext &c{ M.getContext() };
  Constant *main{ M.getOrInsertFunction("main", Type::getInt32Ty(c), nullptr) };
  Constant *f{ M.getOrInsertFunction("f", Type::getInt32Ty(c), nullptr) };
  Constant *g{ M.getOrInsertFunction("g", Type::getInt32Ty(c), nullptr) };
  Constant *ff{ M.getOrInsertFunction("ff", Type::getInt32Ty(c), nullptr) };
  Constant *fg{ M.getOrInsertFunction("fg", Type::getInt32Ty(c), nullptr) };
  Constant *gf{ M.getOrInsertFunction("gf", Type::getInt32Ty(c), nullptr) };
  Constant *gg{ M.getOrInsertFunction("gg", Type::getInt32Ty(c), nullptr) };
  Constant *fff{ M.getOrInsertFunction("fff", Type::getInt32Ty(c), nullptr) };
  Constant *ffg{ M.getOrInsertFunction("ffg", Type::getInt32Ty(c), nullptr) };
  Constant *fgf{ M.getOrInsertFunction("fgf", Type::getInt32Ty(c), nullptr) };
  Constant *fgg{ M.getOrInsertFunction("fgg", Type::getInt32Ty(c), nullptr) };
  Constant *gff{ M.getOrInsertFunction("gff", Type::getInt32Ty(c), nullptr) };
  Constant *gfg{ M.getOrInsertFunction("gfg", Type::getInt32Ty(c), nullptr) };
  Constant *ggf{ M.getOrInsertFunction("ggf", Type::getInt32Ty(c), nullptr) };
  Constant *ggg{ M.getOrInsertFunction("ggg", Type::getInt32Ty(c), nullptr) };
  Constant *work{ M.getOrInsertFunction("do_work", Type::getInt32Ty(c),
                                        nullptr) };

  addInstructions(0u, *cast<Function>(main));
  addInstructions(0u, *cast<Function>(f));
  addInstructions(0u, *cast<Function>(g));
  addInstructions(0u, *cast<Function>(ff));
  addInstructions(0u, *cast<Function>(fg));
  addInstructions(0u, *cast<Function>(gf));
  addInstructions(0u, *cast<Function>(gg));
  addInstructions(0u, *cast<Function>(fff));
  addInstructions(0u, *cast<Function>(ffg));
  addInstructions(0u, *cast<Function>(fgf));
  addInstructions(0u, *cast<Function>(fgg));
  addInstructions(0u, *cast<Function>(gff));
  addInstructions(0u, *cast<Function>(gfg));
  addInstructions(0u, *cast<Function>(ggf));
  addInstructions(0u, *cast<Function>(ggg));
  addInstructions(10000u, *cast<Function>(work));

  auto iter = cast<Function>(main)->getEntryBlock().begin();
  std::vector<Value *> args;
  CallInst::Create(f, args, "", &*iter);
  CallInst::Create(g, args, "", &*iter);

  iter = cast<Function>(f)->getEntryBlock().begin();
  CallInst::Create(ff, args, "", &*iter);
  CallInst::Create(fg, args, "", &*iter);

  iter = cast<Function>(g)->getEntryBlock().begin();
  CallInst::Create(gf, args, "", &*iter);
  CallInst::Create(gg, args, "", &*iter);

  iter = cast<Function>(ff)->getEntryBlock().begin();
  CallInst::Create(fff, args, "", &*iter);
  CallInst::Create(ffg, args, "", &*iter);

  iter = cast<Function>(fg)->getEntryBlock().begin();
  CallInst::Create(fgf, args, "", &*iter);
  CallInst::Create(fgg, args, "", &*iter);

  iter = cast<Function>(gf)->getEntryBlock().begin();
  CallInst::Create(gff, args, "", &*iter);
  CallInst::Create(gfg, args, "", &*iter);

  iter = cast<Function>(gg)->getEntryBlock().begin();
  CallInst::Create(ggf, args, "", &*iter);
  CallInst::Create(ggg, args, "", &*iter);

  iter = cast<Function>(fff)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(ffg)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(fgf)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(fgg)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(gff)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(gfg)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(ggf)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  iter = cast<Function>(ggg)->getEntryBlock().begin();
  CallInst::Create(work, args, "", &*iter);

  return true;
}

static RegisterPass<TestModule3> X("test-module-joinpoints",
                                   "Generate Test Module 3", false, false);
