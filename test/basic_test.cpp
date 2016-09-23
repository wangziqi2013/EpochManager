
#include "../src/AtomicStack.h"

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

void ThreadTest(uint64_t thread_num, uint64_t op_num) {
  dbg_printf("========== Thread Test ==========\n");
  
  AtomicStack<uint64_t> as{};
  
  // This counts the number of push operation we have performed
  std::atomic<uint64_t> counter;
  counter.store(0);
  
  auto push_func = [&as, &counter, thread_num, op_num](uint64_t id) {
                     for(uint64_t i = id;i < thread_num * op_num;i += thread_num) {
                       as.Push(i);
                       
                       // Increase the counter atomically
                       counter.fetch_add(1);
                     }
                   };

  // We use this to count what we have fetched from the stack
  std::atomic<uint64_t> sum;
  sum.store(0);
  counter.store(0);

  auto pop_func = [&as, &sum, &counter, thread_num, op_num](uint64_t id) {
                    for(uint64_t i = 0;i < op_num;i++) {
                      uint64_t data;
                      bool ret = as.Pop(data);
                      
                      // The operation must success
                      assert(ret == true);
                      
                      // Atomically adding the poped value onto the atomic
                      sum.fetch_add(data);
                    }
                  };
                  
  // Let threads start
  StartThreads(thread_num, push_func);
  
  // We must have performed exactly this number of operations
  assert(counter.load() == thread_num * op_num);
  
  StartThreads(thread_num, pop_func);
  
  uint64_t expected = (op_num * thread_num) * (op_num * thread_num - 1) / 2;
  
  dbg_printf("Sum = %lu; Expected = %lu\n", sum.load(), expected);
  assert(sum.load() == expected);
  
  return;
}

void MixedTest(uint64_t thread_num, uint64_t op_num) {
  dbg_printf("========== Mixed Test ==========\n");

  // Make sure thread number is an even number otherwise exit
  if((thread_num % 2) != 0) {
    dbg_printf("MixedTest requires thread_num being an even number!\n");
    
    return;
  }

  AtomicStack<uint64_t> as{};

  // This counts the number of push operation we have performed
  std::atomic<uint64_t> counter;
  // We use this to count what we have fetched from the stack
  std::atomic<uint64_t> sum;
  
  sum.store(0);
  counter.store(0);

  auto func = [&as, &counter, &sum, thread_num, op_num](uint64_t id) {
                // For id = 0, 2, 4, 6, 8 keep popping until op_num
                // operations have succeeded
                if((id % 2) == 0) {
                  for(uint64_t i = 0;i < op_num;i++) {
                    while(1) {
                      bool ret;
                      uint64_t data;
                           
                      ret = as.Pop(data);
                      if(ret == true) {
                        sum.fetch_add(data);
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

  return;
}

int main() {
  BasicTest();
  // Many threads and small number of data
  ThreadTest(1024, 10);
  // Many data and smaller number of threads
  ThreadTest(4, 2000000);
  
  MixedTest(32, 100000);
  return 0;
}
