@echo off
setlocal EnableDelayedExpansion

for /f "delims=" %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"
set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "YELLOW=%ESC%[93m"
set "RESET=%ESC%[0m"

rem * Builds paq8sfx.exe (the compressor), stub.exe (the self-extractor) and stub-free.exe
rem * (the libc-free stub) for x64 Windows with MinGW-w64, using the Makefile in the project
rem * root. The programs are created in this folder (build); the object files go to build\obj.
rem * Unlike build-mingw-w64.cmd, only what changed since the last build is recompiled.
rem *
rem * Requirements:
rem *   - MinGW-w64 with g++.exe and mingw32-make.exe (set its path below)
rem *   - sh, mkdir, rm, printf and cmp, which the Makefile runs; Git for Windows has them
rem *     in its usr\bin folder (set its path below)
rem *
rem * Usage: build-mingw-w64-make.cmd [make arguments]
rem *   (no arguments)   build paq8sfx.exe, stub.exe and stub-free.exe
rem *   clean            remove everything the Makefile created (run it on its own)
rem *   paq8sfx          build only paq8sfx.exe (also: stub, stub-free)
rem *   V=1              show the full command lines
rem *   upx              compress stub.exe with upx (upx.exe must be on the path)
rem *
rem * Stub settings: avx2_only and extract_only below (or AVX2_ONLY=1 / SFX_EXTRACT_ONLY=1
rem * on the command line). Changing them rebuilds the stubs automatically.
rem * If the build fails, the compiler messages are in _error.txt.

rem * Work in the folder of this script
cd /d "%~dp0"

rem * Set your mingw-w64 path below
set "mingw_dir=c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1\bin"
rem * Set your Git for Windows usr\bin path below
set "git_dir=C:\Program Files\Git\usr\bin"

rem * MinGW-w64 first, so that no other g++ on the path is used; Git's tools last, so that
rem * they do not hide Windows' own commands (find, sort, ...)
set "PATH=%mingw_dir%;%PATH%;%git_dir%"

rem * Define base paths
set "project_dir=%~dp0.."

rem * Check for the tools
where g++.exe >nul 2>&1 || (
  echo %RED%[FAILED]%RESET% g++.exe not found.
  echo Suggested path: "c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1\bin"
  echo If your MinGW-w64 installation is in a different folder update mingw_dir in this script.
  pause
  exit /b 1
)
where mingw32-make.exe >nul 2>&1 || (
  echo %RED%[FAILED]%RESET% mingw32-make.exe not found.
  echo It comes with MinGW-w64 ^(in the same bin folder as g++.exe^).
  pause
  exit /b 1
)
for %%T in (sh.exe cmp.exe printf.exe) do (
  where %%T >nul 2>&1 || (
    echo %RED%[FAILED]%RESET% %%T not found.
    echo Install Git for Windows, or update git_dir in this script.
    echo Suggested path: "C:\Program Files\Git\usr\bin"
    pause
    exit /b 1
  )
)

rem * Stubs only: 0 = every code path, runtime selection (any x64 CPU);
rem * 1 = AVX2 code path only (smaller, needs an AVX2 CPU)
set "avx2_only=0"
rem * Stubs only: 0 = full stub (extract + AutoRun, -d, -a);
rem * 1 = extract-only stub, its packages are assembled with paq8sfx -a
set "extract_only=0"

rem * What to build: the command line arguments, or all three programs
set "targets=%*"
if not defined targets set "targets=all stub-free"

rem * One job per CPU thread
if not defined NUMBER_OF_PROCESSORS set "NUMBER_OF_PROCESSORS=1"

del /q _error.txt 2>nul

echo Running make (%NUMBER_OF_PROCESSORS% jobs): !targets!
mingw32-make -C "%project_dir%" -j%NUMBER_OF_PROCESSORS% --no-print-directory AVX2_ONLY=%avx2_only% SFX_EXTRACT_ONLY=%extract_only% !targets! 2>_error.txt
if errorlevel 1 (
  echo.
  type _error.txt
  echo.
  echo %RED%[FAILED]%RESET% The build failed. The messages above are also in _error.txt.
  pause
  exit /b 1
)

rem * Keep _error.txt only if there were warnings
for %%S in (_error.txt) do if %%~zS equ 0 (del /q _error.txt) else (
  echo %YELLOW%[NOTE]%RESET% The compiler printed warnings; see _error.txt.
)

echo %GREEN%[PASSED]%RESET% Done: !targets!
pause
exit /b 0