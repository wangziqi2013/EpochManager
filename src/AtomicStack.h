
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
  
  /*
   * Push() - Pushes a node into the stack
   *
   * Push operation always succeeds no matter what is the value of
   * the head pointer
   */
  void Push(const T &data) {
    Node node{data, head_p.load()};
    
  }
  
 public:
   
};

} // namespace index
} // namespace peloton
