#include "SparseModel.h"

void SparseModel_init(SparseModel* const sm, const Shared* const sh, const uint64_t size) {
  sm->shared = sh;
  ContextMap2_init(&sm->cm, sh, size, SparseModel_nCM, 64);
}

// The contexts, in order. Each is up to 3 bytes of the history, combined as
// buf(d0) << 8*p0 | buf(d1) << 8*p1 | buf(d2) << 8*p2.
// A term is S(d, p): d = distance (1..10, buf(1) is the last byte), p = byte
// position in the context value. 0 = no term. The two contexts marked SPECIAL
// are computed by the code below the table.
#define S(d, p) ((p) << 4 | (d))
#define SPECIAL { 0, 0, 0 }
static const uint8_t sparseContexts[SparseModel_nCM][3] = {
  // text files benefit from these contexts - these strong(er) models for text files
  { S(1, 0), S(3, 2), 0       }, // buf(1) + buf(3)
  { S(1, 0), S(4, 3), 0       }, // buf(1) + buf(4)
  { S(2, 1), 0,       0       }, // buf(2)
  // the following sparse contexts are usually just generating noise for text content
  SPECIAL,                       // 4-byte structures and positions of zeroes (calgary/obj2, canterbury/kennedy.xls, canterbury/sum, etc.)
  { S(1, 0), S(6, 1), 0       }, // combination of two bytes
  { S(3, 0), S(6, 1), 0       },
  { S(1, 0), S(3, 1), S(5, 2) }, // combination of three bytes
  { S(2, 0), S(4, 1), S(6, 2) },
  SPECIAL,                       // some 4-byte fixed length structures (calgary/geo, calgary/obj2, etc.)
  { S(3, 0), 0,       0       }, // single bytes
  { S(4, 0), 0,       0       },
  { S(5, 0), 0,       0       },
  { S(6, 0), 0,       0       },
  { S(7, 0), 0,       0       },
  { S(8, 0), 0,       0       },
  { S(2, 0), S(3, 1), 0       }, // two consecutive bytes
  { S(3, 0), S(4, 1), 0       },
  { S(4, 0), S(5, 1), 0       },
  { S(5, 0), S(6, 1), 0       },
  { S(6, 0), S(7, 1), 0       },
  { S(2, 0), S(4, 1), 0       }, // two bytes with a gap
  { S(3, 0), S(5, 1), 0       },
  { S(4, 0), S(6, 1), 0       },
  { S(5, 0), S(7, 1), 0       },
  { S(6, 0), S(8, 1), 0       },
  { S(1, 0), S(5, 1), 0       }, // combination of two bytes
  { S(4, 0), S(8, 1), 0       },
  { S(7, 0), S(8, 1), 0       }, // two consecutive bytes (in case of text files only useful for silesia/nci)
  { S(8, 0), S(9, 1), 0       }, // silesia/nci
  { S(7, 0), S(9, 1), 0       }, // two bytes with a gap (silesia/nci)
  { S(8, 0), S(10, 1), 0      }, // silesia/nci
};
#undef S
#undef SPECIAL

void SparseModel_mix(SparseModel* const sm, Mixer* const m) {
  const Shared* const shared = sm->shared;
  ContextMap2* const cm = &sm->cm;
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_c4
    INJECT_SHARED_buf

    //context for 4-byte structures and
    //positions of zeroes in the last 16 bytes
    sm->ctx <<= 1;
    sm->ctx |= (c4 & 0xff) == RingBuffer_back(buf, 5); //column matches in a 4-byte fixed structure
    sm->ctx <<= 1;
    sm->ctx |= (c4 & 0xff) == 0; //zeroes

    for (uint32_t i = 0; i < SparseModel_nCM; i++) {
      const uint8_t* const t = sparseContexts[i];
      uint32_t ctx = 0;
      for (int k = 0; k < 3; k++) {
        if (t[k] != 0) {
          ctx |= (uint32_t)RingBuffer_back(buf, t[k] & 15) << ((t[k] >> 4) * 8);
        }
      }
      if (i == 3) {
        ctx = sm->ctx;
      }
      else if (i == 8) {
        ctx = (c4 & 0xffe00000) | (sm->ctx & 0xff);
      }
      ContextMap2_set(cm, hash2(i + 1, ctx));
    }
  }
  ContextMap2_mix(cm, m);

  //support fixed structures of 4 bytes
  INJECT_SHARED_blockPos
  Mixer_set(m, (blockPos & 3)<<8 | (sm->ctx&0xff), 4 * 256);

}
