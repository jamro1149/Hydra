#include <algorithm>
#include <atomic>
#include <cassert>
#include <climits>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>

#include "ThreadPool.h"

#define DEBUG(x) 

#ifndef NUM_THREADS
#error "Num threads not specificed!"
#endif

static constexpr unsigned numThreads{ NUM_THREADS };

using namespace std;

// global mutex for writting to the console
DEBUG(static mutex console_mutex);

static void call_with_args(unsigned num_args, void (*f)(void), void **args) {
  DEBUG(console_mutex.lock());
  DEBUG(cerr << "call_with_args() by " << this_thread::get_id() << "\n");
  DEBUG(console_mutex.unlock());

  assert(f);
  assert(args);
  assert(num_args == 0u || *args);

  // select which call based on number of arguments
  switch (num_args) {
  case 0u: {
    f();
    return;
  }

  case 1u: {
    auto g = (void (*)(void *))f;
    g(args[0]);
    return;
  }

  case 2u: {
    auto g = (void (*)(void *, void *))f;
    g(args[0], args[1]);
    return;
  }

  case 3u: {
    auto g = (void (*)(void *, void *, void *))f;
    g(args[0], args[1], args[2]);
    return;
  }

  case 4u: {
    auto g = (void (*)(void *, void *, void *, void *))f;
    g(args[0], args[1], args[2], args[3]);
    return;
  }

  case 5u: {
    auto g = (void (*)(void *, void *, void *, void *, void *))f;
    g(args[0], args[1], args[2], args[3], args[4]);
    return;
  }

  case 6u: {
    auto g = (void (*)(void *, void *, void *, void *, void *, void *))f;
    g(args[0], args[1], args[2], args[3], args[4], args[5]);
    return;
  }

  case 7u: {
    auto g =
        (void (*)(void *, void *, void *, void *, void *, void *, void *))f;
    g(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    return;
  }

  case 8u: {
    auto g = (void (*)(void *, void *, void *, void *, void *, void *, void *,
                       void *))f;
    g(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    return;
  }
  }
}

namespace {
struct Job {
  unsigned num_args;
  void (*f)(void);
  void *args[8];
};
}

// do not look/modify j or stop unless you hold m
static void do_work(Job *j, mutex *m, atomic<bool> *valid, bool *stop) {
  DEBUG(console_mutex.lock());
  DEBUG(cerr << "do_work() by " << this_thread::get_id() << "\n");
  DEBUG(console_mutex.unlock());

  assert(j);
  assert(m);
  assert(valid);
  assert(stop);

  bool shutdown;
  do {
    // lock m to prevent changes to j and stop
    unique_lock<mutex> l(*m);

    // check if we've been told to shutdown
    shutdown = *stop;

    // check if we've been given a job
    if (valid->load()) {
      // do the work
      call_with_args(j->num_args, j->f, j->args);

      // tell the ThreadPool that we did the work
      valid->store(false);
    } else {
      // give another thread a chance
      l.unlock();
      this_thread::yield();
    }
  } while (!shutdown);
}

namespace {
class ThreadPool {
  Job jobs[numThreads];
  mutex mutexes[numThreads];
  bool stops[numThreads];
  thread threads[numThreads];
  atomic<bool> hasJobs[numThreads];
  bool available[numThreads];
  mutex available_mutex;

public:
  ThreadPool();
  ~ThreadPool();
  unsigned assignJob(unsigned num_args, void *arg1, void *arg2, void *arg3,
                     void *arg4, void *arg5, void *arg6, void *arg7, void *arg8,
                     void (*f)(void));
  void join(unsigned threadID);
};
}

ThreadPool::ThreadPool() {
  DEBUG(console_mutex.lock());
  DEBUG(puts("ThreadPool::ThreadPool()"));
  DEBUG(console_mutex.unlock());

  for (unsigned i{ 0u }; i < numThreads; ++i) {
    stops[i] = false;
    hasJobs[i] = false;
    available[i] = true;
    threads[i] =
        thread{ &do_work, &jobs[i], &mutexes[i], &hasJobs[i], &stops[i] };
  }
}

ThreadPool::~ThreadPool() {
  DEBUG(console_mutex.lock());
  DEBUG(puts("ThreadPool::~ThreadPool()"));
  DEBUG(console_mutex.unlock());

  // signal all threads to stop
  for (unsigned i{ 0u }; i < numThreads; ++i) {
    mutexes[i].lock();
    stops[i] = true;
    mutexes[i].unlock();
  }

  // join on all threads
  for (auto &t : threads) {
    t.join();
  }
}

// returns positive ThreadID if successful, or negative value if unsuccessful
unsigned ThreadPool::assignJob(const unsigned num_args, void *arg1, void *arg2,
                               void *arg3, void *arg4, void *arg5, void *arg6,
                               void *arg7, void *arg8, void (*f)(void)) {
  DEBUG(console_mutex.lock());
  DEBUG(puts("ThreadPool::assignJob()"));
  DEBUG(console_mutex.unlock());

  auto l1 = unique_lock<mutex>(available_mutex);

  // find a thread which isn't busy
  for (unsigned i{ 0u }; i < numThreads; ++i) {
    if (available[i]) {
      // lock mutexes[i] to prevent the thread from reading the job while we
      // change it.
      auto l2 = unique_lock<mutex>(mutexes[i]);

      // update jobs[i] to the job we want
      auto &j = jobs[i];
      j.num_args = num_args;
      j.f = f;
      j.args[0] = arg1;
      j.args[1] = arg2;
      j.args[2] = arg3;
      j.args[3] = arg4;
      j.args[4] = arg5;
      j.args[5] = arg6;
      j.args[6] = arg7;
      j.args[7] = arg8;

      // signal to the thread that it has a job to do
      hasJobs[i] = true;

      // note that the thread isn't available
      available[i] = false;

      // tell the caller the threadID of the thread
      return i;
    }
  }

  // all theads are busy
  return UINT_MAX;
}

// spin until thread with threadID has finished
void ThreadPool::join(unsigned threadID) {
  DEBUG(console_mutex.lock());
  DEBUG(puts("ThreadPool::join()"));
  DEBUG(console_mutex.unlock());

  assert(threadID >= 0);
  assert(threadID < numThreads);

  // wait until the thread which was supposed to do our job has finished
  while (hasJobs[threadID]) {
    this_thread::yield();
  }

  // aquire the available_mutex before changing available
  auto l = unique_lock<mutex>(available_mutex);

  // let the ThreadPool know that the thread with threadID is free
  available[threadID] = true;
}

static ThreadPool tp{};

static thread_local unsigned spawnCount{ 0u };
static thread_local pair<unsigned, unsigned> taskJobPairs[numThreads];

static inline bool updateJobID(const unsigned task, const unsigned jobID) {
  const bool success{ jobID < numThreads };
  if (success) {
    assert(spawnCount < numThreads);
    taskJobPairs[spawnCount] = make_pair(task, jobID);
    ++spawnCount;
  }
  return success;
}

void spawn(const unsigned task, void (*f)(void)) {
  unsigned jobID = tp.assignJob(0u, nullptr, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, f);
  if (!updateJobID(task, jobID)) {
    f();
  }
}

void spawn(const unsigned task, void (*f)(void *), void *arg1) {
  unsigned jobID = tp.assignJob(1u, arg1, nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1);
  }
}

void spawn(const unsigned task, void (*f)(void *, void *), void *arg1,
           void *arg2) {
  unsigned jobID = tp.assignJob(2u, arg1, arg2, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2);
  }
}

