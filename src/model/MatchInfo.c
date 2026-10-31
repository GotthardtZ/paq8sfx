#include "MatchInfo.h"

bool MatchInfo_isInNoMatchMode(const MatchInfo* const mi) {
  return mi->length == 0 && !mi->delta && mi->lengthBak == 0;
}

bool MatchInfo_isInPreRecoveryMode(const MatchInfo* const mi) {
  return mi->length == 0 && !mi->delta && mi->lengthBak != 0;
}

bool MatchInfo_isInRecoveryMode(const MatchInfo* const mi) {
  return mi->length != 0 && mi->lengthBak != 0;
}

uint32_t MatchInfo_recoveryModePos(const MatchInfo* const mi) {
  assert(MatchInfo_isInRecoveryMode(mi)); //must be in recovery mode
  return mi->length - mi->lengthBak;
}

uint64_t MatchInfo_prio(const MatchInfo* const mi) {
  return
    (uint64_t)(mi->length != 0) << 49 | //normal mode (match)
    (uint64_t)(mi->delta) << 48 | //delta mode
    (uint64_t)(mi->delta ? mi->lengthBak : mi->length) << 32 | //the longer wins
    (uint64_t)(mi->index); //the more recent wins
}

bool MatchInfo_isBetterThan(const MatchInfo* const mi, const MatchInfo* const other) {
  return MatchInfo_prio(mi) > MatchInfo_prio(other);
}

void MatchInfo_update(MatchInfo* const mi, const Shared* const shared, const uint32_t minlen_rm) {
#if 0
  {
    INJECT_SHARED_bpos
    INJECT_SHARED_blockPos
    printf("- pos %d %d  index %d  length %d  lengthBak %d  delta %d\n", blockPos, bpos, mi->index, mi->length, mi->lengthBak, mi->delta ? 1 : 0);
  }
#endif
  INJECT_SHARED_buf
  INJECT_SHARED_bpos
  if (mi->length != 0) {
    const int expectedBit = (mi->expectedByte >> ((8 - bpos) & 7)) & 1;
    INJECT_SHARED_y
    if (y != expectedBit) {
      if (MatchInfo_isInRecoveryMode(mi)) { // another mismatch in recovery mode -> give up
        mi->lengthBak = 0;
        mi->indexBak = 0;
      }
      else { //backup match information: maybe we can recover it just after this mismatch
        mi->lengthBak = mi->length;
        mi->indexBak = mi->index;
        mi->delta = true; //enter into delta mode - for the remaining bits in this byte length will be 0; we will exit delta mode and enter into recovery mode on bpos==0
      }
      mi->length = 0;
    }
  }

  if (bpos == 0) {

    // recover match after a 1-byte mismatch
    if (MatchInfo_isInPreRecoveryMode(mi)) { // just exited delta mode, so we have a backup
      //the match failed 2 bytes ago, we must increase indexBak by 2:
      mi->indexBak++;
      if (mi->lengthBak < 65535) {
        mi->lengthBak++;
      }
      INJECT_SHARED_c1
      if (RingBuffer_at(buf, mi->indexBak) == c1) { // match continues -> recover
        mi->length = mi->lengthBak;
        mi->index = mi->indexBak;
      }
      else { // still mismatch
        mi->lengthBak = mi->indexBak = 0; // purge backup (give up)
      }
    }

    // extend current match
    if (mi->length != 0) {
      mi->index++;
      if (mi->length < 65535) {
        mi->length++;
      }
      if (MatchInfo_isInRecoveryMode(mi) && MatchInfo_recoveryModePos(mi) >= minlen_rm) { // recovery seems to be successful and stable -> exit recovery mode
        mi->lengthBak = mi->indexBak = 0; // purge backup
      }
    }
    mi->delta = false;
  }
#if 0
  {
    INJECT_SHARED_blockPos
    printf("  pos %d %d  index %d  length %d  lengthBak %d  delta %d\n", blockPos, bpos, mi->index, mi->length, mi->lengthBak, mi->delta ? 1 : 0);
  }
#endif
}

void MatchInfo_registerMatch(MatchInfo* const mi, const uint32_t pos, const uint32_t LEN, const uint32_t LEN1) {
  assert(pos != 0);
  mi->length = LEN - LEN1 + 1; // rebase
  mi->index = pos;
  mi->lengthBak = mi->indexBak = 0;
  mi->expectedByte = 0;
  mi->delta = false;
}
