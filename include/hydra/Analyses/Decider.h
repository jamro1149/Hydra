#ifndef HYDRA_DECIDER_H
#define HYDRA_DECIDER_H

#include <map>
#include <set>
#include "llvm/Pass.h"

// llvm forward declares
namespace llvm {
  class CallInst;
  class Instruction;
}

namespace hydra {
  class Decider : public llvm::ModulePass {
  public:
    static char ID;
    Decider() : llvm::ModulePass(ID) {}
    virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
    virtual bool runOnModule(llvm::Module &M) override;
    virtual void releaseMemory() override;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const
        override;
    std::set<llvm::Instruction *> getJoinsIfSpawnable(llvm::CallInst &CI); 

  private:
    std::set<llvm::Function *> funsToBeSpawned;
    std::map<llvm::CallInst *, std::set<llvm::Instruction *>>
    profitableJoinPoints;

    // iterators
  public:
    using iterator = decltype(funsToBeSpawned.begin());
    iterator begin() { return funsToBeSpawned.begin(); }
    iterator end() { return funsToBeSpawned.end(); }

    using join_iterator = decltype(profitableJoinPoints.begin());
    join_iterator join_begin() { return profitableJoinPoints.begin(); }
    join_iterator join_end() { return profitableJoinPoints.end(); }
  };
}

inline std::set<llvm::Instruction *>
hydra::Decider::getJoinsIfSpawnable(llvm::CallInst &CI) {
  auto it = profitableJoinPoints.find(&CI);
  // use empty set as a failure value
  return (it != profitableJoinPoints.end() ? it->second
                                           : std::set<llvm::Instruction *>{});
}

#endif
