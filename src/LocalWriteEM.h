
#include "common.h"

#define CACHE_LINE_SIZE 64

template<uint64_t core_num,
         typename GarbageType>
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
 *
 * The second template argument is the type of garbage node. We keep a 
 * pointer type to GarbageType in the garbage node.
 */
template<uint64_t core_num,
         typename GarbageType>
class LocalWriteEM {
  friend class LocalWriteEMFactory<core_num, GarbageType>;
 public:
  // It is the type of the cuonter we use to represent an epoch
  using CounterType = uint64_t;

  // This is a padded version of epoch counter
  using ElementType = PaddedData<std::atomic<CounterType>, CACHE_LINE_SIZE>;
  
 private:

  /*
   * class GarbageNode - The node we use to hold garbage
   *
   * All garbage nodes in the systems forms a garbage chain in which all
   * delayed allocation together with a counter recording the time it was
   * removed are stored.
   *
   * Upon garbage collection, the GC thread scans the garbage chain linked
   * list, and compares the deleted epoch with the current minimum epoch
   * announced by all threads using the per-core counter. Garbage nodes
   * with its deleted epoch being smaller than the global epoch will be
   * removed
   */
  class GarbageNode {
   public:
    CounterType deleted_epoch;
    GarbageType *garbage_p;

    // This will be updated in an unsuccessful CAS, so make it public
    GarbageNode *next_p;
    
    /*
     * Constructor
     *
     * Note that we do not initialize next_p here since it will be part of
     * the CAS process
     */
    GarbageNode(GarbageType *p_garbage_p, CounterType p_deleted_epoch) :
      deleted_epoch{p_deleted_epoch},
      garbage_p{p_garbage_p}
    {}
    
    /*
     * LinkTo() - Given the linked list head, try to link itself onto that
     *            linked list
     *
     * Note that although this function uses CAS instead of lock, it is
     * not wait-free - the CAS loop is effectively like a spin lock
     */
    inline void LinkTo(std::atomic<GarbageNode *> *head_p) {
      next_p->head_p->load();
      
      // Empty loop
      // Note that the next_p will be loaded with the most up-to-date
      // value of head_p, so we do not need to load it explicitly
      while(head_p->compare_exchange_strong(next_p, this) == false) {}
      
      return;
    }
  };

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

  // The following does not have to be cache aligned since they 
  // are not usually operated frequently OR could not benefit from
  // cache alignment

  // This is the head of the linked list where garbage nodes are linked
  // into
  // In the future we might want to use a per core garbage list to reduce
  // contention and further accelerate the Insert() procedure
  std::atomic<GarbageNode *> garbage_head_p;
  
  // This is set if the destructor is called and we need to terminate the
  // GC thread, if there is one
  // Or if there is an external it should also check this flag
  // Note that on Intel platform this need not be an atomic variable since
  // Intel CPU read/write are of acquire/release semantics
  // But to accomondate other platforms that have weaker memory ordering
  // we should make it an atiomic to avoid potential bugs
  std::atomic<bool> exited_flag;
 
