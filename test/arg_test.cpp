
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
  
  char v1[] = "test";
  char v2[] = "-1";
  char v3[] = "--second_key=2nd_value";
  char v4[] = "--3";
  char v5[] = "--fourth_key";
  char vv[] = "--";
  char v6[] = "--b=nonsense";
  char v7[] = "value_1";
  char v8[] = "value_2";
  
  char *argv[] = {v1, v2, v3, v4, v5, vv, v6, v7, v8};
  int argc = static_cast<int>(sizeof(argv) / sizeof(char *));
  
  dbg_printf("*** Command line input: \n");
  dbg_printf("");
  for(int i = 0;i < argc;i++) {
    printf("%s ", argv[i]);
  }
  putchar('\n');
  
  Argv args{argc, argv};
  const auto &kv_map = args.GetKVMap();
  
  dbg_printf("*** kv_list content: \n\n");
  for(auto it = kv_map.begin();it != kv_map.end();it++) {
    dbg_printf("%s -> %s\n", it->first.c_str(), it->second.c_str());
  }
  
  const auto &arg_list = args.GetArgList();
  dbg_printf("*** arg_list content: \n\n");
  for(auto it = arg_list.begin();it != arg_list.end();it++) {
    dbg_printf("%s\n", it->c_str()); 
  }
  
  return;
}

int main() {
  TestArgBasic();
  
  return 0; 
}
