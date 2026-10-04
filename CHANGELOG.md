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
