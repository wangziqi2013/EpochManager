
#include "common.h"

#define CACHE_LINE_SIZE 64

/*
 * class PaddedData() - Pad a data type to a certain fixed length by appending
 *                      extra bytes after useful data field
 *
 * The basic constraint is that the length of the padded structure must be
 * greater than or equal to the streucture being padded
 */
template <typename T, uint64_t length>
class PaddedData {
 public:
  // Define few compile time constants
  static constexpr uint64_t data_size = sizeof(T);
  static constexpr uint64_t padding_size = length - sizeof(T);
  static constexpr uint64_t total_size = length;
  
  T data;
  
  /*
   * operator T() - Type conversion overloading
   */
  operator T() const { return data; }
 private:
  // This is the padding part
  char padding[padding_size];
};

/*
 * class LocalWriteEM - Epoch manager for garbage collection that only uses
 *                      local writes
 *
 * This function is used to reduce scalability problem brought about by a
 * traditional global-counter based epoch manager, where each thread has to
 * call EnterEpoch() and LeaveEpoch() on each operation to maintain global
 * counters that counts the number of active threads entering the system
 * at a certain epoch time period. The innovativity of local write epoch
 * manager is that for each operation, the worker thread only needs to
 * conduct a local write to a variable which is only maintained for each CPU
 * core (i.e. explicitly local for L1 cache dedicated to each core), and there
 * is no global synchronization except for the epoch thread. Epoch thread
 * checks each local counter and finds the minimum one. After that it uses
 * the minimum live worker thread's epoch to reclaim garbage nodes whose
 * epoch of deletion < the epoch of oldest living worker thread
 */
template<uint64_t core_num>
class LocalWriteEM {
 private:
  // They are stored as an array, and we pad it to 64 bytes so that the
  // structure could be shared among cache lines
  // Note: In order for this to work, the class itself must also
  // be allocated on 64 byte aligned memory address
  // To achieve this we use a static member function to do initialization
  // and disallow arbitrary initialization
  PaddedData<std::atomic<uint64_t>, 64> data[core_num];
  
 public:
   
  // Disallow any form of copying and construction without explicitly
  // aligning it to 64 byte boundary
  LocalWriteEM() = delete;
  LocalWriteEM(const LocalWriteEM &) = delete;
  LocalWriteEM(LocalWriteEM &&) = delete;
  LocalWriteEM &operator=(const LocalWriteEM &) = delete;
  LocalWriteEM &operator=(LocalWriteEM &&) = delete;
  
  // This is a map that records the pointer to instances being used
  // and memory addresses being allocated
  // The first is used for construction and destruction, while the latter
  // are used for freeing the memory chunk
  static std::unordered_map<LocalWriteEM *, LocalWriteEM *> instance_map;
  
  /*
   * GetInstance() - Get an instance of the epoch manager
   *
   * We explicitly allcate a chunk of memory from the heap and align
   * it to the 64 byte cache line boundary by always allocating one
   * more slots
   *
   * This function is not thread-safe so please only call it under a
   * single threaded environment
   */
  static LocalWriteEM *GetInstance() {
    char *p = static_cast<char *>(malloc(sizeof()));
  }
  
  /*
   * FreeInstance() - Calls destructor on the pointer and free it
   *
   * Note that this function is not thread-safe - please call it
   * only during single threaded destruction
   */
  static void FreeInstance(LocalWriteEM *p) {
    p->~LocalWriteEM();
    
    auto it = instance_map.find(p);
    
    // This is only valid when debug option is on
    // We use this for debugging and it will be removed by dead code
    // elimination during optimization
    if(it == instance_map.end()) {
      dbg_printf("Invalid LocalWriteEM pointer @ %p\n", p);
      
      assert(false);
    }
    
    // Free the original raw pointer
    free(it->second);
    
    return;
  }
};
