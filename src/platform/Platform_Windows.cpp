// The libc-free stub on Windows (x64).
//
// The stub uses kernel32.dll only. This file contains:
//   - the plat_* functions of Platform.hpp
//   - the C functions that only the Windows code of the stub calls
//     (_wfopen, _wstat64, system)
//   - the entry point of the program, which also splits the command line
//     into argc and argv
//
// It is built with MinGW-w64 and with Visual Studio. The compiler and linker
// settings it needs are explained in the Makefile (the stub-free rules) and
// in paq8sfx.vcxproj (the Release-SFX-Free configuration).

#include "Platform.hpp"

#if defined(SFX) && defined(SFX_FREESTANDING) && defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// ---------------------------------------------------------------- helpers --

static int lastError = 0;   // this is "errno"

// Sets errno after a failed Windows call. The stub only ever asks whether a
// file was "not found" (ENOENT); every other failure is reported as EIO.
static void setLastError() {
  const DWORD error = GetLastError();
  const bool notFound = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
                        error == ERROR_INVALID_NAME;
  lastError = notFound ? ENOENT : EIO;
}

// Windows takes file names in UTF-16, the stub keeps them in UTF-8.
// Both functions return a new string (to be freed with free), or null.
static wchar_t* toUtf16(const char* text) {
  const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);   // counts the final zero
  wchar_t* result = (wchar_t*)malloc((size_t)length * sizeof(wchar_t));
  if (result) MultiByteToWideChar(CP_UTF8, 0, text, -1, result, length);
  return result;
}

static char* toUtf8(const wchar_t* text) {
  const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  char* result = (char*)malloc((size_t)length);
  if (result) WideCharToMultiByte(CP_UTF8, 0, text, -1, result, length, nullptr, nullptr);
  return result;
}

// ReadFile and WriteFile take a 32-bit size. Larger requests are cut down to
// 1 GB; the caller (fread, fwrite) asks again for the rest.
static DWORD atMostOneGigabyte(size_t count) {
  return count < 0x40000000 ? (DWORD)count : 0x40000000;
}

