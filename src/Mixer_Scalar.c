#include "Mixer_Scalar.h"

#if !defined(AVX2_ONLY)

#define SIMD_WIDTH_SCALAR (4 / sizeof(short)) // processes 2 shorts at once

static int Mixer_Scalar_dotProduct(const Mixer* const self, const short* const w, const size_t n) {
  const short* const tx = self->tx;
  int sum = 0;
  for (size_t i = 0; i < n; i += 2) {
    sum += (tx[i] * w[i] + tx[i + 1] * w[i + 1]) >> 8;
  }
  return sum;
}

static int Mixer_Scalar_dotProduct2(const Mixer* const self, const short* const w0, const short* const w1, const size_t n, int* const sum1) {
  const short* const tx = self->tx;
  int s0 = 0;
  int s1 = 0;
  for (size_t i = 0; i < n; i += 2) {
    const int t0 = tx[i];
    const int t1 = tx[i + 1];
    s0 += (t0 * w0[i] + t1 * w0[i + 1]) >> 8;
    s1 += (t0 * w1[i] + t1 * w1[i + 1]) >> 8;
  }
  *sum1 = s1;
  return s0;
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

Mixer* Mixer_Scalar_new(const Shared* const sh, const int n, const int m, const int s, const int promoted) {
  Mixer* const mixer = (Mixer*)calloc(1, sizeof(Mixer));
  if (mixer == NULL) {
    quit("Out of memory.");
  }
  Mixer_init(mixer, sh, n, m, s, (int)SIMD_WIDTH_SCALAR);
  mixer->dotProduct = Mixer_Scalar_dotProduct;
  mixer->dotProduct2 = Mixer_Scalar_dotProduct2;
  mixer->train = Mixer_Scalar_train;
  if (s > 1) {
    mixer->mp = Mixer_Scalar_new(sh, s + promoted, 1, 1, 0);
  }
  return mixer;
}

#endif
