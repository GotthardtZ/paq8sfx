#pragma once

#include "../Shared.h"

typedef struct MatchInfo {
  uint32_t length;
  uint32_t index;
  uint32_t lengthBak;
  uint32_t indexBak;
  uint8_t expectedByte;
  bool delta;
} MatchInfo;

bool MatchInfo_isInNoMatchMode(const MatchInfo* mi);
bool MatchInfo_isInPreRecoveryMode(const MatchInfo* mi);
bool MatchInfo_isInRecoveryMode(const MatchInfo* mi);
uint32_t MatchInfo_recoveryModePos(const MatchInfo* mi);
uint64_t MatchInfo_prio(const MatchInfo* mi);
bool MatchInfo_isBetterThan(const MatchInfo* mi, const MatchInfo* other);

void MatchInfo_update(MatchInfo* mi, const Shared* shared, uint32_t minlen_rm);
void MatchInfo_registerMatch(MatchInfo* mi, uint32_t pos, uint32_t LEN, uint32_t LEN1);
