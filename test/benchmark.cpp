
#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"

// This must be instanciated in the translation unit where it is
// used. Otherwise the compiler is not able to know its existence
// which causes the linker to complain
template <uint64_t core_num, typename GarbageNode>
std::unordered_map<void *, void *>
LocalWriteEMFactory<core_num, GarbageNode>::instance_map{};

// Since the EM type is defined by the EMFactory type, we could
// make it easier 
using EMFactory = LocalWriteEMFactory<4, char>;
using EM = typename EMFactory::TargetType;

int main() {
  // This instance must be created by the factory
  EM *p = EMFactory::GetInstance();
  
  dbg_printf("pointer = %p\n", p);
  dbg_printf("Multiple of 64? %d\n", (int)!((uint64_t)p % 64));
  // Otherwise it fails 64 bit alignment test
  assert(((uint64_t)p % 64) == 0);
  
  // Enjoy the beauty of C++11!!!!!!!
  dbg_printf("ElementType size = %lu\n",
             sizeof(typename std::remove_pointer<decltype(p)>::type::ElementType));
  
  EMFactory::FreeInstance(p);
  
  return 0;
}
