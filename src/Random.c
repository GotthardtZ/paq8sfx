#include "Random.h"
#include "Hash.h"

// This pseudo random number generator is a 
// Mixed Congruential Generator with a period of 2^64
// https://en.wikipedia.org/wiki/Linear_congruential_generator

uint32_t Random_next(Random* const rnd, const int numberOfBits) {
  assert(numberOfBits > 0 && numberOfBits <= 32);
  rnd->_state = (rnd->_state + 1) * PHI64;
  return (uint32_t)(rnd->_state >> (64 - numberOfBits));
}
