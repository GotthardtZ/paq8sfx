#include "Stretch.h"
#include "Squash.h"

/**
 * Inverse of @ref squash. d = ln(p/(1-p)), d scaled by 8 bits, p by 12 bits.
 * d has range -2047 to 2047 representing -8 to 8. p has range 0 to 4095.
 *
 * Built at startup (from paq8l) by inverting the integer @ref squash, so it
 * needs no floating point and is defined to be exactly consistent with squash.
 * The 8 KB table lives in .bss (zero cost on disk).
 */
static short stretchTable[4096];

void Stretch_init(void) {
  int pi = 0;
  for (int x = -2047; x <= 2047; ++x) {   // invert squash()
    int i = squash(x);
    for (int j = pi; j <= i; ++j) stretchTable[j] = (short)x;
    pi = i + 1;
  }
  stretchTable[4095] = 2047;
}

int stretch(int p) {
  return stretchTable[p];
}
