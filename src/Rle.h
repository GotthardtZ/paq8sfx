#pragma once
#include "Utils.h"

/**
 * Expands run-length codes into n bytes (the packed tables, see
 * tools/pack_tables.py). One code byte per run: the value is in the low
 * @p valueBits bits, (run length - 1) in the high bits.
 */
void Rle_decode(uint8_t* out, const uint8_t* codes, size_t n, int valueBits);
