#include "Array.h"

// The address returned by calloc() is kept in the pointer-sized slot just
// in front of the aligned data, so that Array_delete() can free it.

void* Array_new(const uint64_t n, const size_t elementSize, const size_t align) {
#ifdef VERBOSE
  printf("Created Array of size %" PRIu64 "\n", n);
#endif
  assert(isPowerOf2(align));
  if (n == 0) {
    return NULL;
  }
  const uint64_t bytesToAllocate = n * elementSize + (align - 1) + sizeof(void*);
  char* const ptr = (char*)calloc((size_t)bytesToAllocate, 1);
  if (ptr == NULL) {
    quit("Out of memory.");
  }
  const uintptr_t pad = align - 1;
  char* const data = (char*)(((uintptr_t)ptr + sizeof(void*) + pad) & ~pad);
  ((void**)data)[-1] = ptr;
  assert(ptr + sizeof(void*) <= data && data <= ptr + sizeof(void*) + pad);
  assert(((uintptr_t)data & pad) == 0); //aligned as expected?
  return data;
}

void Array_delete(void* const data) {
  if (data != NULL) {
    free(((void**)data)[-1]);
  }
}
