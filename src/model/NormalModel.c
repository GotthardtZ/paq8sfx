#include "NormalModel.h"

void NormalModel_init(NormalModel* const nm, Shared* const sh, const uint64_t cmSize) {
  nm->shared = sh;
  ContextMap2_init(&nm->cm, sh, cmSize, NormalModel_nCM, 64);
  assert(isPowerOf2(cmSize));
}

void NormalModel_destroy(NormalModel* const nm) {
  ContextMap2_destroy(&nm->cm);
}

void NormalModel_reset(NormalModel* const nm) {
  memset(&nm->shared->State.NormalModel.cxt[0], 0, sizeof(nm->shared->State.NormalModel.cxt));
}

void NormalModel_updateHashes(NormalModel* const nm) {
  const Shared* const shared = nm->shared;
  INJECT_SHARED_c1
  INJECT_SHARED_blockType
  uint64_t base_context = c1;
  base_context = (base_context * (uint64_t)BLOCKTYPE_COUNT) + (uint64_t)blockType;
  uint64_t* cxt = nm->shared->State.NormalModel.cxt;
  for( uint64_t i = 14; i > 0; --i ) {
    cxt[i] = (cxt[i - 1] + base_context + i) * PHI64;
  }
}

void NormalModel_mix(NormalModel* const nm, Mixer* const m) {
  const Shared* const shared = nm->shared;
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    NormalModel_updateHashes(nm);
    uint64_t* cxtHashes = nm->shared->State.NormalModel.cxt;
    for(uint64_t i = 1; i <= 6; ++i ) {
      ContextMap2_set(&nm->cm, cxtHashes[i]);
    }
    ContextMap2_set(&nm->cm, cxtHashes[8]); 
    ContextMap2_set(&nm->cm, cxtHashes[11]);
    ContextMap2_set(&nm->cm, cxtHashes[14]);
  }
  ContextMap2_mix(&nm->cm, m);

  const int order = max(0, nm->cm.order - (NormalModel_nCM - 7)); //0-7
  assert(0 <= order && order <= 7);
  Mixer_set(m, order << 3 | bpos, 64);
  nm->shared->State.NormalModel.order = order;
}

void NormalModel_mixPost(NormalModel* const nm, Mixer* const m) {
  const Shared* const shared = nm->shared;
  INJECT_SHARED_c4
  uint32_t c2 = (c4 >> 8) & 0xff;
  uint32_t c3 = (c4 >> 16) & 0xff;
  uint32_t c;

  INJECT_SHARED_c0
  INJECT_SHARED_c1
  INJECT_SHARED_bpos
  INJECT_SHARED_blockType
  Mixer_set(m, (c1 | (bpos > 5) << 8 | (((c0 & ((1u << bpos) - 1)) == 0) || (c0 == ((2u << bpos) - 1))) << 9), 1024);
  Mixer_set(m, c0, 256);
  uint32_t bt = (uint32_t)blockType;
  Mixer_set(m, shared->State.NormalModel.order | ((c1 >> 6) & 3) << 3 | (bpos == 0) << 5 | (c1 == c2) << 6 | bt << 7, 512);
  Mixer_set(m, c2, 256);
  Mixer_set(m, c3, 256);
  if( bpos != 0 ) {
    c = c0 << (8 - bpos);
    if( bpos == 1 ) {
      c |= c3 >> 1;
    }
    c = min(bpos, 5) << 8 | c1 >> 5 | (c2 >> 5) << 3 | (c & 192);
  } else {
    c = c3 >> 7 | (c4 >> 31) << 1 | (c2 >> 6) << 2 | (c1 & 240);
  }
  Mixer_set(m, c, 1536);
}
