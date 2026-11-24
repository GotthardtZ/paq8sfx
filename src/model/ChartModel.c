#include "ChartModel.h"
#include "../BlockType.h"

///////////////////////////// chartModel ////////////////////////////

// The second to last byte ((c4>>8)&0xff) decides which slot in the chart to place
// the context which consists of selected bits from the last 3 bytes.
// Note: some of the (indirect) contexts below are independent from the chart model 

void ChartModel_init(ChartModel* const cmodel, const Shared* const sh, const uint64_t size) {
  cmodel->shared = sh;
  ContextMap2_init(&cmodel->cm, sh, size, ChartModel_nCM, 64);
}

// The selected bits of the sparse indirect bit contexts: 3 fields each
static const uint8_t chartBitShifts[3][3] = { { 15, 10, 5 }, { 12, 7, 2 }, { 10, 4, 0 } };
static const uint16_t chartBitMasks[3][3] = { { 0x1c0, 0x38, 0x07 }, { 0x1c0, 0x38, 0x07 }, { 0x30, 0x0c, 0x03 } };

void ChartModel_mix(ChartModel* const cmodel, Mixer* const m) {
  const Shared* const shared = cmodel->shared;
  ContextMap2* const cm = &cmodel->cm;
  INJECT_SHARED_bpos
  if (bpos == 0) {
    
    INJECT_SHARED_c4
    uint32_t w, w0, c1, w2, w3, w4;
    c1 = c4 & 0xff;
    w = c4 & 0x0000ffff;
    w0 = c4 & 0x00ffffff;
    w4 = w0 << 8;
    w2 = w4 & 0x0000ff00;

    INJECT_SHARED_blockType
    w3 = w4 & 0x00ff0000;
    w4 = w4 & 0xff000000;

    uint32_t a0 = (c4 >> 5) & 0x00070707; // bits 0-2 (top 3 bits)
    uint64_t h = 0; // context index for hash

    // sparse indirect bit contexts: 9 selected bits from 3 bytes,
    // f from c4&0xffffff00 (the bytes before the last one), e from c4&0x00ffffff
    const uint32_t wsel[4] = { 0, w2, w3, w4 };
    for (int i = 0; i < 3; i++) {
      uint32_t f = 0, e = 0;
      for (int k = 0; k < 3; k++) {
        const uint32_t mask = chartBitMasks[i][k];
        const int shift = chartBitShifts[i][k];
        f |= ((c4 >> 8) >> shift) & mask;
        e |= (c4 >> shift) & mask;
      }
      const uint8_t g = cmodel->indirect1[i << 9 | e]; // context from indirect
      cmodel->indirect1[i << 9 | f] = c1; // update indirect
      for (int k = 0; k < 4; k++) {
        ContextMap2_set(cm, hash2(++h, wsel[k] | g));
      }
    }

    const uint8_t buf2 = (c4 >> 8) & 0xff;
    const uint8_t buf3 = (c4 >> 16) & 0xff;

    //sparse indirect 1-byte contexts
    uint8_t g = cmodel->indirect2[c1]; // context from indirect
    cmodel->indirect2[buf2] = c1; // update indirect
    ContextMap2_set(cm, hash2(++h, w4 | g));

    //sparse indirect 2-byte contexts
    g = cmodel->indirect3[buf2 << 8 | c1]; // context from indirect
    cmodel->indirect3[buf3 << 8 | buf2] = c1; // update indirect
    ContextMap2_set(cm, hash2(++h, w4 | g));
    ContextMap2_set(cm, hash2(++h, w3 | g));

    uint32_t
      a[3] = { // content: 3 selected bits of 3 consecutive bytes that is
            a0 , //charGroup (text) or top 3 bits (binary)
            (c4 >> 2) & 0x00070707, //bits 3-5 (3 bits)
            c4 & 0x00070707 }; // bits 5-7 (bottom 3 bits)

    uint32_t cnt = 3;
    
    //update chart
    for (uint32_t i = 0; i < cnt; i++) {
      uint32_t e = a[i];
      cmodel->chart[i << 3 | ((e >> 8) & 7)] = w0;
    }

    cnt *= 8; // the chart has 8+8+8 slots: for text blocks we use only the first 8, for binary blocks we use all the 24 

    //chart model
    for (uint32_t i = 0; i < cnt; i++) {
      uint32_t s = i >> 3;   // selector: which bits are selected: s: 0 for text; 0..2 for binary
      uint32_t e = a[s];     // content: 3 selected bits from 3 consecutive bytes
      uint32_t k = cmodel->chart[i]; // i: text: 0..7; binary: 0..23
                                                              //   k   e
      ContextMap2_set(cm, hash2(++h, k));                               //  111 000
      ContextMap2_set(cm, hash3(++h, e, k & 0xffff));                   //  011 111
      ContextMap2_set(cm, hash2(++h, (e & 7) << 8 | (k & 0xff00ff)));   //  101 001
    }

    assert((int)h == ChartModel_nCM);
  }
  
  ContextMap2_mix(cm, m);
}
