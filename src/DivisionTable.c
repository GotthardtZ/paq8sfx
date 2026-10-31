#include "DivisionTable.h"

int* DivisionTable_getDT(void) {
  static int dt[1024];
  for( int n = 0; n < 1024; ++n ) {
    dt[n] = (1 << 30) / (n + 2); // 1/(n+2) scaled by 30 bits
  }
  return dt;
}
