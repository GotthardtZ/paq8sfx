#include "SparseModel.h"

void SparseModel_init(SparseModel* const sm, const Shared* const sh, const uint64_t size) {
  sm->shared = sh;
  ContextMap2_init(&sm->cm, sh, size, SparseModel_nCM, 64);
  sm->ctx = 0;
}

void SparseModel_destroy(SparseModel* const sm) {
  ContextMap2_destroy(&sm->cm);
}

void SparseModel_mix(SparseModel* const sm, Mixer* const m) {
  const Shared* const shared = sm->shared;
  ContextMap2* const cm = &sm->cm;
  INJECT_SHARED_bpos
  if( bpos == 0 ) {
    INJECT_SHARED_c4
    uint64_t i = 0;

    //textfiles benefit from these contexts - these strong(er) models for text files
    ContextMap2_set(cm, hash2(++i, c4 & 0x00ff00ff)); //buf(1) + buf(3)
    ContextMap2_set(cm, hash2(++i, c4 & 0xff0000ff)); //buf(1) + buf(4)
    ContextMap2_set(cm, hash2(++i, c4 & 0x0000ff00)); //buf(2)

    //The following sparse contexts are usually just generating noise for text content - so skip if not text
    INJECT_SHARED_buf

    //context for 4-byte structures and 
    //positions of zeroes in the last 16 bytes
    sm->ctx <<= 1;
    sm->ctx |= (c4 & 0xff) == RingBuffer_back(buf, 5); //column matches in a 4-byte fixed structure
    sm->ctx <<= 1;
    sm->ctx |= (c4 & 0xff) == 0; //zeroes

    ContextMap2_set(cm, hash2(++i, sm->ctx)); // calgary/obj2, canterbury/kennedy.xls, canterbury/sum, etc.

    //combination of two bytes
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 1) | RingBuffer_back(buf, 6) << 8));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 3) | RingBuffer_back(buf, 6) << 8));
      
    //combination of three bytes
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 1) | RingBuffer_back(buf, 3) << 8 | RingBuffer_back(buf, 5) << 16));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 2) | RingBuffer_back(buf, 4) << 8 | RingBuffer_back(buf, 6) << 16));

    //special model for some 4-byte fixed length structures
    ContextMap2_set(cm, hash2(++i, c4 & 0xffe00000 | (sm->ctx&0xff))); // calgary/geo, calgary/obj2, etc.

    //single bytes
    ContextMap2_set(cm, hash2(++i, (c4 & 0x00ff0000) >> 16)); //buf(3)
    ContextMap2_set(cm, hash2(++i, c4 >> 24)); //buf(4)
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 5)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 6)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 7)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 8)));

    //two consecutive bytes
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 3) << 8 | RingBuffer_back(buf, 2)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 4) << 8 | RingBuffer_back(buf, 3)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 5) << 8 | RingBuffer_back(buf, 4)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 6) << 8 | RingBuffer_back(buf, 5)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 7) << 8 | RingBuffer_back(buf, 6)));
      
    //two bytes with a gap
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 4) << 8 | RingBuffer_back(buf, 2)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 5) << 8 | RingBuffer_back(buf, 3)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 6) << 8 | RingBuffer_back(buf, 4)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 7) << 8 | RingBuffer_back(buf, 5)));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 8) << 8 | RingBuffer_back(buf, 6)));

    //combination of two bytes
      
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 1) | RingBuffer_back(buf, 5) << 8));
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 4) | RingBuffer_back(buf, 8) << 8));

    //two consecutive bytes
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 8) << 8 | RingBuffer_back(buf, 7))); // in case of text files only useful for silesia/nci
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 9) << 8 | RingBuffer_back(buf, 8))); // silesia/nci

    //two bytes with a gap
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 9) << 8 | RingBuffer_back(buf, 7)));  // silesia/nci
    ContextMap2_set(cm, hash2(++i, RingBuffer_back(buf, 10) << 8 | RingBuffer_back(buf, 8))); // silesia/nci
    assert((int) i == SparseModel_nCM);
  }
  ContextMap2_mix(cm, m);

  //support fixed structures of 4 bytes
  INJECT_SHARED_blockPos
  Mixer_set(m, (blockPos & 3)<<8 | (sm->ctx&0xff), 4 * 256);

}
