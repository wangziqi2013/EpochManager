
#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"
#include "test_suite.h"

using namespace peloton;
using namespace index;

// This must be instanciated in the translation unit where it is
// used. Otherwise the compiler is not able to know its existence
// which causes the linker to complain
template <uint64_t core_num, typename GarbageNode>
std::unordered_map<void *, void *>
LocalWriteEMFactory<core_num, GarbageNode>::instance_map{};

// Number of cores we test EM on (this does not have to match the number
// of cores on the testing machine since we only test correctness but
// not performance)
static const uint64_t CoreNum = 40;

// Declear stack and its node type here
using StackType = AtomicStack<uint64_t>;
using NodeType = typename StackType::NodeType;

// Since the EM type is defined by the EMFactory type, we could
// make it easier 
using EMFactory = \
  LocalWriteEMFactory<CoreNum, std::remove_pointer<NodeType>::type>;
using EM = typename EMFactory::TargetType;

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
void GetThreadAffinityBenchmark() {
  PrintTestName("GetThreadAffinityBenchmark");
  
  Timer t{true};
  
  constexpr int iter = 10000000;
  
  // We need this to avoid the following loop being optimized out
  std::vector<int> v{};
  v.reserve(10);
  
  for(int i = 0;i < iter;i++) {
    int core_id = GetThreadAffinity();
    
    v[0] = core_id;
  }
  
  double duration = t.Stop();
  
  dbg_printf("Time usage (iter %d) = %f\n", iter, duration);
  dbg_printf("    Throughput = %f op/second\n", 
             static_cast<double>(iter) / duration); 
             
  return;
}

/*
 * SimpleBenchmark() - Benchmark how EM works without workload - just announce
 *                     entry & exit and it's all       
 */
void SimpleBenchmark(uint64_t thread_num, uint64_t op_num) {
  PrintTestName("SimpleBenchmark");
  
  // This instance must be created by the factory
  EM *em = EMFactory::GetInstance();

  

  auto func = [em, thread_num, op_num](uint64_t id) {
                // This is the core ID this thread is logically pinned to
                // physically we do not make any constraint here
                uint64_t core_id = id % CoreNum;
                
                // And then announce entry on its own processor
                for(uint64_t i = 0;i < op_num;i++) { 
                  em->AnnounceEnter(core_id);
                }
              };

  Timer t{true};
  // Let threads start
  StartThreads(thread_num, func);
  double duration = t.Stop();

  EMFactory::FreeInstance(em);
  
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


int main() {
  GetThreadAffinityBenchmark();
  RandomNumberBenchmark(8, 100000000);
  SimpleBenchmark(40, 1024 * 1024 * 30);
  
  return 0;
}

