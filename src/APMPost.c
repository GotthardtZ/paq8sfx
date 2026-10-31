#include "APMPost.h"
#include "Array.h"
#include "ArithmeticEncoder.h"
#include "UpdateBroadcaster.h"

static void APMPost_update(IPredictor* const predictor) {
  APMPost* const apm = (APMPost*)predictor;
  const Shared* const shared = apm->shared;
  INJECT_SHARED_y
  uint64_t n0, n1, value;
  value = apm->t[apm->index];
  n0 = value >> 32;
  n1 = value & 0xffffffff;

  n0 += 1 - y;
  n1 += y;
  int shift = (n0 | n1) >> 32; // shift: 0 or 1
  n0 >>= shift;
  n1 >>= shift;

  apm->t[apm->index] = n0 << 32 | n1;
}

void APMPost_init(APMPost* const apm, const Shared* const sh, const uint32_t n) {
  apm->predictor.update = APMPost_update;
  apm->shared = sh;
  apm->index = 0;
  apm->n = n;
  apm->t = ARRAY_NEW(uint64_t, n * UINT64_C(4096));
  assert(n > 0);
  for(size_t i = 0; i < n; ++i ) {
    for(uint64_t j = 0; j < 4096; ++j ) {
      apm->t[i * 4096 + j] = ((4096 - j) >> 1) << 32 | (j >> 1);
    }
  }
}

void APMPost_destroy(APMPost* const apm) {
  Array_delete(apm->t);
  apm->t = NULL;
}

uint32_t APMPost_p(APMPost* const apm, const uint32_t pr, const uint32_t cxt) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(apm->shared), &apm->predictor);
  assert(pr < 4096 && cxt < apm->n);
  apm->index = cxt * 4096 + pr;
  uint64_t n0 = apm->t[apm->index] >> 32;
  uint64_t n1 = apm->t[apm->index] &0xffffffff;
  n0 = n0 * 2 + 1;
  n1 = n1 * 2 + 1;
  return (uint32_t)((n1 << ArithmeticEncoder_PRECISION) / (n0+n1));
}

void APMPost_print(const APMPost* const apm) {
  for (int i = 0; i < 4096; i++) {
    printf("%d\t%d\t%d\n",i,(int)(apm->t[i]>>32), (int)(apm->t[i]&0xffffffff));
  }
}
