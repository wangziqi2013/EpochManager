
#include "../src/AtomicStack.h"

using namespace peloton;
using namespace index;

void BasicTest() {
  dbg_printf("========== Basic Test ==========\n");
  
  AtomicStack<uint64_t> as{};
  
  for(uint64_t i = 0;i < 100;i++) {
    as.Push(i);
  }
  
  for(uint64_t i = 99;i >= 0;i--) {
    uint64_t top;
    as.Pop(top);
    
    assert(top == i);
  }
  
  return;
}


int main() {
  BasicTest();
  
  return 0;
}
