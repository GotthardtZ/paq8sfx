#pragma once

#include "Mixer.h"
#include "SystemDefines.h"

#if defined(X64_SIMD_AVAILABLE) && !defined(AVX2_ONLY)

Mixer* Mixer_SSE2_new(const Shared* sh, int n, int m, int s, int promoted);

#endif // X64_SIMD_AVAILABLE
