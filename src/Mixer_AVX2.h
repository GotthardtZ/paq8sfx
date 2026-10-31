#pragma once

#include "Mixer.h"
#include "SystemDefines.h"

#ifdef X64_SIMD_AVAILABLE

Mixer* Mixer_AVX2_new(const Shared* sh, int n, int m, int s, int promoted);

#endif // X64_SIMD_AVAILABLE
