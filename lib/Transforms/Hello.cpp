#define DEBUG_TYPE "hello"

// STL includes
#include <algorithm>
#include <map>
#include <random>
#include <set>

// hydra includes
#include "hydra/Analyses/Decider.h"
#include "hydra/Transforms/MakeSpawnable.h"
#include "hydra/Support/FunAlgorithms.h"
#include "hydra/Support/PrintCollection.h"
#include "hydra/Support/TargetMacros.h"

// llvm includes
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

STATISTIC(NumCallsParallelised, "Number of calls parallelised");

using namespace llvm;
using namespace hydra;

namespace {
  class Hello : public ModulePass {
  public:
    static char ID;
    Hello();
    virtual void getAnalysisUsage(AnalysisUsage &Info) const override;
    virtual bool runOnModule(Module &M) override;
  private:
    void generateCtors(const std::set<unsigned> &arities, Module &M,
                       const unsigned maxargs);
    void generateJoinAndDtor(Module &M);
    bool isNotJoinOrDtor(CallInst *ci) const;
    void createThread(CallInst *ci, Function *spawnableFun,
                      const std::set<Instruction *> &joinPoints);
    std::vector<Value *> genSpawnArgs(CallInst *ci, Function *spawnableFun,
                                      AllocaInst *&retVal);
    void createJoins(const std::set<Instruction *> &joinPoints, Value *id);
    void handleReturnValue(CallInst *ci, AllocaInst *retVal);
    std::map<unsigned, Constant *> ctors; // ctors indexed by arity
    Constant *join;
#if KERNEL_THREADS
    Constant *dtor;
    Type *threadTy;
#elif LIGHT_THREADS
    std::mt19937 twister; // for generating taskIDs
#endif
  };
}

// LLVM uses &Hello::ID to identify the pass, so its value is unimportant
char Hello::ID = 0;

//------------------------------------------------------------------------------
Hello::Hello() : ModulePass { ID }
#if LIGHT_THREADS
, twister{ std::random_device{}() }
#endif
{}

//------------------------------------------------------------------------------
void Hello::getAnalysisUsage(AnalysisUsage &Info) const {
  Info.addRequired<Decider>();
  Info.addRequired<MakeSpawnable>();
}

//------------------------------------------------------------------------------
bool Hello::runOnModule(Module &M) {
  DEBUG(dbgs() << "Hello::runOnModule()\n");

  auto &D = getAnalysis<Decider>();
  auto &MS = getAnalysis<MakeSpawnable>();

#if KERNEL_THREADS
  // define the type of std::thread
  LLVMContext &c{ M.getContext() };
  Type *ts[1];
  ts[0] = Type::getInt64Ty(c);
  StructType *threadIDTy = StructType::create(c, ts, "class.std::thread::id");

  ts[0] = threadIDTy;
  threadTy = StructType::create(c, ts, "class.std::thread");
#endif

  std::set<unsigned> arities;
  for (const auto F : MS) {
    arities.insert(F->getArgumentList().size());
  }

  DEBUG(printCollection(arities, dbgs(), "Arities"));

  // early exit - if arities is empty, spawn nothing
  if (arities.size() == 0u) {
    DEBUG(dbgs() << "Early exit: nothing to spawn.\n");
    return false;
  }
  const unsigned maxargs{ *--arities.end() };

  generateCtors(arities, M, maxargs);
  generateJoinAndDtor(M);
  
  std::for_each(D.join_begin(), D.join_end(),
                [&](decltype(*D.join_begin()) pair) {
    auto *spawnableFun = MS.getSpawnableFun(*pair.first->getCalledFunction());
    assert(spawnableFun && "Spawnable function not found in MakeSpawnable!");
    createThread(pair.first, spawnableFun, pair.second);
    ++NumCallsParallelised;
    pair.first->eraseFromParent();
  });

  return true;
}

