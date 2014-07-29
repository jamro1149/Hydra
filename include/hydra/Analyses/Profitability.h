#ifndef HYDRA_PROFITABILITY_H
#define HYDRA_PROFITABILITY_H

#include "llvm/Pass.h"
#include "hydra/Support/KeyIterator.h"

namespace llvm {
  class CallGraphSCC;
}

namespace hydra {
  class Profitability : public llvm::ModulePass {
  public:
    static char ID;
    Profitability() : llvm::ModulePass(ID) {}
    virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
    virtual bool runOnModule(llvm::Module &M) override;
    virtual void releaseMemory() override;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const
        override;
    struct FunStats {
      unsigned numInstructions; // the number of IR instructions in the function
      unsigned numEmittingInsts; // number which emit code (not BitCast or Phi)
      unsigned numMemAccesses; // number of loads/stores
      std::map<const llvm::Function *, unsigned> numFunctionCalls;
      unsigned totalCost; // aggregate emmitting insts of all this and callees
      bool spawnable; // is it spawnable? (remember so we can pass results on)
      FunStats()
          : numInstructions(0u), numEmittingInsts(0u), numMemAccesses(0u),
            totalCost(0u), spawnable(false) {}
      void print(llvm::raw_ostream &O) const;
    };
    FunStats *getFunStats(const llvm::Function &F);
    const FunStats *getFunStats(const llvm::Function &F) const;

  private:
    void processSCC(const llvm::CallGraphSCC &SCC);
    void processFunction(const llvm::Function &F);
    void handleRecursiveCost(const llvm::CallGraphSCC &SCC);
    std::map<const llvm::Function *, FunStats> statsMap;

  public:
    // iterators
    using iterator = key_iterator<decltype(statsMap.begin())>;

    iterator begin() { return iterator{ statsMap.begin() }; }
    iterator end() { return iterator{ statsMap.end() }; }
  };
}

inline hydra::Profitability::FunStats *
hydra::Profitability::getFunStats(const llvm::Function &F) {
  auto it = statsMap.find(&F);
  return (it != statsMap.end() ? &it->second : nullptr);
}

inline const hydra::Profitability::FunStats *
hydra::Profitability::getFunStats(const llvm::Function &F) const {
  auto it = statsMap.find(&F);
  return (it != statsMap.end() ? &it->second : nullptr);
}

#endif
