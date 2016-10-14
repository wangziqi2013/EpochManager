
/*
 * global_em_test.cpp - Correctness test for global counter EM 
 */

#include "../src/GlobalWriteEM.h"
#include "../src/LocalWriteEM.h"
#include "test_suite.h"

// Declear stack and its node type here
using StackType = AtomicStack<uint64_t>;
using NodeType = typename StackType::NodeType;

// This is the type of the EM we declare
using EM = GlobalWriteEM<NodeType>;

int main() {
  return 0;
}
