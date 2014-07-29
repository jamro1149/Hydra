// std includes
#include <cmath>

// boost includes
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/property_map/property_map.hpp>

// llvm includes
#include "llvm/Support/CFG.h"
#include "llvm/Support/Debug.h"

// hydra includes
#include "hydra/Analyses/Decider.h"
#include "hydra/Analyses/FunArgInfo.h"
#include "hydra/Analyses/Profitability.h"
#include "hydra/Support/FunAlgorithms.h"
#include "hydra/Support/PrintCollection.h"
#include "hydra/Support/TargetMacros.h"

using namespace llvm;
using namespace hydra;

//------------------------------------------------------------------------------
char Decider::ID = 0;

//------------------------------------------------------------------------------
void Decider::getAnalysisUsage(llvm::AnalysisUsage &Info) const {
  Info.setPreservesAll();
  Info.addRequired<Profitability>();
  Info.addRequired<FunArgInfo>();
}

using Graph = boost::adjacency_list<
    boost::listS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<boost::edge_weight_t, int>>;
using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using VSizeType = boost::graph_traits<Graph>::vertices_size_type;
using Edge = std::pair<int, int>;
using bb_citer = BasicBlock::const_iterator;

//------------------------------------------------------------------------------
static int countInstructions(bb_citer first, bb_citer last,
                             const Profitability &Profit) {
  DEBUG(dbgs() << "countInstructions()\n");
  int count = std::accumulate(first, last, 0,
                              [&](int runningTotal, const Instruction &i) {
    int extraInsts{ 1 };
    if (auto *ci = dyn_cast<CallInst>(&i)) {
      extraInsts += Profit.getFunStats(*ci->getCalledFunction())->totalCost;
    }
    return runningTotal + extraInsts;
  });
  DEBUG(dbgs() << "Count is " << count << "\n");
  return count;
}

//FIXME: this function is horrible! try and make shorter... :)
//------------------------------------------------------------------------------
static int genGraph(const CallInst *spawn, const std::set<Instruction *> &joins,
                    const Profitability &Profit, Graph &out_graph,
                    std::set<int> &out_joinVertices) {
  DEBUG(dbgs() << "Decider::genGraph()\n");

  // early exit: if we have a trivial join, tell the caller
  if (joins.size() == 1u &&
      spawn->getParent() == (*joins.begin())->getParent() &&
      inOrder(spawn, *joins.begin())) {
    return -1;
  }

  std::vector<Edge> edges;
  std::vector<int> weights;
  int numNodes{ 0 };
  // each entryInst, terminatorInst and joinInst is associated with a vertex
  std::map<const Instruction *, int> vertices;
  std::map<const BasicBlock *, const Instruction *> joinBlocks;

  auto getOrAddVertex = [&](const Instruction *i) {
    auto vertexIter = vertices.find(i);
    if (vertexIter == vertices.end()) {
      vertices.emplace(i, numNodes);
      return numNodes++;
    } else {
      return vertexIter->second;
    }
  };

  for (const auto *join : joins) {
    joinBlocks.emplace(join->getParent(), join);
    out_joinVertices.insert(getOrAddVertex(join));
  }

  auto addEdgeWithWeight = [&](int v1, int v2, int w) {
    edges.emplace_back(v1, v2);
    weights.push_back(w);
  };

  auto *const spawnBlock = spawn->getParent();
  std::deque<const BasicBlock *> blocksToExplore;
  std::set<const BasicBlock *> exploredBlocks;
  
  int entry = getOrAddVertex(spawn);
  int terminator = getOrAddVertex(&*spawnBlock->end());

  auto addSuccBlock = [&](const BasicBlock *successorBlock) {
    addEdgeWithWeight(terminator, getOrAddVertex(&successorBlock->front()), 0);
    blocksToExplore.push_back(successorBlock);
  };

  addEdgeWithWeight(
      entry, terminator,
      countInstructions(bb_citer{ spawn }, spawnBlock->end(), Profit));
  std::for_each(succ_begin(spawnBlock), succ_end(spawnBlock), addSuccBlock);


  while (!blocksToExplore.empty()) {
    auto *currBlock = blocksToExplore.front();
    blocksToExplore.pop_front();
    
    // if already seen block, discard it
    if (exploredBlocks.count(currBlock) > 0) {
      continue;
    }

    // check if there's a join in this block
    auto joinIter = joinBlocks.find(currBlock);
    const bool joinInBlock{ joinIter != joinBlocks.end() };

    int entry = getOrAddVertex(&currBlock->front());
    terminator = joinInBlock ? getOrAddVertex(joinIter->second)
                             : getOrAddVertex(currBlock->getTerminator());
    int cost = joinInBlock
                   ? countInstructions(currBlock->begin(),
                                       bb_citer{ joinIter->second }, Profit)
                   : (currBlock == spawnBlock
                          ? countInstructions(currBlock->begin(),
                                              bb_citer{ spawn }, Profit)
                          : countInstructions(currBlock->begin(),
                                              currBlock->end(), Profit));
    addEdgeWithWeight(entry, terminator, cost);

    if (!joinInBlock) {
      std::for_each(succ_begin(currBlock), succ_end(currBlock), addSuccBlock);
    }
  }

  DEBUG(printCollection(weights, dbgs(), "Weights"));

  out_graph = Graph{ edges.begin(),   edges.end(),
                     weights.begin(), static_cast<VSizeType>(numNodes) };
  return entry;
}

namespace {
enum class Aggregator { min, arithmeticMean, max };
}

