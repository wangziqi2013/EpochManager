
/*
 * benchmark.cpp - Contains benchmark code for measuring performance related
 *                 numbers in this project
 *
 * This module should be compiled with all possible optimization flags, and
 * also with libc debugging flag turned off
 */

#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"
#include "../src/GlobalWriteEM.h"
#include "test_suite.h"

using namespace peloton;
using namespace index;

// Number of cores we test EM on (this does not have to match the number
// of cores on the testing machine since we only test correctness but
// not performance)
static const uint64_t CoreNum = 40;

// Declear stack and its node type here
using StackType = AtomicStack<uint64_t>;
using NodeType = typename StackType::NodeType;

// EM type declaration
using LEM = LocalWriteEM<NodeType>;
using GEM = GlobalWriteEM<NodeType>;

/*
 * IntHasherRandBenchmark() - Benchmarks integer number hash function from 
 *                            Murmurhash3, which is then used as a random
 *                            number generator
 */
void IntHasherRandBenchmark(uint64_t iter, uint64_t salt_num) {
  PrintTestName("IntHasherRandBenchmark");
  
  std::vector<uint64_t> v{};
  v.reserve(1);
  
  SimpleInt64Random<0, 10000000> r{};
  
  Timer t{true};
  for(uint64_t salt = 0;salt < salt_num;salt++) {
    for(uint64_t i = 0;i < iter;i++) {
      v[0] = r(i, salt);
    }
  }
  
  double duration = t.Stop();
  
  dbg_printf("Iteration = %lu, Duration = %f\n", 
             iter * salt_num, 
             duration);
  dbg_printf("    Throughput = %f M op/sec\n", 
             static_cast<double>(iter * salt_num) / duration / 1024.0 / 1024.0);
             
  return;
}

/*
 * RandomNumberBenchmark() - Benchmark random number genrator inside C++11
 */
void RandomNumberBenchmark(int thread_num, int iter) {
  PrintTestName("RandomNumberBenchmark");
  
  auto f = [thread_num, iter](uint64_t id) -> void {
    Random<uint64_t, 0, -1> r{};
  
    // Avoid optimization 
    std::vector<uint64_t> v{};
    v.reserve(1);
    
    // Bring it into the cache such that we do not suffer from cache miss
    v[0] = 1;
  
    for(int i = 0;i < iter;i++) {
      uint64_t num = r();
      
      // This should be a cached write, so super fast
      v[0] = num;
    }
    
    return; 
  };
  
  Timer t{true};
  
  StartThreads(thread_num, f);
  
  double duration = t.Stop();
  
  dbg_printf("Thread num = %d, Iteration = %d, Duration = %f\n", 
             thread_num, 
             iter, 
             duration);
  dbg_printf("    Throughput = %f M op/sec\n", 
             static_cast<double>(iter) / duration / 1024.0 / 1024.0);
  dbg_printf("    Throughput = %f M op/(sec * thread)\n", 
             static_cast<double>(iter) / duration / 1024.0 / 1024.0 / thread_num);
  
  return;
}

/*
 * GetThreadAffinityBenchmark() - Measures how fast we could call this function
 */
void GetThreadAffinityBenchmark(uint64_t thread_num) {
  PrintTestName("GetThreadAffinityBenchmark");
  
  constexpr int iter = 10000000;
  
  auto f = [iter](uint64_t thread_id) { 
    // We need this to avoid the following loop being optimized out
    std::vector<int> v{};
    v.reserve(10);
    
    for(int i = 0;i < iter;i++) {
      int core_id = GetThreadAffinity();
      
      v[0] = core_id;
    }
    
    return;
  };
  
  // Start Threads and timer
  Timer t{true};
  StartThreads(thread_num, f);
  double duration = t.Stop();
  
  dbg_printf("Time usage (iter %d, thread %lu) = %f\n", 
             iter, 
             thread_num, 
             duration);
  dbg_printf("    Throughput = %f op/second\n", 
             static_cast<double>(iter) / duration); 
  dbg_printf("    Throughput = %f op/(second * thread)\n", 
             static_cast<double>(iter) / duration / thread_num);
             
  return;
}

