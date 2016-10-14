
#pragma once

#ifndef _GLOBAL_WRITE_EM_H
#define _GLOBAL_WRITE_EM_H

#include "common.h"

/*
 * class GlobalWriteEM - An implementation of epoch-based safe memory 
 *                       reclamation scheme, using a globally visible epoch
 *                       counter as an approximation to reference counting
 *
 * This class takes a template argument that represents the type of garbage
 * being collected. Please note that in order to  
 */
template<typename GarbageType>
class GlobalWriteEM {
 public:
  // Garbage collection interval (milliseconds)
  constexpr static int GC_INTERVAL = 50;

  /*
   * class GarbageNode - A linked list of garbages
   *
   * We store a pointer to GarbageType declared in template argument to hold
   * the garbage and put it inside a linked list
   *
   * Note that all members do not need to be atomic since they are 
   * always accessed single threaded
   */
  class GarbageNode {
   public:
    const GarbageType *node_p;
    GarbageNode *next_p;
  };

  /*
   * class EpochNode - A linked list of epoch node that keeps track of the
   *                   number of active threads entering that epoch
   *
   * This struct is also the head of garbage node linked list, which must
   * be made atomic since different worker threads will contend to insert
   * garbage into the head of the list using CAS
   */
  class EpochNode {
    // We need this to be atomic in order to accurately
    // count the number of threads
    // Note that contention on this variable is still possible:
    //    the epoch thread need to check its value and then act
    //     accordingly. If in the meantime between checking and 
    //     acting a thread comes and increases the epoch counter then
    //     the current epoch should not be recycled
    std::atomic<int64_t> active_thread_count;

    // We need this to be atomic to be able to
    // add garbage nodes without any race condition
    // i.e. GC nodes are CASed onto this pointer
    std::atomic<GarbageNode *> garbage_list_p;

    // This does not need to be atomic since it is
    // only maintained by the epoch thread
    EpochNode *next_p;
  };

  // The head pointer does not need to be atomic
  // since it is only accessed by epoch manager
  EpochNode *head_epoch_p;

  // *** NOTE ***
  // This must be atomic and read/write to this variable should be synchronized
  // Consider the following case:
  //   1. CPU 0's epoch manager changes this pointer by creating a new epoch
  //       * CONTEXT SWITCH * -> MEMORY BARRIER HERE?
  //   2. CPU 0's worker thread enters the new epoch
  //      CPU 0's worker thread access node N
  //   3. CPU 1's worker thread has not yet seen the update, enters old epoch
  //      CPU 1's worker thread unlinks node N
  //      CPU 1's worker thread has not seen the update yet, and links
  //        node N into the old epoch's garbage list
  //   4. CPU 1's worker thread exits, marking old epoch's ref count = 0
  //   5. Epoch thread frees all garbage nodes in the old epoch
  //   6. CPU 0's worker thread access node N, but it has been freed
  //      Ouch!!!!!
  //
  EpochNode * current_epoch_p;

  // This flag indicates whether the destructor is running
  // If it is true then GC thread should not clean
  // Therefore, strict ordering is required
  std::atomic<bool> exited_flag;

  // If GC is done with external thread then this should be set
  // to nullptr
  // Otherwise it points to a thread created by EpochManager internally
  std::thread *thread_p;

  #ifdef NDEBUG
  // Statistical maintained for epoches
  
  // Number of nodes we have freed
  size_t freed_count;

  // These two are used for debugging
  // They do not have to be atomic since these two are only accessed by
  // the epoch thread, so R/W atomicity could be guaranteed
  size_t epoch_created;
  size_t epoch_freed;

  std::atomic<size_t> epoch_join;
  std::atomic<size_t> epoch_leave;
  #endif

  /*
   * Constructor - Initialize the epoch list to be a single node
   *
   * NOTE: We do not start thread here since the init of bw-tree itself
   * might take a long time
   */
  GlobalWriteEM() {
    // Creates the initial epoch to count active threads
    current_epoch_p = new EpochNode{};

    current_epoch_p->active_thread_count.store(0);
    current_epoch_p->garbage_list_p = nullptr;
    current_epoch_p->next_p = nullptr;

    // This write is always done by epoch thread
    head_epoch_p = current_epoch_p;

    // We allocate and run this later
    thread_p = nullptr;

    // This is used to notify the cleaner thread that it has ended
    exited_flag.store(false);

    #ifdef NDEBUG
    // Initialize atomic counter to record how many
    // freed has been called inside epoch manager
    
    freed_count = 0UL;

    // It is not 0UL since we create an initial epoch on initialization
    epoch_created = 1UL;
    epoch_freed = 0UL;

    epoch_join.store(0);
    epoch_leave.store(0);
    #endif

    return;
  }

