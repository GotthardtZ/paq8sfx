#pragma once

#if !defined(AVX2_ONLY)

#include "Mixer.h"

Mixer* Mixer_Scalar_new(const Shared* sh, int n, int m, int s, int promoted);

#endif
