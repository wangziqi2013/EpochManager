
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
 * StandardDev() - Compute standard deviation
 *
 * The first component is std dev, and the second component is average
 */
static std::pair<double, double> StandardDev(const std::map<uint64_t, uint64_t> &m) {
  double avg = 0.0;
  double squared_dev = 0.0;
  
  // Aggregate
  for(auto it = m.begin();it != m.end();it++) {
    avg += it->second; 
  }
  
  // Average
  avg /= m.size();
  
  // Aggregate diff
  for(auto it = m.begin();it != m.end();it++) {
    double diff = static_cast<double>(it->second) - avg;
    
    squared_dev += (diff * diff);
  }
  
  // Average squared diff
  squared_dev /= m.size();
  
  return std::make_pair(sqrt(squared_dev), avg);
}

/*
 * TestRandRandomness() - Test the randomness of numbers we generate
 */
void TestRandRandomness(int iter, uint64_t num_salt) {
  PrintTestName("TestRandRandomness");
  
  static const uint64_t lower = 10;
  static const uint64_t higher = 1000;
  
  SimpleInt64Random<lower, higher> r{};
  
  // We run with different salt to see how it distributes
  for(uint64_t salt = 0;salt < num_salt;salt++) {
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
    
    auto ret = StandardDev(m);
    
    dbg_printf("Salt = %lu; std dev = %f; avg = %f\n", 
               salt, 
               ret.first, 
               ret.second);
  }
  
  return;
}


int main() {
  TestRandCorrectness(100000000);
  TestRandRandomness(1000000, 10);
  
  return 0; 
}

