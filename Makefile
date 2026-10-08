# Makefile for paq8sfx (GNU make)
#
# The same sources are compiled twice with different settings:
#   build/paq8sfx   -DFULL -O3   the compressor
#   build/stub      -DSFX  -Os   the self-extractor
# Each variant has its own object tree (build/obj/full, build/obj/sfx), so the
# two never mix and both can be built in parallel.
#
#   make                 build both programs
#   make -j              ... in parallel
#   make paq8sfx | stub  build only one of them
#   make stub-free       build the libc-free stub (Linux x64 or Windows x64)
#   make test            build, then run the roundtrip test (needs ~1 GB RAM)
#   make test-free       run the roundtrip test against the libc-free stub
#   make upx             compress the stub with upx (do this before assembling)
#   make clean           remove everything this Makefile created
#   make CXX=clang++     use another compiler
#   make CXX=x86_64-w64-mingw32-g++   cross-compile the Windows .exe files
#   make V=1             show the full command lines
#
# stub-free is the libc-free stub: the same self-extractor, linked without the
# C/C++ runtime libraries. On Linux it calls the kernel directly, on Windows it
# imports kernel32.dll only. It is smaller and needs nothing at run time.
# See src/platform/.
#
# The target OS is taken from the compiler (-dumpmachine), not from the host,
# so cross-compiling works.
#
#   make AVX2_ONLY=1     build the stubs with the AVX2 code path only (smaller,
#                        but they need an AVX2 CPU)
#   make SFX_EXTRACT_ONLY=1   build extract-only stubs (no -a, -d, AutoRun);
#                        packages for them are assembled with paq8sfx -a
#
# AVX2_ONLY (default 0: every SIMD code path, runs on any x64 CPU) applies to
# the stubs only; paq8sfx always keeps every SIMD code path and picks one at
# run time. SFX_SILENT is set in
# src/SystemDefines.hpp. Editing that file (or any other header) rebuilds
# whatever depends on it.

CXX     ?= g++
SRCDIR  := src
OUTDIR  := build
OBJDIR  := $(OUTDIR)/obj

# Extra flags from the command line, e.g. make EXTRA_CXXFLAGS=-g STRIP=
EXTRA_CXXFLAGS ?=
EXTRA_LDFLAGS  ?=
STRIP          ?= -s

# ---------------------------------------------------------------- platform --

ifeq ($(OS),Windows_NT)
  # Windows host (MinGW-w64): needs sh, mkdir, rm, printf and cmp on the path
  # (Git for Windows: usr\bin). GCC's LTO wrapper needs to know what 'make' is
  # called.
  export MAKE
endif

# ---------------------------------------------------------------- compiler --

CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifeq ($(CXX_VERSION),)
  $(error Compiler '$(CXX)' not found. Install g++ or clang++, or set CXX=...)
endif

# Target OS, from the compiler: windows (MinGW-w64) or linux.
CXX_MACHINE := $(shell $(CXX) -dumpmachine 2>/dev/null)
ifneq ($(findstring mingw,$(CXX_MACHINE))$(findstring windows,$(CXX_MACHINE)),)
  TARGET := windows
  EXE    := .exe
else
  TARGET := linux
  EXE    :=
endif

# NOPIE: gcc may build position independent executables by default; the
# libc-free stub is not one. clang needs no option for that with -static.
ifneq ($(findstring clang,$(CXX_VERSION)),)
  LTO   := -flto
  NOPIE :=
else
  LTO   := -flto=auto
  NOPIE := -no-pie
  ifeq ($(TARGET),windows)
    # gcc bug: LTO does not work properly with -Os without this (MinGW-w64)
    LTO += -fno-declone-ctor-dtor
  endif
endif

# ------------------------------------------------------------------- flags --

# Floating point results must be reproducible: an archive made on one machine
# has to decompress on another. Never add -ffast-math or -Ofast.
SAFEFP := -fno-fast-math -ffp-contract=off

# -march=nocona is the x64 baseline. The SSE4.1/AVX2 code paths enable their
# instruction sets per function, so do NOT raise this to -march=native: that
# would let the compiler use newer instructions everywhere.
COMMON := -DNDEBUG $(SAFEFP) -m64 -march=nocona -mtune=generic -std=gnu++17 \
          -fno-exceptions -fno-rtti $(LTO) $(EXTRA_CXXFLAGS)

