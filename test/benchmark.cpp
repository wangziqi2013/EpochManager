
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
uint64_t CoreNum;

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
 * GetRandomWorkload() - Get a random number with uniform deviation from the 
 *                       given base
 */
uint64_t GetRandomWorkload(uint64_t base, uint64_t dev, uint64_t seed) {
  uint64_t r = base;
  
  // If workload is large enough then make it random
  // Otherwise just use the given workload
  if(base != 0 && dev != 0) {
    // Use default parameter (0 - UINT64_T MAX)
    SimpleInt64Random<> hasher{};
    
    const uint64_t sign = hasher(seed, seed + 1) % 2;
    if(sign == 0) {
      r = base + hasher(seed, seed) % dev;
    } else {
      r = base - hasher(seed, seed) % dev;
    }
  }
  
  return r;
}

/*
 * LEMSimpleBenchmark() - Benchmark how LocalWriteEM works without workload - 
 *                        just announce entry & exit and it's all       
 */
void LEMSimpleBenchmark(uint64_t thread_num, 
                        uint64_t op_num,
                        uint64_t workload) {
  PrintTestName("LEMSimpleBenchmark");
  
  
  // Note that we use the number of counters equal to the number of threads
  // rather than number of cores, i.e. one core could have multiple counters
  LEM *em = new LEM{thread_num};

  auto func = [em, op_num, workload](uint64_t id) {
                // This is the core ID this thread is logically pinned to
                // physically we do not make any constraint here
                uint64_t core_id = id % CoreNum;
                
                // This avoids scheduling problem
                // Without this function the performance will be very
                // unstable especially when # of threads does not match
                // number of core
                PinToCore(core_id);
                
                const uint64_t random_workload = \
                  GetRandomWorkload(workload, workload >> 2, id); 
                
                //printf("**%lu**\n", random_workload);
                
                std::vector<uint64_t> v{};
                v.reserve(random_workload);
                
                // And then announce entry on its own processor
                for(uint64_t i = 0;i < op_num;i++) { 
                  em->AnnounceEnter(core_id);
                  
                  for(uint64_t j = 0;j < random_workload;j++) {
                    v[j] = j; 
                  }
                }
                
                return;
              };

  // Need to start GC thread to periodically increase global epoch
  em->StartGCThread();

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
void GEMSimpleBenchmark(uint64_t thread_num, 
                        uint64_t op_num, 
                        uint64_t workload) {
  PrintTestName("GEMSimpleBenchmark");
  
  // This instance must be created by the factory
  GEM *em = new GEM{};

  auto func = [em, op_num, workload](uint64_t id) {
                // random is between (base [+/-] 1/4 base)
                const uint64_t random_workload = \
                  GetRandomWorkload(workload, workload >> 2, id); 
                
                std::vector<uint64_t> v{};
                v.reserve(random_workload);
                
                // And then announce entry on its own processor
                for(uint64_t i = 0;i < op_num;i++) { 
                  void *epoch_node_p = em->JoinEpoch();
                  
                  // Actual workload is protected by epoch manager
                  for(uint64_t j = 0;j < random_workload;j++) {
                    v[j] = j; 
                  }
                  
                  em->LeaveEpoch(epoch_node_p);
                }
                
                return;
              };

  em->StartGCThread();

  // Let timer start and then start threads
  Timer t{true};
  StartThreads(thread_num, func);
  double duration = t.Stop();

  delete em;
  
  dbg_printf("Tests of %lu threads, %lu operations each took %f seconds\n",
             thread_num,
             op_num,
             duration);
  
  dbg_printf("    Epoch created = %lu; Epoch freed = %lu\n",
             em->GetEpochCreated(),
             em->GetEpochFreed());
  
  dbg_printf("    Throughput = %f M op/sec\n", 
             static_cast<double>(thread_num * op_num) / duration / (1024.0 * 1024.0));

  dbg_printf("    Throughput Per Thread = %f M op/sec\n", 
             static_cast<double>(op_num) / duration / (1024.0 * 1024.0));


  return;
}

/*
 * GetValueOrThrow() - Get an unsigned long typed value from args, or throw 
 *                     exception if the format for key-value is not correct
 *
 * This function throws integer constant 0 on error 
 */
void GetValueAsULOrThrow(Argv &args, 
                         const std::string &key,
                         unsigned long *value_p) {
  bool ret = args.GetValueAsUL(key, value_p);
  
  if(ret == false) {
    dbg_printf("ERROR: Unrecognized value for key %s: \"%s\"\n",
               key.c_str(), 
               args.GetValue("thread_num")->c_str());
               
    throw 0;
  }
  
  return;
}

int main(int argc, char **argv) {
  // This returns the number of logical CPUs
  CoreNum = GetCoreNum();
  dbg_printf("* # of cores (default thread_num) on the platform = %lu\n", 
             CoreNum);
  if(CoreNum == 0) {
    dbg_printf("    ...which is not supported\n");
    
    exit(1);
  }
  
  // This will be overloaded if a thread_num is provided as argument
  uint64_t thread_num = CoreNum;
  // By default no workload is done
  uint64_t workload = 0;
  
  Argv args{argc, argv};
  
  GetValueAsULOrThrow(args, "thread_num", &thread_num);
  GetValueAsULOrThrow(args, "workload", &workload);
  
  dbg_printf("* thread_num = %lu\n", thread_num);
  dbg_printf("* workload = %lu\n", workload);
  dbg_printf("* CoreNum = %lu\n", CoreNum);
  
  if(argc == 1 || args.Exists("thread_affinity")) {
    GetThreadAffinityBenchmark(thread_num);
  }
  
  if(argc == 1 || args.Exists("int_hash")) {
    IntHasherRandBenchmark(100000000, 10);
  }
  
  if(argc == 1 || args.Exists("random_number")) {
    RandomNumberBenchmark(thread_num, 100000000);
  }
  
  if(argc == 1 || args.Exists("lem_simple")) {
    LEMSimpleBenchmark(thread_num, 1024 * 1024 * 30, workload);
  }
  
  if(argc == 1 || args.Exists("gem_simple")) {
    GEMSimpleBenchmark(thread_num, 1024 * 1024 * 10, workload);
  }
  
  return 0;
}

