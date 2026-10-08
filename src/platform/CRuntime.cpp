// The C library of the libc-free stub.
//
// These are the C and C++ library functions the stub calls, and nothing more.
// They are the same for every operating system: everything they need from it
// goes through the plat_* functions of Platform.hpp.
//
// The functions are as simple as they can be, not as fast as they can be:
// files are not buffered, and every malloc asks the operating system for
// memory. The stub spends its time in the model, not here.
//
// This file must be compiled without link-time optimization and, with GCC
// and clang, with -fno-builtin (the Makefile, the build scripts and the
// Visual Studio project do that). Otherwise the compiler may replace the
// body of memcpy by a call to memcpy.

#include "Platform.hpp"

#if defined(SFX) && defined(SFX_FREESTANDING)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if defined(_MSC_VER)
#  include <intrin.h>
// MSVC normally expands these itself; this allows defining them here.
#  pragma function(memcpy, memset, memmove, memcmp, strlen, strcmp)
#endif

extern "C" {

// ------------------------------------------------------ memory and strings --
// memcpy, memset, memmove and memcmp must exist even if no source file calls
// them: the compiler generates calls to them on its own, for example to copy
// a struct or to clear an array.

// memcpy and memset are a single CPU instruction ("rep movsb", "rep stosb").
void* memcpy(void* dst, const void* src, size_t n) {
#if defined(_MSC_VER)
  __movsb((unsigned char*)dst, (const unsigned char*)src, n);
#else
  void* d = dst;
  __asm__ volatile("rep movsb" : "+D"(d), "+S"(src), "+c"(n) : : "memory");
#endif
  return dst;
}

void* memset(void* dst, int value, size_t n) {
#if defined(_MSC_VER)
  __stosb((unsigned char*)dst, (unsigned char)value, n);
#else
  void* d = dst;
  __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(value) : "memory");
#endif
  return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dst;
  const unsigned char* s = (const unsigned char*)src;
  if (d <= s || d >= s + n) return memcpy(dst, src, n);  // copying forwards is safe
  // The areas overlap: copy backwards. "volatile" keeps the compiler from
  // turning this loop into a call to memmove.
  volatile unsigned char* out = d;
  while (n-- > 0) out[n] = s[n];
  return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
  const unsigned char* x = (const unsigned char*)a;
  const unsigned char* y = (const unsigned char*)b;
  for (size_t i = 0; i < n; ++i) {
    if (x[i] != y[i]) return x[i] - y[i];
  }
  return 0;
}

size_t strlen(const char* s) {
  size_t n = 0;
  while (s[n]) ++n;
  return n;
}

int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) { ++a; ++b; }
  return (unsigned char)*a - (unsigned char)*b;
}

// Number of characters at the start of s that are not in reject.
size_t strcspn(const char* s, const char* reject) {
  size_t n = 0;
  for (; s[n]; ++n) {
    for (const char* r = reject; *r; ++r) {
      if (s[n] == *r) return n;
    }
  }
  return n;
}

// ------------------------------------------------------------------- heap --
// Every block comes straight from the operating system, and free() gives it
// straight back. plat_free needs the size of the block, so malloc stores it
// in a 16-byte header in front of the block (16 bytes keep the block aligned).

static const size_t HEADER_SIZE = 16;

void* malloc(size_t size) {
  const size_t total = size + HEADER_SIZE;
  void* header = plat_alloc(total);
  if (!header) return nullptr;
  *(size_t*)header = total;
  return (char*)header + HEADER_SIZE;
}

// plat_alloc returns zero-filled memory, so there is nothing to clear.
void* calloc(size_t count, size_t size) { return malloc(count * size); }

void free(void* block) {
  if (!block) return;
  void* header = (char*)block - HEADER_SIZE;
  plat_free(header, *(size_t*)header);
}

// ------------------------------------------------------------------ files --
// A FILE is just the open file and an end-of-file flag; there is no buffer.
// Callers only pass the FILE* back to these functions, so it can point to
// our own struct instead of the C library's.

struct StubFile {
  plat_file file;
  int       atEnd;   // a read has reached the end of the file
};

