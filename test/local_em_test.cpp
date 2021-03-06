
/*
 * local_em_test.cpp
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

using namespace peloton;
using namespace index;

// Number of cores we test EM on (this does not have to match the number
// of cores on the testing machine since we only test correctness but
// not performance)
static const uint64_t CoreNum = 8;

// Declear stack and its node type here
using StackType = AtomicStack<uint64_t>;
using NodeType = typename StackType::NodeType;

// This is the type of the EM we declare
using EM = LocalWriteEM<NodeType>;

/*
 * FactoryTest() - Tests EM factory
 */
void FactoryTest() {
  PrintTestName("FactoryTest");
  
  // This prints the number of threads supported by CPU 
  // including hyper-threading
  dbg_printf("Hardware concurrency = %u\n", GetOptimalCoreNumber());
  
  EM *p = new EM{CoreNum};
  
  dbg_printf("HasExited() = %d\n", static_cast<int>(p->HasExited()));
  
  // Enjoy the beauty of C++11!!!!!!!
  dbg_printf("ElementType size = %lu\n",
             sizeof(typename std::remove_pointer<decltype(p)>::type::ElementType));

  // Since HasExited() will be checked on destruction
  p->SignalExit();
  
  delete p;
  
  return;
}

/*
 * ThreadTest() - Tests whether the built in GC thread works
 */
void ThreadTest() {
  PrintTestName("ThreadTest");
  
  EM *em = new EM{CoreNum};
  
  // Start GC thread
  em->StartGCThread();
  
  for(int i = 0;i < 3;i++) {
    dbg_printf("Slept for %d seconds\r", i);
    
    SleepFor(1000);
  }
  
  dbg_printf("    Finished waiting. Destroy\n");
  dbg_printf("    Epoch counter = %lu\n", em->GetCurrentEpochCounter());
  
  delete em;
  
  dbg_printf("Thread has exited\n");
  
  return;
}

/*
 * MixedGCTest() - Test GC under MixedTest workload which is part of the 
 *                 BasicTest for AtomicStack
 */
void MixedGCTest(uint64_t thread_num, uint64_t op_num) {
  PrintTestName("MixedGCTest");

  // Make sure thread number is an even number otherwise exit
  if((thread_num % 2) != 0) {
    dbg_printf("MixedTest requires thread_num being an even number!\n");
    
    return;
  }

  StackType as{};
  
  EM *em = new EM{CoreNum};
  em->SetGCInterval(5);

  // This counts the number of push operation we have performed
  std::atomic<uint64_t> counter;
  // We use this to count what we have fetched from the stack
  std::atomic<uint64_t> sum;
  
  sum.store(0);
  counter.store(0);
  
  // Start GC thread
  em->StartGCThread();

  auto func = [&as, &counter, &sum, em, thread_num, op_num](uint64_t id) {
                // This is the core ID this thread is logically pinned to
                // physically we do not make any constraint here
                uint64_t core_id = id % CoreNum;
                
                // For id = 0, 2, 4, 6, 8 keep popping until op_num
                // operations have succeeded
                if((id % 2) == 0) {
                  for(uint64_t i = 0;i < op_num;i++) {
                    while(1) {
                      // Use the core ID of this thread to announcse entering
                      em->AnnounceEnter(core_id);
                      
                      NodeType *node_p = as.Pop();
                      
                      if(node_p != nullptr) {
                        sum.fetch_add(node_p->data);
                        
                        // After we have finished with the popped node just
                        // add it as garbage
                        em->AddGarbageNode(node_p);
                        
                        break;
                      }
                           
                    }
                  }
                } else {
                  // id = 1, 3, 5, 7, 9, ..
                  // but we actually make them 0, 1, 2, 3, 4
                  // such that the numbers pushed into are consecutive
                  id = (id - 1) >> 1;
                  
                  // This is the actual number of threads doing Push()
                  uint64_t delta = thread_num >> 1;
                  
                  for(uint64_t i = id;i < delta * op_num;i += delta) {
                    // For push we also need to protect
                    em->AnnounceEnter(core_id);
                      
                    as.Push(i);

                    // Increase the counter atomically
                    counter.fetch_add(1);
                  }
                }
              };

  // Let threads start
  StartThreads(thread_num, func);

  // Make the following computation easier
  thread_num >>= 1;

  // We must have performed exactly this number of operations
  assert(counter.load() == (thread_num * op_num));

  uint64_t expected = (op_num * thread_num) * (op_num * thread_num - 1) / 2;

  dbg_printf("Sum = %lu; Expected = %lu\n", sum.load(), expected);
  assert(sum.load() == expected);
  
  dbg_printf("    GC Interval = %lu\n", em->GetGCInterval());
  dbg_printf("    GC epoch counter = %lu\n", em->GetCurrentEpochCounter());
  dbg_printf("    # of nodes freed before d'tor = %lu\n", 
             em->GetNodeFreedCount());

  delete em;

  return;
}


int main() {
  FactoryTest();
  ThreadTest();
  
  MixedGCTest(8, 1024);
  MixedGCTest(32, 1024 * 1024);
  MixedGCTest(4, 1024 * 1024);
  
  return 0;
}
