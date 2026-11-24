#include "Bucket16.h"

#define ElementsInBucket Bucket16_ElementsInBucket

static void shiftElementsRight(Bucket16* const bucket, const size_t count) {
  memmove(&bucket->elements[1], &bucket->elements[0], count * sizeof(HashElement));
}

// Shifts the elements before position i down by 1 (overwriting element i) and
// puts a new, empty element with this checksum in the first slot.
static HashElementForContextMap* insert(Bucket16* const bucket, const size_t i, const uint16_t checksum) {
  HashElement* const elements = bucket->elements;
  shiftElementsRight(bucket, i);
  elements[0].checksum = checksum;
  memset(&elements[0].value, 0, sizeof(elements[0].value));
  return &elements[0].value;
}

HashElementForContextMap* Bucket16_find(Bucket16* const bucket, uint16_t checksum, Random* const rnd) {
  HashElement* const elements = bucket->elements;

  checksum += checksum == 0; //don't allow 0 checksums (0 checksums are used for empty slots)

  if (elements[0].checksum == checksum) //there is a high chance that we'll find it in the first slot, so go for it
    return &elements[0].value;

  for (size_t i = 1; i < ElementsInBucket; ++i) { //note: gcc fully unrolls this loop: it gives a tiny speed improvement for a cost of ~4K exe size
    const uint16_t thisChecksum = elements[i].checksum;
    if (thisChecksum == checksum) { // found matching checksum
      //shift elements down and move matching element to front
      HashElement tmpElement = elements[i];
      shiftElementsRight(bucket, i);
      elements[0] = tmpElement;
      return &elements[0].value;
    }
    if (thisChecksum == 0) { // found empty slot
      return insert(bucket, i, checksum);
    }
  }
  //no match and no empty slot -> overwrite an existing element with the new (empty) one
  //Replacement strategy:
  // - In case the hash elements represent an indirect context (we have content but no priority): overwrite the last used element
  // - In case the hash elements are counts/statistics: overwrite one of the the last used elements having low priority.
  //
  //   The priority of an element is established by the counts it represents: large counts usually mean that the element is worthy to keep.
  //   When an element is accessed recently it is probably worthy to keep as it represents recent statistics,
  //   when an element is accessed long ago it is probably obolete and may be overwritten.
  //   In order to keep track of the order the elements were accessed we move the most recently accessed element to the front: thus
  //   recently acessed elements are usually somewhere in the front of the bucket, rarely accessed element slowly move to the back. The
  //   last elements in the bucket are cantidates for eviction.
  //
  //   Overwriting the lowest priority element (with no regard of its position) is in favor of homogenous (semi-stationary) files.
  //   Overwriting the last element (with no regard of its priority) is in favor of mixed-content files.
  //   In order to favor both, we are using a probabilistic replacement startegy where most recently accessed and/or higher priority
  //   elements have higher chance to stay in the bucket = rarely accessed and/or low priority elements have higher chance to be 
  //   evicted (overwritten).
  //
  //   The 2 most recently accessed elements are always protected from overwriting.
  //
  //   Starting from the last element, the elements before it are compared one by one as long as 6 random bits
  //   are at least the threshold of that step (that is: with a probability of 63/64, 60/64, 56/64, 48/64).
  static const uint8_t thresholds[4] = { 1, 4, 8, 16 };
  size_t minElementIdx = ElementsInBucket - 1; //index of element to be evicted
  uint32_t minPrio = HashElementForContextMap_prio(&elements[minElementIdx].value);
  uint32_t RND = Random_next(rnd, 32); //note: we'll need only 4 times 6 = 24 bits
  for (int k = 0; k < 4 && (RND & 63) >= thresholds[k]; k++, RND >>= 6) {
    const size_t idx = ElementsInBucket - 2 - k;
    const uint32_t thisPrio = HashElementForContextMap_prio(&elements[idx].value);
    if (thisPrio < minPrio) {
      minPrio = thisPrio;
      minElementIdx = idx;
    }
  }
  //at the same time owerwrite the element at position "minElementIdx" (the element to be evicted)
  return insert(bucket, minElementIdx, checksum);
}
