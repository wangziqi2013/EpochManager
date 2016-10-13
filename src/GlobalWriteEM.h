
#pragma once

#include "common.h"

#ifndef _GLOBAL_WRITE_EM_H
#define _GLOBAL_WRITE_EM_H

/*
 * class GlobalWriteEM - An implementation of epoch-based safe memory 
 *                       reclamation scheme, using a globally visible epoch
 *                       counter as an approximation to reference counting
 *
 * This class takes a template argument that represents the type of garbage
 * being collected. Please note that in order to  
 */
template<GarbageType>
class EpochManager {
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
    std::atomic<int> active_thread_count;

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

  // This does not need to be atomic because it is only written
  // by the epoch manager and read by worker threads. But it is
  // acceptable that allocations are delayed to the next epoch
  EpochNode *current_epoch_p;

  // This flag indicates whether the destructor is running
  // If it is true then GC thread should not clean
  // Therefore, strict ordering is required
  std::atomic<bool> exited_flag;

  // If GC is done with external thread then this should be set
  // to nullptr
  // Otherwise it points to a thread created by EpochManager internally
  std::thread *thread_p;

  #ifdef NDEBUG
  // The counter that counts how many free is called
  // inside the epoch manager
  // NOTE: We cannot precisely count the size of memory freed
  // since sizeof(Node) does not reflect the true size, since
  // some nodes are embedded with complicated data structure that
  // maintains its own memory
  
  // Number of nodes we have freed
  size_t freed_count;

  // Number of NodeID we have freed
  size_t freed_id_count;

