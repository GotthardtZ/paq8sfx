#!/bin/sh
# Builds paq8sfx, stub and stub-free (the libc-free stub) for x64 Linux with clang.
#
#   sh build-linux-with-clang.sh
#
# The programs are created in this folder (build). The Makefile in the project
# root builds the same programs with the same settings; see README.md.
#
# Requirements: clang
#   Debian, Ubuntu:  sudo apt install clang
#   Arch:            sudo pacman -S clang

set -e
cd "$(dirname "$0")"

# Settings of the stubs (paq8sfx itself has no settings):
#   SFX_SIMD  empty: every SIMD code path, selected at run time; runs on any x64 CPU.
#             -DAVX2_ONLY: AVX2 code path only - smaller, needs an AVX2 CPU.
#   SFX_MODE  empty: full stub (extract + AutoRun, -d, -a).
#             -DSFX_EXTRACT_ONLY: extract-only stub; assemble its packages with paq8sfx -a.
SFX_SIMD=""
SFX_MODE=""

CXX=clang++
LTO=-flto
SOURCES="../src/*.cpp ../src/file/*.cpp ../src/model/*.cpp"

# Floating point results must be reproducible: never add -ffast-math or -Ofast.
# -march=nocona is the x64 baseline. Do not raise it: the SSE4.1 and AVX2 code
# paths enable their instruction sets themselves.
COMMON="-DNDEBUG -fno-fast-math -ffp-contract=off -m64 -march=nocona -mtune=generic -std=gnu++17 -fno-exceptions -fno-rtti"

echo "Building paq8sfx ..."
$CXX -DFULL -O3 $COMMON $LTO $SOURCES -s -Wl,--gc-sections -o paq8sfx

echo "Building stub ..."
$CXX -DSFX $SFX_SIMD $SFX_MODE -Os $COMMON $LTO $SOURCES -s -Wl,--gc-sections -o stub

# stub-free: the stub without the C/C++ runtime libraries (see src/platform/).
# Its two runtime files are compiled first, without link-time optimization and
# without builtins; -ffunction-sections -fdata-sections lets the linker drop
# the functions the stub does not use. The rest is built like the normal stub.
echo "Building stub-free ..."
FREE="-DSFX $SFX_SIMD $SFX_MODE -DSFX_FREESTANDING -Os $COMMON -fno-stack-protector -fno-threadsafe-statics -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pie -fno-pic"
RUNTIME="-fno-lto -fno-builtin -ffunction-sections -fdata-sections"
$CXX $FREE $RUNTIME -c ../src/platform/CRuntime.cpp -o _crt.o
$CXX $FREE $RUNTIME -c ../src/platform/Platform_Linux.cpp -o _platform.o
$CXX $FREE $LTO $SOURCES _crt.o _platform.o -nostdlib -static -Wl,--gc-sections -Wl,-e,_start -Wl,--build-id=none -s "$($CXX -print-libgcc-file-name)" -o stub-free
rm -f _crt.o _platform.o

echo "Done: paq8sfx, stub and stub-free are in $(pwd)"
