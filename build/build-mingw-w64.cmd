@echo off
setlocal EnableDelayedExpansion

for /f "delims=" %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"
set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "RESET=%ESC%[0m"

rem * Builds paq8sfx.exe (the compressor), stub.exe (the self-extractor) and stub-free.exe
rem * (the libc-free stub: no C runtime, imports kernel32.dll only) for x64 Windows with MinGW-w64.
rem * The programs are created in this folder (build). The Makefile in the project root builds
rem * the same programs with the same settings; see README.md.
rem *
rem * Requirements: MinGW-w64 (set its path below)
rem * Stub settings: sfxsimd and sfxmode below
rem * If a build fails, the compiler messages are in _error1.txt (paq8sfx.exe), _error2.txt (stub.exe)
rem * or _error3.txt (stub-free.exe).

rem * Work in the folder of this script
cd /d "%~dp0"

rem * Set your mingw-w64 path below
set path=%path%;c:/mingw/winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1/bin

rem * Define base paths
set "parent_dir=%~dp0.."

rem * Check for MinGW-w64
where gcc.exe >nul 2>&1 || (
  echo %RED%[FAILED]%RESET% gcc.exe not found.
  echo Suggested path: "c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1"
  echo If your MinGW-w64 installation is in a different folder update the path in this script.
  pause
  exit /b 1
)

rem * Set MAKE for parallel LTO
rem * Why: GCC's LTO wrapper needs a 'make' executable to coordinate parallel compilation jobs, and it can't find it automatically.
rem * By explicitly setting MAKE=mingw32-make, we tell it where to look.
set MAKE=mingw32-make

rem * Force floating point reproducibility explicitly
set safefp=-fno-fast-math -ffp-contract=off

rem * Set compiler options (release by default)
rem -fno-declone-ctor-dtor is there because of a gcc bug: FLTO is not working properly with -Os
rem * Stubs only: empty = every code path, runtime selection (any x64 CPU);
rem * -DAVX2_ONLY = AVX2 code path only (smaller, needs an AVX2 CPU)
set sfxsimd=
rem * Stubs only: empty = full stub (extract + AutoRun, -d, -a);
rem * -DSFX_EXTRACT_ONLY = extract-only stub, its packages are assembled with paq8sfx -a
set sfxmode=

set options=-DNDEBUG %safefp% -m64 -march=nocona -mtune=generic -std=gnu++17 -fno-rtti -flto=auto -s -Wl,--gc-sections -fno-exceptions -fno-declone-ctor-dtor

rem * Clean previous build artifacts
del /q _error1.txt _error2.txt _error3.txt paq8sfx.exe stub.exe stub-free.exe *.o _sources.txt 2>nul

rem * Build response file with quoted full paths to *.cpp files
echo Building source file list...
> _sources.txt (
  rem Process .cpp files in src directory and specified subfolders
  for %%D in (src src/file src/model) do (
    for /f "delims=" %%F in ('dir /b /a:-d "%parent_dir%\%%D\*.cpp" 2^>nul') do (
      rem Convert relative path to full path and quote it
      pushd "%parent_dir%\%%D"
      echo "!CD!\%%F"
      popd
    )
  )
)

echo Compiling and linking paq8sfx.exe...
g++.exe -DFULL -static -O3 %options% @_sources.txt -o paq8sfx.exe 2>_error1.txt
if !errorlevel! neq 0 (
  echo %RED%[FAILED]%RESET% paq8sfx.exe compilation failed. See _error1.txt for details.
  pause
  exit /b 1
)
if not exist paq8sfx.exe (
  echo %RED%[FAILED]%RESET% paq8sfx.exe not found after compilation.
  pause
  exit /b 1
)
echo %GREEN%[PASSED]%RESET% paq8sfx.exe compiled successfully.

echo Compiling and linking stub.exe...
g++.exe -DSFX %sfxsimd% %sfxmode% -Os %options% @_sources.txt -o stub.exe 2>_error2.txt
if !errorlevel! neq 0 (
  echo %RED%[FAILED]%RESET% stub.exe compilation failed. See _error2.txt for details.
  pause
  exit /b 1
)
if not exist stub.exe (
  echo %RED%[FAILED]%RESET% stub.exe not found after compilation.
  pause
  exit /b 1
)
echo %GREEN%[PASSED]%RESET% stub.exe compiled successfully.

rem * stub-free.exe: no C runtime. The two runtime files are compiled first, without
rem * LTO (and the C runtime without builtins); everything else is built with LTO.
rem * -D_CRTIMP= : plain (non-dllimport) CRT declarations, so calls reach our own definitions.
rem * No libgcc on purpose: a stack frame over 4 KB must fail to link (undefined ___chkstk_ms).
echo Compiling and linking stub-free.exe...
set freeopts=-DSFX %sfxsimd% %sfxmode% -DSFX_FREESTANDING -D_CRTIMP= -Os -fno-stack-protector -fno-threadsafe-statics -fno-asynchronous-unwind-tables -fno-unwind-tables
rem * -ffunction-sections -fdata-sections: lets the linker drop the runtime functions this stub does not use.
set rtopts=-DNDEBUG %safefp% -m64 -march=nocona -mtune=generic -std=gnu++17 -fno-rtti -fno-exceptions -fno-lto -ffunction-sections -fdata-sections
g++.exe %freeopts% %rtopts% -fno-builtin -c "%parent_dir%\src\platform\CRuntime.cpp" -o _crt.o 2>_error3.txt
if !errorlevel! neq 0 goto :free_failed
g++.exe %freeopts% %rtopts% -fno-builtin -c "%parent_dir%\src\platform\Platform_Windows.cpp" -o _platform.o 2>>_error3.txt
if !errorlevel! neq 0 goto :free_failed
g++.exe %freeopts% %options% @_sources.txt _crt.o _platform.o -nostdlib -static -Wl,-e,sfx_entry -Wl,--subsystem,console -Wl,--disable-dynamicbase -Wl,--disable-reloc-section -lkernel32 -o stub-free.exe 2>>_error3.txt
if !errorlevel! neq 0 goto :free_failed
if not exist stub-free.exe goto :free_failed
echo %GREEN%[PASSED]%RESET% stub-free.exe compiled successfully.

del /q _sources.txt *.o 2>nul
echo %GREEN%[PASSED]%RESET% Build complete: paq8sfx.exe, stub.exe and stub-free.exe created.
pause
exit /b 0

:free_failed
echo %RED%[FAILED]%RESET% stub-free.exe compilation failed. See _error3.txt for details.
pause
exit /b 1