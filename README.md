# PAQ8SFX – A self-extracting archive creator with the power of PAQ8PX

## About

**PAQ** is a family of experimental, high-end lossless data compression programs.
`paq8sfx` is a spin-off from the well-known [paq8px](https://github.com/hxim/paq8px) data compressor, designed specifically for creating self-extracting archives.

For compressing executables it is almost as good as paq8px, but it is faster, has a smaller memory footprint and - most importantly - its decompressor (the *stub*) is much smaller than the executable of the full paq8px compressor.

The project consists of two programs built from the same source:

| Program | Role |
| --- | --- |
| `paq8sfx` | The compressor. Compresses (and decompresses) a single file, and assembles packages from a stub. It is not part of the final package. |
| `stub` | The self-extractor. Decompresses `paq8sfx` archives, carries the payload files and assembles new self-extracting packages. This is what ends up in the final package. A smaller [extract-only stub](#the-extract-only-stub) can be built for packages that only need to unpack themselves. |

Two terms are used throughout this document: an *archive* is a single compressed file (`.paq8sfx`), a *package* is a self-extracting executable - the stub with its payload files attached.

## When to use it

`paq8sfx` is most suitable for creating a self-extracting package for compression benchmarks in which the size of the decompressor's executable is counted in the size of the whole compressed package.

For decompressors with small executables [UPX](https://upx.github.io/) will do fine, but for larger executables (such as cmix and its derivatives, or paq8px) `paq8sfx` is the better choice: the stub costs about 24-31 KB (the libc-free stub after UPX, see [Stub size](#stub-size)), and that is recovered as soon as `paq8sfx` saves more than that over UPX.

It is not a general-purpose archiver:

- it does not compress folders, and each archive holds exactly one file,
- its models are tuned for x86/x64 executables; other data is compressed with the general (binary) models only,
- like all PAQ-style compressors, decompression takes as much time and memory as compression.

## Quick start

The example builds `package.exe`, which carries a compressed decompressor (`mydecomp.exe`) and its data file, unpacks both when started, and runs the decompressor.
File names are for Windows; on Linux drop the `.exe`, use a shell script instead of the `.cmd` file and prefix the programs with `./`.

**1. Compress the large file(s)**

```
paq8sfx -12 mydecomp.exe
```

This creates `mydecomp.exe.paq8sfx`.

**2. Write the AutoRun script** - it is executed after the package has unpacked its files. Here it is called `run.cmd`:

```
@echo off
package -d mydecomp.exe.paq8sfx mydecomp.exe
mydecomp data.bin.compressed data.bin
```

**3. Write the assembly script** - a text file listing the output name and the files to pack. Here it is called `script.txt`:

```
package.exe
run.cmd
mydecomp.exe.paq8sfx
data.bin.compressed
```

**4. Assemble the package**

```
stub -a script.txt
```

**5. Ship `package.exe`** - nothing else is needed. Running it without arguments recreates `run.cmd`, `mydecomp.exe.paq8sfx` and `data.bin.compressed` in the current folder and then runs `run.cmd`.

> [!NOTE]
> The AutoRun script calls the package by its file name (`package -d ...`) to decompress the `.paq8sfx` files. If the package is renamed afterwards, the script will not find it.

## Command line reference

### paq8sfx (the compressor)

```
paq8sfx -LEVEL input [output]      compress
paq8sfx -d archive [output]        decompress
paq8sfx -a stubfile scriptfile     assemble a package
```

- `LEVEL` is `1` to `12`, see [Compression levels](#compression-levels-and-memory-use).
- Started without arguments, `paq8sfx` shows this summary.
- When compressing, the default output is the input name with `.paq8sfx` appended.
- When decompressing, the default output is the archive name without the `.paq8sfx` extension.
- The default output is created in the current folder, also when the input is somewhere else.
- `output` may be a file name or an existing folder.
- The level is stored in the archive; it does not have to be given again for decompression.
- `-a` does the same as the stub's `-a` (see below), but with any stub file - a full or an [extract-only](#the-extract-only-stub) stub, or a package (its payload is dropped). It is the only way to assemble packages for an extract-only stub.

### stub (the self-extractor)

The stub has three modes. A package assembled from the stub understands the same commands as the stub itself.

| Command | What it does |
| --- | --- |
| `stub -a scriptfile` | **Assemble.** Creates a new package: a copy of the stub followed by the files listed in the script. |
| `package` (no arguments, or `-x`) | **Extract and run.** Writes all payload files to the current folder, then runs the AutoRun file. |
| `package -d input output` | **Decompress** a single `paq8sfx` archive. |

Because a package contains the complete stub, a package can itself assemble further packages with `-a` (the test scripts do exactly that).

Exit codes: `-a` and `-d` exit with 0 on success and 1 on failure. Extract-and-run exits with 1 if the package has no payload or a file could not be written; otherwise with 0 on Windows (the exit code of the AutoRun file is not passed on), while on Linux the AutoRun script replaces the stub and its exit code is what the caller sees.

### The extract-only stub

A stub built with `SFX_EXTRACT_ONLY` (see [Build settings](#build-settings)) has a single mode: running the package (with no arguments, or with `-x`) unpacks it.

- Payloads whose name ends in `.paq8sfx` (in any letter case) are decompressed straight to the name without that extension; no `.paq8sfx` file is left behind.
- All other payloads are written as they are.
- There is no AutoRun, no `-d` and no `-a`. Assemble its packages with `paq8sfx -a stubfile scriptfile`; the second line of the script is then an ordinary payload.
- The exit code is 0 if everything was extracted, 1 otherwise.

It is about 0.5-1 KB smaller after UPX than the full stub. Its main use is convenience: a self-extracting archive that anyone can run, without writing an AutoRun script.

Example - package two files, one of them compressed (`stub-extract.exe` is an extract-only stub, renamed so it can sit next to the full one):

```
paq8sfx -12 mydecomp.exe
paq8sfx -a stub-extract.exe script.txt
```

with `script.txt`:

```
package.exe
mydecomp.exe.paq8sfx
readme.txt
```

Running `package.exe` creates `mydecomp.exe` and `readme.txt`.

Each compressed payload is decompressed straight from the package in memory, one after the other, and its memory is released before the next one starts - so the memory needed is that of the largest archive, not the sum.

### The assembly script

A plain text file with one file name per line. File names may contain spaces, except for the AutoRun file on Windows; blank lines are ignored. A script that lists more files than a package can hold (see [Limits](#limits)) is rejected.

| Line | Meaning |
| --- | --- |
| 1 | Name of the package to create. |
| 2 | The AutoRun file: the first payload, and the one that is run after extraction. For an extract-only stub, an ordinary payload. |
| 3 and later | Further payload files. |

Things to know:

- Payload files are stored **as they are**. The stub does not compress them - compress the large ones with `paq8sfx` first and decompress them from the AutoRun script with `-d`.
- Names are stored as written in the script, and files are extracted to the current folder under those names. Use plain file names without a path.
- On Windows the AutoRun file is started through the command interpreter, so it can be a `.cmd`/`.bat` file or an executable. On Linux it is run with `/bin/sh`, so it must be a shell script; all extracted files get the executable permission.
- Existing files with the same names are overwritten without asking.

## Compression levels and memory use

Similarly to paq8px, `paq8sfx` has several compression levels. A higher level uses more memory and may give a smaller archive.
It is not necessarily the highest level that gives the best result for a given executable - experiment with levels 9 to 12 to see which works best.

| Level | Memory use |
| --- | ---: |
| `-1` | 146 MB |
| `-2` | 153 MB |
| `-3` | 167 MB |
| `-4` | 195 MB |
| `-5` | 250 MB |
| `-6` | 362 MB |
| `-7` | 585 MB |
| `-8` | 1'031 MB |
| `-9` | 1'923 MB |
| `-10` | 3'707 MB |
| `-11` | 7'275 MB |
| `-12` | 14'410 MB |

> [!WARNING]
> Decompression needs the same amount of memory as compression. A package compressed with `-12` needs over 14 GB of free memory on the machine where it is unpacked.

## Benchmark results

The tests are about compressing the executables of data compressors (x86/x64). All tests were run on Windows.

> [!NOTE]
> The benchmark figures have not been updated for the current source yet: the paq8sfx rows include a 40'448-byte stub (see [Stub size](#stub-size) for the current ones), and the compressed sizes may differ slightly.

- **UPX**: `upx --best --ultra-brute`, UPX 5.2.0. The result is a self-extracting executable; its decompression stub is already included in the compressed size.
- **paq8sfx**: paq8sfx_v1, level `-12`. The total includes the UPX-compressed generic glibc-free static Windows x64 stub (26'600 bytes, see [Stub size](#stub-size)), i.e. it is the size of the complete self-extracting package.
- **paq8px**: paq8px_v216, level `-12`, shown for reference. Its result is not self-extracting: it needs the 1.3 MB paq8px executable to decompress, which is not counted here.

**paq8px_v216.exe, [paq8px v216](https://github.com/hxim/paq8px/releases/tag/v216)** - 1'293'312 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 510'976 | included | 510'976 | 39.5% | 7 sec |
| paq8sfx -12 | 323'052 | 25'600 | **348'652** | 26.9% | 120 sec |
| paq8px -12 | 314'761 | - | 314'761 | 24.3% | 270 sec |

**bsc.exe, [bsc 3.3.12](https://github.com/IlyaGrebnov/libbsc/releases/tag/v3.3.12)** - 6'114'304 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 957'440 | included | 957'440 | 15.7% | 87 sec |
| paq8sfx -12 | 667'328 | 25'600 | **692'928** | 11.3% | 477 sec |
| paq8px -12 | 646'119 | - | 646'119 | 10.6% | 1'008 sec |

**mcm.exe, [mcm 0.83](https://encode.su/threads/2127-MCM-LZP?p=43220&viewfull=1#post43220), x64** - 2'444'886 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 1'478'742 | included | 1'478'742 | 60.5% | 8 sec |
| paq8sfx -12 | 268'760 | 25'600 | **394'360** | 12.0% | 193 sec |
| paq8px -12 | 257'256 | - | 257'256 | 10.5% | 366 sec |

**zpaq64.exe [zpaq 7.15](https://github.com/zpaq/zpaq/releases/tag/7.15), 64-bit** - 1'125'376 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 326'144 | included | 326'144 | 29.0% | 8 sec |
| paq8sfx -12 | 201'466 | 25'600 | **227'066** | 20.2% | 103 sec |
| paq8px -12 | 197'083 | - | 197'083 | 17.5% | 215 sec |

Even with its stub counted, `paq8sfx` is significantly better than UPX in every test and close behind the full-fledged paq8px compressor, in about half of its time.

### Stub size

Size of the stub in bytes after `upx --best --ultra-brute` (UPX 5.2.0), which is how it is meant to be shipped.

| Toolchain | Stub | Default | `AVX2_ONLY` | Extract-only | Extract-only + `AVX2_ONLY` | Needs at run time |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Windows, MinGW-w64 (GCC 15.2.0) | libc-free | *25'600* | 24'576 | 25'088 | **23'552** | `kernel32.dll` only |
| | normal | 41'472 | 40'448 | 40'960 | 39'936 | GCC runtime DLLs |
| Windows, VS 2022 (MSVC 19.44.35216) | libc-free | 30'720 | 29'696 | 29'696 | 28'672 | `kernel32.dll` only |
| | normal | 36'352 | 34'816 | 35'840 |  34'304 | VC++ Redistributable |
| | normal, x86 | 32'768 | n/a | 32'768 | n/a | VC++ Redistributable |
| Linux, gcc 13.3.0 | libc-free | *25'992* | 24'984 | 25'396 | **24'408** | nothing (static) |
| | normal | 30'948 | 29'624 | 29'332 | 29'104 | system C/C++ libraries |
| Linux, clang 18.1.3 | libc-free | 30'744 | 29'164 | 30'320 | 28'724 | nothing (static) |
| | normal | 35'060 | 33'952 | 33'304 | 33'736 | system C/C++ libraries |

- The rows: *libc-free* is the stub without any C/C++ runtime library (see [The libc-free stub](#the-libc-free-stub)), *normal* is the ordinary build. All are x64 unless noted.
- The columns: *Default* is the full stub as every build method produces it. `AVX2_ONLY` is a [build setting](#build-settings); *Extract-only* is the [extract-only stub](#the-extract-only-stub).
- `n/a` means that this variant does not exist.

The normal stubs are dynamically linked; `paq8sfx.exe`, the compressor, is statically linked on Windows - see [Runtime dependencies](#runtime-dependencies).

## How to compile

A C++17 compiler for x64 is required. The same source is compiled twice: once with `-DFULL` (gives `paq8sfx`) and once with `-DSFX` (gives `stub`). The build scripts and the Makefile do both. The [libc-free stub](#the-libc-free-stub) is a third build of the stub (`-DSFX -DSFX_FREESTANDING`).

### Windows, MinGW-w64

Run `build\build-mingw-w64.cmd`. It creates `paq8sfx.exe`, `stub.exe` and `stub-free.exe` (the libc-free stub) in the `build` folder.
The script expects MinGW-w64 in `c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1`; edit the path in the script if yours is elsewhere.

### Windows, Visual Studio

1. Open `paq8sfx.sln`. Select either `x64` or `x86` as the target platform. No configuration defines `AVX2_ONLY` (see [Build settings](#build-settings)).
2. Select the `Release-FULL` configuration. Build.
3. Select the `Release-SFX` configuration. Build.
4. For the libc-free stub: select `Release-SFX-Free` with the `x64` platform. Build. It creates `stub-free.exe`. There is no x86 libc-free stub: with `x86` selected, this configuration builds nothing.

### Linux

Run one of the scripts in the `build` folder:

```
sh build/build-linux-with-gcc.sh       # or: sh build/build-linux-with-clang.sh
```

It creates `paq8sfx`, `stub` and `stub-free` (the libc-free stub) in the `build` folder.

### Linux (and MinGW-w64), with make

Alternatively, use the `Makefile` in the project root (GNU make, with gcc or clang). It uses the same compiler settings as the scripts, but compiles the source files separately and in parallel, and after a change it rebuilds only what is affected.

```
make -j              # build paq8sfx and stub into the build folder
make test            # build, then run the roundtrip test
```

| Command | What it does |
| --- | --- |
| `make` | Builds both programs. Add `-j` to compile in parallel. |
| `make paq8sfx`, `make stub` | Builds only one of them. |
| `make test` | Builds both, then runs the roundtrip test (see [How to test](#how-to-test)). |
| `make stub-free` | Builds the libc-free stub, `build/stub-free`. |
| `make test-free` | Runs the roundtrip test with the libc-free stub. It copies `stub-free` over `build/stub`. |
| `make upx` | Compresses the stub with `upx --best --ultra-brute`. |
| `make clean` | Removes the object files (`build/obj`) and the executables. |
| `make CXX=clang++` | Builds with clang instead of gcc. |
| `make CXX=x86_64-w64-mingw32-g++` | Cross-compiles the Windows `.exe` files on Linux. |
| `make OUTDIR=dir` | Uses another output folder instead of `build` (e.g. for a second compiler). |
| `make AVX2_ONLY=1` | Builds the stubs with the AVX2 code path only: smaller, but they need an AVX2 CPU (see [Build settings](#build-settings)). |
| `make SFX_EXTRACT_ONLY=1` | Builds [extract-only](#the-extract-only-stub) stubs. Combine with `OUTDIR=...` to keep the full stubs, e.g. `make stub-free SFX_EXTRACT_ONLY=1 OUTDIR=build-x`. |
| `make V=1` | Shows the full compiler command lines. |

Changing the compiler, a setting such as `AVX2_ONLY`, or `src/SystemDefines.hpp` is picked up automatically; there is no need to run `make clean` first.

The target (Linux or Windows) is taken from the compiler, so the same rules cross-compile with MinGW-w64; this was tested with Ubuntu's MinGW-w64 GCC 13 and the results run under Wine. It also runs natively on Windows, see below. For Windows the Makefile links `paq8sfx.exe` statically (`-static`), as `build-mingw-w64.cmd` does.

### Windows, MinGW-w64, with make

The Makefile needs MinGW-w64 (it brings `g++` and `mingw32-make`) and a few Unix tools: `sh`, `mkdir`, `rm`, `printf` and `cmp`. Git for Windows has them all, in its `usr\bin` folder. It is installed either on its own (it is the package that provides Git Bash) or by Visual Studio, when the "Git" individual component is selected in the Visual Studio Installer.

From a Command Prompt, in the project root (adjust the two paths to your installation):

```
set PATH=c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1\bin;C:\Program Files\Git\usr\bin;%PATH%
mingw32-make -j%NUMBER_OF_PROCESSORS%
mingw32-make test
```

Or from Git Bash:

```
export PATH="/c/mingw/winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1/bin:$PATH"
mingw32-make -j$(nproc)
```

All targets and settings of the table above work the same way; write `mingw32-make` instead of `make`. This was tested with MinGW-w64 GCC 15.2.0 (WinLibs) and the Git for Windows installed by Visual Studio 2022.

- Use `mingw32-make`, not the `make` of MSYS or MSYS2.
- If the build stops with `Compiler 'g++' not found` although `g++` is on the path, `mingw32-make` did not find `sh.exe`: check the Git `usr\bin` part of the path.

### Making the stub smaller

Use the [libc-free stub](#the-libc-free-stub), and compress it with UPX **before** assembling a package with it:

```
upx --best --ultra-brute stub-free.exe
```

With the Makefile, `make upx` compresses the normal stub (`build/stub`).

### The libc-free stub

The libc-free stub is the same self-extractor - same modes, same package and archive format - built without the C and C++ runtime libraries. It is smaller, and a package made from it has no run-time dependencies. It exists for x64 Linux and x64 Windows.

The rest of the program still calls the usual C library functions (`fopen`, `malloc`, `memcpy`, ...). In this stub they come from `src/platform/`:

| File | Content |
| --- | --- |
| `Platform.hpp` | The eight functions the stub needs from the operating system: open, close, read, write and seek a file, allocate and free memory, exit. |
| `CRuntime.cpp` | The C library functions the stub calls (memory, strings, files, `new`/`delete`). Written once for both operating systems, using only the eight functions above. |
| `Platform_Linux.cpp` | The eight functions as Linux system calls, the entry point of the program, and the C functions that only the Linux code calls (`stat`, `mkdir`, `chmod`, `execv`). |
| `Platform_Windows.cpp` | The eight functions as `kernel32.dll` calls, the entry point of the program (it also splits the command line into arguments), and the C functions that only the Windows code calls (`_wfopen`, `_wstat64`, `system`). Built with MinGW-w64 and with Visual Studio. |

How it differs from the normal stub:

- It prints nothing at all: no messages and no progress display (see `SFX_SILENT` under [Build settings](#build-settings)).
- On Windows the command line is split at spaces and tabs, and double quotes keep an argument with spaces together. There is no way to write a double quote inside an argument; none is needed, because the arguments are file names.
- File access is not buffered, and every memory block comes straight from the operating system. This keeps the code small; the stub spends its time in the model.

Build notes:

- The files in `src/platform/` are compiled only into the libc-free stub; in every other build they are empty.
- `CRuntime.cpp` and the `Platform_*.cpp` files are compiled without link-time optimization (the compiler could otherwise replace the body of `memcpy` by a call to `memcpy`); the rest of the stub uses it.
- No function of the stub may use more than 4 KB of stack. On Windows a larger stack frame needs a helper function (`__chkstk`) from the C runtime. The MinGW-w64 build does not link libgcc, so such a function fails to link (`undefined reference to ___chkstk_ms`) instead of slipping through. Large buffers are therefore `static` or allocated.
- Visual Studio: the `Release-SFX-Free` configuration contains all required settings, with a comment explaining each. Inline function expansion is `/Ob1`, which gave the smallest stub after UPX.

### Build settings

Three settings affect the stub. None of them changes the archive or the package format.

**`AVX2_ONLY`** - set by the build, for the stub only; off by default.

| | Not defined (the default) | Defined |
| --- | --- | --- |
| Stub | SSE2, SSE4.1, AVX2 and plain C++ code paths are all compiled in, and the best one is selected at run time. Runs on any x64 CPU. | Only the AVX2 code path is compiled in: about 1.0-1.6 KB smaller after UPX, but it **requires a CPU with AVX2**. |

`paq8sfx` (the compressor) is always built with every code path and selects one at run time, so it runs on any x64 CPU. All code paths give bit-identical results, so an archive made on a CPU without AVX2 decompresses with an `AVX2_ONLY` stub, and vice versa.

How to build the stub with `AVX2_ONLY`:

| Build method | Change |
| --- | --- |
| Makefile | `make AVX2_ONLY=1` |
| `build-mingw-w64.cmd` | Set `sfxsimd=-DAVX2_ONLY` near the top of the script. |
| `build-linux-with-*.sh` | Set `SFX_SIMD="-DAVX2_ONLY"` near the top of the script. |
| Visual Studio | Add `AVX2_ONLY` to *C/C++ → Preprocessor → Preprocessor Definitions* of the `Release-SFX` / `Release-SFX-Free` x64 configuration. Do not add it to the x86 configurations. |

> [!WARNING]
> With `AVX2_ONLY` defined there is no check for AVX2 at run time. On a CPU without AVX2 the stub stops without any message as soon as it starts to decompress (illegal instruction). Use it only when every machine that will run the package has AVX2.

**`SFX_EXTRACT_ONLY`** - set by the build, for the stub only; off by default.

Builds the [extract-only stub](#the-extract-only-stub) instead of the full one. The full stub is needed whenever a package must assemble further packages or decompress with `-d` from an AutoRun script (e.g. for the Hutter Prize).

| Build method | Change |
| --- | --- |
| Makefile | `make SFX_EXTRACT_ONLY=1` |
| `build-mingw-w64.cmd` | Set `sfxmode=-DSFX_EXTRACT_ONLY` near the top of the script. |
| `build-linux-with-*.sh` | Set `SFX_MODE="-DSFX_EXTRACT_ONLY"` near the top of the script. |
| Visual Studio | Add `SFX_EXTRACT_ONLY` to *C/C++ → Preprocessor → Preprocessor Definitions* of the `Release-SFX` or `Release-SFX-Free` configuration. |

**`SFX_SILENT`** - set in `src/SystemDefines.hpp`.

| | Defined (the default) | Commented out |
| --- | --- | --- |
| Stub | Prints no messages of its own, not even error messages. The normal stub still shows the progress of a decompression (`Extracting ... done`) and reports a file it cannot open or create. | Also reports what it is doing and what went wrong when extracting and assembling. Useful while putting a package together. |

The libc-free stub prints nothing either way (it has no `printf`); use the normal stub to see the messages.

### Archive compatibility

Archives are created and read by the same model code, so always use a `paq8sfx` and a `stub` built from the same version of the source.

> [!WARNING]
> An archive can only be decompressed by a stub built from the same version of the source. The archive header has no version field, so a mismatch is not detected: decompression silently produces wrong output.

### Runtime dependencies

`paq8sfx` (the compressor):

- Windows: `paq8sfx.exe` is statically linked by every build method - MinGW-w64 (`build-mingw-w64.cmd` and the Makefile, with `-static`) and Visual Studio (`Release-FULL`, x64 and x86, with the static runtime library, `/MT`). It needs neither the GCC runtime DLLs nor the VC++ Redistributable.
- Linux: `paq8sfx` is dynamically linked against the system C and C++ runtime libraries.

The stub, and therefore every package assembled from it:

- The libc-free stub needs nothing: on Linux it is a static executable that calls the kernel directly, on Windows it imports `kernel32.dll` only. A package assembled from it runs on any x64 Linux or Windows machine without extra files.
- The normal stub of the MinGW-w64 build links the GCC runtime dynamically. `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll` must be reachable (on the `PATH` or next to the executable) on the machine where the package runs.
- The normal stub of the MSVC 14 (Visual Studio 2022) build (`Release-SFX`, x64 and x86) links the VC++ runtime dynamically (`/MD`). The [Microsoft Visual C++ 2015–2022 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version) or a compatible higher version must be installed on the machine where the package runs.
- The normal stub of the Linux builds is dynamically linked against the system C and C++ runtime libraries.

## How to test

The `test` folder contains a roundtrip test for Windows and for Linux. It compresses, packages, unpacks and verifies a pair of small files, and reports crashes of the tested programs. See [test/README.md](test/README.md) for how to run it and how to read its output. With the Makefile, `make test` builds both programs and runs it, and `make test-free` runs it with the libc-free stub. Without the Makefile, test the libc-free stub by copying `stub-free` (`stub-free.exe`) over `stub` (`stub.exe`) in the `build` folder. The test does not cover the extract-only stub.

## How it works

### What was kept from paq8px

`paq8sfx` is a spin-off from paq8px_v216. It adds the self-extracting capabilities and removes much of the logic to keep the stub small.

What remained:

- Block type detection and transformation for x86/x64 code. The relative addresses of CALL/JMP instructions are converted to absolute addresses (E8/E9 transform); the transformation is verified during compression and skipped when it would not be reversible.
- The general (binary) models: NormalModel (order 0-14 contexts), MatchModel, SparseModel, SparseBitModel, ChartModel and the SimilarityModel pair.
- The ExeModel for x86/x64 code.
- The mixer (neural network) with AVX2, SSE2 and plain C++ implementations.
- The SSE (secondary symbol estimation) stage for the default and the x86/x64 block types.
- The arithmetic encoder.

What was removed: all other block types with their detection, transformations and models (text, images, audio, and so on), the LSTM model, and multi-file archives with their command line options.

For an introduction to context mixing, see the paq8px README.

### Archive format (`.paq8sfx`)

| Content | Size |
| --- | --- |
| The text `paq8sfx` | 7 bytes |
| Compression level | 1 byte |
| Arithmetic-coded stream: file size, then for each block its type, size and content | rest |

There is no version field; see [Archive compatibility](#archive-compatibility).

### Package format

A package is the stub executable with the payload files and a footer appended:

```
[stub executable] [payload 1] ... [payload n] [names] [sizes] [count] [footer size]
```

| Footer part | Content |
| --- | --- |
| names | The payload file names in listed order, each terminated by a zero byte. |
| sizes | One 4-byte little-endian size per payload. |
| count | Number of payloads, 1 byte. |
| footer size | Size of the whole footer, 1 byte. `0` means there is no payload (a bare stub). |

At startup the stub reads its own executable file, locates the footer from the last byte and works backwards. If the last bytes do not form a valid footer (the names and sizes must fit the file), the file is taken for a bare stub.

## Limits

| Limit | Value |
| --- | --- |
| Files per `.paq8sfx` archive | 1 |
| Size of a file to compress | below 2 GB |
| Payload files per package | 8 |
| Size of one payload file | below 4 GB |
| Footer size | 255 bytes: the sum of all payload name lengths, plus 5 bytes per payload, plus 2 |
| Platforms | x64 Windows and Linux (Visual Studio can also build x86, but only the normal stub) |

## Todo and outlook

- The emphasis was on keeping the code similar to paq8px, so that `paq8sfx` can still receive model updates from paq8px. On the other hand, keeping this similarity makes it difficult to truly optimize `paq8sfx` into a dedicated self-extracting archive or an executable compressor such as UPX.
- The full stub does not pass on the exit code of the AutoRun file on Windows.

## Security

`paq8sfx` is experimental software and is **not safe to use on untrusted input**. The stub does not protect against buffer overruns: a damaged or deliberately crafted archive or package may crash it or worse. Only run packages you created yourself or received from a source you trust.

## Copyright

`paq8sfx` is derived from paq8px; the copyright of the inherited code belongs to the respective paq8px contributors (see the paq8px README for the list).

See `CHANGELOG.md` for the version history.

## License

> This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
> This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the [GNU General Public License](http://www.gnu.org/copyleft/gpl.html) for more details.

A summary in plain language is available at [https://tldrlegal.com/license/gnu-general-public-license-v2](https://tldrlegal.com/license/gnu-general-public-license-v2).
