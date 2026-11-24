#include "IndirectMap.h"
#include "Array.h"
#include "Stretch.h"

static void IndirectMap_update(IPredictor* const predictor) {
  IndirectMap* const im = (IndirectMap*)predictor;
  const Shared* const shared = im->shared;
  INJECT_SHARED_y
  StateTable_update(im->cp, y, &im->rnd);
  im->b += (uint32_t)((y != 0) && im->b > 0);
}

void IndirectMap_init(IndirectMap* const im, const Shared* const sh, const int bitsOfContext, const int inputBits, const int scale, const int limit) {
  (void)limit;
  im->predictor.update = IndirectMap_update;
  im->shared = sh;
  im->data = ARRAY_NEW(uint8_t, (UINT64_C(1) << bitsOfContext) * ((UINT64_C(1) << inputBits) - 1));
  StateMap_init(&im->sm, sh, 1, 256, 1023, STATEMAP_BITHISTORY);
  im->mask = (1 << bitsOfContext) - 1;
  im->maskBits = bitsOfContext;
  im->stride = (1 << inputBits) - 1;
  im->bTotal = inputBits;
  im->scale = scale;
  assert(inputBits > 0 && inputBits <= 8);
  assert(bitsOfContext + inputBits <= 24);
}

void IndirectMap_set(IndirectMap* const im, const uint64_t ctx) {
  im->context = (finalize64(ctx, im->maskBits)) * im->stride;
  im->bCount = im->b = 0;
}

void IndirectMap_setScale(IndirectMap* const im, const int Scale) { im->scale = Scale; }

void IndirectMap_mix(IndirectMap* const im, Mixer* const m) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(im->shared), &im->predictor);
  im->cp = &im->data[im->context + im->b];
  const uint8_t state = *im->cp;
  if (state == 0) {
    Mixer_add(m, 0);
    Mixer_add(m, 0);
  }
  else {
    const int p1 = StateMap_p1(&im->sm, state);
    Mixer_addProbability(m, p1, im->scale);
  }
  im->bCount++;
  im->b += im->b + 1;
  assert(im->bCount <= im->bTotal);
}
