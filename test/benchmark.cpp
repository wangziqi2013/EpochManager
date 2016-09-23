
#include "../src/AtomicStack.h"
#include "../src/LocalWriteEM.h"

// This must be instanciated in the translation unit where it is
// used. Otherwise the compiler is not able to know its existence
// which causes the linker to complain
template <uint64_t core_num>
std::unordered_map<void *, void *>
LocalWriteEMFactory<core_num>::instance_map{};

int main() {
  LocalWriteEM<4> *p = LocalWriteEMFactory<4>::GetInstance();
  
  dbg_printf("pointer = %p\n", p);
  dbg_printf("Multiple of 64? %d\n", (int)!((uint64_t)p % 64));
  
  LocalWriteEMFactory<4>::FreeInstance(p);
  
  return 0;
}
