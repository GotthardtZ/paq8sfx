#include "FileExeRead.h"

#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif

static FILE* openMyself(void) {
#if defined(WINDOWS)
  wchar_t path[MAX_PATH];
  const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
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

void FileExeRead_init(FileExeRead* const self) {
  self->buf = NULL;
  self->fileSize = 0;
  FILE* f = openMyself();
  if (!f) quit("Cannot open exe.");
  fseeko(f, 0, SEEK_END);
  self->fileSize = (uint64_t)ftello(f);
  self->buf = (uint8_t*)malloc((size_t)self->fileSize);
  if (!self->buf) quit("Out of memory.");
  fseeko(f, 0, SEEK_SET);
  if (fread(self->buf, 1, (size_t)self->fileSize, f) != self->fileSize) quit("Cannot read exe.");
  fclose(f);
}

void FileExeRead_destroy(FileExeRead* const self) {
  free(self->buf);
  self->buf = NULL;
}
