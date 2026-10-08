#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include "../String.hpp"
#include "../SystemDefines.hpp"

//////////////////// IO functions and classes ///////////////////
// File names are UTF-8 (char*) everywhere in this program. Linux takes them
// as they are; Windows needs UTF-16 (wchar_t*), so the wrappers below convert
// them before calling the Windows functions.


// The preferred slash for displaying
// We will change the BADSLASH to GOODSLASH before displaying a path string to the user
#ifdef WINDOWS
#define BADSLASH '/'
#define GOODSLASH '\\'
#else
#define BADSLASH '\\'
#define GOODSLASH '/'
#endif

// Only for Windows: a UTF-16 copy of a UTF-8 string.
// The copy is freed when the object goes out of scope.
#ifdef WINDOWS
class WcharStr {
public:
  wchar_t *wchar_str;
  explicit WcharStr(const char *utf8_str) {
    const int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, nullptr, 0);
    wchar_str = new wchar_t[bufferSize];
    MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wchar_str, bufferSize);
  }
  ~WcharStr() {
    delete[] wchar_str;
  }
};
#endif

#ifdef WINDOWS
#define STAT __stat64
#else
#define STAT stat
#endif

static constexpr int READ = 0;
static constexpr int WRITE = 1;

/**
 * Wrapper function (Linux vs Windows) to open a file
 * @param filename
 * @param mode READ: open an existing file for reading;
 *             WRITE: create the file (or empty it) for reading and writing
 * @return the file, or nullptr on failure
 */
static FILE* openFile(const char *filename, const int mode) {
#ifdef WINDOWS
  return _wfopen(WcharStr(filename).wchar_str, mode == READ ? L"rb" : L"wb+");
#else
  return fopen(filename, mode == READ ? "rb" : "wb+");
#endif
}

/**
 * Wrapper function (Linux vs Windows) to examine a path
 * @param path
 * @param status receives the details of the file or directory
 * @return true on success; on failure errno tells why
 */
static bool statPath(const char *path, struct STAT &status) {
#ifdef WINDOWS
  return _wstat64(WcharStr(path).wchar_str, &status) == 0;
#else
  return stat(path, &status) == 0;
#endif
}

/**
 * examines given "path" and returns:
 * 0: on error
 * 1: when exists and is a file
 * 2: when exists and is a directory
 * 3: when does not exist, but looks like a file       /abcd/efgh
 * 4: when does not exist, but looks like a directory  /abcd/efgh/
 */
static int examinePath(const char *path) {
  struct STAT status {};
  if( !statPath(path, status) ) {
    if( errno == ENOENT ) { //no such file or directory
      const int len = static_cast<int>(strlen(path));
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

/**
 * Creates a directory if it does not exist
 * @return 0: failed, 1: created successfully, 2: the directory already exists
 */
static int makeDir(const char *dir) {
  if( examinePath(dir) == 2 ) { //existing directory
    return 2;
  }
#ifdef WINDOWS
  const bool created = (CreateDirectoryW(WcharStr(dir).wchar_str, nullptr) == TRUE);
#else
  const bool created = (mkdir(dir, 0777) == 0);
#endif
  return created ? 1 : 0;
}

/**
 * Creates the directories in the path of filename if they don't exist.
 */
static void makeDirectories(const char *filename) {
  String path(filename);
  uint64_t start = 0;
  if( path[1] == ':' ) {
    start = 2; //skip drive letter (c:)
  }
  if( path[start] == '/' || path[start] == '\\' ) {
    start++; //skip leading slashes (root dir)
  }
  for( uint64_t i = start; path[i] != 0; ++i ) {
    if( path[i] == '/' || path[i] == '\\' ) {
      char saveChar = path[i];
      path[i] = 0;
      const char *dirName = path.c_str();
      const int created = makeDir(dirName);
      if( created == 0 ) {
        printf("Unable to create directory %s", dirName);
        quit();
      }
      if( created == 1 ) {
        printf("Created directory %s\n", dirName);
      }
      path[i] = saveChar;
    }
  }
}
