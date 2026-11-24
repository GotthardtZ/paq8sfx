#include "Rle.h"

void Rle_decode(uint8_t* out, const uint8_t* codes, size_t n, const int valueBits) {
  while (n != 0) {
    const uint8_t code = *codes++;
    size_t run = (size_t)(code >> valueBits) + 1;
    n -= run;
    while (run-- != 0) {
      *out++ = code & ((1 << valueBits) - 1);
    }
  }
}
