#pragma once

#include "Mixer.h"
#include "SystemDefines.h"

#if defined(X64_SIMD_AVAILABLE) && !defined(AVX2_ONLY)

extern const MixerImpl Mixer_SSE2;

#endif // X64_SIMD_AVAILABLE
