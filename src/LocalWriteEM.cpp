
#include "LocalWriteEM.h"

/*
 * GetOptimalCoreNumber() - Return the concurrency level on hardware
 *
 * This function returns the hardware concurrency level which usually
 * means independent sets of execution contexts. On C++11 this is the
 * return value of std::thread::hardware_concurrency, and its return value
 * counts hyper-threaded core as two different execution units.
 *
 * Nevertheless, the return value of this function is considered as the
 * optimal way of constructing an EM
 *
 * Please note that the EM is declared as a template which must take compile
 * time constant as its instanciation value. This function is only used for
 * debugging, and should not be used to derive the template argument 
 */
unsigned int GetOptimalCoreNumber() {
  return std::thread::hardware_concurrency();
}