  // These two are used for debugging
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
  EpochManager() {
    // Creates the initial epoch to count active threads
    current_epoch_p.store(new EpochNode{});

    current_epoch_p->active_thread_count.store(0);
    current_epoch_p->garbage_list_p = nullptr;
    current_epoch_p->next_p = nullptr;

    head_epoch_p = current_epoch_p;

    // We allocate and run this later
    thread_p = nullptr;

    // This is used to notify the cleaner thread that it has ended
    exited_flag.store(false);

    #ifdef NDEBUG
    // Initialize atomic counter to record how many
    // freed has been called inside epoch manager
    
    freed_count = 0UL;
    freed_id_count = 0UL;

    // It is not 0UL since we create an initial epoch on initialization
    epoch_created = 1UL;
    epoch_freed = 0UL;

    epoch_join = 0UL;
    epoch_leave = 0UL;
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
  ~EpochManager() {
    // Set stop flag and let thread terminate
    // Also if there is an external GC thread then it should
    // check this flag everytime it does cleaning since otherwise
    // the un-thread-safe function ClearEpoch() would be ran
    // by more than 1 threads
    exited_flag.store(true);

    // If thread pointer is nullptr then we know the GC thread
    // is not started. In this case do not wait for the thread, and just
    // call destructor
    //
    // NOTE: The destructor routine is not thread-safe, so if an external
    // GC thread is being used then that thread should check for
    // exited_flag everytime it wants to do GC
    //
    // If the external thread calls ThreadFunc() then it is safe
    if(thread_p != nullptr) {
      bwt_printf("Waiting for thread\n");
      
      thread_p->join();

      // Free memory
      delete thread_p;
      
      bwt_printf("Thread stops\n");
    }

    // So that in the following function the comparison
    // would always fail, until we have cleaned all epoch nodes
    current_epoch_p = nullptr;

    // If all threads has exited then all thread counts are
    // 0, and therefore this should proceed way to the end
    ClearEpoch();

    // If we have a bug (currently there is one) then as a temporary
    // measure just force cleaning all epoches no matter whether they
    // are cleared or not
    if(head_epoch_p != nullptr) {
      bwt_printf("ERROR: After cleanup there is still epoch left\n");
      bwt_printf("==============================================\n");
      bwt_printf("DUMP\n");

      for(EpochNode *epoch_node_p = head_epoch_p;
          epoch_node_p != nullptr;
          epoch_node_p = epoch_node_p->next_p) {
        bwt_printf("Active thread count: %d\n",
               epoch_node_p->active_thread_count.load());
        epoch_node_p->active_thread_count = 0;
      }

      bwt_printf("RETRY CLEANING...\n");
      ClearEpoch();
    }

    assert(head_epoch_p == nullptr);
    bwt_printf("Garbage Collector has finished freeing all garbage nodes\n");

    #ifdef BWTREE_DEBUG
    bwt_printf("Stat: Freed %lu nodes and %lu NodeID by epoch manager\n",
               freed_count,
               freed_id_count);

    bwt_printf("      Epoch created = %lu; epoch freed = %lu\n",
               epoch_created,
               epoch_freed);

    bwt_printf("      Epoch join = %lu; epoch leave = %lu\n",
               epoch_join.load(),
               epoch_leave.load());
    #endif

    return;
  }

  /*
   * CreateNewEpoch() - Create a new epoch node
   *
   * This functions does not have to consider race conditions
   */
  void CreateNewEpoch() {
    bwt_printf("Creating new epoch...\n");

    EpochNode *epoch_node_p = new EpochNode{};

    epoch_node_p->active_thread_count = 0;
    epoch_node_p->garbage_list_p = nullptr;

    // We always append to the tail of the linked list
    // so this field for new node is always nullptr
    epoch_node_p->next_p = nullptr;

    // Update its previous node (current tail)
    current_epoch_p->next_p = epoch_node_p;

    // And then switch current epoch pointer
    current_epoch_p = epoch_node_p;

    #ifdef BWTREE_DEBUG
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
  void AddGarbageNode(const BaseNode *node_p) {
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
    GarbageNode *garbage_node_p = new GarbageNode;
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
      } else {
        bwt_printf("Add garbage node CAS failed. Retry\n");
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
try_join_again:
    // We must make sure the epoch we join and the epoch we
    // return are the same one because the current point
    // could change in the middle of this function
    EpochNode *epoch_p = current_epoch_p;

    int64_t prev_count = epoch_p->active_thread_count.fetch_add(1);

    // We know epoch_p is now being cleaned, so need to read the
    // current epoch again because it must have been moved
    if(prev_count < 0) {
      goto try_join_again;
    }

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
   * FreeEpochDeltaChain() - Free a delta chain (used by EpochManager)
   *
   * This function differs from the one of the same name in BwTree definition
   * in the sense that for tree destruction there are certain node
   * types not being accepted. But in EpochManager we must support a wider
   * range of node types.
   *
   * NOTE: For leaf remove node and inner remove, the removed node id should
   * also be freed inside this function. This is because the Node ID might
   * be accessed by some threads after the time the remove node was sent
   * here. So we need to make sure all accessing threads have exited before
   * recycling NodeID
   */
  void FreeEpochDeltaChain(const BaseNode *node_p) {
    const BaseNode *next_node_p = node_p;

    while(1) {
      node_p = next_node_p;
      assert(node_p != nullptr);

      NodeType type = node_p->GetType();

      switch(type) {
        case NodeType::LeafInsertType:
          next_node_p = ((LeafInsertNode *)node_p)->child_node_p;

          delete (LeafInsertNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif
          break;
        case NodeType::LeafDeleteType:
          next_node_p = ((LeafDeleteNode *)node_p)->child_node_p;

          delete (LeafDeleteNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          break;
        case NodeType::LeafSplitType:
          next_node_p = ((LeafSplitNode *)node_p)->child_node_p;

          delete (LeafSplitNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          break;
        case NodeType::LeafMergeType:
          FreeEpochDeltaChain(((LeafMergeNode *)node_p)->child_node_p);
          FreeEpochDeltaChain(((LeafMergeNode *)node_p)->right_merge_p);

          delete (LeafMergeNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          // Leaf merge node is an ending node
          return;
        case NodeType::LeafRemoveType:
          // This recycles node ID
          tree_p->InvalidateNodeID(((LeafRemoveNode *)node_p)->removed_id);

          delete (LeafRemoveNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          freed_id_count++;
          #endif

          // We never try to free those under remove node
          // since they will be freed by recursive call from
          // merge node
          //
          // TODO: Put remove node into garbage list after
          // IndexTermDeleteDelta was posted (this could only be done
          // by one thread that succeeds CAS)
          return;
        case NodeType::LeafType:
          delete (LeafNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          // We have reached the end of delta chain
          return;
        case NodeType::InnerInsertType:
          next_node_p = ((InnerInsertNode *)node_p)->child_node_p;

          delete (InnerInsertNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          break;
        case NodeType::InnerDeleteType:
          next_node_p = ((InnerDeleteNode *)node_p)->child_node_p;

          delete (InnerDeleteNode *)node_p;
          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          break;
        case NodeType::InnerSplitType:
          next_node_p = ((InnerSplitNode *)node_p)->child_node_p;

          delete (InnerSplitNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          break;
        case NodeType::InnerMergeType:
          FreeEpochDeltaChain(((InnerMergeNode *)node_p)->child_node_p);
          FreeEpochDeltaChain(((InnerMergeNode *)node_p)->right_merge_p);

          delete (InnerMergeNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          // Merge node is also an ending node
          return;
        case NodeType::InnerRemoveType:
          // Recycle NodeID here together with RemoveNode
          // Since we need to guatantee all threads that could potentially
          // see the remove node exit before cleaning the NodeID
          tree_p->InvalidateNodeID(((InnerRemoveNode *)node_p)->removed_id);

          delete (InnerRemoveNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          freed_id_count++;
          #endif

          // We never free nodes under remove node
          return;
        case NodeType::InnerType:
          delete (InnerNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          return;
        case NodeType::InnerAbortType:
          // NOTE: Deleted abort node is also appended to the
          // garbage list, to prevent other threads reading the
          // wrong type after the node has been put into the
          // list (if we delete it directly then this will be
          // a problem)
          delete (InnerAbortNode *)node_p;

          #ifdef BWTREE_DEBUG
          freed_count++;
          #endif

          // Inner abort node is also a terminating node
          // so we do not delete the beneath nodes, but just return
          return;
        default:
          // This does not include INNER ABORT node
          bwt_printf("Unknown node type: %d\n", (int)type);

          assert(false);
          return;
      } // switch
    } // while 1

    return;
  }

  /*
   * ClearEpoch() - Sweep the chain of epoch and free memory
   *
   * The minimum number of epoch we must maintain is 1 which means
   * when current epoch is the head epoch we should stop scanning
   *
   * NOTE: There is no race condition in this function since it is
   * only called by the cleaner thread
   */
  void ClearEpoch() {
    bwt_printf("Start to clear epoch\n");

    while(1) {
      // Even if current_epoch_p is nullptr, this should work
      if(head_epoch_p == current_epoch_p) {
        bwt_printf("Current epoch is head epoch. Do not clean\n");

        break;
      }

      // Since it could only be acquired and released by worker thread
      // the value must be >= 0
      int active_thread_count = head_epoch_p->active_thread_count.load();
      assert(active_thread_count >= 0);

      // If we have seen an epoch whose count is not zero then all
      // epochs after that are protected and we stop
      if(active_thread_count != 0) {
        bwt_printf("Head epoch is not empty. Return\n");

        break;
      }

      // If some thread joins the epoch between the previous branch
      // and the following fetch_sub(), then fetch_sub() returns a positive
      // number, which is the number of threads that have joined the epoch
      // since last epoch counter testing.

      if(head_epoch_p->active_thread_count.fetch_sub(MAX_THREAD_COUNT) > 0) {
        bwt_printf("Some thread sneaks in after we have decided"
                   " to clean. Return\n");

        // Must add it back to let the next round of cleaning correctly
        // identify empty epoch
        head_epoch_p->active_thread_count.fetch_add(MAX_THREAD_COUNT);

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
        FreeEpochDeltaChain(garbage_node_p->node_p);

        // Save the next pointer so that we could
        // delete current node directly
        next_garbage_node_p = garbage_node_p->next_p;

        // This invalidates any further reference to its
        // members (so we saved next pointer above)
        delete garbage_node_p;
      } // for

      // First need to save this in order to delete current node
      // safely
      EpochNode *next_epoch_node_p = head_epoch_p->next_p;

      delete head_epoch_p;

      #ifdef BWTREE_DEBUG
      epoch_freed++;
      #endif

      // Then advance to the next epoch
      // It is possible that head_epoch_p becomes nullptr
      // this happens during destruction, and should not
      // cause any problem since that case we also set current epoch
      // pointer to nullptr
      head_epoch_p = next_epoch_node_p;
    } // while(1) through epoch nodes

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
      std::chrono::milliseconds duration(GC_INTERVAL);
      std::this_thread::sleep_for(duration);
    }

    bwt_printf("exit flag is true; thread return\n");

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