//------------------------------------------------------------------------------
void Hello::generateCtors(const std::set<unsigned> &arities, Module &M,
                          const unsigned maxargs) {
  LLVMContext &c{ M.getContext() };

  // kernal threads sig: (std::thread *, function *, args *...)
  // lightw threads sig: (task, function *, args *...)
  // baseArgs is num of args to spawn a 0-arg function
#if KERNEL_THREADS || LIGHT_THREADS
  constexpr unsigned baseArgs = 2u;
#endif

  std::vector<Type *> ctorSig(baseArgs);

#if KERNEL_THREADS
  ctorSig[0] = PointerType::getUnqual(threadTy);
#elif LIGHT_THREADS
  ctorSig[0] = Type::getInt32Ty(c);
#endif

  Type *voidStarTy{ PointerType::getUnqual(Type::getInt8Ty(c)) };

  // build the function name from 4 pieces
  // s1 and s2 will grow with additional arguments
  // the function name depends on the threading model
#if KERNEL_THREADS
  const std::string s0{ "_ZNSt6threadC2IRFv" };
  const std::string s1{ "P" };
  std::string s2{ "v" };
  const std::string s3{ "EJ" };
  std::string s4{ "RS1_" };
  const std::string s5{ "EEEOT_DpOT0_" };
  const std::string deltaS2{ "S1_" };
  const std::string deltaS4{ "S4_" };
  Type *ctorArgTy{ PointerType::getUnqual(voidStarTy) };
#elif LIGHT_THREADS
  const std::string s0{ "_Z5spawnjPFv" };
  const std::string s1{ "P" };
  std::string s2{ "v" };
  const std::string s3{ "E" };
  std::string s4{ "S_" };
  const std::string s5{ "" };
  const std::string deltaS2{ "S_" };
  const std::string deltaS4{ "S_" };
  Type *ctorArgTy{ voidStarTy };
#endif

  std::vector<Type *> args;

  // s1 and s4 are not present in the first name
  std::string name = s0 + s2 + s3 + s5;

  for (unsigned i = 0u; i <= maxargs; ++i) {
    if (arities.count(i)) {
      ctorSig[baseArgs - 1u] = PointerType::getUnqual(
          FunctionType::get(Type::getVoidTy(c), args, false));
      FunctionType *ctorTy =
          FunctionType::get(Type::getVoidTy(c), ctorSig, false);
      ctors[i] = M.getOrInsertFunction(name, ctorTy);
    }

    // extend ctor's signature
    ctorSig.push_back(ctorArgTy);

    // extend the name
    s2 += deltaS2;
    s4 += deltaS4;
    name = s0 + s1 + s2 + s3 + s4 + s5;

    // extend f's signature
    args.push_back(voidStarTy);
  }
}

//------------------------------------------------------------------------------
void Hello::generateJoinAndDtor(Module &M) {
  LLVMContext &c{ M.getContext() };
  Type *ts[1];

#if LIGHT_THREADS

  ts[0] = Type::getInt32Ty(c);
  FunctionType *joinTy = FunctionType::get(Type::getVoidTy(c), ts, false);
  join = M.getOrInsertFunction("_Z4joinj", joinTy);

#elif KERNEL_THREADS

  // the dtor signature is (std::thread *)
  ts[0] = PointerType::getUnqual(threadTy);
  FunctionType *dtorTy = FunctionType::get(Type::getVoidTy(c), ts, false);

  // join has same signature as dtor
  join = M.getOrInsertFunction("_ZNSt6thread4joinEv", dtorTy);
  dtor = M.getOrInsertFunction("_ZNSt6threadD2Ev", dtorTy);

#endif
}

//------------------------------------------------------------------------------
inline bool Hello::isNotJoinOrDtor(CallInst *ci) const {
  Function *fun = ci->getCalledFunction();
  return (fun != join)
#if KERNEL_THREADS
    && (fun != dtor)
#endif
    ;
}

//------------------------------------------------------------------------------
void Hello::createThread(CallInst *ci, Function *spawnableFun,
                         const std::set<Instruction *> &joinPoints) {
  DEBUG(dbgs() << "Hello::createThread()\n");

  AllocaInst *retVal{ nullptr };
  auto args = genSpawnArgs(ci, spawnableFun, retVal);

  const auto numArgs = spawnableFun->getArgumentList().size();

  assert(args.size() == numArgs + 2u && "Number of args differs!");

  auto ctorIt = ctors.find(numArgs);

  assert(ctorIt != ctors.end() && "no ctor in ctors!");
  assert(cast<Function>(ctorIt->second)->getArgumentList().size() ==
             args.size() &&
         "wrong ctor in ctors!");

  CallInst::Create(ctorIt->second, args, "", ci);

  // the join function needs the first ctor arg
  std::vector<Value *> jargs{ args[0] };
  Value *id{ args[0] };
  createJoins(joinPoints, id);

  // if the functions returned a value, swap all uses of that value with the
  // value returned by our spawned thread, which is at the address of retVal
  if (retVal) {
    handleReturnValue(ci, retVal);
  }
}

