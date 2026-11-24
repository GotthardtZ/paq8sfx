#pragma once

#include "Mixer.h"
#include "Shared.h"
#include "Mixer_Scalar.h"
#include "Mixer_SSE2.h"
#include "Mixer_AVX2.h"

/**
 * Creates a mixer with the best SIMD implementation for the CPU
 * (shared->chosenSimd). See Mixer.h for the parameters; promoted is the
 * number of extra inputs of the second layer (see Mixer_promote()).
 * Everything it allocates is freed by Array_deleteAll().
 */
Mixer* MixerFactory_createMixer(const Shared* sh, int n, int m, int s, int promoted);
