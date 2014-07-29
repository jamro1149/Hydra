#include "hydra/Analyses/Fitness.h"
#include "hydra/Analyses/Profitability.h"
#include "hydra/Support/ForEachSCC.h"
#include "hydra/Support/FunAlgorithms.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace hydra;

char Profitability::ID = 0;

void Profitability::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.setPreservesAll();
  Info.addRequired<Fitness>();
  Info.addRequired<CallGraph>();
  Info.addRequired<LoopInfo>();
  Info.addRequired<ScalarEvolution>();
}

bool Profitability::runOnModule(Module &M) {
  DEBUG(dbgs() << "Profitability::runOnModule()\n");

  auto &CG = getAnalysis<CallGraph>();

  for_each_scc([this](CallGraphSCC &SCC) { processSCC(SCC); }, CG);

  return false;
}

void Profitability::releaseMemory() {
  statsMap.clear();
}

void Profitability::print(raw_ostream &O, const Module *) const {
  O << "Printing stats of " << statsMap.size() << " functions\n\n";
  for (const auto &pair : statsMap) {
    O << "Function " << pair.first->getName() << " has:\n";
    pair.second.print(O);
    O << "\n";
  }
}

void Profitability::FunStats::print(raw_ostream &O) const {
  O << numInstructions << " IR instructions\n";
  O << numEmittingInsts << " IR instructions which emit code\n";
  O << numMemAccesses << " memory accesses\n";
  for (const auto &p : numFunctionCalls) {
    O << p.first->getName() << "() is called " << p.second <<
      (p.second == 1 ? " time\n" : " times\n");
  }
  O << totalCost << " totalCost\n";
  O << (spawnable ? "is" : "is not") << " spawnable\n";
}

// calculate fun stats, except for totalCost and spawnable
static Profitability::FunStats
calculateFunStats(const Function &F, const LoopInfo &LI, ScalarEvolution &SE) {
  DEBUG(dbgs() << "Protiability::FunStats::calculateFunStates()\n");
  // initialise the return value with all zero data
  Profitability::FunStats ret{};
  for (const auto &BB : F) {
    unsigned bbInsts = 0u;
    unsigned bbEmittingInsts = 0u;
    unsigned bbMemAccesses = 0u;
    std::map<Function *, unsigned> bbFunCalls;
    for (const auto &I : BB) {
      ++bbInsts;
      if (isEmittingInst(I)) ++bbEmittingInsts;
      if (isMemoryAccess(I)) ++bbMemAccesses;
      if (const auto *ci = dyn_cast<CallInst>(&I)) {
        ++bbFunCalls[ci->getCalledFunction()];
      }
    }

    // if the BB is inside a loop, multiply instructions by the trip count
    if (Loop *l = LI.getLoopFor(&BB)) {
      unsigned tripCount = SE.getSmallConstantTripCount(l, nullptr);
      DEBUG(dbgs() << tripCount << " is the tripCount for this BB\n");
      if (tripCount > 0u) {
        // the trip count is known! multiply all our numbers by it
        // FIXME: should really use saturating multiplication here
        bbInsts *= tripCount;
        bbEmittingInsts *= tripCount;
        bbMemAccesses *= tripCount;
        for (auto &p : bbFunCalls) {
          p.second *= tripCount;
        }
      }
    }

    // add basic block values to function aggregates
    ret.numInstructions += bbInsts;
    ret.numEmittingInsts += bbEmittingInsts;
    ret.numMemAccesses += bbMemAccesses;
    for (auto &p : bbFunCalls) {
      ret.numFunctionCalls[p.first] += p.second;
    }
  }
  return ret;
}

void Profitability::processSCC(const CallGraphSCC &SCC) {
  DEBUG(dbgs() << "Profitability::ProcessSCC()\n");

  // work out the 'initial' cost of all nodes in the SCC, excluding recursion
  for (const auto *node : SCC) {
    auto *fun = node->getFunction();
    if (!fun) {
      DEBUG(
          dbgs() << "Warning: fun == nullptr in Profitability::ProcessSCC()\n");
      continue;
    }

    // if fun is just a declaration, be conservative
    if (fun->empty()) {
      statsMap[fun] = FunStats{};
    } else {

      // get FunStats with numInsts, numEmittingInsts and numMemAccesses
      // precalculated
      auto fs = calculateFunStats(*fun, getAnalysis<LoopInfo>(*fun),
                                  getAnalysis<ScalarEvolution>(*fun));

      // calculate total cost
      fs.totalCost = fs.numEmittingInsts;
      for (const auto &p : fs.numFunctionCalls) {
        if (FunStats *calledStats = getFunStats(*p.first)) {
          fs.totalCost += (calledStats->totalCost * p.second);
        } // else it was a (mutually) recursive call
      };

      auto &Fit = getAnalysis<Fitness>();
      fs.spawnable = (Fit.getFunType(*fun) == Fitness::FunType::Functional);

      statsMap[fun] = std::move(fs);
    }
  }

  // handle all self-recursive and mutually-recursive calls
  std::map<const Function *, unsigned> extraRecursiveCost{};
  for (const auto *callerNode : SCC) {
    // check that this isn't a "fake node"
    const auto *caller = callerNode->getFunction();
    if (!caller) {
      continue;
    }

    const auto *funStats = getFunStats(*caller);
    assert(funStats && "Node should have been handled earlier!");
    const auto &funsCalled = funStats->numFunctionCalls;

    for (const auto *calleeNode : SCC) {
      // check that this isn't a "fake node"
      const auto *callee = calleeNode->getFunction();
      if (!callee) {
        continue;
      }

      const auto calleeIter = funsCalled.find(callee);
      if (calleeIter != funsCalled.end()) {
        extraRecursiveCost[caller] +=
            (getFunStats(*calleeIter->first)->totalCost * calleeIter->second);
      }
    }
  }

  for (const auto &p : extraRecursiveCost) {
    getFunStats(*p.first)->totalCost += p.second;
  }
}

static RegisterPass<Profitability>
X("profitability",
  "Profitability of Function Spawning Analysis", false, true);
