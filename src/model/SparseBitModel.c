#include "SparseBitModel.h"

void SparseBitModel_init(SparseBitModel* const sbm, const Shared* const sh, const uint64_t size) {
  sbm->shared = sh;
  ContextMap2_init(&sbm->cm, sh, size, SparseBitModel_nCM, 64);
}

// The contexts: c4 (the last 4 bytes) masked with these, in order.
// They are beneficial for both text and binary files.
static const uint32_t sparseBitMasks[SparseBitModel_nCM] = {
  0x00f0f0ff, 0xdfdfdfe0,
  0xffc0ffc0, 0xe0ffffe0, // note: silesia/osdb does not like these two contexts
  0x00e0e0e0, 0xe0e0e0e0, 0x0000f8f8, 0x00f8f8f8, 0xf8f8f8f8, 0x0f0f0f0f
};

void SparseBitModel_mix(SparseBitModel* const sbm, Mixer* const m) {
  const Shared* const shared = sbm->shared;
  ContextMap2* const cm = &sbm->cm;

  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_c4
    for (uint32_t i = 0; i < SparseBitModel_nCM; i++) {
      ContextMap2_set(cm, hash2(i + 1, c4 & sparseBitMasks[i]));
    }
  }

  ContextMap2_mix(cm, m);
}