# The stubs: every SIMD code path, selected at run time (any x64 CPU), unless
# AVX2_ONLY=1: AVX2 code path only (smaller, needs an AVX2 CPU).
AVX2_ONLY ?= 0
SFX_SIMD  := $(if $(filter 1,$(AVX2_ONLY)),-DAVX2_ONLY)
# The stubs: extract-only mode if SFX_EXTRACT_ONLY=1 (default 0: full stub).
SFX_EXTRACT_ONLY ?= 0
SFX_MODE  := $(if $(filter 1,$(SFX_EXTRACT_ONLY)),-DSFX_EXTRACT_ONLY)

FULL_CXXFLAGS := -DFULL -O3 $(COMMON)
SFX_CXXFLAGS  := -DSFX  -Os $(SFX_SIMD) $(SFX_MODE) $(COMMON)

LDFLAGS := $(STRIP) -Wl,--gc-sections $(EXTRA_LDFLAGS)

# paq8sfx.exe (Windows) is linked statically, like build-mingw-w64.cmd and the
# Visual Studio Release-FULL configurations do: it runs without the GCC runtime
# DLLs. The normal stub stays dynamically linked (static would make it larger).
ifeq ($(TARGET),windows)
  FULL_LDFLAGS := -static $(LDFLAGS)
else
  FULL_LDFLAGS := $(LDFLAGS)
endif

# --- the libc-free stub -----------------------------------------------------
# Built like the normal stub, but without the features that need support code
# from the C/C++ runtime: stack protector, thread-safe initialization of local
# statics, _FORTIFY_SOURCE checks, unwind tables.
FREE_EXTRA := -DSFX_FREESTANDING -fno-stack-protector -fno-threadsafe-statics \
              -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
              -fno-asynchronous-unwind-tables -fno-unwind-tables
# libgcc holds helper routines the compiler may call (Linux build only).
LIBGCC := $(shell $(CXX) -print-libgcc-file-name 2>/dev/null)

ifeq ($(TARGET),windows)
  # -D_CRTIMP= makes the C library headers declare ordinary functions (not
  # DLL imports), so that the functions of src/platform/ are used.
  # Linked with kernel32.dll only; the program starts at sfx_entry
  # (Platform_Windows.cpp).
  FREE_EXTRA   += -D_CRTIMP=
  # No relocation table (no ASLR for the stub): 512 bytes less before UPX.
  FREE_LDFLAGS := -nostdlib -static -Wl,--gc-sections -Wl,-e,sfx_entry \
                  -Wl,--subsystem,console -Wl,--disable-dynamicbase \
                  -Wl,--disable-reloc-section -s $(EXTRA_LDFLAGS)
  # No libgcc on purpose. A function that uses more than 4 KB of stack needs
  # a helper (___chkstk_ms) from libgcc; without libgcc it fails to link. That
  # is wanted: the Visual Studio build could not link such a function either.
  FREE_LIBS    := -lkernel32
  PLATFORM_SRC := Platform_Windows.cpp
else
  # A fixed-address executable; it starts at _start (Platform_Linux.cpp).
  FREE_EXTRA   += -fno-pie -fno-pic
  FREE_LDFLAGS := -nostdlib -static $(NOPIE) -Wl,--gc-sections -Wl,-e,_start \
                  -Wl,--build-id=none -s $(EXTRA_LDFLAGS)
  FREE_LIBS    := $(LIBGCC)
  PLATFORM_SRC := Platform_Linux.cpp
endif

FREE_CXXFLAGS := -DSFX -Os $(SFX_SIMD) $(SFX_MODE) $(COMMON) $(FREE_EXTRA)

