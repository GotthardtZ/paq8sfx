#include "File.h"

void File_append(File* const f, const char* const s) {
  for( int i = 0; s[i] != 0; i++ ) {
    File_putChar(f, (uint8_t)s[i]);
  }
}

uint32_t File_get32(File* const f) {
  uint32_t x = 0;
  for (int i = 0; i < 4; i++) {
    x = (x << 8) | (uint32_t)File_getChar(f);
  }
  return x;
}

void File_put32(File* const f, const uint32_t x) {
  File_putChar(f, (x >> 24) & 255);
  File_putChar(f, (x >> 16) & 255);
  File_putChar(f, (x >> 8) & 255);
  File_putChar(f, x & 255);
}

uint64_t File_getVLI(File* const f) {
  uint64_t i = 0;
  int k = 0;
  uint8_t b = 0;
  do {
    b = (uint8_t)File_getChar(f);
    i |= (uint64_t)(b & 0x7F) << k;
    k += 7;
  } while((b >> 7) > 0 );
  return i;
}

void File_putVLI(File* const f, uint64_t i) {
  while( i > 0x7F ) {
    File_putChar(f, 0x80 | (i & 0x7F));
    i >>= 7;
  }
  File_putChar(f, (uint8_t)i);
}
