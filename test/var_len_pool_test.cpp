
/*
 * var_len_pool_test.cpp - Test suite for varrable lengthed allocation pool
 */

#include "test_suite.h"

/*
 * VarLenPoolBasicTest() - This function allocates a series of memory
 *                         using an increasing sequence and then check
 *                         for correctness
 */
void VarLenPoolBasicTest() {
  PrintTestName("VarLenPoolBasicTest");
  
  static const iter = 10;
  
  void *p_list[iter];
  
  // Chunk size = 64
  VarLenPool vlp{64};
  for(int i = 1;i <= 10;i++) {
    p_list[i - 1] = vlp.Allocate(i);
    memset(p_list[i - 1], i, i);
  }
  
  // Verify for correctness
  for(int i = 1;i <= 10;i++) {
    char *p = reinterpret_cast<char *>(p_list[i - 1]);
    for(int j = 0;j < i;j++) {
      assert(p[j] == static_cast<char>(i));
    }
  }
  
  return; 
}


int main() {
  VarLenPoolBasicTest();
  
  return;
}
