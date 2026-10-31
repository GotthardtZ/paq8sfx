#include "Stretch.hpp"
#include "Squash.hpp"

/**
 * Inverse of @ref squash. d = ln(p/(1-p)), d scaled by 8 bits, p by 12 bits.
 * d has range -2047 to 2047 representing -8 to 8. p has range 0 to 4095.
 *
 * Built at startup (from paq8l) by inverting the integer @ref squash, so it
 * needs no floating point and is defined to be exactly consistent with squash.
 * The 8 KB table lives in .bss (zero cost on disk). squash() is a pure
 * function with a constant table, so calling it here does not depend on static
 * initialisation order.
 */
namespace {
  class StretchTable {
  public:
    short t[4096];
    StretchTable() {
      int pi = 0;
      for (int x = -2047; x <= 2047; ++x) {   // invert squash()
        int i = squash(x);
        for (int j = pi; j <= i; ++j) t[j] = (short)x;
        pi = i + 1;
      }
      t[4095] = 2047;
    }
  } stretchTable;
}

int stretch(int p) {
  return stretchTable.t[p];
}
