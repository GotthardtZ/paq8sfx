#include "StateMap.h"
#include "Utils.h"

static void StateMap_update(IPredictor* const predictor) {
  StateMap* const sm = (StateMap*)predictor;
  assert(sm->currentContextSetIndex <= sm->numContextSets);
  while( sm->currentContextSetIndex > 0 ) {
    sm->currentContextSetIndex--;
    const uint32_t idx = sm->cxt[sm->currentContextSetIndex];
    if( idx == UINT32_MAX) {
      continue; // skipped context
    }
    assert(idx % sm->numContextSets == sm->currentContextSetIndex && idx / sm->numContextSets < sm->numContextsPerSet);
    AdaptiveMap_update(&sm->am, &sm->am.t[idx], sm->limit);
  }
}

void StateMap_init(StateMap* const sm, const Shared* const sh, const int s, const int n, const int lim, const StateMapType mapType) {
  sm->predictor.update = StateMap_update;
  AdaptiveMap_init(&sm->am, sh, n * s);
  sm->limit = lim;
  sm->numContextSets = s;
  sm->numContextsPerSet = n;
  sm->cxt = ARRAY_NEW(uint32_t, s);
  uint32_t* const t = sm->am.t;
  const uint32_t numContextSets = sm->numContextSets;
  const uint32_t numContextsPerSet = sm->numContextsPerSet;
  assert(numContextSets > 0 && numContextsPerSet > 0);
  for (uint32_t cx = 0; cx < numContextsPerSet; ++cx) {
    uint32_t p = 2048 << 20 | 0; // no a-priory in the general case: p=0.5, count=0
    if( mapType == STATEMAP_BITHISTORY ) { // when the context is a bit history byte, we have a-priory for p
      assert((numContextsPerSet & 255) == 0);
      const uint8_t state = cx & 255;
      uint32_t n0 = StateTable_next(state, 2);
      uint32_t n1 = StateTable_next(state, 3);
      if (state < 205) {
        n0 = n0 * 3 + 1;
        n1 = n1 * 3 + 1;
        p = ((n1 << 20) / (n0 + n1)) << 12;
      }
      else if (state < 253) {
        int incremental_state = (state - 205) >> 2;
        if (((state - 205) & 3) <= 1)
          n0 = 29 + (1 << incremental_state);
        else
          n1 = 29 + (1 << incremental_state);
        n0 = n0 * 3 + 1;
        n1 = n1 * 3 + 1;
        assert(n0 < 16384 && n1 < 16384);
        p = ((n1 << 18) / (n0 + n1)) << 14 | min((n0 + n1) >> 3, 1023);
      }
      // else 253, 254, 255: unused states: p=0.5
    }
    else if( mapType == STATEMAP_RUN ) { // when the context is a run count: we have a-priory for p
      const int predictedBit = (cx) & 1;
      const int uncertainty = (cx >> 1) & 1;
      //const int bp = (cx>>2)&1; // unused in calculation - a-priory does not seem to depend on bitPosition for a RunMap
      const int runCount = (cx >> 4); // 0..254
      uint32_t n0 = uncertainty + 1;
      uint32_t n1 = (runCount + 1) * 12;
      if( predictedBit == 0 ) {
        const uint32_t tmp = n0;
        n0 = n1;
        n1 = tmp;
      }
      assert(n0 < 4096 && n1 < 4096);
      p = ((n1 << 20) / (n0 + n1)) << 12 | sm->limit;
    }
    for (uint32_t s = 0; s < numContextSets; ++s) {
      t[cx * numContextSets + s] = p;
    }
  }
}

int StateMap_p1(StateMap* const sm, const uint32_t cx) {
  assert(sm->numContextSets == 1);
  assert(sm->currentContextSetIndex == 0);
  assert(cx < sm->numContextsPerSet);
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(sm->am.shared), &sm->predictor);
  sm->cxt[0] = cx;
  sm->currentContextSetIndex++;
  return sm->am.t[cx] >> 20;
}

int StateMap_p2(StateMap* const sm, const uint32_t s, const uint32_t cx) {
  assert(s < sm->numContextSets);
  assert(cx < sm->numContextsPerSet);
  assert(s == sm->currentContextSetIndex);
  (void)s;
  const uint32_t idx = cx * sm->numContextSets + sm->currentContextSetIndex;
  sm->cxt[sm->currentContextSetIndex] = idx;
  sm->currentContextSetIndex++;
  return sm->am.t[idx] >> 20;
}

void StateMap_subscribe(StateMap* const sm) {
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(sm->am.shared), &sm->predictor);
}

void StateMap_skip(StateMap* const sm, const uint32_t contextSetIndex) {
  assert(contextSetIndex < sm->numContextSets);
  assert(contextSetIndex == sm->currentContextSetIndex);
  (void)contextSetIndex;
  sm->cxt[sm->currentContextSetIndex] = UINT32_MAX; // mark for skipping
  sm->currentContextSetIndex++;
}

void StateMap_print(const StateMap* const sm) {
  for (uint32_t cx = 0; cx < sm->numContextsPerSet; cx++) {
    for (uint32_t s = 0; s < sm->numContextSets; s++) {
      uint32_t p0 = sm->am.t[cx * sm->numContextSets + s] >> 10;
      printf("%d\t%d\t%d\n", cx, s, p0);
    }
  }
}
