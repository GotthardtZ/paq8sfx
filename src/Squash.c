#include "Squash.hpp"

/**
 * return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
 *
 * Integer-only implementation (from paq8l): a 33-point curve with linear
 * interpolation between the points. No floating point and no math library, so
 * the result is identical on every compiler and CPU - the compressor and the
 * stub are guaranteed to agree. @ref stretch is built as its exact inverse.
 */
int squash(int d /*-2047..2047*/) {
  static const int t[33] = {
    1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101,
    1546, 2047, 2549, 2994, 3348, 3607, 3785, 3901, 3975, 4022,
    4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094 };
  if (d > 2047) return 4095;
  if (d < -2047) return 0;
  int w = d & 127;
  d = (d >> 7) + 16;
  return (t[d] * (128 - w) + t[d + 1] * w + 64) >> 7;
}
