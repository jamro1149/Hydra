#include "hydra/Analyses/Fitness.h"
#include "hydra/Analyses/FunArgInfo.h"
#include "hydra/Support/ForEachSCC.h"
#include "hydra/Support/TargetMacros.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace hydra;
using bb_iter = BasicBlock::iterator;

// Find all of the "returned" arguments from a function call.
// I.e. those which are passed by pointer and are not tagged with 'byval'.
// This function is deprecared.
/*
static std::vector<Value *> getPointerArguments(CallInst *CI) {
  std::vector<Value *> ret;

  Function *fun = CI->getCalledFunction();

  std::for_each(fun->arg_begin(), fun->arg_end(), [&](Argument &arg) {
    if (!isa<Constant>(arg) && arg.getType()->isPointerTy() &&
        !arg.hasByValAttr()) {
      ret.push_back(&arg);
    }
  });

  return ret;
}
*/

static Instruction *findJoinPoint(CallInst *ci, const bb_iter I,
                                  const bb_iter E, AliasAnalysis &AA) {
  DEBUG(dbgs() << "findJoinPoint()\n");

  const bool returnsVal{ ci->getCalledFunction()->getReturnType() !=
                         Type::getVoidTy(ci->getContext()) };

  Value *retVal{ returnsVal ? ci : nullptr };

  auto join = std::find_if(I, E, [&](Instruction &inst) {
    return std::any_of(inst.value_op_begin(), inst.value_op_end(),
                       [=](Value *v1) { return v1 == retVal; });
  });

  return (join != E ? &*join : nullptr);
}

static std::set<Instruction *> findJoinPoints(CallInst *ci, AliasAnalysis &AA) {
  DEBUG(dbgs() << "findJoinPoints()\n");
  std::set<Instruction *> ret;
  auto *const spawnBlock = ci->getParent();

  // early exit: if the join is in the spawn block, just return singleton set
  // note: need ++iter(ci) so that the CallIsnt is outside the range
  if (auto *join = findJoinPoint(ci, ++bb_iter{ ci }, spawnBlock->end(), AA)) {
    DEBUG(dbgs() << "Early exit: join was trivial.\n");
    ret.insert(join);
    return ret;
  } else // kernel threads must always have join at the end
#if !KERNEL_THREADS
      if (spawnBlock->getTerminator()->getNumSuccessors() == 0)
#endif
  {
    DEBUG(dbgs() << "Early exit: spawn is in exit block.\n");
    ret.insert(spawnBlock->getTerminator());
    return ret;
  }

#if !KERNEL_THREADS

  std::deque<BasicBlock *> blocksToExplore;
  std::set<BasicBlock *> exploredBlocks;

  auto addBlocks = [&](BasicBlock *bb) { blocksToExplore.push_back(bb); };

  std::for_each(succ_begin(spawnBlock), succ_end(spawnBlock), addBlocks);

  while (!blocksToExplore.empty()) {
    auto *currBlock = blocksToExplore.front();
    blocksToExplore.pop_front();

    // if already seen block, discard it
    if (exploredBlocks.count(currBlock) > 0) {
      DEBUG(dbgs() << "Block already seen\n");
      continue;
    }

    // if currBlock == spawnBlock, want to scan until ci
    auto end = (currBlock == spawnBlock) ? bb_iter{ ci } : currBlock->end();

    // check if should join in block, else look at successor blocks, unless
    // there are none
    if (auto *join = findJoinPoint(ci, currBlock->begin(), end, AA)) {
      DEBUG(dbgs() << "Found a joinpoint!\n");
      ret.insert(join);
    } else if (currBlock->getTerminator()->getNumSuccessors() > 0) {
      DEBUG(dbgs() << "No join point, adding successors to blocksToExplore\n");
      std::for_each(succ_begin(currBlock), succ_end(currBlock), addBlocks);
    } else {
      DEBUG(dbgs() << "No join point, adding terminator\n");
      ret.insert(currBlock->getTerminator());
    }

    exploredBlocks.insert(currBlock);
  }

  assert(!ret.empty());
  return ret;

#endif
}

// LLVM uses &Hello::ID to identify the pass, so its value is unimportant
char FunArgInfo::ID = 0;

void FunArgInfo::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.setPreservesAll();
  Info.addRequired<Fitness>();
  Info.addRequiredTransitive<AliasAnalysis>();
  Info.addRequired<CallGraph>();
}

//bool FunArgInfo::doInitialization(Module &M) {
//  return false;
//}

bool FunArgInfo::runOnModule(Module &M) {
  DEBUG(dbgs() << "FunArgInfo::runOnModule()\n");

  auto &CG = getAnalysis<CallGraph>();

  for_each_scc([this](CallGraphSCC &SCC) { processSCC(SCC); }, CG);

  return false;
}

void FunArgInfo::releaseMemory() {
  joinPoints.clear();
}

void FunArgInfo::print(raw_ostream &O, const Module *) const {
  O << "Printing joinpoints of " << joinPoints.size() << " CallInsts\n\n";
  for (const auto &pair : joinPoints) {
    pair.first->print(O);
    O << "\nhas join point(s) at\n";
    for (auto *i : pair.second) {
      i->print(O);
      O << "\n";
    }
    O << "\n\n";
  }
}

void FunArgInfo::processSCC(const CallGraphSCC &SCC) {
  DEBUG(dbgs() << "FunArgInfo::processSCC()\n");

  auto &AA = getAnalysis<AliasAnalysis>();
  auto &Fit = getAnalysis<Fitness>();

  for (const auto *node : SCC) {
    Function *fun{ node->getFunction() };
    if (!fun) {
      DEBUG(dbgs() << "Warning: fun == nullptr in FunArgInfo::processSCC()\n");
      continue;
    }

    for (auto &BB : *fun) {
      for (auto RI = BB.rbegin(), RE = BB.rend(); RI != RE; ++RI) {
        if (RI->getOpcode() == Instruction::Call) {
          CallInst *CI = cast<CallInst>(&*RI);
          if (Fit.isFunctional(*CI->getCalledFunction())) {
            joinPoints.emplace_back(cast<CallInst>(&*RI),
                                    findJoinPoints(CI, AA));
          }
        }
      }
    }
  }
}

static RegisterPass<FunArgInfo> X("joinpoints", "Function Argument Analysis",
                                  false, true);
