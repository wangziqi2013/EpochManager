
#include "common.h"

#define CACHE_LINE_SIZE 64

template<uint64_t core_num>
class LocalWriteEMFactory;

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
  operator T() { return data; }

  /*
   * operator-> - We use this to access elements inside the data member of
   *              the wrapped class
   *
   * So the class being wrapped is accessed like we are using a pointer
   */
  T *operator->() { return &data; }

  /*
   * Get() - Explicitly call to return a reference of the data being wrapped
   */
  T &Get() const { return data; }

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
  friend class LocalWriteEMFactory<core_num>;
 public:
  // It is the type of the cuonter we use to represent an epoch
  using CounterType = uint64_t;

  // This is a padded version of epoch counter
  using ElementType = PaddedData<std::atomic<uint64_t>, CACHE_LINE_SIZE>;
  
 private:
  // They are stored as an array, and we pad it to 64 bytes so that the
  // structure could be shared among cache lines
  // Note: In order for this to work, the class itself must also
  // be allocated on 64 byte aligned memory address
  // To achieve this we use a static member function to do initialization
  // and disallow arbitrary initialization
  ElementType per_core_counter_list[core_num];
 
  // This is the epoch counter that each thread needs to read when entering
  // an epoch
  // Note that according to the design, the epoch timer is set to a relatively
  // large value (50 ms, etc.) so most of the read operation for every thread
  // should be a local read unless the counter happens to be increamented by
  // the epoch thread, which does not consitute a major overhead
  ElementType epoch_counter;
 
  /*
   * Constructor - This is the only valid way of constructing an instance
   */
  LocalWriteEM() {
    dbg_printf("C'tor for %lu cores called. p = %p\n", core_num, this);

    // Initialization - all counter should be set to 0 since the global
    // epoch counter also starts at 0
    for(size_t i = 0;i < core_num;i++) {
      per_core_counter_list[i]->store(0);
    }

    // Also set the current epoch to be 0
    epoch_counter->store(0);
    
    return;
  }
  
  /*
   * Destructor - This could only be called by the factory class
   */
  ~LocalWriteEM() {
    dbg_printf("D'tor for %lu cores called. p = %p\n", core_num, this);
    
    return;
  }
  
 public:
   
  // Disallow any form of copying and construction without explicitly
  // aligning it to 64 byte boundary by the public
  LocalWriteEM(const LocalWriteEM &) = delete;
  LocalWriteEM(LocalWriteEM &&) = delete;
  LocalWriteEM &operator=(const LocalWriteEM &) = delete;
  LocalWriteEM &operator=(LocalWriteEM &&) = delete;

  /*
   * AnnounceEnter() - Announces that a thread enters the system
   *
   * This effectively let a thread running on the core it claimed to be
   * (through function argument) read the global epoch counter (which
   * should be a local cache read in most of the time) and then write
   * into its local latest enter epoch
   */
  void AnnounceEnter(uint64_t core_id) {
    // Under debug mopde let's assert core id is correct to avoid
    // serious bugs
    assert(core_id < core_num);
 
    // This is a strict read/write ordering - load must always happen
    // before store
    per_core_counter_list[core_id]->store(epoch_counter->load());

    return;
  }
};

/*
 * class LocalWriteEMFactory - Factory class for constructing EM instances
 *
 * This class should be used as the only way of constructing and destroying
 * a LocalWriteEM instance
 */
template<uint64_t core_num>
class LocalWriteEMFactory {
 public:
  // This is a map that records the pointer to instances being used
  // and memory addresses being allocated
  // The first is used for construction and destruction, while the latter
  // are used for freeing the memory chunk
  static std::unordered_map<void *, void *> instance_map;

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
  static LocalWriteEM<core_num> *GetInstance() {
    char *p = reinterpret_cast<char *>(malloc(sizeof(LocalWriteEM<core_num>) +
                                              CACHE_LINE_SIZE));
                                              
    dbg_printf("Malloc() returns p = %p\n", p);
    
    // 0xFFFF FFFF FFFF FFC0
    const uint64_t cache_line_mask = ~(CACHE_LINE_SIZE - 1);
    
    // If p is not aligned to 64 byte boundary then make it aligned
    // If it is aligned then this does not change it
    void *q = reinterpret_cast<void *>(
                reinterpret_cast<uint64_t>(
                  p + CACHE_LINE_SIZE - 1) & cache_line_mask);
    
    // Insert it into the map and since it does not exist yet the
    // insertion must be a success
    // Map from "q" to "p", i.e. from adjusted address to allocated address
    auto it = instance_map.insert(std::make_pair(q, static_cast<void *>(p)));
    assert(it.second == true); (void)it;
    
    // At last call constructor for the class
    new (q) LocalWriteEM<core_num>{};
    
    return reinterpret_cast<LocalWriteEM<core_num> *>(q);
  }

  /*
   * FreeInstance() - Calls destructor on the pointer and free it
   *
   * Note that this function is not thread-safe - please call it
   * only during single threaded destruction
   */
  static void FreeInstance(void *p) {
    // Call destructor after casting it to appropriate type
    reinterpret_cast<LocalWriteEM<core_num> *>(p)->~LocalWriteEM<core_num>();

    auto it = instance_map.find(p);

    // Since it must be a valid allocated pointer we should always be able
    // to find it
    assert(it != instance_map.end());

    // Free the original raw pointer
    free(it->second);

    return;
  }
};
