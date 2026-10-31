#pragma once

/**
context map for large contexts.
maps to a bit history state, a 3 mostRecentlyUsed byte history, and 1 byte RunStats.

Bit and byte histories are stored in a hash table with 64 byte buckets.
The buckets are indexed by a context ending after 0, 2 or 5 bits of the
current byte. Thus, each byte modeled results in 3 main memory accesses
per context, with all other accesses to cache.

On a byte boundary (bit 0), only 3 of the 7 bit history states are used.
Of the remaining 4 bytes, 1 byte is used as a run length (the consecutive
occurrences of the previously seen byte), 3 are used to store the last
3 distinct bytes seen in this context. The byte history is then combined
with the bit history states to provide additional states that are then
mapped to predictions.
*/

#include "IPredictor.h"
#include "Bucket16.h"
#include "HashElementForContextMap.h"
#include "Hash.h"
#include "Mixer.h"
#include "Random.h"
#include "StateMap.h"
#include "StateTable.h"
#include "Stretch.h"
#include "UpdateBroadcaster.h"

#define CM_DEFERRED 64
#define CM_SKIPPED_CONTEXT 128

enum { ContextMap2_MIXERINPUTS = 4 + 1 + 2 };

typedef struct ContextInfo {
  HashElementForContextMap* hashElementInSlot0; /**< pointer to current bit history states, byte history and byte run statistics in slot0 */
  HashElementForContextMap* hashElementInSlot012; /**< pointer to current bit history states in current slot (either slot0 or slot1 or slot2) */
  uint32_t tableIndex; /**< @ref C whole byte context hashes */
  uint16_t tableChecksum; /**< @ref C whole byte context checksums */
  uint8_t flags;
  HashElementForContextMap bitStateTmp;
} ContextInfo;

typedef struct ContextMap2 {
  IPredictor predictor;
  const Shared* shared;
  Random rnd;
  uint32_t C; /**< max number of contexts */
  Bucket16* hashTable; /**< bit and byte histories (statistics) */
  ContextInfo* contextInfoList;
  StateMap runMap;
  StateMap stateMap;
  StateMap bhMap8B;
  StateMap bhMap12B;
  uint32_t index; /**< next context to set by @ref ContextMap2_set(), resets to zero after every round */
  uint32_t mask;
  int hashBits;
  int scale;
  int order; // is set after mix()
} ContextMap2;

/**
  * Construct using @ref size bytes of memory for @ref contexts contexts.
  * @param size bytes of memory to use
  * @param contexts max number of contexts
  * @param scale
  */
void ContextMap2_init(ContextMap2* cm, const Shared* sh, uint64_t size, uint32_t contexts, int scale);
void ContextMap2_destroy(ContextMap2* cm);

/**
  * Set next whole byte context to @ref ctx.
  * @param ctx
  */
void ContextMap2_set(ContextMap2* cm, uint64_t ctx);
void ContextMap2_skip(ContextMap2* cm);
void ContextMap2_setScale(ContextMap2* cm, int Scale);
void ContextMap2_mix(ContextMap2* cm, Mixer* m);
