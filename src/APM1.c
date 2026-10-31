#include "APM1.h"
#include "Array.h"
#include "UpdateBroadcaster.h"
#include "Squash.h"
#include "Stretch.h"

static void APM1_update(IPredictor* const predictor) {
  APM1* const apm = (APM1*)predictor;
  const Shared* const shared = apm->shared;
  INJECT_SHARED_y
  const int rate = apm->rate;
  uint16_t* const t = apm->t;
  const int index = apm->index;
  const int g = (y << 16) + (y << rate) - y - y;
  t[index] += (g - t[index]) >> rate;
  t[index + 1] += (g - t[index + 1]) >> rate;
}

void APM1_init(APM1* const apm, const Shared* const sh, const int n, const int r) {
  apm->predictor.update = APM1_update;
  apm->shared = sh;
  apm->index = 0;
  apm->n = n;
  apm->t = ARRAY_NEW(uint16_t, n * 33);
  apm->rate = r;
  assert(n > 0 && apm->rate > 0 && apm->rate < 32);
  // maps p, cxt -> p initially
  uint16_t* const t = apm->t;
  for( int i = 0; i < n; ++i ) {
    for( int j = 0; j < 33; ++j ) {
      if( i == 0 ) {
        t[i * 33 + j] = squash((j - 16) * 128) * 16;
      } else {
        t[i * 33 + j] = t[j];
      }
    }
  }
}

void APM1_destroy(APM1* const apm) {
  Array_delete(apm->t);
  apm->t = NULL;
}

int APM1_p(APM1* const apm, int pr, const int cxt) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(apm->shared), &apm->predictor);
  assert(pr >= 0 && pr < 4096 && cxt >= 0 && cxt < apm->n);
  pr = stretch(pr);
  const int w = pr & 127; // interpolation weight (33 points)
  apm->index = ((pr + 2048) >> 7) + cxt * 33;
  const uint16_t* const t = apm->t;
  return (t[apm->index] * (128 - w) + t[apm->index + 1] * w) >> 11;
}
