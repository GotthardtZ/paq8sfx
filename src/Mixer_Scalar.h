#pragma once

#include "SystemDefines.h"
#if !defined(AVX2_ONLY) && !defined(X64_SIMD_AVAILABLE)

#include "Mixer.h"

extern const MixerImpl Mixer_Scalar;

#endif
