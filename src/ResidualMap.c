#include "Clz.h"
#include "Array.h"
#include "ResidualMap.h"

#define BINS ResidualMap_BINS

static void ResidualMap_update(IPredictor* const predictor) {
  ResidualMap* const rm = (ResidualMap*)predictor;
  const Shared* const shared = rm->shared;
  assert(shared->State.bitPosition == 0);
  assert(rm->currentContextIndex == rm->numContexts);

  while (rm->currentContextIndex > 0) {
    rm->currentContextIndex--;
    const uint32_t base = rm->bases[rm->currentContextIndex];
    if (base == UINT32_MAX)
      continue;

    INJECT_SHARED_c1
    const int bin = (192 + c1 - rm->predictions[rm->currentContextIndex]) & 255; // moving the center to bin #192 gives better cache locality on read and on updates

    // Increment all prefix entries after bin: sums[bin .. BINS-1] += 1.
    uint16_t* s = &rm->sums[base];
    for (int k = bin; k < BINS; k++)
      s[k]++;

    // Aging: when the total reaches UINT16_MAX halve all prefix entries.
    // A bin's count is s[k]-s[k-1] (s[-1] treated as 0); total is s[BINS-1].
    if (s[BINS - 1] == UINT16_MAX) {
      for (int k = 0; k < BINS; k++)
        s[k] >>= 1;
    }
  }
}

void ResidualMap_init(ResidualMap* const rm, const Shared* const sh, const int numContexts, const int histogramsPerContext, const int scale) {
  rm->predictor.update = ResidualMap_update;
  rm->shared = sh;
  rm->scale = scale;
  rm->numContexts = numContexts;
  rm->histogramsPerContext = histogramsPerContext;
  rm->predictions = ARRAY_NEW(short, numContexts);
  rm->bases = ARRAY_NEW(uint32_t, numContexts);
  rm->sums = ARRAY_NEW(uint16_t, BINS * numContexts * histogramsPerContext);
  rm->dt = DivisionTable_getDT();
}

void ResidualMap_setscale(ResidualMap* const rm, const int scale) {
  rm->scale = scale;
}

void ResidualMap_set(ResidualMap* const rm, const short prediction, const uint32_t histogram_id) {
  assert(histogram_id < rm->histogramsPerContext);
  assert(rm->currentContextIndex < rm->numContexts);
  rm->predictions[rm->currentContextIndex] = prediction;
  rm->bases[rm->currentContextIndex] = (rm->currentContextIndex * rm->histogramsPerContext + histogram_id) * BINS;
  rm->currentContextIndex++;
}

void ResidualMap_skip(ResidualMap* const rm) {
  assert(rm->currentContextIndex < rm->numContexts);
  rm->bases[rm->currentContextIndex] = UINT32_MAX;
  rm->currentContextIndex++;
}

// The number of observations in the circular bin range [offset, offset+range)
// of the prefix sums s (see ResidualMap_mix()).
static uint64_t ResidualMap_count(const uint16_t* const s, const int offset, const int range) {
  if (offset + range <= BINS)
    return s[offset + range - 1] - (offset == 0 ? 0 : s[offset - 1]);
  return s[BINS - 1] - s[offset - 1] + s[offset + range - BINS - 1];
}

void ResidualMap_mix(ResidualMap* const rm, Mixer* const m) {
  const Shared* const shared = rm->shared;
  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  if (bpos == 7)
    UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(shared), &rm->predictor);
  assert(rm->currentContextIndex == rm->numContexts);

  for (size_t i = 0; i < rm->currentContextIndex; i++) {
    const uint32_t base = rm->bases[i];
    if (base == UINT32_MAX) { // skipped context
      Mixer_add(m, 0);
      Mixer_add(m, 0);
      continue;
    }

    const uint16_t* s = &rm->sums[base];
    const uint8_t  c1 = c0 << (8 - bpos);
    const int range = 1 << (7 - bpos);  // 128 down to 1

    // n0: bins [offset, offset+range) mod 256
    // n1: bins [mid,    mid+range)    mod 256
    // s[k] = count[0]+...+count[k], k=0..BINS-1, s[BINS-1]=total.
    // Circular range [a, a+len): no wrap → s[a+len-1]-s[a-1]; wrap → s[BINS-1]-s[a-1]+s[a+len-BINS-1].
    // boundary cases:
    //   a==0 → s[a-1] treated as 0.
    //   offset+range == BINS is the exact-fit case: s[BINS-1] is the last valid entry, no wrap needed.
    const int bin0_offset = (192 + c1 - rm->predictions[i]) & 255;
    const int bin1_offset = (bin0_offset + range) & 255;

    // n0: observation count in the "next bit = 0" residual window (lower half)
    // n1: observation count in the "next bit = 1" residual window (upper half)
    const uint64_t n0 = ResidualMap_count(s, bin0_offset, range);
    uint64_t n1 = ResidualMap_count(s, bin1_offset, range);

    // we need p1 = 4096*(n1+1)/(n0+n1+2)
    // to avoid the division:

    const uint64_t raw_sum = n0 + n1; // [0..131070] where 131070 = two times UINT16_MAX

    // to use dt, we need sum to be [0..1023] → we need to shift raw_sum by how much?
    // 131070 >> 7 = 1023
    //  65535 >> 6 = 1023
    //  32767 >> 5 = 1023
    //       ...
    //   4095 >> 2 = 1023
    //   2047 >> 1 = 1023
    //   1023 >> 0 = 1023
    // the shift needed is the number of bits in raw_sum - 10
    // the number of bits in raw_sum is 32 - clz(raw_sum)

    // clz is undefined for 0, but raw_sum==0 → shift=0 anyway,
    // so we OR with 1 to make clz safe; it doesn't affect the result when raw_sum>0.
    const int shift_raw = 32 - 10 - clz((uint32_t)(raw_sum | 1));
    const int shift = shift_raw & ~(shift_raw >> 31);  // max(shift_raw, 0) without branch

    const size_t sum_small = raw_sum >> shift;  // [0..1023]
    n1 = (n1 >> shift) + 1;

    const int p1 = (int)(((n1 << 12) * (uint64_t)rm->dt[sum_small]) >> 30); //4096 * (n1+1) / (n0+n1+2)

    Mixer_addProbability(m, p1, rm->scale);
  }
}

void ResidualMap_print(const ResidualMap* const rm) {
  for (size_t i = 0; i < rm->numContexts; i++) {
    for (size_t j = 0; j < rm->histogramsPerContext; j++) {
      const uint16_t* s = &rm->sums[(i * rm->histogramsPerContext + j) * BINS];
      printf("%zu\t%zu", i, j);
      for (int k = 0; k < BINS; k++)
        printf("\t%d", k == 0 ? s[0] : s[k] - s[k - 1]);
      printf("\n");
    }
  }
}
