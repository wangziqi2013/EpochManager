
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
 * GetThreadAffinity() - Returns the ID of the CPU thread is running on
 *
 * This function halts the entire process if it returns -1 which is a sign of 
 * system call error
 */
int GetThreadAffinity() {
  int core_id = sched_getcpu();
  if(unlikely(core_id == -1)) {
    throw 0;
  }
  
  return core_id;
}

/*
 * PinToCore() - Pin the current calling thread to a particular core
 */
void PinToCore(size_t core_id) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(core_id, &cpu_set);

  int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  assert(ret == 0);
  (void)ret;

  return;
}
