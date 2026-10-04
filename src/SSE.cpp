#include "SSE.hpp"
#include "Hash.hpp"
#include "Utils.hpp"

SSE::SSE(Shared* const sh) : shared(sh),
  x86_64{
    { /*APM:*/ {sh,1 << 14,20,1023}, {sh,1 << 16,16,1023}, {sh,1 << 16,16,1023} },
    { /*APM1:*/ {sh,64*257,7}, {sh,1 << 16,7}, {sh,1 << 16,7} },
    { /*ApmPostA: */ sh,1},
    { /*ApmPostB: */ sh,1}
  },
  Generic {
    { /*APM:*/  {sh,1<<8,20,1023}, {sh,1 << 8,24,1023}, {sh,1 << 16,24,1023}, {sh,1 << 16,24,1023}}, /* APM: contexts, steps */
    { /*APM1:*/ {sh,256*257,7}, {sh,1 << 16,7}, {sh,1 << 16,7}},
    { /*ApmPostA: */ sh,8},
    { /*ApmPostB: */ sh,8}
  }
  {}

uint32_t SSE::p(const uint32_t pr_orig) {

  INJECT_SHARED_c0
  INJECT_SHARED_bpos
  INJECT_SHARED_c4
  INJECT_SHARED_blockPos
  INJECT_SHARED_blockType

  assert(shared->State.NormalModel.order <= 7);

  assert(shared->State.x86_64.state <= 255);

  //uint32_t misses4 = shared->State.misses & 15u;
  uint32_t misses = shared->State.misses << ((8 - bpos) & 7); //byte-aligned
  misses = (misses & 0xffffff00) | (misses & 0xff) >> ((8 - bpos) & 7);

  uint32_t misses3 =
    ((misses & 0x1) != 0) |
    ((misses & 0xfe) != 0) << 1 |
    ((misses & 0xff00) != 0) << 2;
  
  const BlockType normalizedBlockType = blockType;

  switch(normalizedBlockType) {
    case BlockType::EXE: {
      uint32_t pr0 = x86_64.APMs[0].p(pr_orig, shared->State.x86_64.state << 6 | misses3 << 3 | bpos);//14
      uint32_t pr1 = x86_64.APMs[1].p(pr_orig, shared->State.x86_64.state << 8 | c0); // 16
      uint32_t pr2 = x86_64.APMs[2].p((pr0 + pr1 + 1) >> 1, finalize64(hash(c4 & 0xFF, bpos, misses3 & 1, shared->State.x86_64.state >> 3), 16)); //16

      uint32_t prA = (pr_orig + pr0 + pr1 + pr2 + 2) >> 2;

      uint32_t pr4 = x86_64.APM1s[0].p(prA, shared->State.Match.expectedByte + (shared->State.NormalModel.order << 3 | shared->State.Match.mode3) * 257); //64*257
      uint32_t pr5 = x86_64.APM1s[1].p(prA, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr6 = x86_64.APM1s[1].p(pr_orig, c0 | (c4 & 0xFF) << 8); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (x86_64.APMPostA.p(prA, 0) + x86_64.APMPostB.p(prB, 0) + 1) >> 1;
      return pr;
      break;
    }
    default: {
      uint32_t todo3bit = 0;

      uint32_t pr0 = Generic.APMs[0].p(pr_orig, shared->State.Match.length2 << 6 | static_cast<uint32_t>(bpos) << 3 | misses3); //8
      uint32_t pr1 = Generic.APMs[1].p(pr_orig, shared->State.NormalModel.order << 5 | shared->State.Match.length2 << 3 | bpos); //8
      uint32_t pr2 = Generic.APMs[2].p(pr_orig, c0 | (c4 & 0xFF) << 8); //16
      uint32_t pr3 = Generic.APMs[3].p(pr_orig, c0 << 8 | (c4 & 0xF0) | (c4 & 0xF000 >> 12)); //16

      uint32_t prA = (pr_orig + pr1 + pr2 + pr3 + 2) >> 2;
      
      uint32_t pr4 = Generic.APM1s[0].p(prA, shared->State.Match.expectedByte + (misses3 << 5 | shared->State.NormalModel.order << 2 | shared->State.Match.length2) * 257); //256*257
      uint32_t pr5 = Generic.APM1s[1].p(pr0, misses3<<13 | shared->State.Match.length2 << 11 | todo3bit << 8 | c0); //16
      uint32_t pr6 = Generic.APM1s[2].p(pr0, (misses3&3)<<14 | (bpos>>1) << 12 | (c4 & 0xFF) << 4 | todo3bit); //16

      uint32_t prB = (pr0 + pr4 + pr5 + pr6 + 2) >> 2;

      uint32_t pr = (Generic.APMPostA.p(prA, shared->State.NormalModel.order) + Generic.APMPostB.p(prB, shared->State.Match.mode3) + 1) >> 1;
      return pr;
    }
  }
  
}
