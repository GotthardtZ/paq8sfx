#include "fileUtils.h"
#include "../String.h"

#ifdef WINDOWS
// Only for Windows: a UTF-16 copy of a UTF-8 string. Free it with free().
static wchar_t* toWide(const char* const utf8_str) {
  const int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
  wchar_t* const wchar_str = (wchar_t*)malloc((size_t)bufferSize * sizeof(wchar_t));
  if (wchar_str == NULL) {
    quit("Out of memory.");
  }
  MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wchar_str, bufferSize);
  return wchar_str;
}
#endif

#ifdef WINDOWS
#define STAT __stat64
#else
#define STAT stat
#endif

FILE* openFile(const char* const filename, const int mode) {
#ifdef WINDOWS
  wchar_t* const wideName = toWide(filename);
  FILE* const file = _wfopen(wideName, mode == FILE_READ ? L"rb" : L"wb+");
  free(wideName);
  return file;
#else
  return fopen(filename, mode == FILE_READ ? "rb" : "wb+");
#endif
}

/**
 * Wrapper function (Linux vs Windows) to examine a path
 * @param path
 * @param status receives the details of the file or directory
 * @return true on success; on failure errno tells why
 */
static bool statPath(const char* const path, struct STAT* const status) {
#ifdef WINDOWS
  wchar_t* const widePath = toWide(path);
  const bool success = _wstat64(widePath, status) == 0;
  const int error = errno;
  free(widePath);
  errno = error;
  return success;
#else
  return stat(path, status) == 0;
#endif
}

int examinePath(const char* const path) {
  struct STAT status;
  memset(&status, 0, sizeof(status));
  if( !statPath(path, &status) ) {
    if( errno == ENOENT ) { //no such file or directory
      const int len = (int)strlen(path);
      if( len == 0 ) {
        return 0; //error: path is an empty string
      }
      const char lastChar = path[len - 1];
      if( lastChar != '/' && lastChar != '\\' ) {
        return 3; //looks like a file
      }

      return 4; //looks like a directory
    }
    return 0; //error
  }
  if((status.st_mode & S_IFREG) != 0 ) {
    return 1; //existing file
  }
  if((status.st_mode & S_IFDIR) != 0 ) {
    return 2; //existing directory
  }
  return 0; //error: "path" may be a socket, symlink, named pipe, etc.
}

int makeDir(const char* const dir) {
  if( examinePath(dir) == 2 ) { //existing directory
    return 2;
  }
#ifdef WINDOWS
  wchar_t* const wideDir = toWide(dir);
  const bool created = (CreateDirectoryW(wideDir, NULL) == TRUE);
  free(wideDir);
#else
  const bool created = (mkdir(dir, 0777) == 0);
#endif
  return created ? 1 : 0;
}

void makeDirectories(const char* const filename) {
  String path;
  String_init(&path, filename);
  uint64_t start = 0;
  if( path.data[1] == ':' ) {
    start = 2; //skip drive letter (c:)
  }
  if( path.data[start] == '/' || path.data[start] == '\\' ) {
    start++; //skip leading slashes (root dir)
  }
  for( uint64_t i = start; path.data[i] != 0; ++i ) {
    if( path.data[i] == '/' || path.data[i] == '\\' ) {
      char saveChar = path.data[i];
      path.data[i] = 0;
      const char *dirName = String_c_str(&path);
      const int created = makeDir(dirName);
      if( created == 0 ) {
        printf("Unable to create directory %s", dirName);
        quit(NULL);
      }
      if( created == 1 ) {
        printf("Created directory %s\n", dirName);
      }
      path.data[i] = saveChar;
    }
  }
  String_destroy(&path);
}
