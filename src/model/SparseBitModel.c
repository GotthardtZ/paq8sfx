#include "SparseBitModel.h"

void SparseBitModel_init(SparseBitModel* const sbm, const Shared* const sh, const uint64_t size) {
  sbm->shared = sh;
  ContextMap2_init(&sbm->cm, sh, size, SparseBitModel_nCM, 64);
}

void SparseBitModel_destroy(SparseBitModel* const sbm) {
  ContextMap2_destroy(&sbm->cm);
}

void SparseBitModel_mix(SparseBitModel* const sbm, Mixer* const m) {
  const Shared* const shared = sbm->shared;
  ContextMap2* const cm = &sbm->cm;

  INJECT_SHARED_bpos
  if (bpos == 0) {
    INJECT_SHARED_c4
    uint64_t i = 0;


    //the following context are beneficial for both text and binary files
    ContextMap2_set(cm, hash2(++i, c4 & 0x00f0f0ff));
    ContextMap2_set(cm, hash2(++i, c4 & 0xdfdfdfe0));
    ContextMap2_set(cm, hash2(++i, c4 & 0xffc0ffc0)); //note: silesia/osdb does not like this context
    ContextMap2_set(cm, hash2(++i, c4 & 0xe0ffffe0)); //note: silesia/osdb does not like this context
    ContextMap2_set(cm, hash2(++i, c4 & 0x00e0e0e0));
    ContextMap2_set(cm, hash2(++i, c4 & 0xe0e0e0e0));

    ContextMap2_set(cm, hash2(++i, c4 & 0x0000f8f8));
    ContextMap2_set(cm, hash2(++i, c4 & 0x00f8f8f8));
    ContextMap2_set(cm, hash2(++i, c4 & 0xf8f8f8f8));
    ContextMap2_set(cm, hash2(++i, c4 & 0x0f0f0f0f));

    assert((int)i == SparseBitModel_nCM);
  }

  ContextMap2_mix(cm, m);
}