static StubFile* toStubFile(FILE* stream) { return (StubFile*)stream; }

// Modes used by the stub: "rb" (read an existing file) and "wb", "wb+"
// (create or empty the file). Files are always binary.
FILE* fopen(const char* path, const char* mode) {
  const plat_file file = plat_open(path, mode[0] == 'w');
  if (file == PLAT_NO_FILE) return nullptr;
  StubFile* f = (StubFile*)malloc(sizeof(StubFile));
  if (!f) { plat_close(file); return nullptr; }
  f->file = file;
  f->atEnd = 0;
  return (FILE*)f;
}

int fclose(FILE* stream) {
  plat_close(toStubFile(stream)->file);
  free(stream);
  return 0;
}

// The operating system may transfer less than asked for, so read and write
// repeat until everything is done.
size_t fread(void* buffer, size_t size, size_t count, FILE* stream) {
  StubFile* f = toStubFile(stream);
  const size_t wanted = size * count;
  size_t done = 0;
  while (done < wanted) {
    const int64_t n = plat_read(f->file, (char*)buffer + done, wanted - done);
    if (n == 0) f->atEnd = 1;
    if (n <= 0) break;
    done += (size_t)n;
  }
  return size ? done / size : 0;
}

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream) {
  const size_t wanted = size * count;
  size_t done = 0;
  while (done < wanted) {
    const int64_t n = plat_write(toStubFile(stream)->file, (const char*)buffer + done, wanted - done);
    if (n <= 0) break;
    done += (size_t)n;
  }
  return size ? done / size : 0;
}

int fgetc(FILE* stream) {
  unsigned char c;
  return fread(&c, 1, 1, stream) == 1 ? c : EOF;
}

int fputc(int value, FILE* stream) {
  const unsigned char c = (unsigned char)value;
  return fwrite(&c, 1, 1, stream) == 1 ? c : EOF;
}

// Reads one line, including its line feed, but at most size - 1 characters.
char* fgets(char* line, int size, FILE* stream) {
  int length = 0;
  while (length < size - 1) {
    const int c = fgetc(stream);
    if (c == EOF) break;
    line[length++] = (char)c;
    if (c == '\n') break;
  }
  if (length == 0) return nullptr;   // end of file
  line[length] = '\0';
  return line;
}

int feof(FILE* stream) { return toStubFile(stream)->atEnd; }

// Moving in a file and asking for the position, with 64-bit offsets. The C
// libraries name these two functions differently (see SystemDefines.hpp).
#if defined(_WIN32)
int __cdecl _fseeki64(FILE* stream, __int64 offset, int whence) {
#else
int fseeko(FILE* stream, off_t offset, int whence) {
#endif
  if (plat_seek(toStubFile(stream)->file, offset, whence) < 0) return -1;
  toStubFile(stream)->atEnd = 0;
  return 0;
}

#if defined(_WIN32)
__int64 __cdecl _ftelli64(FILE* stream) {
#else
off_t ftello(FILE* stream) {
#endif
  return plat_seek(toStubFile(stream)->file, 0, SEEK_CUR);
}

// -------------------------------------------------------------- the end ----

void exit(int exitCode) { plat_exit(exitCode); }

// The compiler registers the destructors of global objects with atexit (MSVC)
// or __cxa_atexit (GCC, clang). The stub never runs them: when it ends, the
// operating system takes back its memory and closes its files anyway.
int   atexit(void (*)(void)) { return 0; }
int   __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void* __dso_handle = nullptr;

// Called if a pure virtual function is ever called, which would be a bug.
// (GCC and clang; the MSVC name is in Platform_Windows.cpp.)
void __cxa_pure_virtual() { plat_exit(70); }

} // extern "C"

// ------------------------------------------------------- new and delete ----

void* operator new(size_t size)   { return malloc(size); }
void* operator new[](size_t size) { return malloc(size); }
void  operator delete(void* p) noexcept           { free(p); }
void  operator delete[](void* p) noexcept         { free(p); }
void  operator delete(void* p, size_t) noexcept   { free(p); }
void  operator delete[](void* p, size_t) noexcept { free(p); }

#endif
