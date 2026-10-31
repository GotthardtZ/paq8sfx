#pragma once

// The libc-free stub: what the operating system has to provide.
//
// The libc-free stub is linked without the C and C++ runtime libraries. The
// rest of the program still calls the usual C functions (fopen, malloc,
// memcpy, ...), so this folder supplies them:
//
//   CRuntime.cpp           the C functions themselves, written once for every
//                          operating system, using only the functions below
//   Platform_Linux.cpp     the functions below as Linux system calls
//   Platform_Windows.cpp   the functions below as kernel32.dll calls
//
// Each Platform_*.cpp also holds the program's entry point and the few C
// functions that exist on that operating system only.
//
// These files are compiled into the libc-free stub only (-DSFX
// -DSFX_FREESTANDING); in every other build they are empty.

#if defined(SFX) && defined(SFX_FREESTANDING)

#include <stddef.h>
#include <stdint.h>

extern "C" {

// An open file: a file descriptor on Linux, a HANDLE on Windows.
typedef intptr_t plat_file;
const plat_file PLAT_NO_FILE = -1;

// Opens a file; the path is UTF-8.
//   create = false: the file must exist; it is opened for reading
//   create = true:  the file is created or emptied, and opened for reading and writing
// Returns PLAT_NO_FILE and sets errno on failure.
plat_file plat_open(const char* path, bool create);
void      plat_close(plat_file file);

// Read and write return the number of bytes transferred, which may be less
// than count, or -1 on failure. A read at the end of the file returns 0.
int64_t plat_read (plat_file file, void* buffer, size_t count);
int64_t plat_write(plat_file file, const void* buffer, size_t count);

// Moves the file position (whence: SEEK_SET, SEEK_CUR or SEEK_END).
// Returns the new position, or -1 on failure.
int64_t plat_seek(plat_file file, int64_t offset, int whence);

// Memory straight from the operating system, filled with zeros.
// plat_alloc returns null on failure; plat_free needs the size again.
void* plat_alloc(size_t size);
void  plat_free (void* memory, size_t size);

// Ends the program.
[[noreturn]] void plat_exit(int exitCode);

} // extern "C"

#endif
