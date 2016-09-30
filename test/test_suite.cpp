
#include "test_suite.h"

/*
 * PrintTestName() - As name suggests 
 */
void PrintTestName(const char *name) {
  dbg_printf("=\n");
  dbg_printf("========== %s ==========\n", name);
  dbg_printf("=\n");
  
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



/*
 * PinToCore() - Pin the current calling thread to a particular core
 */
void PinToCore(size_t core_id) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(core_id, &cpu_set);

  int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);

  printf("pthread_setaffinity_np() returns %d\n", ret);

  return;
}
