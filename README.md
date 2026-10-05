# PAQ8SFX – A self-extracting archive creator with the power of PAQ8PX

## About

**PAQ** is a family of experimental, high-end lossless data compression programs.
`paq8sfx` is a spin-off from the well-known [paq8px](https://github.com/hxim/paq8px) data compressor, designed specifically for creating self-extracting archives.

For compressing executables it is almost as good as paq8px, but it is faster, has a smaller memory footprint and - most importantly - its decompressor (the *stub*) is much smaller than the executable of the full paq8px compressor.

The project consists of two programs built from the same source:

| Program | Role |
| --- | --- |
| `paq8sfx` | The compressor. Compresses (and decompresses) a single file. It is not part of the final package. |
| `stub` | The self-extractor. Decompresses `paq8sfx` archives, carries the payload files and assembles new self-extracting packages. This is what ends up in the final package. |

Two terms are used throughout this document: an *archive* is a single compressed file (`.paq8sfx`), a *package* is a self-extracting executable - the stub with its payload files attached.

## When to use it

`paq8sfx` is most suitable for creating a self-extracting package for compression benchmarks in which the size of the decompressor's executable is counted in the size of the whole compressed package.

For decompressors with small executables [UPX](https://upx.github.io/) will do fine, but for larger executables (such as cmix and its derivatives, or paq8px) `paq8sfx` is the better choice: the stub costs about 40 KB, and that is recovered as soon as `paq8sfx` saves more than that over UPX.

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
```

- `LEVEL` is `1` to `12`, see [Compression levels](#compression-levels-and-memory-use).
- When compressing, the default output is the input name with `.paq8sfx` appended.
- When decompressing, the default output is the archive name without the `.paq8sfx` extension.
- `output` may be a file name or an existing folder.
- The level is stored in the archive; it does not have to be given again for decompression.

### stub (the self-extractor)

The stub has three modes. A package assembled from the stub understands the same commands as the stub itself.

| Command | What it does |
| --- | --- |
| `stub -a scriptfile` | **Assemble.** Creates a new package: a copy of the stub followed by the files listed in the script. |
| `package` (no arguments, or `-x`) | **Extract and run.** Writes all payload files to the current folder, then runs the AutoRun file. |
| `package -d input output` | **Decompress** a single `paq8sfx` archive. |

Because a package contains the complete stub, a package can itself assemble further packages with `-a` (the test scripts do exactly that).

### The assembly script

A plain text file with one file name per line. File names may contain spaces, except for the AutoRun file on Windows; blank lines are ignored.

| Line | Meaning |
| --- | --- |
| 1 | Name of the package to create. |
| 2 | The AutoRun file: the first payload, and the one that is run after extraction. |
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

- **UPX**: `upx --best --ultra-brute`, UPX 5.2.0. The result is a self-extracting executable; its decompression stub is already included in the compressed size.
- **paq8sfx**: paq8sfx_v1, level `-12`. The total includes the stub (40'448 bytes, see [Stub size](#stub-size)), i.e. it is the size of the complete self-extracting package.
- **paq8px**: paq8px_v216, level `-12`, shown for reference. Its result is not self-extracting: it needs the 1.3 MB paq8px executable to decompress, which is not counted here.

**paq8px_v216.exe, [paq8px v216](https://github.com/hxim/paq8px/releases/tag/v216)** - 1'293'312 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 510'976 | included | 510'976 | 39.5% | 7 sec |
| paq8sfx -12 | 323'802 | 40'448 | **364'250** | 28.2% | 118 sec |
| paq8px -12 | 314'761 | - | 314'761 | 24.3% | 270 sec |

**bsc.exe, [bsc 3.3.12](https://github.com/IlyaGrebnov/libbsc/releases/tag/v3.3.12)** - 6'114'304 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 957'440 | included | 957'440 | 15.7% | 87 sec |
| paq8sfx -12 | 667'366 | 40'448 | **707'814** | 11.6% | 475 sec |
| paq8px -12 | 646'119 | - | 646'119 | 10.6% | 1'008 sec |

**mcm.exe, [mcm 0.83](https://encode.su/threads/2127-MCM-LZP?p=43220&viewfull=1#post43220), x64** - 2'444'886 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 1'478'742 | included | 1'478'742 | 60.5% | 8 sec |
| paq8sfx -12 | 268'772 | 40'448 | **309'220** | 12.6% | 190 sec |
| paq8px -12 | 257'256 | - | 257'256 | 10.5% | 366 sec |

**zpaq64.exe [zpaq 7.15](https://github.com/zpaq/zpaq/releases/tag/7.15), 64-bit** - 1'125'376 bytes

| Compressor | Compressed size | Stub | Total | Ratio | Time |
| --- | ---: | ---: | ---: | ---: | ---: |
| UPX | 326'144 | included | 326'144 | 29.0% | 8 sec |
| paq8sfx -12 | 201'476 | 40'448 | **241'924** | 21.5% | 101 sec |
| paq8px -12 | 197'083 | - | 197'083 | 17.5% | 215 sec |

Even with its stub counted, `paq8sfx` is significantly better than UPX in every test and close behind the full-fledged paq8px compressor, in about half of its time.

### Stub size

| Platform | Compiler | Architecture| Stub size | After `upx --best --ultra-brute` |
| --- | --- | --- | ---: | ---: |
| Windows | MinGW-w64 (GCC 15.2.0) | x64, Generic | 88'576| 41'984 |
| Windows | MinGW-w64 (GCC 15.2.0) | x64, AVX2_ONLY | 86'016 | 40'448 |
| Windows | VS 2022 (MSVC 19.44.35216) | x86 | 59'392 | 33'280 |
| Windows | VS 2022 (MSVC 19.44.35216) | x64, Generic | 68'096 | 36'352 |
| Windows | VS 2022 (MSVC 19.44.35216) | x64, AVX2_Only | 65'024 | 35'328 |
| Ubuntu | gcc 13.3.0 | x64, AVX2_Only | 59'600 | 30'504 |
| Ubuntu | clang 18.1.3 | x64, AVX2_Only | 68'152 | 34'612 |

These stub executables are dynamically linked - see [Runtime dependencies](#runtime-dependencies).

## How to compile

A C++17 compiler for x64 is required. The same source is compiled twice: once with `-DFULL` (gives `paq8sfx`) and once with `-DSFX` (gives `stub`). The build scripts and the Makefile do both.

### Windows, MinGW-w64

Run `build\build-mingw-w64.cmd`. It creates `paq8sfx.exe` and `stub.exe` in the `build` folder.
The script expects MinGW-w64 in `c:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1`; edit the path in the script if yours is elsewhere.

### Windows, Visual Studio

1. Open `paq8sfx.sln`. Select either `x64` or `x86` as the target platform. Note: AVX2_ONLY must be commented out in `src/SystemDefines.hpp` for x86 otherwise the build will fail.
2. Select the `Release-FULL` configuration. Build.
3. Select the `Release-SFX` configuration. Build.

### Linux

Run one of the scripts from within the `build` folder:

```
cd build
sh build-linux-with-gcc.sh       # or: sh build-linux-with-clang.sh
```

It creates `paq8sfx` and `stub` in the `build` folder.

### Linux, with make

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
| `make upx` | Compresses the stub with `upx --best --ultra-brute`. |
| `make clean` | Removes the object files (`build/obj`) and the two executables. |
| `make CXX=clang++` | Builds with clang instead of gcc. |
| `make V=1` | Shows the full compiler command lines. |

Changing the compiler or a setting in `src/SystemDefines.hpp` is picked up automatically; there is no need to run `make clean` first.

The Makefile also has settings for MinGW-w64 on Windows, to be run from a shell that provides `sh`, `mkdir` and `rm` (MSYS2, Git Bash).

### Making the stub smaller

Compress the stub with UPX **before** assembling a package with it:

```
upx --best --ultra-brute stub.exe
```

With the Makefile: `make upx`.

### Build settings

Two settings in `src/SystemDefines.hpp` affect the result. They are set in that file for every build method - they cannot be given on the `make` command line:

| Setting | Effect when defined (the default) | Effect when commented out |
| --- | --- | --- |
| `AVX2_ONLY` | Only the AVX2 code path is compiled in. The executables are smaller but **require a CPU with AVX2**. | SSE2, SSE4.1, AVX2 and plain C++ code paths are all compiled in and the best one is selected at run time. Runs on any x64 CPU. |
| `SFX_SILENT` | The stub prints no messages of its own, not even error messages. | The stub reports what it is doing and what went wrong. Useful while putting a package together. |

> [!WARNING]
> With `AVX2_ONLY` defined there is no check for AVX2 at run time. On a CPU without AVX2 both `paq8sfx` and the stub stop without any message as soon as they start to compress or decompress (illegal instruction); an interrupted compression leaves an incomplete archive behind. If a package must run on older machines, build without `AVX2_ONLY`.

Archives are created and read by the same model code, so always use a `paq8sfx` and a `stub` built from the same version of the source.

### Runtime dependencies

- The MinGW-w64 build links the GCC runtime dynamically. `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and `libwinpthread-1.dll` must be reachable (on the `PATH` or next to the executable) on the machine where the package runs.
- The MSVC 14 (Visual Studio 2022) build links the VC++ runtime dynamically. The [Microsoft Visual C++ 2015–2022 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version) or a compatible higher version must be installed on the machine where the package runs.
- The Linux builds are dynamically linked against the system C and C++ runtime libraries.

## How to test

The `test` folder contains a roundtrip test for Windows and for Linux. It compresses, packages, unpacks and verifies a pair of small files, and reports crashes of the tested programs. See [test/README.md](test/README.md) for how to run it and how to read its output. On Linux, `make test` builds both programs and runs it.

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

For an introduction to context mixing and a description of the components above, see the paq8px README.

### Archive format (`.paq8sfx`)

| Content | Size |
| --- | --- |
| The text `paq8sfx` | 7 bytes |
| Compression level | 1 byte |
| Arithmetic-coded stream: file size, then for each block its type, size and content | rest |

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

At startup the stub reads its own executable file, locates the footer from the last byte and works backwards.

## Limits

| Limit | Value |
| --- | --- |
| Files per `.paq8sfx` archive | 1 |
| Size of a file to compress | below 2 GB |
| Payload files per package | 8 |
| Size of one payload file | below 4 GB |
| Footer size | 255 bytes: the sum of all payload name lengths, plus 5 bytes per payload, plus 2 |
| Platforms | x64 Windows and Linux |

## Todo and outlook

- The C/C++ runtime library is still referenced. Replacing those references with direct system calls would decrease the stub size further.
- The emphasis was on keeping the code similar to paq8px, so that `paq8sfx` can still receive model updates from paq8px. On the other hand, keeping this similarity makes it difficult to truly optimize `paq8sfx` into a dedicated executable compressor such as UPX.
- The stub does not pass on the exit code of the AutoRun file on Windows, and a failed `-a` still exits with code 0.

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
