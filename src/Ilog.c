#include "Ilog.h"

static uint8_t ilogTable[65536];

/**
  * Compute lookup table by numerical integration of 1/x
  */
void Ilog_init(void) {
  uint32_t x = 14155776;
  for (uint32_t i = 2; i < 65536; ++i) {
    x += 774541002 / (i * 2 - 1); // numerator is 2^29/ln 2
    ilogTable[i] = x >> 24;
  }
}

int Ilog_log(uint16_t x) { return (int)ilogTable[x]; }

int llog(uint32_t x) {
  if( x >= 0x1000000 ) {
    return 256 + Ilog_log((uint16_t)(x >> 16));
  }
  if( x >= 0x10000 ) {
    return 128 + Ilog_log((uint16_t)(x >> 8));
  }

  return Ilog_log((uint16_t)x);
}
