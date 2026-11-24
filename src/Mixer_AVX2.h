#pragma once

#include "Mixer.h"
#include "SystemDefines.h"

#ifdef X64_SIMD_AVAILABLE

extern const MixerImpl Mixer_AVX2;

#endif // X64_SIMD_AVAILABLE
