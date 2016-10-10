
/*
 * arg_test.cpp - Tests argument analyzer which is part of the test suite
 *                of this project, and could extended further to be 
 *                user in other projects
 */
 
#include "test_suite.h"
 
/*
 * TestArgBasic() - Tests basic functionality 
 */
void TestArgBasic() {
  PrintTestName("TestArgBasic");
  
  int argc = 6;
  std::string argv_[] = {"test"}, 
                         "-a", 
                         "--second_key=2nd_value", 
                         "-3", 
                         "--fourth_key",
                         "--b"};
  
  Argv args{argc, argv};
  auto kv_map = args.GetKVMap();
  
  for(auto it = kv_map.begin();it != kv_map.end();it++) {
    dbg_printf("%s -> %s\n", ) 
  }
  
  return;
}

int main() {
  return 0; 
}
