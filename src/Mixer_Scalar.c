#include "Mixer_Scalar.h"

// Not on x64: SSE2 is part of every x64 CPU (the rest of the program
// is compiled for it too), so the SSE2 mixer is always available there.
#if !defined(AVX2_ONLY) && !defined(X64_SIMD_AVAILABLE)

#define SIMD_WIDTH_SCALAR (4 / sizeof(short)) // processes 2 shorts at once

static int Mixer_Scalar_dotProduct(const Mixer* const self, const short* const w, const size_t n) {
  const short* const tx = self->tx;
  int sum = 0;
  for (size_t i = 0; i < n; i += 2) {
    sum += (tx[i] * w[i] + tx[i + 1] * w[i + 1]) >> 8;
  }
  return sum;
}

static void Mixer_Scalar_train(const Mixer* const self, short* const w, const size_t n, const int e) {
  const short* const tx = self->tx;
  for (size_t i = 0; i < n; i++) {
    int wt = w[i] + ((((tx[i] * e * 2) >> 16) + 1) >> 1);
    if (wt < -32768) { wt = -32768; }
    else if (wt > 32767) { wt = 32767; }
    w[i] = (short)wt;
  }
}

const MixerImpl Mixer_Scalar = {
  (int)SIMD_WIDTH_SCALAR,
  Mixer_Scalar_dotProduct,
  Mixer_Scalar_train
};

#endif
