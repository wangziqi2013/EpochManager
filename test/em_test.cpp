
/*
 * em_test.cpp
 *
 * This file tests the local write epoch manager (EM) for basic functionality
 * and also for correctness of GC.
 *
 * Unlike the benchmark, this file should be compiled with debugging flags
 * turned on, and also without optimization
 */

#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"
#include "test_suite.h"

// This must be instanciated in the translation unit where it is
// used. Otherwise the compiler is not able to know its existence
// which causes the linker to complain
template <uint64_t core_num, typename GarbageNode>
std::unordered_map<void *, void *>
LocalWriteEMFactory<core_num, GarbageNode>::instance_map{};

// Since the EM type is defined by the EMFactory type, we could
// make it easier 
using EMFactory = LocalWriteEMFactory<4, char>;
using EM = typename EMFactory::TargetType; 

/*
 * FactoryTest() - Tests EM factory
 */
void FactoryTest() {
  PrintTestName("FactoryTest");
  
  // This prints the number of threads supported by CPU 
  // including hyper-threading
  dbg_printf("Hardware concurrency = %u\n", GetOptimalCoreNumber());
  
  // This instance must be created by the factory
  EM *p = EMFactory::GetInstance();
  
  dbg_printf("HasExited() = %d\n", static_cast<int>(p->HasExited()));
  
  dbg_printf("pointer = %p\n", p);
  dbg_printf("Multiple of 64? %d\n", (int)!((uint64_t)p % 64));
  // Otherwise it fails 64 bit alignment test
  assert(((uint64_t)p % 64) == 0);
  
  // Enjoy the beauty of C++11!!!!!!!
  dbg_printf("ElementType size = %lu\n",
             sizeof(typename std::remove_pointer<decltype(p)>::type::ElementType));

  // Since HasExited() will be checked on destruction
  p->SignalExit();
  
  EMFactory::FreeInstance(p);
  
  return;
}

/*
 * ThreadTest() - Tests whether the built in GC thread works
 */
void ThreadTest() {
  PrintTestName("ThreadTest");
  
  // This instance must be created by the factory
  EM *em = EMFactory::GetInstance();
  
  // Start GC thread
  em->StartGCThread();
  
  for(int i = 0;i < 3;i++) {
    dbg_printf("Slept for %d seconds\r", i);
    
    SleepFor(1000);
  }
  
  dbg_printf("    Finished waiting. Destroy\n");
  dbg_printf("    Epoch counter = %lu\n", em->GetCurrentEpochCounter());
  
  EMFactory::FreeInstance(em);
  
  dbg_printf("Thread has exited\n");
  
  return;
} 

int main() {
  FactoryTest();
  ThreadTest();
  
  return 0;
}
