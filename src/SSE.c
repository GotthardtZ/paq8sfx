#include "SSE.h"
#include "Hash.h"
#include "Utils.h"

// The sizes of the maps: contexts (APM: log2) and steps
static const uint8_t apmSetup[SSE_APMS][2] = {
  { 14, 20 }, { 16, 16 }, { 16, 16 },           // x86/x64
  { 8, 20 }, { 8, 24 }, { 16, 24 }, { 16, 24 }  // generic
};
static const uint32_t apm1Contexts[SSE_APM1S] = {
  64 * 257, 1 << 16,                            // x86/x64
  256 * 257, 1 << 16, 1 << 16                   // generic
};
static const uint8_t postContexts[SSE_POSTS] = { 1, 1, 8, 8 };

void SSE_init(SSE* const sse, Shared* const sh) {
  sse->shared = sh;
  for (int i = 0; i < SSE_APMS; i++) {
    APM_init(&sse->apm[i], sh, 1 << apmSetup[i][0], apmSetup[i][1], 1023);
  }
  for (int i = 0; i < SSE_APM1S; i++) {
    APM1_init(&sse->apm1[i], sh, apm1Contexts[i], 7);
  }
  for (int i = 0; i < SSE_POSTS; i++) {
    APMPost_init(&sse->post[i], sh, postContexts[i]);
  }
}

uint32_t SSE_p(SSE* const sse, const uint32_t pr_orig) {
  const Shared* const shared = sse->shared;

  INJECT_SHARED_c0
  INJECT_SHARED_bpos
  INJECT_SHARED_c4
  INJECT_SHARED_blockType

  assert(shared->State.NormalModel.order <= 7);

  //uint32_t misses4 = shared->State.misses & 15u;
  uint32_t misses = shared->State.misses << ((8 - bpos) & 7); //byte-aligned
  misses = (misses & 0xffffff00) | (misses & 0xff) >> ((8 - bpos) & 7);

  uint32_t misses3 =
    ((misses & 0x1) != 0) |
    ((misses & 0xfe) != 0) << 1 |
    ((misses & 0xff00) != 0) << 2;
  
  const BlockType normalizedBlockType = blockType;

  switch(normalizedBlockType) {
    case BLOCKTYPE_EXE: {
      uint32_t pr0 = APM_p(&sse->apm[0], pr_orig, shared->State.x86_64.state << 6 | misses3 << 3 | bpos);//14
      uint32_t pr1 = APM_p(&sse->apm[1], pr_orig, shared->State.x86_64.state << 8 | c0); // 16
      uint32_t pr2 = APM_p(&sse->apm[2], (pr0 + pr1 + 1) >> 1, finalize64(hash4(c4 & 0xFF, bpos, misses3 & 1, shared->State.x86_64.state >> 3), 16)); //16

      uint32_t prA = (pr_orig + pr0 + pr1 + pr2 + 2) >> 2;

      uint32_t pr4 = APM1_p(&sse->apm1[0], prA, shared->State.Match.expectedByte + (shared->State.NormalModel.order << 3 | shared->State.Match.mode3) * 257); //64*257
      uint32_t pr5 = APM1_p(&sse->apm1[1], prA, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr6 = APM1_p(&sse->apm1[1], pr_orig, c0 | (c4 & 0xFF) << 8); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (APMPost_p(&sse->post[0], prA, 0) + APMPost_p(&sse->post[1], prB, 0) + 1) >> 1;
      return pr;
      break;
    }
    default: {
      uint32_t todo3bit = 0;

      uint32_t pr0 = APM_p(&sse->apm[3], pr_orig, shared->State.Match.length2 << 6 | (uint32_t)(bpos) << 3 | misses3); //8
      uint32_t pr1 = APM_p(&sse->apm[4], pr_orig, shared->State.NormalModel.order << 5 | shared->State.Match.length2 << 3 | bpos); //8
      uint32_t pr2 = APM_p(&sse->apm[5], pr_orig, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr3 = APM_p(&sse->apm[6], pr_orig, c0 << 8 | (c4 & 0xF0) | (c4 & 0xF000 >> 12)); //16

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      
      uint32_t pr4 = APM1_p(&sse->apm1[2], prA, shared->State.Match.expectedByte + (misses3 << 5 | shared->State.NormalModel.order << 2 | shared->State.Match.length2) * 257); //256*257
      uint32_t pr5 = APM1_p(&sse->apm1[3], pr0, misses3<<13 | shared->State.Match.length2 << 11 | todo3bit << 8 | c0); //16
      uint32_t pr6 = APM1_p(&sse->apm1[4], pr0, (misses3&3)<<14 | (bpos>>1) << 12 | (c4 & 0xFF) << 4 | todo3bit); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (APMPost_p(&sse->post[2], prA, shared->State.NormalModel.order) + APMPost_p(&sse->post[3], prB, shared->State.Match.mode3) + 1) >> 1;
      return pr;
    }
  }
  
}
