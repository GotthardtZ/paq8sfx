#pragma once

#include "Utils.h"

/**
 * Array_new(n, sizeof(T), align) allocates memory for n elements of T.
 * The base address is aligned to "align" bytes (a power of 2).
 * The allocated memory is initialized to 0s.
 * It's the caller's responsibility to populate the array with elements.
 * Quits when out of memory. Returns NULL when n is 0.
 * Free the array with Array_delete().
 */
void* Array_new(uint64_t n, size_t elementSize, size_t align);

/**
 * Frees an array allocated by Array_new(). Does nothing for NULL.
 */
void Array_delete(void* data);

// Shorthand for the usual case: n elements of type T, aligned to 16 bytes.
#define ARRAY_NEW(T, n) ((T*)Array_new((n), sizeof(T), 16))
