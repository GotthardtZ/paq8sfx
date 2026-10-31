#include "SSE.h"
#include "Hash.h"
#include "Utils.h"

void SSE_init(SSE* const sse, Shared* const sh) {
  sse->shared = sh;
  /*APM:*/
  APM_init(&sse->x86_64.APMs[0], sh, 1 << 14, 20, 1023);
  APM_init(&sse->x86_64.APMs[1], sh, 1 << 16, 16, 1023);
  APM_init(&sse->x86_64.APMs[2], sh, 1 << 16, 16, 1023);
  /*APM1:*/
  APM1_init(&sse->x86_64.APM1s[0], sh, 64*257, 7);
  APM1_init(&sse->x86_64.APM1s[1], sh, 1 << 16, 7);
  APM1_init(&sse->x86_64.APM1s[2], sh, 1 << 16, 7);
  /*ApmPostA: */
  APMPost_init(&sse->x86_64.APMPostA, sh, 1);
  /*ApmPostB: */
  APMPost_init(&sse->x86_64.APMPostB, sh, 1);

  /*APM: contexts, steps */
  APM_init(&sse->Generic.APMs[0], sh, 1 << 8, 20, 1023);
  APM_init(&sse->Generic.APMs[1], sh, 1 << 8, 24, 1023);
  APM_init(&sse->Generic.APMs[2], sh, 1 << 16, 24, 1023);
  APM_init(&sse->Generic.APMs[3], sh, 1 << 16, 24, 1023);
  /*APM1:*/
  APM1_init(&sse->Generic.APM1s[0], sh, 256*257, 7);
  APM1_init(&sse->Generic.APM1s[1], sh, 1 << 16, 7);
  APM1_init(&sse->Generic.APM1s[2], sh, 1 << 16, 7);
  /*ApmPostA: */
  APMPost_init(&sse->Generic.APMPostA, sh, 8);
  /*ApmPostB: */
  APMPost_init(&sse->Generic.APMPostB, sh, 8);
}

void SSE_destroy(SSE* const sse) {
  for (int i = 0; i < 3; i++) {
    APM_destroy(&sse->x86_64.APMs[i]);
    APM1_destroy(&sse->x86_64.APM1s[i]);
    APM1_destroy(&sse->Generic.APM1s[i]);
  }
  for (int i = 0; i < 4; i++) {
    APM_destroy(&sse->Generic.APMs[i]);
  }
  APMPost_destroy(&sse->x86_64.APMPostA);
  APMPost_destroy(&sse->x86_64.APMPostB);
  APMPost_destroy(&sse->Generic.APMPostA);
  APMPost_destroy(&sse->Generic.APMPostB);
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
      uint32_t pr0 = APM_p(&sse->x86_64.APMs[0], pr_orig, shared->State.x86_64.state << 6 | misses3 << 3 | bpos);//14
      uint32_t pr1 = APM_p(&sse->x86_64.APMs[1], pr_orig, shared->State.x86_64.state << 8 | c0); // 16
      uint32_t pr2 = APM_p(&sse->x86_64.APMs[2], (pr0 + pr1 + 1) >> 1, finalize64(hash4(c4 & 0xFF, bpos, misses3 & 1, shared->State.x86_64.state >> 3), 16)); //16

      uint32_t prA = (pr_orig + pr0 + pr1 + pr2 + 2) >> 2;

      uint32_t pr4 = APM1_p(&sse->x86_64.APM1s[0], prA, shared->State.Match.expectedByte + (shared->State.NormalModel.order << 3 | shared->State.Match.mode3) * 257); //64*257
      uint32_t pr5 = APM1_p(&sse->x86_64.APM1s[1], prA, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr6 = APM1_p(&sse->x86_64.APM1s[1], pr_orig, c0 | (c4 & 0xFF) << 8); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (APMPost_p(&sse->x86_64.APMPostA, prA, 0) + APMPost_p(&sse->x86_64.APMPostB, prB, 0) + 1) >> 1;
      return pr;
      break;
    }
    default: {
      uint32_t todo3bit = 0;

      uint32_t pr0 = APM_p(&sse->Generic.APMs[0], pr_orig, shared->State.Match.length2 << 6 | (uint32_t)(bpos) << 3 | misses3); //8
      uint32_t pr1 = APM_p(&sse->Generic.APMs[1], pr_orig, shared->State.NormalModel.order << 5 | shared->State.Match.length2 << 3 | bpos); //8
      uint32_t pr2 = APM_p(&sse->Generic.APMs[2], pr_orig, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr3 = APM_p(&sse->Generic.APMs[3], pr_orig, c0 << 8 | (c4 & 0xF0) | (c4 & 0xF000 >> 12)); //16

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      
      uint32_t pr4 = APM1_p(&sse->Generic.APM1s[0], prA, shared->State.Match.expectedByte + (misses3 << 5 | shared->State.NormalModel.order << 2 | shared->State.Match.length2) * 257); //256*257
      uint32_t pr5 = APM1_p(&sse->Generic.APM1s[1], pr0, misses3<<13 | shared->State.Match.length2 << 11 | todo3bit << 8 | c0); //16
      uint32_t pr6 = APM1_p(&sse->Generic.APM1s[2], pr0, (misses3&3)<<14 | (bpos>>1) << 12 | (c4 & 0xFF) << 4 | todo3bit); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (APMPost_p(&sse->Generic.APMPostA, prA, shared->State.NormalModel.order) + APMPost_p(&sse->Generic.APMPostB, prB, shared->State.Match.mode3) + 1) >> 1;
      return pr;
    }
  }
  
}