//------------------------------------------------------------------------------
std::vector<Value *> Hello::genSpawnArgs(CallInst *callInst,
                                         Function *spawnableFun,
                                         AllocaInst *&retVal) {
  DEBUG(dbgs() << "Hello::genSpawnArgs()\n");

  LLVMContext &c{ callInst->getContext() };

  Type *voidStarTy{ PointerType::getUnqual(Type::getInt8Ty(c)) };

  // in kernel threads, need to pass the threadID as an arg
  // in light threads, need a "task" (random number) as an arg
#if KERNEL_THREADS
  auto *allocaThread = new AllocaInst(threadTy, "t", callInst);
  std::vector<Value *> args{ allocaThread, spawnableFun };
#elif LIGHT_THREADS
  std::vector<Value *> args{
    ConstantInt::get(Type::getInt32Ty(c),
                     std::uniform_int_distribution<unsigned>{}(twister)),
    spawnableFun
  };
#endif

  for (unsigned i = 0u, e = callInst->getNumArgOperands(); i < e; ++i) {
    Value *arg = callInst->getArgOperand(i);
    Type *argTy = arg->getType();

    // if arg is not a pointer, need to store it so we can take its address
    Value *castarg;
    if (argTy->isPointerTy()) {
      castarg = arg;
    } else {
      castarg = new AllocaInst(argTy, "", callInst);
      new StoreInst(arg, castarg, callInst);
    }

    assert(castarg->getType()->isPointerTy());
    auto bc = new BitCastInst(castarg, voidStarTy, "", callInst);

    // for light threads, we just use bc
#if LIGHT_THREADS
    args.push_back(bc);

    // for kernel threads, need to store castarg to get a void **
#elif KERNEL_THREADS
    auto argVal = new AllocaInst(voidStarTy, "", callInst);
    new StoreInst(bc, argVal, callInst);

    args.push_back(argVal);

#endif
  }

  Type *returnTy{ callInst->getCalledFunction()->getReturnType() };
  const bool returnsVal{ returnTy != Type::getVoidTy(c) };
  if (returnsVal) {
    retVal = new AllocaInst(returnTy, "", callInst);
    auto bc = new BitCastInst(retVal, voidStarTy, "", callInst);
    // in kernel threads, need to store bc to get a void **
#if KERNEL_THREADS
    auto retArg = new AllocaInst(voidStarTy, "", callInst);
    new StoreInst(bc, retArg, callInst);
    args.push_back(retArg);
#elif LIGHT_THREADS
    args.push_back(bc);
#endif
  }

  return args;
}

//------------------------------------------------------------------------------
void Hello::createJoins(const std::set<Instruction *> &joinPoints, Value *id) {
  Value *jargs[] = { id };

  for (auto *joinPoint : joinPoints) {
    CallInst::Create(join, jargs, "", joinPoint);
#if KERNEL_THREADS
    CallInst::Create(dtor, jargs, "", joinPoint);
#endif
  }
}

//------------------------------------------------------------------------------
void Hello::handleReturnValue(CallInst *ci, AllocaInst *retVal) {
  DEBUG(dbgs() << "Hello::handleReturnValue()\n");
  assert(retVal);

  std::vector<User *> oldUsersOfCall{};
  oldUsersOfCall.reserve(std::distance(ci->use_begin(), ci->use_end()));
  std::for_each(ci->use_begin(), ci->use_end(),
                [&](User *u) { oldUsersOfCall.push_back(u); });

  for (User *u : oldUsersOfCall) {
    auto *load = new LoadInst{ retVal, "retVal", cast<Instruction>(u) };
    std::transform(u->op_begin(), u->op_end(), u->op_begin(),
                   [=](Value * v)->Value * {
      if (v == ci) {
        return load;
      } else {
        return v;
      }
    });
  }
}

//------------------------------------------------------------------------------
static RegisterPass<Hello> X("parallelisecalls", // command-line argument
                             "Hello World Pass", // name
                             false,              // only look at CFG?
                             false);             // Analysis Pass?
