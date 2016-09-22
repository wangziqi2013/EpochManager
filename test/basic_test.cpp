
#include "../src/AtomicStack.h"
#include <thread>

using namespace peloton;
using namespace index;

void BasicTest() {
  dbg_printf("========== Basic Test ==========\n");
  
  AtomicStack<uint64_t> as{};
  
  for(uint64_t i = 0;i < 100;i++) {
    as.Push(i);
  }
  
  for(uint64_t i = 0;i < 100;i++) {
    uint64_t top;
    uint64_t expected = 99 - i;
    bool ret = as.Pop(top);
    
    assert(ret == true);
    
    assert(top == expected);
  }
  
  return;
}

void ThreadTest(int thread_num, int op_num) {
  dbg_printf("========== Thread Test ==========\n");
  
  AtomicStack<int> as{};
  
  auto push_func = [&as, thread_num, op_num](uint64_t id) {
                     for(int i = static_cast<int>(id);i < thread_num * op_num;i += thread_num) {
                       as.Push(i);
                     }
                   };

  // We use this to count what we have fetched from the stack
  std::atomic<int> sum;
  sum.store(0);

  auto pop_func = [&as, &sum, thread_num, op_num](uint64_t id) {
                    for(int i = 0;i < op_num;i++) {
                      int data;
                      as.Pop(data);
                      
                      // Atomically adding the poped value onto the atomic
                      sum.fetch_add(data);
                    }
                  };
                  
  StartThreads(thread_num, push_func);
  StartThreads(thread_num, pop_func);
  
  int expected = (op_num * thread_num) * (op_num * thread_num - 1) / 2;
  
  dbg_printf("Sum = %d; Expected = %d\n", sum.load(), expected);
  assert(sum.load() == expected);
  
  return;
}

int main() {
  BasicTest();
  ThreadTest(16, 2);
  
  return 0;
}
