#pragma once

#include "file/File.h"

/**
 * Binary arithmetic encoder/decoder with configurable probability precision.
 *
 * Encoding narrows an interval [x1, x2) based on a probability p, then emits
 * the leading bits that have been determined. Pending bits handle the case where
 * the interval straddles the midpoint without resolving a leading bit.
 *
 * Decoding mirrors encoding: it tracks the same interval alongside a 32-bit
 * window `x` into the compressed bitstream, and resolves each bit by comparing
 * x against the interval midpoint.
 */

#define ArithmeticEncoder_PRECISION 31 ///< Probability precision in bits; p is in [1, 2^PRECISION)
_Static_assert(ArithmeticEncoder_PRECISION < 32, "Arithmetic encoder supports max 31 bits of probability precision");

typedef struct ArithmeticEncoder {
  uint32_t x1;           ///< Low  end of the current coding interval, initially 0
  uint32_t x2;           ///< High end of the current coding interval, initially UINT32_MAX (0xFFFFFFFF)
  uint32_t x;            ///< Only for decoding: current code value, kept in sync with the coding interval [x1, x2)
  uint32_t pending_bits; ///< The number of bits whose value depends on the next resolved bit
  uint8_t  B;            ///< Bit-buffer for packing/unpacking individual bits into bytes
  uint32_t bits_in_B;    ///< Number of valid bits currently held in B (0–8)
  File* archive;         ///< Compressed data file to write during encoding and read during decoding
} ArithmeticEncoder;

void ArithmeticEncoder_init(ArithmeticEncoder* ari, File* archive);
void ArithmeticEncoder_prefetch(ArithmeticEncoder* ari);                       ///< For decoding: Fill `x` with the first 32 bits of the archive before decoding
#ifdef FULL
void ArithmeticEncoder_flush(ArithmeticEncoder* ari);                          ///< For encoding: Flush any remaining bits to the archive after encoding
void ArithmeticEncoder_encodeBit(ArithmeticEncoder* ari, uint32_t p, int bit); ///< For encoding: Encode one bit with probability p/2^PRECISION that the bit is 1
#endif
int  ArithmeticEncoder_decodeBit(ArithmeticEncoder* ari, uint32_t p);          ///< For decoding: Decode one bit; returns 1 with probability p/2^PRECISION