  /*
   * Destructor - Stop the worker thread and cleanup resources not freed
   *
   * This function waits for the worker thread using join() method. After the
   * worker thread has exited, it synchronously clears all epochs that have
   * not been recycled by calling ClearEpoch()
   *
   * NOTE: If no internal GC is started then thread_p would be a nullptr
   * and we neither wait nor free the pointer.
   */
  ~GlobalWriteEM() {
    // Set stop flag and let thread terminate
    // Also if there is an external GC thread then it should
    // check this flag everytime it does cleaning since otherwise
    // the un-thread-safe function ClearEpoch() would be ran
    // by more than 1 threads
    exited_flag.store(true);

    // If thread pointer is nullptr then we know the GC thread
    // is not started. In this case do not wait for the thread, and just
    // call destructor
    if(thread_p != nullptr) {
      thread_p->join();

      // Free memory
      delete thread_p;
      
      dbg_printf("Internal GC Thread stops\n");
    }

    // So that in the following function the comparison
    // would always fail, until we have cleaned all epoch nodes
    current_epoch_p = nullptr;

    // If all threads has exited then all thread counts are
    // 0, and therefore this should proceed way to the end
    ClearEpoch();

    // Since we guarantee all counters must be cleared at this point,
    // and we called ClearEpoch() just now, this should work
    assert(head_epoch_p == nullptr);

    #ifdef NDEBUG
    dbg_printf("Stat: Freed %lu nodes by epoch manager\n",
               freed_count);

    dbg_printf("      Epoch created = %lu; epoch freed = %lu\n",
               epoch_created,
               epoch_freed);

    dbg_printf("      Epoch join = %lu; epoch leave = %lu\n",
               epoch_join.load(),
               epoch_leave.load());
    #endif

    return;
  }

  /*
   * CreateNewEpoch() - Create a new epoch node and append it to the current
   *                    tail of linked list of epoch nodes
   *
   * Note that the "append node" oepration does not have to be atomic
   * since even if the visibility of the new epoch node differs among
   * different cores, it does not matter since it implies some cores will
   * still see older epoch and this does not cause premature free of resources
   */
  void CreateNewEpoch() {
    EpochNode *epoch_node_p = new EpochNode{};

    epoch_node_p->active_thread_count.store(0);
    epoch_node_p->garbage_list_p.store(nullptr);

    // We always append to the tail of the linked list
    // so this field for new node is always nullptr
    epoch_node_p->next_p = nullptr;

    // Update its previous node (current tail)
    current_epoch_p->next_p = epoch_node_p;

    // And then switch current epoch pointer
    current_epoch_p = epoch_node_p;

    #ifdef NDEBUG
    epoch_created++;
    #endif

    return;
  }

  /*
   * AddGarbageNode() - Add garbage node into the current epoch
   *
   * NOTE: This function is called by worker threads so it has
   * to consider race conditions
   */
  void AddGarbageNode(const GarbageType *node_p) {
    // We need to keep a copy of current epoch node
    // in case that this pointer is increased during
    // the execution of this function
    //
    // NOTE: Current epoch must not be recycled, since
    // the current thread calling this function must
    // come from an epoch <= current epoch
    // in which case all epochs before that one should
    // remain valid
    EpochNode *epoch_p = current_epoch_p;

    // These two could be predetermined
    GarbageNode *garbage_node_p = new GarbageNode{};
    garbage_node_p->node_p = node_p;

    garbage_node_p->next_p = epoch_p->garbage_list_p.load();

    while(1) {
      // Then CAS previous node with new garbage node
      // If this fails, then garbage_node_p->next_p is the actual value
      // of garbage_list_p, in which case we do not need to load it again
      bool ret = \
        epoch_p->garbage_list_p.compare_exchange_strong(garbage_node_p->next_p,
                                                        garbage_node_p);

      // If CAS succeeds then just return
      if(ret == true) {
        break;
      }
    } // while 1

    return;
  }

  /*
   * JoinEpoch() - Let current thread join this epoch
   *
   * The effect is that all memory deallocated on and after
   * current epoch will not be freed before current thread leaves
   *
   * NOTE: It is possible that prev_count < 0, because in ClearEpoch()
   * the cleaner thread will decrease the epoch counter by a large amount
   * to prevent this function using an epoch currently being recycled
   */
  inline EpochNode *JoinEpoch() {
    int64_t prev_count;
    EpochNode *epoch_p;
    
    do {
      // Contention: current_epoch_p might be moved after we
      // read it, and then it could be under GC
      // So we should check whether it is latched by GC thread
      epoch_p = current_epoch_p;
  
      // If the value is < 0 then we know the current epoch is being latched
      // by the GC thread and will be soon removed.
      // Thus we try reloading the current epoch pointer and try again
      prev_count = epoch_p->active_thread_count.fetch_add(1);
    } while(prev_count < 0);

    #ifdef BWTREE_DEBUG
    epoch_join.fetch_add(1);
    #endif

    return epoch_p;
  }

