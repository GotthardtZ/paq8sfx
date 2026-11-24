#include "ContextMap2.h"
#include "Array.h"
#include "Utils.h"

static void updatePendingContextsInSlot(ContextMap2* const cm, HashElementForContextMap* const p, uint32_t c) {
  // in case of a collision updating (mixing) is slightly better (but slightly slower) then resetting, so we update
  StateTable_update(&p->slot1and2.bitState,  (c >> 2) & 1, &cm->rnd);
  StateTable_update(&p->slot1and2.bitState0  + ((c >> 2) & 1), (c >> 1) & 1, &cm->rnd);
  StateTable_update(&p->slot1and2.bitState00 + ((c >> 1) & 3), (c & 1), &cm->rnd);
}

static void updatePendingContexts(ContextMap2* const cm, uint32_t ctx, uint16_t checksum, uint32_t c) {
  // update pending bit histories for bits 2, 3, 4 (in slot 1)
  HashElementForContextMap* const p1A = Bucket16_find(&cm->hashTable[(ctx + (c >> 6)) & cm->mask], checksum, &cm->rnd);
  updatePendingContextsInSlot(cm, p1A, c >> 3);
  // update pending bit histories for bits 5, 6, 7 (in slot 2)
  HashElementForContextMap* const p1B = Bucket16_find(&cm->hashTable[(ctx + (c >> 3)) & cm->mask], checksum, &cm->rnd);
  updatePendingContextsInSlot(cm, p1B, c);
}

static size_t getStateByteLocation(const uint32_t bpos, const uint32_t c0) {
  uint32_t pis = 0; //state byte position in slot
#if 0
  // this version is for readability
  switch (bpos) {
  case 0: //slot0
    pis = 0;
    break;
  case 1: //slot0
    pis = 1 + (c0 & 1);
    break;
  case 2: //slot1
    pis = 0;
    break;
  case 3: //slot1
    pis = 1 + (c0 & 1);
    break;
  case 4: //slot1
    pis = 3 + (c0 & 3);
    break;
  case 5: //slot2
    break;
  case 6: //slot2
    pis = 1 + (c0 & 1);
    break;
  case 7: //slot2
    pis = 3 + (c0 & 3);
    break;
  }
#else
  // this is a speed optimized (branchless) version of the above
  const uint32_t smask = (UINT32_C(0x31031010) >> (bpos << 2)) & 0x0F;
  pis = smask + (c0 & smask);
#endif

  return pis;
}

static void ContextMap2_update(IPredictor* const predictor) {
  ContextMap2* const cm = (ContextMap2*)predictor;
  const Shared* const shared = cm->shared;
  INJECT_SHARED_y
  INJECT_SHARED_bpos
  INJECT_SHARED_c1
  INJECT_SHARED_c0
  for( uint32_t i = 0; i < cm->index; i++ ) {
    ContextInfo* contextInfo = &cm->contextInfoList[i];
    if((contextInfo->flags & CM_SKIPPED_CONTEXT) == 0) {

      size_t stateIndex = getStateByteLocation((bpos - 1) & 7, (bpos == 0 ? c1 + 256u : c0) >> 1);
      uint8_t* pState = &contextInfo->hashElementInSlot012->states[stateIndex];
      StateTable_update(pState, y, &cm->rnd);

      HashElementForContextMap* const slot0e = contextInfo->hashElementInSlot0;

      ASSUME(bpos <= 7);

      if (bpos == 0) {
        // update byte history and run statistics
        if (slot0e->slot0.bitState < 3) {
          slot0e->slot0.byte3 = slot0e->slot0.byte2 = slot0e->slot0.byte1 = c1;
          slot0e->slot0.runcount = 1;
        }
        else {
          const bool isMatch = slot0e->slot0.byte1 == c1;
          if (isMatch) {
            uint8_t runCount = slot0e->slot0.runcount;
            if (runCount < 255) {
              slot0e->slot0.runcount = runCount + 1;
            }
          }
          else {
            // shift byte candidates
            slot0e->slot0.runcount = 1;
            slot0e->slot0.byte3 = slot0e->slot0.byte2;
            slot0e->slot0.byte2 = slot0e->slot0.byte1;
            slot0e->slot0.byte1 = c1; //last byte seen
          }
        }
      }
      else if( bpos==2 || bpos==5 ) {
        if (contextInfo->flags & CM_DEFERRED) { //when in deferred mode...
          // ...reconstruct bit states in temporary location from last seen bytes 

          memset(&contextInfo->bitStateTmp, 0, 7);
          contextInfo->hashElementInSlot012 = &contextInfo->bitStateTmp;
          
          const uint8_t bit0state = slot0e->slot0.bitState;
          if (bit0state >= 3) { // at least 1 byte was seen
            // the bytes seen (oldest first): byte2 and byte1 if at least 2 bytes were seen, otherwise byte1
            const int seen2 = bit0state >= 7;
            const uint8_t* const bytes = &slot0e->slot0.byte1; // byte1, byte2
            const uint8_t mask = ((1 << bpos) - 1);
            const int shift = 8 - bpos;
            for (int k = seen2; k >= 0; k--) {
              const uint8_t b = bytes[k];
              if (((c0 ^ (b >> shift)) & mask) == 0) { // last 2/5 bits must match otherwise it's not the current slot location
                updatePendingContextsInSlot(cm, &contextInfo->bitStateTmp, b >> (shift - 3)); // simulate the current states at the temporary location
              }
            }
          }
        }
        else {
          //when pbos==2: switch from slot 0 to slot 1
          //when bpos==5: switch from slot 1 to slot 2
          const uint32_t ctx = contextInfo->tableIndex;
          const uint16_t chk = contextInfo->tableChecksum;
          contextInfo->hashElementInSlot012 = Bucket16_find(&cm->hashTable[(ctx + c0) & cm->mask], chk, &cm->rnd);
        }
      }
    }
  }
  if( bpos == 0 ) {
    cm->index = 0;
  } // start over
}

