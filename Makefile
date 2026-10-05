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
#   make test            build, then run the roundtrip test (needs ~1 GB RAM)
#   make upx             compress the stub with upx (do this before assembling)
#   make clean           remove everything this Makefile created
#   make CXX=clang++     use another compiler
#   make V=1             show the full command lines
#
# AVX2_ONLY and SFX_SILENT are set in src/SystemDefines.hpp, not here. Editing
# that file (or any other header) rebuilds whatever depends on it.

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
  # MinGW-w64; run from a shell that has sh, mkdir and rm (MSYS2, Git Bash).
  EXE := .exe
  # GCC's LTO wrapper needs to know what 'make' is called here.
  export MAKE
else
  EXE :=
endif

# ---------------------------------------------------------------- compiler --

CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifeq ($(CXX_VERSION),)
  $(error Compiler '$(CXX)' not found. Install g++ or clang++, or set CXX=...)
endif

ifneq ($(findstring clang,$(CXX_VERSION)),)
  LTO := -flto
else
  LTO := -flto=auto
  ifeq ($(OS),Windows_NT)
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

FULL_CXXFLAGS := -DFULL -O3 $(COMMON)
SFX_CXXFLAGS  := -DSFX  -Os $(COMMON)

LDFLAGS := $(STRIP) -Wl,--gc-sections $(EXTRA_LDFLAGS)

# ----------------------------------------------------------------- sources --

SRCS := $(sort $(wildcard $(SRCDIR)/*.cpp $(SRCDIR)/file/*.cpp $(SRCDIR)/model/*.cpp))

FULL_OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/full/%.o)
SFX_OBJS  := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/sfx/%.o)

PAQ8SFX := $(OUTDIR)/paq8sfx$(EXE)
STUB    := $(OUTDIR)/stub$(EXE)

ifeq ($(V),1)
  Q :=
else
  Q := @
endif

# ----------------------------------------------------------------- targets --

.PHONY: all paq8sfx stub test upx clean
.DELETE_ON_ERROR:
.SUFFIXES:

all: paq8sfx stub

paq8sfx: $(PAQ8SFX)
stub:    $(STUB)

# The optimisation happens at link time (LTO), so the link step gets the full
# set of compiler flags as well.
$(PAQ8SFX): $(FULL_OBJS)
	@echo "  LINK  $@"
	$(Q)$(CXX) $(FULL_CXXFLAGS) $(FULL_OBJS) $(LDFLAGS) -o $@

$(STUB): $(SFX_OBJS)
	@echo "  LINK  $@"
	$(Q)$(CXX) $(SFX_CXXFLAGS) $(SFX_OBJS) $(LDFLAGS) -o $@

$(OBJDIR)/full/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/full/.flags
	@echo "  CXX   $< (FULL)"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(FULL_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/sfx/%.o: $(SRCDIR)/%.cpp $(OBJDIR)/sfx/.flags
	@echo "  CXX   $< (SFX)"
	@mkdir -p $(@D)
	$(Q)$(CXX) $(SFX_CXXFLAGS) -MMD -MP -c $< -o $@

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
  $(call stamp,$(OBJDIR)/sfx/.flags,$(SFX_CXXFLAGS))
endif

# The roundtrip test copies both programs from build/ itself.
test: all
ifeq ($(OS),Windows_NT)
	cd test && cmd //c roundtrip_test_windows.cmd nopause
else
	cd test && bash roundtrip_test_linux.sh nopause
endif

upx: $(STUB)
	upx --best --ultra-brute $(STUB)

clean:
	rm -rf $(OBJDIR)
	rm -f $(PAQ8SFX) $(STUB)

# Header dependencies, written by the compiler (-MMD)
-include $(FULL_OBJS:.o=.d) $(SFX_OBJS:.o=.d)
