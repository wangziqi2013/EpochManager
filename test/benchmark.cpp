
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


int main() {
  GetThreadAffinityBenchmark();
}

