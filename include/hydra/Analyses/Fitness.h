#ifndef HYDRA_FITNESS_H
#define HYDRA_FITNESS_H

#include <map>
#include "llvm/Pass.h"

// llvm forward declare
namespace llvm {
  class CallGraphNode;
}

namespace hydra {
  class Fitness : public llvm::ModulePass {
  public:
    static char ID;
    Fitness() : llvm::ModulePass(ID) {}
    virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
    virtual bool runOnModule(llvm::Module &M) override;
    virtual void releaseMemory() override;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const
        override;
    enum class FunType {
      Functional, // Pass all args byval and returns a val, no globals.
      Unknown     // Anything goes.
    };
    FunType getFunType(const llvm::Function &F) const;
    bool isFunctional(const llvm::Function &F) const;

  private:
    bool callsUnknownFunction(const llvm::CallGraphNode &CGN) const;
    std::map<const llvm::Function *, FunType> funTypes;
  };
}

inline hydra::Fitness::FunType
hydra::Fitness::getFunType(const llvm::Function &F) const {
  auto it = funTypes.find(&F);
  return (it != funTypes.end() ? it->second : FunType::Unknown);
}

inline bool hydra::Fitness::isFunctional(const llvm::Function &F) const {
  return getFunType(F) == FunType::Functional;
}

#endif
