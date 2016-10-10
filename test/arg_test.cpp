
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
  char v1[] = "test";
  char v2[] = "-1";
  char v3[] = "--second_key=2nd_value";
  char v4[] = "-3";
  char v5[] = "--fourth_key";
  char v6[] = "--b";
  
  char *argv[] = {v1, v2, v3, v4, v5, v6};
  
  Argv args{argc, argv};
  const auto &kv_map = args.GetKVMap();
  
  dbg_printf("kv_list content: \n");
  for(auto it = kv_map.begin();it != kv_map.end();it++) {
    dbg_printf("%s -> %s\n", it->first.c_str(), it->second.c_str());
  }
  
  const auto &arg_list = args.GetArgList();
  dbg_printf("arg_list content: \n");
  for(auto it = arg_list.begin();it != arg_list.end();it++) {
    dbg_printf("%s\n", it->c_str()); 
  }
  
  return;
}

int main() {
  return 0; 
}