extern "C" {

// --------------------------------------------------------------- Platform.hpp

plat_file plat_open(const char* path, bool create) {
  wchar_t* widePath = toUtf16(path);
  if (!widePath) { lastError = EIO; return PLAT_NO_FILE; }
  const HANDLE handle = CreateFileW(widePath,
                                    create ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    create ? CREATE_ALWAYS : OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
  free(widePath);
  if (handle == INVALID_HANDLE_VALUE) { setLastError(); return PLAT_NO_FILE; }
  return (plat_file)handle;
}

void plat_close(plat_file file) {
  CloseHandle((HANDLE)file);
}

int64_t plat_read(plat_file file, void* buffer, size_t count) {
  DWORD done = 0;
  if (!ReadFile((HANDLE)file, buffer, atMostOneGigabyte(count), &done, nullptr)) {
    setLastError();
    return -1;
  }
  return done;
}

int64_t plat_write(plat_file file, const void* buffer, size_t count) {
  DWORD done = 0;
  if (!WriteFile((HANDLE)file, buffer, atMostOneGigabyte(count), &done, nullptr)) {
    setLastError();
    return -1;
  }
  return done;
}

int64_t plat_seek(plat_file file, int64_t offset, int whence) {
  // SEEK_SET, SEEK_CUR and SEEK_END have the same values (0, 1, 2) as
  // FILE_BEGIN, FILE_CURRENT and FILE_END.
  LARGE_INTEGER distance, position;
  distance.QuadPart = offset;
  if (!SetFilePointerEx((HANDLE)file, distance, &position, (DWORD)whence)) {
    setLastError();
    return -1;
  }
  return position.QuadPart;
}

void* plat_alloc(size_t size) {
  return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void plat_free(void* memory, size_t) {
  VirtualFree(memory, 0, MEM_RELEASE);
}

[[noreturn]] void plat_exit(int exitCode) {
  ExitProcess((UINT)exitCode);
}

// ----------------------------------------------- C functions, Windows only --

// "errno" is a macro for *_errno() in the C library headers.
int* __cdecl _errno(void) { return &lastError; }

// fopen with a UTF-16 file name. Only the first letter of the mode matters.
FILE* __cdecl _wfopen(const wchar_t* path, const wchar_t* mode) {
  char* utf8Path = toUtf8(path);
  if (!utf8Path) { lastError = EIO; return nullptr; }
  FILE* file = fopen(utf8Path, mode[0] == L'w' ? "wb" : "rb");
  free(utf8Path);
  return file;
}

// Tells whether a path is a file or a folder, and the size of the file.
// The other fields of the result are not used by the stub and stay zero.
int __cdecl _wstat64(const wchar_t* path, struct _stat64* info) {
  WIN32_FILE_ATTRIBUTE_DATA data;
  if (!GetFileAttributesExW(path, GetFileExInfoStandard, &data)) {
    setLastError();
    return -1;
  }
  memset(info, 0, sizeof(*info));
  const bool isFolder = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  info->st_mode = isFolder ? _S_IFDIR : _S_IFREG;
  info->st_size = ((__int64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
  return 0;
}

// Runs a command with the command interpreter (cmd.exe), waits until it has
// finished and returns its exit code.
int __cdecl system(const char* command) {
  // The command line to run:  cmd.exe /c command
  static const wchar_t prefix[] = L"cmd.exe /c ";
  const size_t prefixLength = sizeof(prefix) / sizeof(wchar_t) - 1;
  const int commandLength = MultiByteToWideChar(CP_UTF8, 0, command, -1, nullptr, 0);   // counts the final zero
  wchar_t* commandLine = (wchar_t*)malloc((prefixLength + (size_t)commandLength) * sizeof(wchar_t));
  if (!commandLine) return -1;
  memcpy(commandLine, prefix, prefixLength * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, command, -1, commandLine + prefixLength, commandLength);

  // The ComSpec environment variable holds the full path of cmd.exe.
  // Without it, Windows searches for "cmd.exe" itself.
  wchar_t interpreter[MAX_PATH];
  const DWORD length = GetEnvironmentVariableW(L"ComSpec", interpreter, MAX_PATH);
  const bool haveInterpreter = length > 0 && length < MAX_PATH;

  STARTUPINFOW startupInfo;
  memset(&startupInfo, 0, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION process;
  const BOOL started = CreateProcessW(haveInterpreter ? interpreter : nullptr, commandLine,
                                      nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                                      &startupInfo, &process);
  free(commandLine);
  if (!started) return -1;

  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return (int)exitCode;
}

#if defined(_MSC_VER)
// MSVC expects this variable in every program that uses floating point.
int _fltused = 0;

// Called if a pure virtual function is ever called, which would be a bug.
// (MSVC; the GCC name is in CRuntime.cpp.)
int __cdecl _purecall(void) { plat_exit(70); }
#endif

} // extern "C"

// ----------------------------------------------------------- command line --
// Windows hands a program its command line as one string. This splits it
// into arguments at spaces and tabs. Double quotes keep an argument with
// spaces together; the quotes themselves are removed:
//
//   "C:\my folder\package.exe" -d "my archive.paq8sfx" output.bin
//
// There is no way to write a double quote inside an argument. None is
// needed: the arguments of the stub are file names, and a Windows file name
// cannot contain one.

static bool isBlank(char c) { return c == ' ' || c == '\t'; }

static char** splitCommandLine(int* argc) {
  // UTF-8, like every other string in the stub. Blanks and quotes are single
  // bytes in UTF-8, so the string can be split in place: argv points into it.
  char* p = toUtf8(GetCommandLineW());
  // An argument and the blank after it take at least two characters.
  char** argv = (char**)malloc((strlen(p) / 2 + 2) * sizeof(char*));
  int count = 0;
  for (;;) {
    while (isBlank(*p)) ++p;
    if (!*p) break;
    char* out = p;               // the argument is copied onto itself, without its quotes
    argv[count++] = out;
    bool inQuotes = false;
    while (*p && (inQuotes || !isBlank(*p))) {
      if (*p == '"') inQuotes = !inQuotes;
      else *out++ = *p;
      ++p;
    }
    if (*p) ++p;                 // step over the blank that ended the argument
    *out = '\0';
  }
  argv[count] = nullptr;
  *argc = count;
  return argv;
}

// ------------------------------------------------------------ entry point --
// Windows starts the program at sfx_entry (the linker is told so). Before
// main can run, the constructors of the global objects have to be called.
// The compiler collects pointers to them in a table.

typedef void (__cdecl *Constructor)(void);

#if defined(_MSC_VER)

// MSVC: the table is everything the linker places between the sections
// .CRT$XCA and .CRT$XCZ (it sorts sections by name). Entries may be null.
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCZ", long, read)
extern "C" __declspec(allocate(".CRT$XCA")) Constructor sfx_xc_a[] = { nullptr };
extern "C" __declspec(allocate(".CRT$XCZ")) Constructor sfx_xc_z[] = { nullptr };
#pragma comment(linker, "/merge:.CRT=.rdata")

static void runConstructors() {
  // "volatile" keeps the optimizer from reasoning about these two pointers:
  // to the compiler they point into different arrays, and it could conclude
  // that the loop never runs.
  Constructor* volatile begin = sfx_xc_a + 1;
  Constructor* volatile end   = sfx_xc_z;
  for (Constructor* constructor = begin; constructor < end; ++constructor) {
    if (*constructor) (*constructor)();
  }
}

#else

// GCC: the table is __CTOR_LIST__: an unused first entry, then the
// constructors, then a null. GCC makes main call __main first, which has to
// call them, last one first.
extern "C" Constructor __CTOR_LIST__[];

extern "C" void __main(void) {
  size_t count = 0;
  while (__CTOR_LIST__[count + 1]) ++count;
  for (size_t i = count; i >= 1; --i) __CTOR_LIST__[i]();
}

#endif

extern "C" int main(int argc, char** argv);

extern "C" void __cdecl sfx_entry(void) {
#if defined(_MSC_VER)
  runConstructors();   // with GCC, main does this by calling __main
#endif
  int argc = 0;
  char** argv = splitCommandLine(&argc);
  plat_exit(main(argc, argv));
}

#endif