// The StateMaps (see ContextMapMaps): contexts per set (bits), limit, a-priori type
static const struct { uint8_t bits; uint16_t limit; uint8_t type; } mapSetup[CM_MAPS] = {
  { 12, 127, STATEMAP_RUN },        // 63-255
  {  8, 511, STATEMAP_BITHISTORY }, // 511-1023
  {  8, 511, STATEMAP_GENERIC },    // 511-1023
  { 12, 511, STATEMAP_GENERIC },    // 255-1023
};

void ContextMap2_init(ContextMap2* const cm, const Shared* const sh, const uint64_t size, const uint32_t contexts, const int scale) {
  cm->predictor.update = ContextMap2_update;
  cm->shared = sh;
  cm->C = contexts;
  cm->contextInfoList = ARRAY_NEW(ContextInfo, contexts);
  cm->hashTable = ARRAY_NEW(Bucket16, size >> 6);
  for (int k = 0; k < CM_MAPS; k++) {
    StateMap_init(&cm->maps[k], sh, contexts, 1 << mapSetup[k].bits, mapSetup[k].limit, (StateMapType)(mapSetup[k].type));
  }
  cm->index = 0;
  cm->mask = (uint32_t)((size >> 6) - 1);
  cm->hashBits = ilog2(cm->mask + 1);
  cm->scale = scale;
  assert(size >= 64 && isPowerOf2(size));
}

void ContextMap2_set(ContextMap2* const cm, const uint64_t contexthash) {
  assert(cm->index < cm->C);
  ContextInfo *contextInfo = &cm->contextInfoList[cm->index];
  const uint32_t ctx = contextInfo->tableIndex = finalize64(contexthash, cm->hashBits);
  const uint16_t chk = contextInfo->tableChecksum = checksum16(contexthash, cm->hashBits);
  HashElementForContextMap* const hashElementInSlot0 = Bucket16_find(&cm->hashTable[ctx], chk, &cm->rnd);
  contextInfo->hashElementInSlot0 = hashElementInSlot0;
  contextInfo->hashElementInSlot012 = hashElementInSlot0;

  uint8_t ctxflags = 0;
  if (hashElementInSlot0->slot0.bitState <= 6) { // while constructing statistics for the first 3 bytes (states: 0; 1-2; 3-6) defer updating bit statistics in slot1 and slot2
    ctxflags |= CM_DEFERRED;
  }
  else if (hashElementInSlot0->slot0.bitState <= 14) { // the first 3 bytes in this context are now known, it's time to update pending bit histories
    // replay the last 3 bytes, oldest first: byte3, byte2, byte1
    // (with a run count of 2: byte2, byte1, byte1)
    const uint8_t* const bytes = &hashElementInSlot0->slot0.byte1; // byte1, byte2, byte3
    const int run2 = hashElementInSlot0->slot0.runcount == 2;
    for (int k = 2; k >= 0; k--) {
      updatePendingContexts(cm, ctx, chk, bytes[k - (run2 && k > 0)] + 256);
    }
  }
  
  contextInfo->flags = ctxflags;
  cm->index++;
}


void ContextMap2_skip(ContextMap2* const cm) {
  assert(cm->index < cm->C);
  cm->contextInfoList[cm->index].flags = CM_SKIPPED_CONTEXT;
  cm->index++;
}

void ContextMap2_setScale(ContextMap2* const cm, const int Scale) { cm->scale = Scale; }