//------------------------------------------------------------------------------
static unsigned getSpawnToJoinCost(const CallInst *spawn,
                                   const std::set<Instruction *> &joins,
                                   Profitability &Profit, Aggregator aggr) {
  DEBUG(dbgs() << "Decider::getSpawnToJoinCost()\n");

  Graph g;
  std::set<int> joinVertices;
  int spawnVertex = genGraph(spawn, joins, Profit, g, joinVertices);

  // if the join is trivial, return the trivial cost
  if (spawnVertex < 0) {
    return countInstructions(++bb_citer{ spawn }, bb_citer{ *joins.begin() },
                             Profit);
  }

  std::vector<int> distances(num_vertices(g));

  boost::dijkstra_shortest_paths(g, spawnVertex,
                                 boost::distance_map(distances.data()));
  DEBUG(printCollection(distances, dbgs(), "Distances"));

  std::vector<unsigned> joinDistances;
  joinDistances.reserve(joinVertices.size());
  DEBUG(dbgs() << "signed distances: ");
  for (int v : joinVertices) {
    DEBUG(dbgs() << v << " has " << distances[v] << ", ");
    joinDistances.push_back(distances[v]);
  }
  DEBUG(dbgs() << "\n");

  // aggregate the joinDistances using the inputed method
  switch (aggr) {
  case Aggregator::min:
    return *std::min_element(joinDistances.begin(), joinDistances.end());

  case Aggregator::arithmeticMean: {
    DEBUG(printCollection(joinDistances, dbgs(), "Costs"));

    const double acc =
        std::accumulate(joinDistances.begin(), joinDistances.end(), 0.0);
    DEBUG(dbgs() << "total cost is " << acc << "\n");
    const double mean = acc / joinDistances.size();
    DEBUG(dbgs() << "mean cost is " << mean << "\n");
    const double rounded = round(mean);
    DEBUG(dbgs() << "rounded mean is " << rounded << "\n");
    const unsigned casted = static_cast<unsigned>(rounded);
    DEBUG(dbgs() << "casted mean is " << casted << "\n");
    return casted;
    /*
    return static_cast<unsigned>(
        round(std::accumulate(joinDistances.begin(), joinDistances.end(), 0.0) /
              joinDistances.size()));
    */
  }

  case Aggregator::max:
    return *std::max_element(joinDistances.begin(), joinDistances.end());
  }
}

#if KERNEL_THREADS
  static constexpr unsigned spawnCost = 1000u;
  static constexpr unsigned syncCost = 0u;
#elif LIGHT_THREADS
  static constexpr unsigned spawnCost = 100u;
  static constexpr unsigned syncCost = 0u;
#endif

//------------------------------------------------------------------------------
static bool
decide(const std::pair<const CallInst *, std::set<Instruction *>> &pair,
       FunArgInfo &FAI, Profitability &Profit) {
  DEBUG(dbgs() << "decide() for:\n");
  DEBUG(pair.first->print(dbgs()));
  DEBUG(dbgs() << "\nIn " << pair.first->getCalledFunction()->getName()
               << "()\n");

  auto *funStats = Profit.getFunStats(*pair.first->getCalledFunction());

  assert(funStats);

  const unsigned calleeInsts{ funStats->totalCost };
  DEBUG(dbgs() << "calleeInsts is " << calleeInsts << "\n");

  //TODO: uses arithmeticMean for now... make configurable later?
  const unsigned callerInsts{ getSpawnToJoinCost(
      pair.first, pair.second, Profit, Aggregator::arithmeticMean) };
  DEBUG(dbgs() << "callerInsts is " << callerInsts << "\n");

  const unsigned serialCost{ calleeInsts + callerInsts };
  const unsigned parallelCost{ spawnCost + std::max(calleeInsts, callerInsts) +
                               syncCost };

  DEBUG(dbgs() << "serialCost == " << serialCost << "\n");
  DEBUG(dbgs() << "parallelCost == " << parallelCost << "\n\n");

  const bool decision{ serialCost > parallelCost };
  if (decision) {
    // update the new total cost to reflect that the fun has been parallelised
    const unsigned costReduction{ serialCost - parallelCost };

    assert(Profit.getFunStats(*pair.first->getParent()->getParent()));
    assert(Profit.getFunStats(*pair.first->getParent()->getParent())
               ->totalCost > costReduction);

    Profit.getFunStats(*pair.first->getParent()->getParent())->totalCost -=
        costReduction;
  }
  return decision;
}

//------------------------------------------------------------------------------
bool Decider::runOnModule(llvm::Module &M) {
  DEBUG(dbgs() << "Decider::runOnModule()\n");

  auto &Profit = getAnalysis<Profitability>();
  auto &FAI = getAnalysis<FunArgInfo>();

  for (auto &pair : FAI) {
    if (decide(pair, FAI, Profit)) {
      profitableJoinPoints.insert(pair);
      funsToBeSpawned.insert(pair.first->getCalledFunction());
    }
  }

  return false;
}

//------------------------------------------------------------------------------
void Decider::releaseMemory() {
  funsToBeSpawned.clear();
  profitableJoinPoints.clear();
}

//------------------------------------------------------------------------------
void Decider::print(raw_ostream &O, const llvm::Module *) const {
  if (funsToBeSpawned.size() == 0u) {
    O << "No functions should be spawned.\n";
    return;
  }

  O << "The following function(s) should be spawned:\n\n";
  for (const auto F : funsToBeSpawned) {
    O << F->getName() << "()\n";
  }

  O << "\nThey should be spawned and synced here:\n\n";
  for (const auto &pair : profitableJoinPoints) {
    O << "Spawn:\t";
    pair.first->print(O);
    O << "\nSync:\t";
    for (const auto *i : pair.second) {
      i->print(O);
      O << "\n\t";
    }
    O << "\n\n";
  }
}

static RegisterPass<Decider>
X("decider", "Decide which functions to spawn", false, true);
