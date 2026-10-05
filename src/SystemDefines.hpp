#pragma once

// User-controlled build settings

// set by the build scripts: either FULL or SFX
//#define FULL
//#define SFX

#if defined(FULL) && (defined SFX)
#error
#endif

#if !defined(FULL) && !defined(SFX)
#error
#endif

#define SFX_SILENT // remove if you want to see error messages from the SFX module
#define AVX2_ONLY // remove of you don't have AVX2


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
constexpr bool IS_X64_SIMD_AVAILABLE = true;
#include <immintrin.h> // AVX2
#else
constexpr bool IS_X64_SIMD_AVAILABLE = false;
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
#warning MSVC floating-point mode unclear. Ensure /fp:precise or /fp:strict is set
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

// Platform-specific includes
#ifdef WINDOWS

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>   //CreateDirectoryW, CommandLineToArgvW, GetConsoleOutputCP, SetConsoleOutputCP
//GetCommandLineW, GetModuleFileNameW, GetStdHandle, GetTempFileName
//MultiByteToWideChar, WideCharToMultiByte,
//FileType, FILE_TYPE_PIPE, FILE_TYPE_DISK,
//uRetVal, DWORD, UINT, TRUE, MAX_PATH, CP_UTF8, etc.
#endif

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cstdio>
// Determining the proper printf() format specifier for 64 bit unsigned integers:
// - on Windows MSVC and MinGW-w64 use the MSVCRT runtime where it is "%I64u"
// - on Linux it is "%llu"
// The correct value is provided by the PRIu64 macro which is defined here:
#include <cinttypes> //PRIu64

// Platform-specific includes

#ifdef UNIX
#include <cerrno>  //errno
#include <climits> //PATH_MAX (for OSX)
#include <cstring> //strlen(), strcpy(), strcat(), strerror(), memset(), memcpy(), memmove()
#include <unistd.h> //isatty()
#endif

#ifdef _MSC_VER
#define fseeko(a, b, c) _fseeki64(a, b, c)
#define ftello(a) _ftelli64(a)
#else
#ifndef UNIX
#ifndef fseeko
#define fseeko(a, b, c) fseeko64(a, b, c)
#endif
#ifndef ftello
#define ftello(a) ftello64(a)
#endif
#endif
#endif

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif

#ifdef FULL
// Error handler: print message if any, and exit
[[noreturn]] static void quit(const char* const message = nullptr) {
  if (message != nullptr) {
    printf("\n%s", message);
  }
  printf("\n");
  exit(1);
}
#else
// Error handler: exit silently
[[noreturn]] static void quit(const char* const message = nullptr) {
  exit(1);
}
#endif



