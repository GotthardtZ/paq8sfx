@echo off
setlocal EnableDelayedExpansion

for /f "delims=" %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"
set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "RESET=%ESC%[0m"

rem * This script builds paq8sfx.exe (the compressor) and stub.exe (the self-extractor) for x64 CPUs.
rem * SIMD: AVX2 only by default. For runtime SIMD dispatch (any x64 CPU) comment out AVX2_ONLY in src\SystemDefines.hpp.
rem * Requirements: MinGW-w64 (set path below)
rem * Output: paq8sfx.exe and stub.exe, with errors and/or warnings in _error1.txt and/or in _error2.txt if compilation fails.

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
set options=-DNDEBUG %safefp% -m64 -march=nocona -mtune=generic -std=gnu++17 -fno-rtti -flto=auto -s -Wl,--gc-sections -fno-exceptions -fno-declone-ctor-dtor

rem * Clean previous build artifacts
del /q _error1.txt _error2.txt paq8sfx.exe stub.exe *.o _sources.txt 2>nul

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
g++.exe -DFULL -O3 %options% @_sources.txt -o paq8sfx.exe 2>_error1.txt
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
g++.exe -DSFX -Os %options% @_sources.txt -o stub.exe 2>_error2.txt
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

del /q _sources.txt *.o 2>nul
echo %GREEN%[PASSED]%RESET% Build complete: paq8sfx.exe and stub.exe created.
pause
exit /b 0