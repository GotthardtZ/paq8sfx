#pragma once

#include "../SystemDefines.hpp"
#include <cstdlib>

#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif

/**
 * The running executable file, loaded into memory.
 * The stub uses it to find the payload files that are attached to it.
 * Owns the buffer (malloc/free).
 */
class FileExeRead {
  static FILE* openMyself() {
#if defined(WINDOWS)
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) quit("Cannot determine exe path.");
    return _wfopen(path, L"rb");
#elif defined(__APPLE__)
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) quit("Cannot determine exe path.");
    return fopen(path, "rb");
#else
    return fopen("/proc/self/exe", "rb");  // Linux: always the running executable
#endif
  }

public:
  uint8_t* buf;
  uint64_t fileSize;

  FileExeRead() : buf(nullptr), fileSize(0) {
    FILE* f = openMyself();
    if (!f) quit("Cannot open exe.");
    fseeko(f, 0, SEEK_END);
    fileSize = static_cast<uint64_t>(ftello(f));
    buf = static_cast<uint8_t*>(malloc(fileSize));
    if (!buf) quit("Out of memory.");
    fseeko(f, 0, SEEK_SET);
    if (fread(buf, 1, fileSize, f) != fileSize) quit("Cannot read exe.");
    fclose(f);
  }

  ~FileExeRead() {
    free(buf);
  }
};
