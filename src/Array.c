#include "Array.h"

// Two pointer-sized slots are kept just in front of the aligned data:
//   data[-1]: the address returned by calloc(), for free()
//   data[-2]: the next array in the list of all arrays (newest first), so
//             that Array_deleteAll() can free every array at once.

static void* arrayList = NULL; // the newest array (its data address)

void* Array_new(const uint64_t n, const size_t elementSize, const size_t align) {
#ifdef VERBOSE
  printf("Created Array of size %" PRIu64 "\n", n);
#endif
  assert(isPowerOf2(align));
  if (n == 0) {
    return NULL;
  }
  const uint64_t bytesToAllocate = n * elementSize + (align - 1) + 2 * sizeof(void*);
  char* const ptr = (char*)calloc((size_t)bytesToAllocate, 1);
  if (ptr == NULL) {
    quit("Out of memory.");
  }
  const uintptr_t pad = align - 1;
  char* const data = (char*)(((uintptr_t)ptr + 2 * sizeof(void*) + pad) & ~pad);
  ((void**)data)[-1] = ptr;
  ((void**)data)[-2] = arrayList;
  arrayList = data;
  assert(ptr + 2 * sizeof(void*) <= data && data <= ptr + 2 * sizeof(void*) + pad);
  assert(((uintptr_t)data & pad) == 0); //aligned as expected?
  return data;
}

void Array_delete(void* const data) {
  if (data != NULL) {
    void** link = &arrayList; // unlink it from the list
    while (*link != data) {
      link = &((void**)*link)[-2];
    }
    *link = ((void**)data)[-2];
    free(((void**)data)[-1]);
  }
}

void Array_deleteAll(void) {
  while (arrayList != NULL) {
    void** const data = (void**)arrayList;
    arrayList = data[-2];
    free(data[-1]);
  }
}
