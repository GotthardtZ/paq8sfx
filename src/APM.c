#include "APM.h"
#include "Squash.h"
#include "Stretch.h"
#include "UpdateBroadcaster.h"

/**
  * updates probability map. y=(0..1) is the last bit
  */
static void APM_update(IPredictor* const predictor) {
  APM* const apm = (APM*)predictor;
  assert(apm->cxt >= 0 && apm->cxt < apm->N);
  AdaptiveMap_update(&apm->am, &apm->am.t[apm->cxt], apm->limit);
}

void APM_init(APM* const apm, const Shared* const sh, const int n, const int s, const int limit) {
  apm->predictor.update = APM_update;
  AdaptiveMap_init(&apm->am, sh, n * s);
  apm->N = n * s;
  apm->steps = s;
  apm->limit = limit;
  assert(s > 4); // number of steps - must be a positive integer bigger than 4
  for( int i = 0; i < apm->N; ++i ) {
    int p = ((i % apm->steps * 2 + 1) * 4096) / (apm->steps * 2) - 2048;
    apm->am.t[i] = ((uint32_t)squash(p) << 20) + 6; //initial count: 6
  }
}

int APM_p(APM* const apm, int pr, int cx) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(apm->am.shared), &apm->predictor);
  assert(pr >= 0 && pr < 4096);
  assert(cx >= 0 && cx < apm->N / apm->steps);
  pr = (stretch(pr) + 2048) * (apm->steps - 1);
  int wt = pr & 0x0fff; // interpolation weight (0..4095)
  cx = cx * apm->steps + (pr >> 12);
  assert(cx >= 0 && cx < apm->N - 1);
  apm->cxt = cx + (wt >> 11);
  const uint32_t* const t = apm->am.t;
  pr = ((t[cx] >> 13) * (4096 - wt) + (t[cx + 1] >> 13) * wt) >> 19;
  return pr;
}