void spawn(const unsigned task, void (*f)(void *, void *, void *), void *arg1,
           void *arg2, void *arg3) {
  unsigned jobID = tp.assignJob(3u, arg1, arg2, arg3, nullptr, nullptr, nullptr,
                                nullptr, nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3);
  }
}

void spawn(const unsigned task, void (*f)(void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4) {
  unsigned jobID = tp.assignJob(4u, arg1, arg2, arg3, arg4, nullptr, nullptr,
                                nullptr, nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3, arg4);
  }
}

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *), void *arg1,
           void *arg2, void *arg3, void *arg4, void *arg5) {
  unsigned jobID = tp.assignJob(5u, arg1, arg2, arg3, arg4, arg5, nullptr,
                                nullptr, nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3, arg4, arg5);
  }
}

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6) {
  unsigned jobID = tp.assignJob(6u, arg1, arg2, arg3, arg4, arg5, arg6, nullptr,
                                nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3, arg4, arg5, arg6);
  }
}

void spawn(const unsigned task,
           void (*f)(void *, void *, void *, void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6, void *arg7) {
  unsigned jobID = tp.assignJob(7u, arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                                nullptr, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
  }
}

void spawn(const unsigned task, void (*f)(void *, void *, void *, void *,
                                          void *, void *, void *, void *),
           void *arg1, void *arg2, void *arg3, void *arg4, void *arg5,
           void *arg6, void *arg7, void *arg8) {
  unsigned jobID = tp.assignJob(8u, arg1, arg2, arg3, arg4, arg5, arg6, arg7,
                                arg8, (void (*)(void))f);
  if (!updateJobID(task, jobID)) {
    f(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
  }
}

void join(const unsigned task) {
  // join with and remove all jobs in taskJobPairs which match task
  auto *end = remove_if(taskJobPairs, taskJobPairs + spawnCount,
                        [task](const pair<unsigned, unsigned> &p) {
    const bool join{ p.first == task };
    if (join) {
      tp.join(p.second);
    }
    return join;
  });
  assert(end - taskJobPairs >= 0 && end - taskJobPairs <= numThreads);
  spawnCount = end - taskJobPairs;
}