  /*
   * LeaveEpoch() - Leave epoch a thread has once joined
   *
   * After an epoch has been cleared all memories allocated on
   * and before that epoch could safely be deallocated
   */
  inline void LeaveEpoch(EpochNode *epoch_p) {
    // This might return a negative value if the current epoch
    // is being cleaned
    epoch_p->active_thread_count.fetch_sub(1);

    #ifdef BWTREE_DEBUG
    epoch_leave.fetch_add(1);
    #endif

    return;
  }

  /*
   * FreeGarbageType() - Free a garbage type node by GC thread
   *
   * This function should be overloaded if the default free operation
   * is not simply deleting the garbage node
   */
  void FreeGarbageType(GarbageType *node_p) {
    delete node_p;
    
    #ifdef NDEBUG
    freed_count++;  
    #endif 

    return;
  }

  /*
   * ClearEpoch() - Sweep the chain of epoch and free memory
   *
   * The minimum number of epoch we must maintain is 1 which means
   * when current epoch is the head epoch we should stop scanning
   */
  void ClearEpoch() {
    // Keep cleaning until this is the only epoch left
    // *OR* there is no epoch left depending on whether 
    // current_epoch_p == nullptr
    while(head_epoch_p != current_epoch_p) {
      // Latch it using a very large negative number, such that all
      // threads trying to fetch_add() it will get a negative number
      // and thus try to reload current epoch pointer
      bool ret = \
        head_epoch_p->active_thread_count.compare_exchange_strong(0, INT64_MIN);
        
      // The head epoch is not 0; could not recollect it
      if(ret == false) {
        break;
      }

      // After this point all fetch_add() on the epoch counter would return
      // a negative value which will cause re-read of current_epoch_p
      // to prevent joining an epoch that is being deleted

      // If the epoch has cleared we just loop through its garbage chain
      // and then free each delta chain

      const GarbageNode *next_garbage_node_p = nullptr;

      // Walk through its garbage chain
      for(const GarbageNode *garbage_node_p = head_epoch_p->garbage_list_p.load();
          garbage_node_p != nullptr;
          garbage_node_p = next_garbage_node_p) {
        FreeGarbageType(garbage_node_p->node_p);
        next_garbage_node_p = garbage_node_p->next_p;

        delete garbage_node_p;
      }

      EpochNode *next_epoch_node_p = head_epoch_p->next_p;
      delete head_epoch_p;

      #ifdef BWTREE_DEBUG
      epoch_freed++;
      #endif

      // This may or may not leads to a nullptr
      head_epoch_p = next_epoch_node_p;
    } // whule(current != head)

    return;
  }

  /*
   * PerformGarbageCollection() - Actual job of GC is done here
   *
   * We need to separate the GC loop and actual GC routine to enable
   * external threads calling the function while also allows BwTree maintains
   * its own GC thread using the loop
   */
  void PerformGarbageCollection() {
    // The order is important - If CreateNewEpoch() is called
    // before ClearEpoch() then very likely contention will
    // happen on current_epoch_p, in a sense that:
    //   1. Worker thread load current_epoch_p, and its counter = 0
    //      Assume head_epoch_p == current_epoch_p at this moment
    //   2. CreateNewEpoch() moves current_epoch_p to the newly created
    //      epoch node
    //   3. ClearEpoch() latches head_epoch_p, which assigns a large negative
    //      number to its counter
    //   4. Worker thread fetch_add() the counter, failure!!!
    //
    // However, with the sequence of ClearEpoch() and CreateNewEpoch() reversed
    // such changes are really slim or even negligible, since the interval 
    // between CreateNewEpoch() and ClearEpoch() would almost definitely
    // let the worker thread pass without getting a negative number from
    // fetch_add()
     
    ClearEpoch();
    CreateNewEpoch();
    
    return;
  }

  /*
   * ThreadFunc() - The cleaner thread executes this every GC_INTERVAL ms
   *
   * This function exits when exit flag is set to true
   */
  void ThreadFunc() {
    // While the parent is still running
    // We do not worry about race condition here
    // since even if we missed one we could always
    // hit the correct value on next try
    while(exited_flag.load() == false) {
      //printf("Start new epoch cycle\n");
      PerformGarbageCollection();

      // Sleep for 50 ms
      std::chrono::milliseconds duration{GC_INTERVAL};
      std::this_thread::sleep_for(duration);
    }

    return;
  }

  /*
   * StartThread() - Start cleaner thread for garbage collection
   *
   * NOTE: This is not called in the constructor, and needs to be
   * called manually
   */
  void StartThread() {
    thread_p = new std::thread{[this](){this->ThreadFunc();}};

    return;
  }

}; // Epoch manager

#endif 
