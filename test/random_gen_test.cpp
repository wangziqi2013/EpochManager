
/*
 * random_gen_test.cpp - Tests for random number generator
 *
 * This file is not performance critical and thus could be compield with lower
 * level optimization in order to enable assert()
 */

#include "test_suite.h"

/*
 * TestRandCorrectness() - Tests whether the generator gives correct result
 *                         lying in the specified range
 */
void TestRandCorrectness(int iter) {
  PrintTestName("TestRandCorrectness");
  
  SimpleInt64Random<10, 1000> r{};
  
  for(int i = 0;i < iter;i++) {
    uint64_t num = r(i, 0);
    
    assert(num >= 10 && num < 1000); 
  }
  
  dbg_printf("Iteration = %d; Done\n", iter);
  
  return;
}


int main() {
  TestRandCorrectness(100000000);
  
  return 0; 
}

