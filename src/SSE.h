#pragma once

#include "APM.h"
#include "APM1.h"
#include "APMPost.h"
#include "Mixer.h"

/**
 * Filter the context model with APMs
 */
typedef struct SSE {
  Shared* shared;
  struct {
    APM APMs[3];
    APM1 APM1s[3];
    APMPost APMPostA, APMPostB;
  } x86_64;
  struct {
    APM APMs[4];
    APM1 APM1s[3];
    APMPost APMPostA, APMPostB;
  } Generic;
} SSE;

void SSE_init(SSE* sse, Shared* sh);
void SSE_destroy(SSE* sse);
uint32_t SSE_p(SSE* sse, uint32_t pr_orig);