# ----------------------------------------------------------------- sources --
# The src/platform/ files belong only to the freestanding build and are kept
# out of the normal globs on purpose (they define malloc, _start, ... which
# would clash with the hosted runtime).
SRCS := $(sort $(wildcard $(SRCDIR)/*.cpp $(SRCDIR)/file/*.cpp $(SRCDIR)/model/*.cpp))
PLAT_SRCS := $(SRCDIR)/platform/CRuntime.cpp $(SRCDIR)/platform/$(PLATFORM_SRC)

FULL_OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/full/%.o)
SFX_OBJS  := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/sfx/%.o)
FREE_OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/free/%.o) \
             $(PLAT_SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/free/%.o)

PAQ8SFX  := $(OUTDIR)/paq8sfx$(EXE)
STUB     := $(OUTDIR)/stub$(EXE)
STUBFREE := $(OUTDIR)/stub-free$(EXE)

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

# ----------------------------------------------------------------- targets --

.PHONY: all paq8sfx stub stub-free test test-free upx clean
.DELETE_ON_ERROR:
.SUFFIXES:

all: paq8sfx stub

paq8sfx:   $(PAQ8SFX)
stub:      $(STUB)
stub-free: $(STUBFREE)

# The optimisation happens at link time (LTO), so the link step gets the full
# set of compiler flags as well.
$(PAQ8SFX): $(FULL_OBJS) $(OBJDIR)/full/.ldflags
	@echo "  LINK  $@"
	$(Q)$(CXX) $(FULL_CXXFLAGS) $(FULL_OBJS) $(FULL_LDFLAGS) -o $@

$(STUB): $(SFX_OBJS)
	@echo "  LINK  $@"
	$(Q)$(CXX) $(SFX_CXXFLAGS) $(SFX_OBJS) $(LDFLAGS) -o $@

$(STUBFREE): $(FREE_OBJS)
	@echo "  LINK  $@ (libc-free)"
	$(Q)$(CXX) $(FREE_CXXFLAGS) $(FREE_OBJS) $(FREE_LDFLAGS) $(FREE_LIBS) -o $@

$(OBJDIR)/full/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/full/.flags
	@echo "  CXX   $< (FULL)"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(FULL_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/sfx/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/sfx/.flags
	@echo "  CXX   $< (SFX)"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(SFX_CXXFLAGS) -MMD -MP -c $< -o $@

# The files of src/platform/ are compiled without link-time optimization and
# without builtins: otherwise the compiler may replace the body of memcpy by a
# call to memcpy, or a plain loop by a call to a C function the stub does not
# have, and the entry point may get lost. Everything else gets LTO.
# -ffunction-sections -fdata-sections: every function gets its own section, so
# that --gc-sections can drop the ones a stub variant does not use.
CRT_FILEFLAGS  := -fno-lto -fno-builtin -ffunction-sections -fdata-sections
PLAT_FILEFLAGS := -fno-lto -fno-builtin -ffunction-sections -fdata-sections
$(OBJDIR)/free/platform/CRuntime.o:   FREE_FILEFLAGS := $(CRT_FILEFLAGS)
$(OBJDIR)/free/platform/Platform_%.o: FREE_FILEFLAGS := $(PLAT_FILEFLAGS)

$(OBJDIR)/free/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/free/.flags
	@echo "  CXX   $< (FREE)"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(FREE_CXXFLAGS) $(FREE_FILEFLAGS) -MMD -MP -c $< -o $@

# Remember the compiler and flags of the last build; a change (another CXX,
# EXTRA_CXXFLAGS, ...) rebuilds that variant instead of mixing object files.
# The stamp is only rewritten when its content changes. Skipped for 'clean'.
define stamp
$(shell mkdir -p $(dir $(1)); \
  printf '%s\n' '$(subst ','\'',$(CXX_VERSION) $(2))' | cmp -s - $(1) || \
  printf '%s\n' '$(subst ','\'',$(CXX_VERSION) $(2))' > $(1))
endef

ifeq ($(filter clean,$(MAKECMDGOALS)),)
  $(call stamp,$(OBJDIR)/full/.flags,$(FULL_CXXFLAGS))
  $(call stamp,$(OBJDIR)/full/.ldflags,$(FULL_LDFLAGS))
  $(call stamp,$(OBJDIR)/sfx/.flags,$(SFX_CXXFLAGS))
  $(call stamp,$(OBJDIR)/free/.flags,$(FREE_CXXFLAGS) $(CRT_FILEFLAGS) $(PLAT_FILEFLAGS))
endif

# The roundtrip test copies both programs from build/ itself.
test: all
ifeq ($(OS),Windows_NT)
	cd test && cmd //c roundtrip_test_windows.cmd nopause
else
	cd test && bash roundtrip_test_linux.sh nopause
endif

# Run the roundtrip test with the libc-free stub standing in as 'stub'.
# (The test copies build/paq8sfx and build/stub into test/ at startup.)
# Note: this overwrites build/stub; 'make stub' rebuilds it if needed.
test-free: paq8sfx stub-free
	$(Q)cp $(STUBFREE) $(STUB)
ifeq ($(OS),Windows_NT)
	cd test && cmd //c roundtrip_test_windows.cmd nopause
else
	cd test && bash roundtrip_test_linux.sh nopause
endif

upx: $(STUB)
	upx --best --ultra-brute $(STUB)

clean:
	rm -rf $(OBJDIR)
	rm -f $(PAQ8SFX) $(STUB) $(STUBFREE)

# Header dependencies, written by the compiler (-MMD)
-include $(FULL_OBJS:.o=.d) $(SFX_OBJS:.o=.d) $(FREE_OBJS:.o=.d)