  /*
   * Constructor 
   *
   * The constructor has been deliberately declared as private member to
   * prevent construction on unaligned address. Please use WriteLocalEMFactory 
   * class to allocate it in a cache aligned manner
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

    // The end of the linked list
    garbage_head_p.store(nullptr);
    
    // This will be set true in destructor
    exited_flag.store(false);
    
    return;
  }
  
  /*
   * Destructor - This could only be called by the factory class
   */
  ~LocalWriteEM() {
    dbg_printf("D'tor for %lu cores called. p = %p\n", core_num, this);
    
    // Signal all threads reading this variable that the epoch manager object 
    // will soon be destroyed, so just stop
    // Even if external threads are used, this does not pose any problem
    // since setting atomic variable to false is idempotent
    SignalExit();
    
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
   * HasExited() - Whether the exit signal has been issued
   *
   * This function is a wrapped to allow external access of the exited_flag
   * variable. If an external thread is used as the GC thread then the user
   * of this EM should signal exiting first, wait for external threads on this
   * condition, and then call destructor of the EM 
   */
  bool HasExited() {
    return exited_flag.load();  
  }

  /*
   * SignalExit() - Signals that the epoch manager will exit by setting an 
   *                atomic flag to true
   *
   * If the epoch manager uses its own thread as the GC thread then after this
   * function we should wait for that thread to stop and continue. However,
   * if an external thread is used for GC, then after signaling this, the 
   * external thread should react to the signal by calling query function for
   * the status of exited_flag, and then exit. The user of the epoch manager
   * should then wait for the external thread to exit before destroying the 
   * EM object. Otherwise the thread might still be running after the EM has
   * been destroyed, corrupting random memory location.
   */
  void SignalExit() {
    exited_flag.store(true);
    
    return; 
  }

  /*
   * AnnounceEnter() - Announces that a thread enters the system
   *
   * This effectively let a thread running on the core it claimed to be
   * (through function argument) read the global epoch counter (which
   * should be a local cache read in most of the time) and then write
   * into its local latest enter epoch
   */
  inline void AnnounceEnter(uint64_t core_id) {
    // Under debug mopde let's assert core id is correct to avoid
    // serious bugs
    assert(core_id < core_num);
 
    // This is a strict read/write ordering - load must always happen
    // before store
    per_core_counter_list[core_id]->store(epoch_counter->load());

    return;
  }
  
  /*
   * AddGarbageNode() - Adds a node whose deallocation will be delayed
   *
   * This function creates a garbage node linked list node and puts both
   * garbage node and the epoch counter into the garbage chain
   *
   * Note: When this function is called the caller should guarantee that the
   * node is already not visible by other threads, otherwise the assumption
   * made about the garbage collection mechanism will break
   */
  void AddGarbageNode(GarbageType *garbage_p) {
    // We load epoch counter here such that the time the garbage node disappears
    // from the system <= current epoch time
    // As long as we only do GC for nodes whose epoch counter < earlest
    // accessing epoch counter which further <= time of the thread touching
    // any shared resource, then we know it is saft to reclaim the memory
    GarbageNode *gn_p = new GarbageNode{garbage_p, epoch_counter->load()};
    
    // Use CAS to link the node onto the linked list
    gn_p->LinkTo(&garbage_head_p);
    
    return;
  }
  
  /*
   * FreeGarbageNode() - Frees a garbage type
   *
   * If users want to write their own epoch manager to destroy objects in a 
   * customized way, then they should modify this function. Here we just 
   * call operator delete to free
   */
  inline void FreeGarbageNode(GarbageType *garbage_p) {
    delete garbage_p;
    
    return;
  }
  
  /*
   * GotoNextEpoch() - Increases the epoch counter value by 1
   */
  void GotoNextEpoch() {
    // Atomically increase the epoch counter - This function does not
    epoch_counter->fetch_add(1);
    
    return; 
  }
  
  /*
   * DoGC() - This is the main function for doing garbage collection
   *
   * Note that worker threads could only access the head of linked list, which
   * should not be modified by the GC thread, otherwise ABA problem might
   * emerge since if we free a node during GC, and in the meantime a worker
   * thread comes in and allocates a node that is exactly the node we
   * just freed (malloc() tends to do that, actually) then we will have a
   * ABA problem. ABA problem might or might not be harmful depending on the
   * context, but it is best practice for us to aovid it since if the design
   * is changed in the future we will have less potential undocumented problems
   *
   * NOTE: This function does not increase epoch counter, since the pace that
   * epoch counter increases could optionally differ from the GC pace
   */
  void DoGC() {
    // We use this to remember the minimum number of cores
    uint64_t min_epoch = per_core_counter_list[0]->load();
    
    // If there are more than 1 core then we just loop through
    // counters for each core and pick the smaller one everytime
    for(uint64_t i = 1;i < core_num;i++) {
      uint64_t counter = per_core_counter_list[i]->load();
      
      if(counter < min_epoch) {
        min_epoch = counter; 
      }
    }
    
    // Now we have the miminum epoch which is the time <= the earlist thread
    // entering the system
    // We could collect all garbage nodes before this time
    
    // Load the head of the linked list
    GarbageNode *current_node_p = garbage_head_p.load();
    if(current_node_p == nullptr) {
      return; 
    }
    
    GarbageNode *next_node_p = current_node_p->next_p;
    
    while(next_node_p != nullptr) {
      CounterType next_counter = next_node_p->deleted_epoch;
      
      if(next_counter < min_epoch) {
        // If next node is qualified then remove both the garbage node
        // and the wrapper, and since current_node_p->next_p has already
        // been set to the next node, we know these two pointers are still
        // pointing to neighbor nodes
        current_node_p->next_p = next_node_p->next_p;
        
        FreeGarbageNode(next_node_p->garbage_p);
        delete next_node_p;
        
        next_node_p = current_node_p->next_p;
      } else {
        // Otherwise, we know next_node_p will not be freed, and it is
        // a valid pointer, so change current_node_p to it
        // and check its next node
        current_node_p = next_node_p;
        next_node_p = next_node_p->next_p;  
      }
    }
    
    return;
  }
  
  /*
   * ThreadFunc() - This is the function body for GC thread
   *
   * This function mainly wraps DoGC(), with a delay, the purpose of which is
   * to control the frequency we do GC (and invalidate local caches of the 
   * global counter kept by each worker thread in their own CPU cores).
   */
  void ThreadFunc() {
    // By default the thread sleeps for 50 milli seconds and then do GC
    const uint64_t sleep_ms = 50UL;
    
    // Loop on the atomic flag that will be set when destructor is called
    // (it is the first operation inside the destructor)
    while(HasExited() == false) {
      GotoNextEpoch();
      DoGC();
      
      std::chrono::milliseconds duration{sleep_ms};
      std::this_thread::sleep_for(duration);
    }
    
    return;
  }
};

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

/*
 * class LocalWriteEMFactory - Factory class for constructing EM instances
 *
 * This class should be used as the only way of constructing and destroying
 * a LocalWriteEM instance
 */
template<uint64_t core_num, 
         typename GarbageType>
class LocalWriteEMFactory {
 public:

  // This is the type of the EM it derives
  using TargetType = LocalWriteEM<core_num, GarbageType>;

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
  static TargetType *GetInstance() {
    // The extra cache line size is used to aligning all elements
    // to the cache line boundary
    char *p = \
      reinterpret_cast<char *>(
        malloc(sizeof(TargetType) + CACHE_LINE_SIZE));
                                              
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
    // Note that we should construct on the aligned address
    new (q) LocalWriteEM<core_num, GarbageType>{};
    
    return reinterpret_cast<LocalWriteEM<core_num, GarbageType> *>(q);
  }

  /*
   * FreeInstance() - Calls destructor on the pointer and free it
   *
   * Note that this function is not thread-safe - please call it
   * only during single threaded destruction
   */
  static void FreeInstance(TargetType *p) {
    // Call destructor after casting it to appropriate type
    // Note that we should call destructor on aligned address, i.e. before
    // translating it to the malloc'ed address
    p->~TargetType();

    auto it = instance_map.find(p);

    // Since it must be a valid allocated pointer we should always be able
    // to find it
    assert(it != instance_map.end());

    // Free the original raw pointer
    free(it->second);

    return;
  }
};
