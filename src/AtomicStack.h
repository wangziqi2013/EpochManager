
#pragma once

#include "common.h"

#include <vector>
#include <cstdio>
#include <atomic>

namespace peloton {
namespace index {

/*
 * AtomicStack - A lock-free stack that supports concurrent access of multiple
 *               threads
 *
 * This class is implemented to assist in designing of an efficient and
 * scalable epoch based garbage collector. It could also be used as a general
 * purpose lock free stack data structure, disregarding the bottleneck brought
 * by malloc() and pointer chasing (so it might not worth the effort of making
 * it lock-free)
 */
template <typename T>
class AtomicStack {
 private:
  /*
   * class Node - Stack node that forms a linked list
   *
   * Value will be copied constructed into this node when being pushed.
   * Therefore type T must at least be copy-constructable
   */
  class Node {
   public:
    T data;
    
    // This does not have to be atomic since it only gets modified locally
    // by the thread inserting into the stack
    Node *next_p;
    
    /*
     * Constructor - Initializes data member
     */
    Node(const T &p_data, Node *p) :
      data{p_data},
      next_p{p}
    {}
  };
  
  // This will be modified using CAS both when inserting into and deleteing
  // from the stack
  std::atomic<Node *> head_p;
  
  /*
   * Constructor() - Initialize head pointer to nullptr
   */
  AtomicStack() :
    head_p{nullptr}
  {}
  
 public:

  /*
   * Push() - Pushes a node into the stack
   *
   * Push operation always succeeds no matter what is the value of
   * the head pointer
   */
  void Push(const T &data) {
    Node *node_p = new Node{data, head_p.load()};
    Node *old_p = head_p.load();

    // Note that the first argument to CAS is a reference which means
    // if CAS fails then it will be updated to the current value of CAS
    // and it could thus be used immediately
    while(head_p.compare_exchange_strong(old_p, node_p) == false);

    return;
  }

  /*
   * Pop() - Pop an element out of the stack and assign it to the reference
   *
   * The function changes the argument
   */
  void Pop(T &data) {
    Node *old_p = head_p.load();
    Node *new_p = old_p->next_p;

    // Keeps CAS on the head pointer
    // If CAS fails then the most up to date heap_p is loaded into
    // old_p, and we should reset new_p with the pointer inside old_p
    while(head_p.compare_exchange_strong(old_p, new_p) == false) {
      new_p = old_p->next_p;
    }

    data = old_p->data;

    // Need GC here...

    return;
  }
  
  /*
   * Top() - Access the top node of the stack at the moment the exeuting
   *         thread loads the head
   */
  void Top(T &data) {
    // Start epoch protection
    data = head_p->load()->data;
    // End epoch protection
    
    return;
  }
};

} // namespace index
} // namespace peloton
