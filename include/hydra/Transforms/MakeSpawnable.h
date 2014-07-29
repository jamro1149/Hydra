#ifndef HYDRA_MAKE_SPAWNABLE_H
#define HYDRA_MAKE_SPAWNABLE_H

#include <map>
#include <set>
#include "llvm/Pass.h"

namespace hydra {
  class MakeSpawnable : public llvm::ModulePass {
  public:
    static char ID;
    MakeSpawnable() : ModulePass(ID) {}
    virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
    virtual bool runOnModule(llvm::Module &M) override;
    virtual void releaseMemory() override;
    llvm::Function *getSpawnableFun(llvm::Function &F);
    bool isSpawnableFun(llvm::Function &F);

  private:
    void addSpawnableFun(llvm::Function *F, llvm::Function *spF);
    std::map<llvm::Function *, llvm::Function *> funsToSpawnableFuns;
    std::set<llvm::Function *> spawnableFuns;

  public: 
    // iterators
    using iterator = decltype(spawnableFuns.begin());
    iterator begin() { return spawnableFuns.begin(); }
    iterator end() { return spawnableFuns.end(); }    
 };
}

inline llvm::Function *
hydra::MakeSpawnable::getSpawnableFun(llvm::Function &F) {
  auto it = funsToSpawnableFuns.find(&F);
  return (it != funsToSpawnableFuns.end() ? it->second : nullptr);
}

inline bool hydra::MakeSpawnable::isSpawnableFun(llvm::Function &F) {
  return spawnableFuns.count(&F) > 0;
}

inline void hydra::MakeSpawnable::addSpawnableFun(llvm::Function *F,
                                                  llvm::Function *spF) {
  funsToSpawnableFuns[F] = spF;
  spawnableFuns.insert(spF);
}

#endif
