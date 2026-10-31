#pragma once

//////////////////////// Build settings ////////////////////////////////////
//
// The build (build scripts, Makefile, Visual Studio project) defines exactly
// one of these for every source file:
//   FULL   builds paq8sfx, the compressor
//   SFX    builds the stub, the self-extractor
//
// and optionally, for the stub:
//   AVX2_ONLY          keep only the AVX2 code path: a smaller stub that needs
//                      a CPU with AVX2 (default: every code path is compiled
//                      in and one is selected at run time)
//   SFX_EXTRACT_ONLY   build the extract-only stub
//   SFX_FREESTANDING   build the libc-free stub (see src/platform/)
//
// See "Build settings" in README.md.

#if defined(FULL) && defined(SFX)
#error Define either FULL or SFX, not both
#endif

#if !defined(FULL) && !defined(SFX)
#error Define FULL (to build paq8sfx) or SFX (to build the stub)
#endif

#if defined(SFX_FREESTANDING) && !defined(SFX)
#error SFX_FREESTANDING is a setting of the stub: define SFX as well
#endif

// The only setting that is made here: with SFX_SILENT the stub prints no
// messages of its own. Comment it out to see what the stub is doing and what
// went wrong.
#define SFX_SILENT


//////////////////////// Target OS/Compiler ////////////////////////////////

#if defined(_WIN32) || defined(_MSC_VER)
#ifndef WINDOWS
#define WINDOWS  //to compile for Windows
#endif
#endif

#if defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
#ifndef UNIX
#define UNIX //to compile for Unix, Linux, Solaris, MacOS / Darwin, etc)
#endif
#endif

#if !defined(WINDOWS) && !defined(UNIX)
#error Unknown target system
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define X64_SIMD_AVAILABLE
#include <immintrin.h> // AVX2
#endif


// Floating point operations need IEEE compliance
// Do not use unsafe compiler optimization options
// gcc : -ffast-math (and -Ofast, -funsafe-math-optimizations, -fno-rounding-math) => __FAST_MATH__
// MSVC: /fp:fast => _M_FP_FAST
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
#error Avoid using aggressive floating-point compiler optimization flags
#endif

// MSVC: Verify we're in a safe floating-point mode
#if defined(_MSC_VER)
#if !defined(_M_FP_PRECISE) && !defined(_M_FP_STRICT)
#pragma message("MSVC floating-point mode unclear. Ensure /fp:precise or /fp:strict is set")
#endif
#endif

#if defined(__clang__)
#define ASSUME(cond) do { assert(cond); __builtin_assume(cond); } while (0)
#elif defined(_MSC_VER)
#define ASSUME(cond) do { assert(cond); __assume(cond); } while (0)
#elif defined(__GNUC__)
#define ASSUME(cond) do { assert(cond); if (!(cond)) __builtin_unreachable(); } while (0)
#else
#define ASSUME(cond) assert(cond)
#endif

// A function that does not return
#if defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((noreturn))
#endif

// Lets a function use instructions of a newer instruction set than the rest
// of the program (gcc, clang). MSVC needs no such marking.
#if defined(__GNUC__) || defined(__clang__)
#define TARGET(isa) __attribute__((target(isa)))
#else
#define TARGET(isa)
#endif


//////////////////////// Includes //////////////////////////////////////////

#ifdef WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   //CreateDirectoryW, GetModuleFileNameW, MultiByteToWideChar, DWORD, MAX_PATH, CP_UTF8, etc.
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> //exit()
#include <string.h> //strlen(), strcspn(), strerror(), memset(), memcpy(), memmove()
#include <errno.h>  //errno
// Determining the proper printf() format specifier for 64 bit unsigned integers:
// - on Windows MSVC and MinGW-w64 use the MSVCRT runtime where it is "%I64u"
// - on Linux it is "%llu"
// The correct value is provided by the PRIu64 macro which is defined here:
#include <inttypes.h> //PRIu64

#ifdef UNIX
#include <limits.h> //PATH_MAX (for OSX)
#include <unistd.h> //execv()
#endif

// Some C libraries define min() and max() as macros (Windows headers without
// NOMINMAX, the MinGW-w64 and Microsoft <stdlib.h>). This program has its own
// min() and max() functions, see Utils.h.
#undef min
#undef max


//////////////////////// Differences between C libraries ///////////////////

#ifdef WINDOWS
// Seeking in files larger than 2 GB: fseeko() and ftello() are called
// _fseeki64() and _ftelli64() in the Microsoft C library (MSVC and MinGW-w64).
#undef fseeko
#undef ftello
#define fseeko(file, offset, whence) _fseeki64(file, offset, whence)
#define ftello(file) _ftelli64(file)

#define strcasecmp _stricmp
#endif

#ifdef SFX_FREESTANDING
// The libc-free stub has no console output. These macros remove every
// printf(), fprintf() and fflush() call from it; their arguments are not
// evaluated. (Defined after <stdio.h>, so the declarations are not affected.)
#define printf(...)  ((void)0)
#define fprintf(...) ((void)0)
#define fflush(...)  ((void)0)
#endif


//////////////////////// Error handler /////////////////////////////////////

#ifdef FULL
// Print the message (if any) and exit
NORETURN static inline void quit(const char* const message) {
  if (message != NULL) {
    printf("\n%s", message);
  }
  printf("\n");
  exit(1);
}
#else
// The stub exits silently
NORETURN static inline void quit(const char* const message) {
  (void)message;
  exit(1);
}
#endif
