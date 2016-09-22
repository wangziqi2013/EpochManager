
#pragma once

#include "common.h"

#include <vector>
#include <cstdio>

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
  class Node {
    
  };
  
 public:
   
};

} // namespace index
} // namespace peloton
