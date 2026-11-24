#pragma once

#include "SystemDefines.h"
#include "SIMDType.h"

///////////////////////// SIMD Vectorization detection //////////////////////////////////

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define HAS_CPUID 1
#endif

#ifdef HAS_CPUID
#if defined(_MSC_VER)
#include <intrin.h>
#define cpuid(info, x) __cpuidex(info, x, 0)
#elif defined(__GNUC__) || defined(__clang__)
#include <x86intrin.h>
#include <cpuid.h>
#define cpuid(info, x) __cpuid_count(x, 0, (info)[0], (info)[1], (info)[2], (info)[3])
#endif
#endif

// Define interface to xgetbv instruction
#ifdef HAS_CPUID
static inline unsigned long long xgetbv(unsigned long ctr) {
#if (defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 160040000) || (defined(__INTEL_COMPILER) && __INTEL_COMPILER >= 1200)
  return _xgetbv(ctr);
#elif (defined(__GNUC__) || defined(__clang__))
  uint32_t a = 0;
  uint32_t d;
  __asm("xgetbv"
  : "=a"(a), "=d"(d)
  : "c"(ctr)
  :);
  return a | (((uint64_t)d) << 32);
#else
#error Unknown compiler
#endif
}
#endif

/* Returns the best SIMD code path the program has for this CPU (and OS):
SIMD_AVX2, SIMD_SSE41, or SIMD_SSE2, which is part of every x64 CPU.
On other systems there is no SIMD code path: SIMD_NONE.
*/
static inline SIMDType simdDetect(void) {
#ifdef X64_SIMD_AVAILABLE
  int cpuidResult[4] = {0, 0, 0, 0};
  cpuid(cpuidResult, 1); // call cpuid function 1 ("Processor Info and Feature Bits")
  if ((cpuidResult[2] & (1 << 19)) == 0) {
    return SIMD_SSE2; // no SSE4.1
  }
  // AVX needs OSXSAVE (bit 27: XGETBV is available), AVX (bit 28), and the OS must save the AVX state
  if ((cpuidResult[2] & (3 << 27)) != (3 << 27) || (xgetbv(0) & 6) != 6) {
    return SIMD_SSE41;
  }
  cpuid(cpuidResult, 7); // call cpuid function 7 ("Extended Feature Bits")
  if ((cpuidResult[1] & (1 << 5)) == 0) {
    return SIMD_SSE41; // no AVX2
  }
  return SIMD_AVX2;
#else
  return SIMD_NONE;
#endif
}
