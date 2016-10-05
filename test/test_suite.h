
#pragma once

#include <random>

#include <sched.h>
#include "../src/common.h"

void PrintTestName(const char *name);
void SleepFor(uint64_t sleep_ms); 
int GetThreadAffinity();
void PinToCore(size_t core_id);
 
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

/*
 * class Random - A random number generator
 *
 * This generator is a template class letting users to choose the number
 */
template <typename IntType,
          IntType lower,
          IntType upper>
class Random {
 private:
  std::random_device device;
  std::default_random_engine engine;
  std::uniform_int_distribution<IntType> dist;

 public:
  
  /*
   * Constructor - Initialize random seed and distribution object
   */
  Random() :
    device{},
    engine{device()},
    dist{lower, upper}
  {}
  
  /*
   * Get() - Get a random number of specified type
   */
  inline IntType Get() {
    return dist(engine);
  }
  
  /*
   * operator() - Grammar sugar
   */
  inline IntType operator()() {
    return Get(); 
  }
};

/*
 * class Timer - Measures time usage for testing purpose
 */
class Timer {
 private:
  std::chrono::time_point<std::chrono::system_clock> start;
  std::chrono::time_point<std::chrono::system_clock> end;
  
 public: 
 
  /* 
   * Constructor
   *
   * It takes an argument, which denotes whether the timer should start 
   * immediately. By default it is true
   */
  Timer(bool start = true) : 
    start{},
    end{} {
    if(start == true) {
      Start();
    }
    
    return;
  }
  
  /*
   * Start() - Starts timer until Stop() is called
   *
   * Calling this multiple times without stopping it first will reset and
   * restart
   */
  inline void Start() {
    start = std::chrono::system_clock::now();
    
    return;
  }
  
  /*
   * Stop() - Stops timer and returns the duration between the previous Start()
   *          and the current Stop()
   *
   * Return value is represented in double, and is seconds elapsed between
   * the last Start() and this Stop()
   */
  inline double Stop() {
    end = std::chrono::system_clock::now();
    
    return GetInterval();
  }
  
  /*
   * GetInterval() - Returns the length of the time interval between the latest
   *                 Start() and Stop()
   */
  inline double GetInterval() const {
    std::chrono::duration<double> elapsed_seconds = end - start;
    return elapsed_seconds.count();
  }
};
