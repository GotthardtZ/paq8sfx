---------------
VERSION HISTORY
---------------

Newest entries are at the bottom.
Dates are in YYYY.MM.DD format.

paq8sfx_v1 by Zoltán Gotthardt
2026.10.04
- Spin-off from paq8px_v216: a self-extracting archive creator for x86/x64 executables
- Two programs are built from the same source:
  - paq8sfx (-DFULL): the compressor, compresses/decompresses a single file
  - stub (-DSFX): the self-extractor
- Stub modes:
  - [no arguments]: extract all payload files and run the AutoRun file (the first payload)
  - -d input output: decompress a single paq8sfx archive
  - -a scriptfile: assemble a new self-extracting package from the stub and the files listed in the script
- Kept from paq8px:
  - Block type detection and transformation for x86/x64
  - NormalModel, MatchModel, SparseModel, SparseBitModel, ChartModel, SimilarityModel, ExeModel
  - Mixer (AVX2, SSE2 and scalar implementations)
  - SSE stage for the default and x86/x64 block types
  - Arithmetic encoder
- Removed from paq8px:
  - All other block types with their detection, transformations and models, the LSTM model
  - Multi-file archives and their command line options
- New archive format (single file, extension: .paq8sfx); archives are not compatible with paq8px
- Compression levels: -1 .. -12
- Build settings in SystemDefines.hpp:
  - AVX2_ONLY: compile the AVX2 code path only (smaller executables, AVX2 capable CPU required)
  - SFX_SILENT: strip the messages from the stub
- Added build scripts for MinGW-w64 (Windows), gcc and clang (Linux), and a Visual Studio solution
- Added roundtrip test scripts for Windows and Linux; they detect and report crashes of the tested programs
- Added documentation: README.md, test/README.md

paq8sfx_v1 by Zoltán Gotthardt
2026.10.08
- New: the libc-free stub (stub-free): the stub without the C/C++ runtime libraries
  - Linux x64: static executable, calls the kernel directly;
  - Windows x64: imports kernel32.dll only
      - No run-time dependencies (no GCC runtime DLLs, no VC++ Redistributable)
      - Built by 
          - the Makefile (make stub-free), 
          - the build-mingw-w64.cmd script (stub-free.exe) and the
          - the Visual Studio configuration Release-SFX-Free (x64)
- New: the extract-only stub (SFX_EXTRACT_ONLY): 
    running the package extracts all payloads and decompresses the .paq8sfx ones; no AutoRun, -d or -a
- New: paq8sfx -a stubfile scriptfile assembles a package from any stub (or package)
- AVX2_ONLY is set by the build instead of SystemDefines.hpp, and applies to the stubs only;
    paq8sfx always contains every SIMD code path and selects one at run time
- The stub's -a exits with code 1 when the package cannot be created
- New: Makefile (Linux; MinGW-w64, natively on Windows or cross-compiling from Linux)
  - targets: paq8sfx, stub, stub-free, test, test-free, upx, clean
  - settings: CXX, OUTDIR, AVX2_ONLY, SFX_EXTRACT_ONLY, EXTRA_CXXFLAGS, EXTRA_LDFLAGS, V
- AVX2_ONLY is opt-in everywhere (Makefile: AVX2_ONLY=1; build scripts: the variable at the top):
    by default every stub contains all SIMD code paths and runs on any x64 CPU
- Build scripts with stub settings (AVX2_ONLY, SFX_EXTRACT_ONLY)
- Visual Studio: the stub configurations don't define AVX2_ONLY
- paq8sfx.exe is always built statically on Windows (no dependencies)

