#pragma once

#include "Random.h"
#include "HashElementForContextMap.h"

/**
 * Hash bucket to be used in a hash table
 * A hash bucket consists of a list of HashElements
 * Each hash element consists of a 16-bit checksum for collision detection
 * and a value. This is the bucket of ContextMap2: the value is its
 * bit and byte statistics ( sizeof(HashElement) = 2+7 = 9 bytes )
 *
 */

#pragma pack(push,1)
typedef struct HashElement {
  uint16_t checksum;
  HashElementForContextMap value;
} HashElement;
#pragma pack(pop)

#define Bucket16_ElementsInBucket 7

typedef struct Bucket16 {
  HashElement elements[Bucket16_ElementsInBucket];
} Bucket16;

_Static_assert(sizeof(Bucket16) == (sizeof(HashElementForContextMap) + 2) * 7, "Something is wrong, please pack that struct");

HashElementForContextMap* Bucket16_find(Bucket16* bucket, uint16_t checksum, Random* rnd);
