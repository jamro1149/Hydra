#define DEBUG_TYPE "make-spawnable"

#include <string>
#include <vector>
#include "hydra/Analyses/Decider.h"
#include "hydra/Transforms/MakeSpawnable.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

STATISTIC(NumSpawnable, "Number of spawnable functions synthesised");

using namespace llvm;
using namespace hydra;

char MakeSpawnable::ID = 0;

void MakeSpawnable::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<Decider>();
  Info.addPreserved<Decider>();
}

bool MakeSpawnable::runOnModule(Module &M) {
  DEBUG(dbgs() << "MakeSpawnable::runOnModule()\n");
  LLVMContext &c{ M.getContext() };

  auto &decider = getAnalysis<Decider>();

  std::vector<Function *> functions;

  // fill functions with funs which are fit for spawning
  for (auto *F : decider) {
    functions.push_back(F);
  }

  // for each function, add a spawnable one to M
  for (auto *F : functions) {
    DEBUG(dbgs() << "Generating Spawnable Fun for Function " << F->getName()
                 << "()\n");

    const bool returnsVal{ F->getReturnType() != Type::getVoidTy(c) };
    auto &fArgList = F->getArgumentList();

    // create the new function signature with F's arglist length, plus 1 for ret
    const auto numArgs = returnsVal ? fArgList.size() + 1 : fArgList.size();

    std::vector<Type *> funSig;
    funSig.reserve(numArgs);

    Type *const voidStarTy{ PointerType::getUnqual(Type::getInt8Ty(c)) };

    for (unsigned i{ 0u }; i < numArgs; ++i) {
      funSig.push_back(voidStarTy);
    }

    FunctionType *fTy{ FunctionType::get(Type::getVoidTy(c), funSig, false) };

    std::string name{ ("_Spawnable_" + F->getName()).str() };

    // add a new function to M
    Function *spF{ cast<Function>(
        Function::Create(fTy, Function::InternalLinkage, name, &M)) };

    BasicBlock *BB{ BasicBlock::Create(c, "entry", spF) };
    
    auto &spFArgList = spF->getArgumentList();

    // cast all of spF's args to F's args
    std::vector<Value *> castArgs;
    for (auto fArgIt = fArgList.begin(), spFArgIt = spFArgList.begin();
         fArgIt != fArgList.end(); ++fArgIt, ++spFArgIt) {
      Type *ty{ fArgIt->getType() };
      
      DEBUG(dbgs() << "Dealing with argument of type ");
      DEBUG(ty->print(dbgs()));
      DEBUG(dbgs() << "\n");

      // check if ty is a pointer type; if not, need to do a load
      if (ty->isPointerTy()) {
        castArgs.push_back(new BitCastInst{ &*spFArgIt, ty, "", BB });
      } else {
        auto bc =
            new BitCastInst{ &*spFArgIt, PointerType::getUnqual(ty), "", BB };
        castArgs.push_back(new LoadInst{ bc, "", BB });
      }
    }
    
    // call F
    auto call = CallInst::Create(F, castArgs, "", BB);

    if (returnsVal) {
      DEBUG(dbgs() << "Dealing with return of type ");
      DEBUG(F->getReturnType()->print(dbgs()));
      DEBUG(dbgs() << "\n");

      auto bc = new BitCastInst{
        &spFArgList.back(), PointerType::getUnqual(F->getReturnType()), "", BB
      };
      new StoreInst{ call, bc, BB };
    }

    ReturnInst::Create(c, nullptr, BB); // ret void

    addSpawnableFun(F, spF);
    ++NumSpawnable;
  }
  return !functions.empty();
}

void MakeSpawnable::releaseMemory() {
  funsToSpawnableFuns.clear();
  spawnableFuns.clear();
}

static RegisterPass<MakeSpawnable>
X("makespawnable", "Generation of Spawnable functions", false, true);