void ContextMap2_mix(ContextMap2* const cm, Mixer* const m) {
  const Shared* const shared = cm->shared;
  UpdateBroadcaster_subscribe(Shared_getUpdateBroadcaster(shared), &cm->predictor);
  for (int k = 0; k < CM_MAPS; k++) {
    StateMap_subscribe(&cm->maps[k]);
  }

  const int scale = cm->scale;
  cm->order = 0;
  INJECT_SHARED_bpos
  INJECT_SHARED_c0
  for( uint32_t i = 0; i < cm->index; i++ ) {
    ContextInfo* contextInfo = &cm->contextInfoList[i];
    // the mixer inputs of this context: run map, state map (4), bhMap8B, bhMap12B;
    // an input that is not computed (skipped context, unknown state) is 0
    int in[ContextMap2_MIXERINPUTS] = { 0 };
    if((contextInfo->flags & CM_SKIPPED_CONTEXT) != 0 ) {
      for (int k = 0; k < CM_MAPS; k++) {
        StateMap_skip(&cm->maps[k], i);
      }
    } else {
      const HashElementForContextMap* const slot0e = contextInfo->hashElementInSlot0;
      uint8_t* pState = &contextInfo->hashElementInSlot012->states[0] + getStateByteLocation(bpos, c0);
      const int state = *pState;
      const int n0 = StateTable_next(state, 2);
      const int n1 = StateTable_next(state, 3);
      const int bitIsUncertain = (int)(n0 != 0 && n1 != 0);

      // predict from last byte(s) in context
      uint8_t byteState = slot0e->slot0.bitState;
      const bool complete1 = (byteState >= 3) || (byteState >= 1 && bpos == 0);
      const bool complete2 = (byteState >= 7) || (byteState >= 3 && bpos == 0);
      const bool complete3 = (byteState >= 15) || (byteState >= 7 && bpos == 0);
      const int bp = (UINT32_C(0x33322210) >> (bpos << 2)) & 0xF; // {bpos:0}->0  {bpos:1}->1  {bpos:2,3,4}->2  {bpos:5,6,7}->3
      if( complete1 && ((slot0e->slot0.byte1 + 256u) >> (8 - bpos)) == c0 ) { // 1st candidate (last byte seen) matches
        const int predictedBit = (slot0e->slot0.byte1 >> (7 - bpos)) & 1;
        const int byte1IsUncertain = (int)(slot0e->slot0.byte2 != slot0e->slot0.byte1);
        const int runCount = slot0e->slot0.runcount; // 1..255
        in[0] = stretch(StateMap_p2(&cm->maps[CM_RUNMAP], i, runCount << 4 | bp << 2 | byte1IsUncertain << 1 | predictedBit)) >> (1 + byte1IsUncertain);
      } else if( complete1 && complete2 && ((slot0e->slot0.byte2 + 256u) >> (8 - bpos)) == c0 ) { // 2nd candidate matches
        const int predictedBit = (slot0e->slot0.byte2 >> (7 - bpos)) & 1;
        const int byte2IsUncertain = (int)(slot0e->slot0.byte3 != slot0e->slot0.byte2);
        in[0] = stretch(StateMap_p2(&cm->maps[CM_RUNMAP], i, bitIsUncertain << 1 | predictedBit)) >> (2 + byte2IsUncertain);
      } else {
        // remark: considering the 3rd byte is not beneficial in most cases, except for some 8bpp images
        StateMap_skip(&cm->maps[CM_RUNMAP], i);
      }

      // predict from bit context
      if( state == 0 ) {
        StateMap_skip(&cm->maps[CM_STATEMAP], i);
      } else {
        const int p1 = StateMap_p2(&cm->maps[CM_STATEMAP], i, state);
        const int st = (stretch(p1) * scale) >> 8;
        const int contextIsYoung = (int)(state <= 2);
        in[1] = st >> contextIsYoung;
        in[2] = ((p1 - 2048) * scale) >> 9;
        in[3] = (bitIsUncertain - 1) & st; // when both counts are nonzero add(0) otherwise add(st)
        const int p0 = 4095 - p1;
        in[4] = (((p1 & (-!n0)) - (p0 & (-!n1))) * scale) >> 10;
        cm->order++;
      }

      const int bhBits =
        (((slot0e->slot0.byte1 >> (7 - bpos)) & 1)) |
        (((slot0e->slot0.byte2 >> (7 - bpos)) & 1) << 1) |
        (((slot0e->slot0.byte3 >> (7 - bpos)) & 1) << 2);

      int bhState = 0; // 4 bits
      if( complete3 ) {
        bhState = 8 | (bhBits); //we have seen 3 bytes (at least)
      } else if( complete2 ) {
        bhState = 4 | (bhBits & 3); //we have seen 2 bytes
      } else if( complete1 ) {
        bhState = 2 | (bhBits & 1); //we have seen 1 byte only
      }
      //else new context (bhState=0)

      const uint8_t stateGroup = StateTable_group(state); //0..31
      in[5] = stretch(StateMap_p2(&cm->maps[CM_BHMAP8B], i, bitIsUncertain << 7 | (bhState << 3) | bpos)) >> 2; // using bitIsUncertain is generally beneficial except for some 8bpp image (noticeable loss)
      in[6] = stretch(StateMap_p2(&cm->maps[CM_BHMAP12B], i, stateGroup << 7 | (bhState << 3) | bpos)) >> 2;
    }
    for (int k = 0; k < ContextMap2_MIXERINPUTS; k++) {
      Mixer_add(m, in[k]);
    }
  }
}
