#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraphSCCPass.h"

namespace hydra {
  template<typename Fun> void for_each_scc(Fun f, llvm::CallGraph &CG) {
    auto CGI = llvm::scc_begin(&CG);
    llvm::CallGraphSCC CurrSCC{ &CGI };

    // iterate over the SCCs, calling f() on each
    for (; !CGI.isAtEnd(); ++CGI) {
      auto &NodeVec = *CGI;
      CurrSCC.initialize(&NodeVec[0], &NodeVec[0]+NodeVec.size());
      f(CurrSCC);
    }
  }
}
