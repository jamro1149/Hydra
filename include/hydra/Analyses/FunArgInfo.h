#ifndef HYDRA_FUN_ARG_INFO_H
#define HYDRA_FUN_ARG_INFO_H

#include <algorithm>
#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "hydra/Support/KeyIterator.h"

// llvm forward declares
namespace llvm {
  class CallGraphSCC;
  class CallInst;
  class Instruction;
}

namespace hydra {
class FunArgInfo : public llvm::ModulePass {
public:
  static char ID;
  FunArgInfo() : ModulePass(ID) {}
  virtual void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
  virtual bool runOnModule(llvm::Module &M) override;
  virtual void releaseMemory() override;
  virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const
      override;
  std::set<llvm::Instruction *> getJoinPoints(llvm::CallInst *CI);

private:
  void processSCC(const llvm::CallGraphSCC &SCC);

  // CallIsnts should be ordered with callees before callers, and later
  // calls in a function should appear before earlier ones.
  std::vector<std::pair<llvm::CallInst *, std::set<llvm::Instruction *>>>
  joinPoints;

  // iterators
public:
  using iterator = decltype(joinPoints.begin());
  iterator begin() { return joinPoints.begin(); }
  iterator end() { return joinPoints.end(); }

  using key_iterator = ::hydra::key_iterator<decltype(joinPoints.begin())>;
  key_iterator key_begin() { return joinPoints.begin(); }
  key_iterator key_end() { return joinPoints.end(); }
};
} // namespace hydra

inline std::set<llvm::Instruction *>
hydra::FunArgInfo::getJoinPoints(llvm::CallInst *CI) {
  auto it = std::find_if(
      joinPoints.begin(), joinPoints.end(),
      [CI](std::pair<llvm::CallInst *, std::set<llvm::Instruction *>> &p) {
        return p.first == CI;
      });
  // use the empty set as an error value
  return (it != joinPoints.end() ? it->second
                                 : std::set<llvm::Instruction *>{});
}

#endif
