
#pragma once

#include <sched.h>
#include "../src/common.h"

void PrintTestName(const char *name);
void SleepFor(uint64_t sleep_ms); 
 
// Template function to launch threads
// This does not define anything but to specify a template so it is
// OK to put it here
template <typename Fn, typename... Args>
void StartThreads(uint64_t num_threads, Fn &&fn, Args &&... args) {
  std::vector<std::thread> thread_list;

  // Launch a group of threads
  for(uint64_t thread_id = 0;thread_id < num_threads;thread_id++) {
    thread_list.push_back(std::thread(fn, thread_id, args...));
  }

  // Join the threads with the main thread
  for(uint64_t thread_id = 0;thread_id < num_threads;thread_id++) {
    thread_list[thread_id].join();
  }
  
  return;
}
