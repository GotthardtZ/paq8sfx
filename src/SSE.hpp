#pragma once

#include "APM.hpp"
#include "APM1.hpp"
#include "APMPost.hpp"
#include "Mixer.hpp"

/**
 * Filter the context model with APMs
 */
class SSE {
private:
  Shared * const shared;
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

public:
  explicit SSE(Shared* const sh);
  uint32_t p(uint32_t pr_orig);
};
