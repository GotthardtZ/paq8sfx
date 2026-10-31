#pragma once

#include "SystemDefines.h"

/**
 * ilog(x) = round(log2(x) * 16), 0 <= x < 64K
 * Ilog_init() must be called once before the first Ilog_log() or llog() call
 * (Shared_init() does that).
 */
void Ilog_init(void);
int Ilog_log(uint16_t x);

/**
 * llog(x) accepts 32 bits
 * @param x
 * @return
 */
int llog(uint32_t x);
