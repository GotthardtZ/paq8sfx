#include "ArithmeticEncoder.h"

void ArithmeticEncoder_init(ArithmeticEncoder* const ari, File* const f) {
  ari->x1 = 0;
  ari->x2 = UINT32_MAX;
  ari->x = 0;
  ari->pending_bits = 0;
  ari->bits_in_B = 0;
  ari->B = 0;
  ari->archive = f;
}

// Read one bit from the archive, buffering a full byte at a time.
static int bit_read(ArithmeticEncoder* const ari) {
  if (ari->bits_in_B == 0) {
    ari->B = (uint8_t)File_getChar(ari->archive); // EOF is handled gracefully by the caller
    ari->bits_in_B = 8;
  }
  ari->bits_in_B--; //7..0
  return (ari->B >> ari->bits_in_B) & 1;
}

#ifdef FULL
// Write one bit to the archive, flushing a full byte when the buffer is full.
static void bit_write(ArithmeticEncoder* const ari, const int bit) {
  ari->B = (ari->B << 1) | bit;
  ari->bits_in_B++;
  if (ari->bits_in_B == 8) {
    File_putChar(ari->archive, ari->B);
    ari->B = 0;
    ari->bits_in_B = 0;
  }
}

// Write one bit, then emit all pending bits with the opposite value (E3 carry resolution).
static void bit_write_with_pending(ArithmeticEncoder* const ari, const int bit) {
  bit_write(ari, bit);
  for (; ari->pending_bits > 0; ari->pending_bits--)
    bit_write(ari, bit ^ 1);
}

// Flush the encoder: emit enough bits from x1 to complete the final byte.
void ArithmeticEncoder_flush(ArithmeticEncoder* const ari) {
  do {
    bit_write_with_pending(ari, ari->x1 >> 31);
    ari->x1 <<= 1;
  } while (ari->bits_in_B != 0);
}
#endif // FULL

// Prime the decode window with the first 32 bits of compressed data.
void ArithmeticEncoder_prefetch(ArithmeticEncoder* const ari) {
  for (int i = 0; i < 32; ++i)
    ari->x = (ari->x << 1) | bit_read(ari);
}

#ifdef FULL
// Encode one bit. p is the probability (scaled by 2^PRECISION) that bit == 1.
void ArithmeticEncoder_encodeBit(ArithmeticEncoder* const ari, uint32_t p, const int bit) {
  if (p == 0) p++;
  assert(p > 0 && p < (1u << ArithmeticEncoder_PRECISION));

  // Split [x1, x2] at xmid proportional to p; round to nearest for accuracy.
  const uint32_t xmid = ari->x1 + (uint32_t)(((uint64_t)(ari->x2 - ari->x1) * p) >> ArithmeticEncoder_PRECISION);
  assert(xmid >= ari->x1 && xmid < ari->x2);

  if (bit != 0) {
    ari->x2 = xmid;
  }
  else {
    ari->x1 = xmid + 1;
  }

  // Emit leading bits that are identical in x1 and x2.
  while (((ari->x1 ^ ari->x2) >> 31) == 0) {
    bit_write_with_pending(ari, ari->x2 >> 31);
    ari->x1 <<= 1;
    ari->x2 = (ari->x2 << 1) | 1;
  }

  // Rescale when the interval straddles the midpoint; defer the carry bit.
  while (ari->x1 >= 0x40000000 && ari->x2 < 0xC0000000) {
    ari->pending_bits++;
    ari->x1 = (ari->x1 << 1) & 0x7FFFFFFF;
    ari->x2 = (ari->x2 << 1) | 0x80000001;
  }
}
#endif // FULL

// Decode one bit. p is the probability (scaled by 2^PRECISION) that the bit is 1.
int ArithmeticEncoder_decodeBit(ArithmeticEncoder* const ari, uint32_t p) {
  if (p == 0) p++;
  assert(p > 0 && p < (1u << ArithmeticEncoder_PRECISION));

  // Mirror the encoder's midpoint split.
  const uint32_t xmid = ari->x1 + (uint32_t)(((uint64_t)(ari->x2 - ari->x1) * p) >> ArithmeticEncoder_PRECISION);
  assert(xmid >= ari->x1 && xmid < ari->x2);

  const int bit = (ari->x <= xmid) ? 1 : 0;
  if (bit != 0) {
    ari->x2 = xmid;
  }
  else {
    ari->x1 = xmid + 1;
  }

  // Consume leading bits and refill x from the bitstream.
  while (((ari->x1 ^ ari->x2) >> 31) == 0) {
    ari->x1 <<= 1;
    ari->x2 = (ari->x2 << 1) | 1;
    ari->x = (ari->x << 1) | bit_read(ari);
  }

  // Undo midpoint rescaling and keep x in sync with the interval.
  while (ari->x1 >= 0x40000000 && ari->x2 < 0xC0000000) {
    ari->x1 = (ari->x1 << 1) & 0x7FFFFFFF;
    ari->x2 = (ari->x2 << 1) | 0x80000001;
    ari->x = (ari->x << 1) ^ 0x80000000;
    ari->x += bit_read(ari);
  }

  return bit;
}
