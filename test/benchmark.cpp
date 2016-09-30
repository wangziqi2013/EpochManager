
#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"
#include "test_suite.h"

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
void SimpleBenchmark() {
  PrintTestName("SimpleBenchmark");

  // Make sure thread number is an even number otherwise exit
  if((thread_num % 2) != 0) {
    dbg_printf("SimpleBenchmark requires thread_num being an even number!\n");
    
    return;
  }
  
  // This instance must be created by the factory
  EM *em = EMFactory::GetInstance();

  auto func = [em, thread_num, op_num](uint64_t id) {
                // This is the core ID this thread is logically pinned to
                // physically we do not make any constraint here
                uint64_t core_id = id % CoreNum;
                
                em->AnnounceEnter(core_id);
              };

  // Let threads start
  StartThreads(thread_num, func);


  EMFactory::FreeInstance(em);

  return;
}


int main() {
  GetThreadAffinityBenchmark();
}

