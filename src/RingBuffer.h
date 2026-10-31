#pragma once

#include "Array.h"

/**
 * A ring buffer of bytes: the history of the input.
 */
typedef struct RingBuffer {
  uint8_t* b;
  uint32_t bufferSize;
  uint32_t offset; /**< Number of input bytes in buffer (not wrapped), will be masked when used for indexing */
  uint32_t mask;
} RingBuffer;

/**
  * Creates an array of @ref newSize bytes (must be a power of 2).
  * The content of the buffer is lost.
  * @param newSize number of bytes in array
  */
static inline void RingBuffer_setSize(RingBuffer* const rb, const uint32_t newSize) {
  assert(newSize > 0 && isPowerOf2(newSize));
  Array_delete(rb->b);
  rb->b = (uint8_t*)Array_new(newSize, 1, 16);
  rb->bufferSize = newSize;
  rb->offset = 0;
  rb->mask = newSize - 1;
}

/**
  * Frees the buffer.
  */
static inline void RingBuffer_destroy(RingBuffer* const rb) {
  Array_delete(rb->b);
  rb->b = NULL;
  rb->bufferSize = 0;
  rb->offset = 0;
  rb->mask = 0;
}

static inline uint32_t RingBuffer_getpos(const RingBuffer* const rb) {
  return rb->offset;
}

static inline void RingBuffer_fill(RingBuffer* const rb, const uint8_t B) {
  const uint32_t n = rb->bufferSize;
  for (uint32_t i = 0; i < n; i++) {
    rb->b[i] = B;
  }
}

static inline void RingBuffer_add(RingBuffer* const rb, const uint8_t B) {
  rb->b[rb->offset & rb->mask] = B;
  rb->offset++;
}

/**
  * Returns the i'th byte with wrap (no out of bounds).
  * @param i
  * @return
  */
static inline uint8_t RingBuffer_at(const RingBuffer* const rb, const uint32_t i) {
  return rb->b[i & rb->mask];
}

static inline void RingBuffer_set(RingBuffer* const rb, const uint32_t i, const uint8_t B) {
  rb->b[i & rb->mask] = B;
}

/**
  * Returns i'th byte back from pos (i>0) with wrap (no out of bounds)
  * @param i
  * @return
  */
static inline uint8_t RingBuffer_back(const RingBuffer* const rb, const uint32_t i) {
  //assert(i!=0);
  return rb->b[(rb->offset - i) & rb->mask];
}

static inline void RingBuffer_reset(RingBuffer* const rb) {
  RingBuffer_fill(rb, 0);
  rb->offset = 0;
}

/**
  * @return the size of the RingBuffer
  */
static inline uint32_t RingBuffer_size(const RingBuffer* const rb) {
  return rb->bufferSize;
}

// leak internals for high-performance access
static inline void RingBuffer_leakInternals(const RingBuffer* const rb, uint8_t** const buffer_ptr, size_t* const buffer_mask, size_t* const head_pos) {
  *buffer_ptr = rb->b;
  *buffer_mask = rb->mask;
  *head_pos = rb->offset;
}
