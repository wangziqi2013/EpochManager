
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

/*
 * TestRandRandomness() - Test the randomness of numbers we generate
 */
void TestRandRandomness(int iter, int num_salt) {
  PrintTestName("TestRandRandomness");
  
  static const uint64_t lower = 10;
  static const uint64_t higher = 1000;
  
  SimpleInt64Random<lower, higher> r{};
  
  // We run with different salt to see how it distributes
  for(int salt = 0;salt < num_salt;salt++) {
    std::map<uint64_t, uint64_t> m{};
    
    for(int i = 0;i < iter;i++) {
      uint64_t num = r(i, salt);
      
      // This will return false to if key already exists
      // to indicate that we should just increment
      auto ret = m.insert({num, 1});
      if(ret.second == false) {
        ret.first->second++; 
      }
    }
  }
}


int main() {
  TestRandCorrectness(100000000);
  TestRandRandomness(10000000, 10);
  
  return 0; 
}

