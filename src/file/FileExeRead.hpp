#pragma once

#include "File.hpp"
#include <cstdlib>

#ifdef WINDOWS
#  include <windows.h>
#  include "../String.hpp"
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <climits>
#else
#  include <unistd.h>
#  include <climits>
#endif

/**
 * Read-only in-memory file, pre-loaded from the running executable.
 * Owns the buffer (malloc/free).
 */
class FileExeRead
{
  static FILE* openMyself() {
#ifdef WINDOWS
    Array<wchar_t> path(MAX_PATH);
    int i = GetModuleFileNameW(nullptr, &path[0], MAX_PATH);
    if (i == 0 || i >= MAX_PATH) quit("Cannot determine exe path.");
    return _wfopen(&path[0], L"rb");
#elif defined(__APPLE__)
    char path[PATH_MAX];
    uint32_t sz = sizeof(path);
    if (_NSGetExecutablePath(path, &sz) != 0) quit("Cannot determine exe path.");
    return fopen(path, "rb");
#else
    char path[PATH_MAX + 1] = {};
    if (readlink("/proc/self/exe", path, PATH_MAX) == -1) quit("Cannot determine exe path.");
    return fopen(path, "rb");
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
    fread(buf, 1, fileSize, f);
    fclose(f);
  };


  ~FileExeRead() {
    free(buf);
  }

};
