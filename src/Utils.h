#pragma once

#include "SystemDefines.h"

static inline int max(int a, int b) { return a > b ? a : b; }

static inline int min(int a, int b) { return a < b ? a : b; }

/**
 * Returns floor(log2(x)).
 * 0/1->0, 2->1, 3->1, 4->2 ..., 30->4,  31->4, 32->5,  33->5
 * @param x
 * @return floor(log2(x))
 */
static inline uint32_t ilog2(uint32_t x) {
#ifdef _MSC_VER
  unsigned long tmp = 0;
  if (x != 0) {
    _BitScanReverse(&tmp, x);
  }
  return tmp;
#elif (defined(__GNUC__) || defined(__clang__))
  if (x != 0) {
    x = 31 - __builtin_clz(x);
  }
  return x;
#else
#error Unknown target system
#endif
}

#define isPowerOf2(x) ((((x) & ((x) - 1)) == 0))


// Circular/wraparound distance on a 256-value ring
// that is: what is the shortest distance between two byte values in any direction (up or down) when we allow a wrap-around at 255 → 0?
//
// Examples:
//  - Distance between 3 and 10 (or 10 and 3) is 7
//  - Distance between 2 and 200 (or 200 and 2) is 58
// For the latter the true linear distance is 198, but the wraparound distance is only 58
// why: starting from 2 down to 0 then to 255 then further down to 200 takes 58 steps
//
// Where such wrap-around distances are useful:
// For image compression (pixel prediction):
//   images with color-channel transform (b, g, r) -> (g, g-r, g-b): the 2nd and 3rd transformed components experience a wrap-around.
// For multi-byte numeric data prediction:
//   less significant bytes wrap around when the whole multi-byte value is increased/decreased sequentially.
static inline int rabs(int x1, int x2) {
  int d = (int8_t)(x1 - x2); // -128..127
  return d >= 0 ? d : -d; // abs(d) → 0..128
}
