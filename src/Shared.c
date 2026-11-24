#include "Shared.h"
#include "ArithmeticEncoder.h"
#include "Ilog.h"
#include "Simd.h"
#include "StateTable.h"
#include "Stretch.h"
#include "Utils.h"

void Shared_init(Shared* const shared, const uint8_t level) {
  shared->State.c0 = 1;

  // The lookup tables of the models
  Stretch_init();
  StateTable_init();
  Ilog_init();

  shared->level = level;
  shared->mem = UINT64_C(65536) << level;
  // No reason to go over 256 MB.
  const uint64_t want = shared->mem * 8;
  const uint32_t bufMem = (uint32_t)(want < (UINT64_C(1) << 28) ? want : (UINT64_C(1) << 28));
  assert(isPowerOf2(bufMem));
  RingBuffer_setSize(&shared->buf, bufMem);

  // Determine CPU's (and OS) support for SIMD vectorization instruction set
  shared->chosenSimd = simdDetect();
#ifdef TEST_SIMD_MAX // test builds only: force a lower SIMD code path (a SIMDType value)
  if (shared->chosenSimd > TEST_SIMD_MAX) shared->chosenSimd = (SIMDType)TEST_SIMD_MAX;
#endif
}

void Shared_update(Shared* const shared, const int y, const uint32_t p, const bool isMissed) {
  shared->State.y = y;
  shared->State.c0 += shared->State.c0 + y;
  shared->State.bitPosition = (shared->State.bitPosition + 1) & 7;
  if(shared->State.bitPosition == 0 ) {
    shared->State.c1 = shared->State.c0;
    RingBuffer_add(&shared->buf, shared->State.c1);
    shared->State.c8 = (shared->State.c8 << 8) | (shared->State.c4 >> 24);
    shared->State.c4 = (shared->State.c4 << 8) | shared->State.c0;
    shared->State.c0 = 1;
  }
 
  shared->State.misses = (shared->State.misses << 1) | (uint32_t)isMissed;

  const uint32_t shift = ArithmeticEncoder_PRECISION - 6;
  const uint32_t maxp = (1u << ArithmeticEncoder_PRECISION) - 1;

  shared->State.loss = ((y == 0 ? p : maxp - p)) >> shift; //0..1023
  assert(shared->State.loss <= 1023);

  // Broadcast to all current subscribers: y (and c0, c1, c4, etc) is known
  UpdateBroadcaster_broadcastUpdate(&shared->updateBroadcaster);
}

void Shared_reset(Shared* const shared) {
  RingBuffer_reset(&shared->buf);
  memset(&shared->State, 0, sizeof(shared->State));
  shared->State.c0 = 1;
}