/*
 * LEMSimpleBenchmark() - Benchmark how LocalWriteEM works without workload - 
 *                        just announce entry & exit and it's all       
 */
void LEMSimpleBenchmark(uint64_t thread_num, uint64_t op_num) {
  PrintTestName("LEMSimpleBenchmark");
  
  // This instance must be created by the factory
  LEM *em = new LEM{CoreNum};

  auto func = [em, thread_num, op_num](uint64_t id) {
                // This is the core ID this thread is logically pinned to
                // physically we do not make any constraint here
                uint64_t core_id = id % CoreNum;
                
                // And then announce entry on its own processor
                for(uint64_t i = 0;i < op_num;i++) { 
                  em->AnnounceEnter(core_id);
                }
              };

  // Let timer start and then start threads
  Timer t{true};
  StartThreads(thread_num, func);
  double duration = t.Stop();

  delete em;
  
  dbg_printf("Tests of %lu threads, %lu operations each took %f seconds\n",
             thread_num,
             op_num,
             duration);
             
  dbg_printf("    Throughput = %f M op/sec\n", 
             static_cast<double>(thread_num * op_num) / duration / (1024.0 * 1024.0));

  dbg_printf("    Throughput Per Thread = %f M op/sec\n", 
             static_cast<double>(op_num) / duration / (1024.0 * 1024.0));


  return;
}

/*
 * GEMSimpleBenchmark() - Runs GlobalWriteEM repeatedly for benchmark numbers
 *
 * For global EM since it uses coarse grained reference counting, we have to
 * increase and decrease the counter whenever a thread enters and leaves the 
 * epoch, which is two times the overhead a LocalWriteEM would have
 */
void GEMSimpleBenchmark(uint64_t thread_num, uint64_t op_num) {
  PrintTestName("GEMSimpleBenchmark");
  
  // This instance must be created by the factory
  GEM *em = new GEM{};

  auto func = [em, op_num](uint64_t id) {
                // And then announce entry on its own processor
                for(uint64_t i = 0;i < op_num;i++) {
                  auto epoch_node_p = em->JoinEpoch();
                  em->LeaveEpoch(epoch_node_p);
                }
              };

  // Let timer start and then start threads
  Timer t{true};
  StartThreads(thread_num, func);
  double duration = t.Stop();

  delete em;
  
  dbg_printf("Tests of %lu threads, %lu operations each took %f seconds\n",
             thread_num,
             op_num,
             duration);
             
  dbg_printf("    Throughput = %f M op/sec\n", 
             static_cast<double>(thread_num * op_num) / duration / (1024.0 * 1024.0));

  dbg_printf("    Throughput Per Thread = %f M op/sec\n", 
             static_cast<double>(op_num) / duration / (1024.0 * 1024.0));


  return;
}


int main(int argc, char **argv) {
  // This will be overloaded if a thread_num is provided as argument
  uint64_t thread_num = 8;
  bool ret;
  
  Argv args{argc, argv};
  ret = args.GetValueAsUL("thread_num", &thread_num);
  
  // Value found but illegal
  if(ret == false) {
    dbg_printf("ERROR: Unrecognized thread_num: \"%s\"\n", 
               args.GetValue("thread_num")->c_str());
    
    return 1; 
  }
  
  dbg_printf("*** Using thread_num = %lu\n", thread_num);
  
  //if(argc == 1 || )
  GetThreadAffinityBenchmark(thread_num);
  IntHasherRandBenchmark(100000000, 10);
  RandomNumberBenchmark(thread_num, 100000000);
  LEMSimpleBenchmark(thread_num, 1024 * 1024 * 30);
  GEMSimpleBenchmark(thread_num, 1024 * 1024 * 10);
  
  return 0;
}

