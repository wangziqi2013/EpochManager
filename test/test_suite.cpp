
#include "test_suite.h"

/*
 * PrintTestName() - As name suggests 
 */
void PrintTestName(const char *name) {
  dbg_printf("========== %s ==========\n", name);
  
  return;
}

/*
 * SleepFor() - Sleeps in the main thread
 */
void SleepFor(uint64_t sleep_ms) {
  std::chrono::milliseconds duration{sleep_ms};
  std::this_thread::sleep_for(duration);
  
  return;
}
