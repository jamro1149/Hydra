#include <vector>
#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "hydra/Support/FunAlgorithms.h"

using namespace llvm;
using namespace hydra;

namespace {
  class TestModule1 : public ModulePass {
  public:
    static char ID;
    TestModule1() : ModulePass(ID) {}
    virtual bool runOnModule(Module &M) override;
  };
}

char TestModule1::ID{ 0 };

bool TestModule1::runOnModule(Module &M) {
  LLVMContext &c{ M.getContext() };

  Type *const intTy{ Type::getInt32Ty(c) };

  auto *glob =
      new GlobalVariable(M, intTy, false, GlobalValue::ExternalLinkage,
                         ConstantInt::get(intTy, 42u), "glob");

  Constant *pointerArgs{ M.getOrInsertFunction(
      "pointerArgs", intTy, PointerType::getUnqual(intTy), nullptr) };
  Constant *refGlob{ M.getOrInsertFunction("refsGlobal", intTy, nullptr) };
  M.getOrInsertFunction("opaque", intTy, nullptr);
  Constant *callsUnfit{ M.getOrInsertFunction("callsUnfit", intTy, nullptr) };
  Constant *fit{ M.getOrInsertFunction("noneOfTheAbove", intTy, nullptr) };

  addInstructions(0u, *cast<Function>(pointerArgs));
  addInstructions(0u, *cast<Function>(refGlob));
  addInstructions(0u, *cast<Function>(callsUnfit));
  addInstructions(0u, *cast<Function>(fit));

  auto iter = cast<Function>(refGlob)->getEntryBlock().begin();
  new LoadInst(glob, "loadedGlob", &*iter);

  iter = cast<Function>(callsUnfit)->getEntryBlock().begin();
  std::vector<Value *> args;
  CallInst::Create(refGlob, args, "", &*iter);

  return true;
}

static RegisterPass<TestModule1> X("test-module-fitness",
                                   "Generate Test Module 1", false, false);
