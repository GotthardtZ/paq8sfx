#pragma once

#include "APM.h"
#include "APM1.h"
#include "APMPost.h"
#include "Mixer.h"

/**
 * Filter the context model with APMs
 */
// The maps of the two SSE chains (x86/x64 and generic), see SSE_init()
enum {
  SSE_APMS = 3 + 4,  /**< x86/x64: 0..2, generic: 3..6 */
  SSE_APM1S = 2 + 3, /**< x86/x64: 0..1, generic: 2..4 */
  SSE_POSTS = 2 + 2  /**< x86/x64: 0..1, generic: 2..3 */
};

typedef struct SSE {
  Shared* shared;
  APM apm[SSE_APMS];
  APM1 apm1[SSE_APM1S];
  APMPost post[SSE_POSTS];
} SSE;

void SSE_init(SSE* sse, Shared* sh);
uint32_t SSE_p(SSE* sse, uint32_t pr_orig);
