
#pragma once

#include <random>

#include <sched.h>
#include "../src/common.h"
#include <map>
#include <cmath>
#include <cstring>
#include <string>

void PrintTestName(const char *name);
void SleepFor(uint64_t sleep_ms); 
int GetThreadAffinity();
void PinToCore(size_t core_id);
uint64_t GetCoreNum();
 
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
 *
 * Note that this object uses C++11 library generator which is slow, and super
 * non-scalable. For 
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

/*
 * class SimpleInt64Random - Simple paeudo-random number generator 
 *
 * This generator does not have any performance bottlenect even under
 * multithreaded environment, since it only uses local states. It hashes
 * a given integer into a value between 0 - UINT64T_MAX, and in order to derive
 * a number inside range [lower bound, upper bound) we should do a mod and 
 * addition
 *
 * This function's hash method takes a seed for generating a hashing value,
 * together with a salt which is used to distinguish different callers
 * (e.g. threads). Each thread has a thread ID passed in the inlined hash
 * method (so it does not pose any overhead since it is very likely to be 
 * optimized as a register resident variable). After hashing finishes we just
 * normalize the result which is evenly distributed between 0 and UINT64_T MAX
 * to make it inside the actual range inside template argument (since the range
 * is specified as template arguments, they could be unfold as constants during
 * compilation)
 *
 * Please note that here upper is not inclusive (i.e. it will not appear as the 
 * random number)
 */
template <uint64_t lower = 0UL, uint64_t upper = UINT64_MAX>
class SimpleInt64Random {
 public:
   
  /*
   * operator()() - Mimics function call
   *
   * Note that this function must be denoted as const since in STL all
   * hashers are stored as a constant object
   */
  inline uint64_t operator()(uint64_t value, uint64_t salt) const {
    //
    // The following code segment is copied from MurmurHash3, and is used
    // as an answer on the Internet:
    // http://stackoverflow.com/questions/5085915/what-is-the-best-hash-
    //   function-for-uint64-t-keys-ranging-from-0-to-its-max-value
    //
    // For small values this does not actually have any effect
    // since after ">> 33" all its bits are zeros
    //value ^= value >> 33;
    value += salt;
    value *= 0xff51afd7ed558ccd;
    value ^= value >> 33;
    value += salt;
    value *= 0xc4ceb9fe1a85ec53;
    value ^= value >> 33;

    return lower + value % (upper - lower);
  }
};

/*
 * class Argv - Process argument vector of a C program
 *
 * Three classes of options:
 *   1. Short option, i.e. begins with '-' followed by a single character
 *      optionally with '=' followed by a string
 *   2. Long option, i.e. begins with "--" followed by a string
 *      optionally with '=' followed by a string
 *
 * Both 1 and 2 will be stored in a std::map for key and value retrival
 * and option key is string, option value is empty string if does not exist,
 * or the string value if there is a value
 *
 *   3. Argument, i.e. without "--" and "-". 
 *
 * The last type of argument will be stored in std::vector in the order
 * they appear in the argument list
 */
class Argv {
 private:
  // This is the map for string key and value
  std::map<std::string, std::string> kv_map;
  
  // This is the array for string values
  std::vector<std::string> arg_list;
 
 private:
  
  /*
   * AnalyzeArguments() - Analyze arguments in argv and dispatch them into
   *                      either key-value pairs or argument values
   *
   * The return value is a pair indicating whether the construction has 
   * succeeded or not. The second argument is a string telling the reason
   * for a failed construction
   */
  std::pair<bool, const char *> AnalyzeArguments(int argc, char **argv) {
    for(int i = 0;i < argc;i++) {
      char *s = argv[i];
      int len = strlen(s);
      assert(len >= 1);
      
      // -- without anything means all following are argument values
      if((len == 2) && (s[0] == '-') && s[1] == '-') {
        for(int j = i + 1;j < argc;j++) {
          // Construct string object in place
          arg_list.emplace_back(argv[j]);
        }
        
        break; 
      }
      
      if(s[0] == '-') {
        char *key, *value;
        
        if(len == 1) {
          return std::make_pair(false, "Unknown switch: -");
        } else if(s[1] == '-') {
          // We have processed '--' and '-' case
          // Now we know this is a string with prefix '--' and there is at 
          // least one char after '--'
          key = s + 2;
          assert(*key != '\0');
          
          // Advance value until we see nil or '='
          // This makes --= a valid switch (empty string key and empty value)
          value = s + 2;
        } else {
          key = s + 1;
          value = s + 1;
        }
        
        while((*value != '=') && (*value != '\0')) {
          value++; 
        }
        
        // If there is a value then cut the value;
        // otehrwise use empty as value
        if(*value == '=') {
          // This delimits key and value
          *value = '\0';
          value++;
        }
        
        // 1. If "key=" then key is key and value is empty string
        // 2. If "key" then key is key and value is empty string
        // 3. If "key=value" then key is key and value is value
        kv_map[std::string{key}] = std::string{value};
      } else {
        arg_list.emplace_back(s); 
      } // s[0] == '-'
    } // loop through all arguments 
    
    return std::make_pair(true, "");
  }
 
 public:
  Argv(int argc, char **argv) {
    assert(argc > 0);
    assert(argv != nullptr);
    
    // Always ignore the first argument
    auto ret = AnalyzeArguments(argc - 1, argv + 1); 
    if(ret.first == false) {
      throw ret.second; 
    }
    
    return;
  }
  
  /*
   * Exists() - Whetehr a key exists
   *
   * This could either be used for switches, i.e. --key or -key
   * or with key-value pairs, i.e. --key=value or -key=value which just
   * ignores the value
   */
  bool Exists(const std::string &key) {
    return GetValue(key) != nullptr;
  }
  
  /*
   * GetValue() - Returns the value of the key
   *
   * If key does not exist return nullptr
   * If there is no value then there the returned string is empty string
   * pointer 
   */
  std::string *GetValue(const std::string &key) {
    auto it = kv_map.find(key);
    
    if(it == kv_map.end()) {
      return nullptr; 
    } else {
      return &it->second;
    }
  }
  
  /*
   * GetValueAsUL() - This function returns value as a unsigned long integer
   *                  the length of which is platform dependent
   *
   * Whether the value is correctly parsed or not depends on the input. This
   * function checks correctness by:
   *   1. Call std::atoul() to convert the number in a local buffer
   *   2. Call std::to_string() to cast it back to string and check against
   *      the stored string object. If they are not equivalent then we
   *      know the digits are not parsed correctly
   *
   * This function takes a pointer argument pointing to the value that
   * will be modified if the argument value is found and is legal.
   * The return value is true if either the argument value is not found
   * or it is found and legal. If the value is found but illegal then 
   * return value is false
   *
   * This function is not thread-safe since it uses global variable
   */
  bool GetValueAsUL(const std::string &key,
                    unsigned long *result_p) {
    const std::string *value = GetValue(key);
    
    // Not found: not modified, return true 
    if(value == nullptr) {
      return true;
    }
    
    unsigned long result;
    
    // Convert string to number; cathing std::invalid_argumrnt to deal with
    // invalid format
    try {
      result = std::stoul(*value);
    } catch(...) {
      return false; 
    } 
    
    *result_p = result;
    
    // Found: Modified, return true
    return true;
  }
  
  /*
   * GetKVMap() - Returns the key value map
   */
  const std::map<std::string, std::string> &GetKVMap() const {
    return kv_map; 
  }
  
  /*
   * GetArgList() - Returns the argument list
   */
  const std::vector<std::string> &GetArgList() const {
    return arg_list; 
  }
 };
