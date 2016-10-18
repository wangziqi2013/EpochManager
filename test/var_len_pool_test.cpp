
/*
 * var_len_pool_test.cpp - Test suite for varrable lengthed allocation pool
 */

#include "test_suite.h"
#include "../src/VarLenPool.h"

/*
 * VarLenPoolBasicTest() - This function allocates a series of memory
 *                         using an increasing sequence and then check
 *                         for correctness
 */
void VarLenPoolBasicTest() {
  PrintTestName("VarLenPoolBasicTest");
  
  static const int iter = 10;
  
  void *p_list[iter];
  
  // Chunk size = 64
  VarLenPool vlp{64};
  for(int i = 1;i <= iter;i++) {
    p_list[i - 1] = vlp.Allocate(i);
    memset(p_list[i - 1], i, i);
    
    dbg_printf("Allocating mem of size = %d\n", i);
  }
  
  // Verify for correctness
  for(int i = 1;i <= iter;i++) {
    char *p = reinterpret_cast<char *>(p_list[i - 1]);
    for(int j = 0;j < i;j++) {
      assert(p[j] == static_cast<char>(i));
    }
    
    dbg_printf("Checking content of size = %d\n", i);
  }
  
  return; 
}

/*
 * VarLenPoolThreadTest() - Multithreaded test of VarLenPool
 */
void VarLenPoolThreadTest(int thread_num, int iter) {
  PrintTestName("VarLenPoolThreadTest");
  
  std::atomic<bool> all_finished;
  all_finished.store(false);
  
  std::atomic<int> finished_count;
  finished_count.store(0);
  
  VarLenPool vlp{64};
  
  auto f = [iter, 
            thread_num, 
            &all_finished, 
            &finished_count, 
            &vlp](uint64_t id) {
    void *p_list[iter];
    
    for(int i = 0;i < iter;i++) {
      // Start allocation at length = 1
      void *p = vlp.Allocate(i + 1);
      assert(p != nullptr);
      
      // Assign a unique value to each thread
      memset(p, static_cast<char>((int)thread_num * i + (int)id), i);
      
      p_list[i] = p;
    }
    
    int t = finished_count.fetch_add(1);
    if(t == (thread_num - 1)) {
      dbg_printf("All threads finished... proceed to validate\n");
      
      // This will let all threads proceed including this thread
      all_finished.store(true); 
    }
    
    // Memory barrier here to ensure progress
    
    // Busy loop wait
    while(all_finished.load() == false);
    
    for(int i = 0;i < iter;i++) {
      char *p = reinterpret_cast<char *>(p_list[i]);
      for(int j = 0;j < i;j++) {
        assert(p[j] == static_cast<char>(static_cast<char>((int)thread_num * i + (int)id))); 
      }
    }
    
    return;
  };
  
  StartThreads(thread_num, f);
  
  return;
}


int main() {
  VarLenPoolBasicTest();
  VarLenPoolThreadTest(10, 100);
  
  return 0;
}
