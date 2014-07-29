#include <algorithm>
#include <string>
#include "hydra/Analyses/Fitness.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstIterator.h"

using namespace llvm;
using namespace hydra;

char Fitness::ID = 0;

void Fitness::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<CallGraph>();
  Info.setPreservesAll();
}

// helper functions for runOnSCC:

static bool hasPointerArgs(const Function &F) {
  return std::any_of(F.arg_begin(), F.arg_end(), [](const Argument &arg) {
    return arg.getType()->isPointerTy();
  });
}

// currently not used...
//
//static bool hasNonConstPointerArgs(const Function &F) {
//  return std::any_of(F.arg_begin(), F.arg_end(), [](const Argument &arg) {
//    return arg.getType()->isPointerTy() && !arg.onlyReadsMemory() &&
//           !arg.hasByValAttr();
//  });
//}

static bool referencesGlobalVariables(const Function &F) {
  return std::any_of(inst_begin(F), inst_end(F), [](const Instruction &I) {
    return std::any_of(I.op_begin(), I.op_end(), [](const Use &U) {
      return isa<GlobalVariable>(U) || isa<GlobalAlias>(U);
    });
  });
}

bool Fitness::callsUnknownFunction(const CallGraphNode &CGN) const {
  DEBUG(dbgs() << "Fitness::callsUnknownFunction()\n");
  DEBUG(CGN.print(dbgs()));
  return std::any_of(CGN.begin(), CGN.end(),
                     [this](const CallGraphNode::CallRecord &CR) {
    DEBUG(dbgs() << "Checking another CallRecord\n");
    assert(CR.second);
    auto fun = CR.second->getFunction();
    // if fun is nullptr, it is external
    return (!fun || getFunType(*fun) == FunType::Unknown);
  });
}

bool Fitness::runOnModule(Module &M) {
  DEBUG(dbgs() << "Fitness::runOnModule()\n");
  
  CallGraph &CG{ getAnalysis<CallGraph>() };

  DEBUG(CG.print(dbgs(), nullptr));
  
  // initialisation
  for (const Function &F : M) {
    funTypes[&F] =
        (hasPointerArgs(F) || referencesGlobalVariables(F) || F.isVarArg())
            ? FunType::Unknown
            : FunType::Functional;
  }

  // iteration
  bool changesMade;
  do {
    DEBUG(dbgs() << "Doing another iteration.\n");
    changesMade = false;
    for (auto p : CG) {
      DEBUG(dbgs() << "Iterating though another CallRecord\n");
      const Function *fun = p.first;
      if (!fun) {
        DEBUG(dbgs() << "Warning: fun == nullptr in Fitness::runOnModule\n");
        continue;
      }
      assert(p.second);
      if (getFunType(*fun) == FunType::Functional &&
          callsUnknownFunction(*p.second)) {
        funTypes[fun] = FunType::Unknown;
        changesMade = true;
      }
    }
  } while (changesMade);

  DEBUG(dbgs() << "Finished iterating.\n");

  return false;
}

void Fitness::releaseMemory() {
  funTypes.clear();
}

static std::string funTypeToString(Fitness::FunType f) {
  switch (f) {
  case Fitness::FunType::Functional:
    return "Functional";
  case Fitness::FunType::Unknown:
    return "Unknown";
  }
}

void Fitness::print(raw_ostream &O, const Module *) const {
  DEBUG(dbgs() << "Fitness::print()\n");
  O << "Printing info for " << funTypes.size() << " functions\n";
  for (auto &pair : funTypes) {
    O << "The function "; 
    O << pair.first->getName();
    O << "() is ";
    O << funTypeToString(pair.second);
    O << "\n";
  }
}

static RegisterPass<Fitness>
X("fitness", "Function Fitness for Spawning Analysis", false, true);
