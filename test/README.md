# PAQ8SFX roundtrip test

A roundtrip test for each platform: it compresses, packages, unpacks and verifies a pair of small files, using both programs (`paq8sfx` and `stub`) and all three modes of the stub.

| Platform | Script |
| --- | --- |
| Windows | `roundtrip_test_windows.cmd` |
| Linux | `roundtrip_test_linux.sh` |

## Preparing

Build `paq8sfx` and `stub` first - see "How to compile" in the [main README](../README.md#how-to-compile). In short:

| Toolchain | How |
| --- | --- |
| Windows, MinGW-w64 | Run `build\build-mingw-w64.cmd`. |
| Windows, Visual Studio | Build the `Release-FULL` and the `Release-SFX` configuration, then copy `paq8sfx.exe` and `stub.exe` from the output folders to the `build` or the `test` folder. |
| Linux | Run `sh build/build-linux-with-gcc.sh` or `sh build/build-linux-with-clang.sh`. |
| Linux or Windows with MinGW-w64, Makefile | Run `make test` (Windows: `mingw32-make test`). It builds both programs and starts the test. |

Optionally compress the stub with `upx --best --ultra-brute stub.exe` (Linux: `stub`) to test the stub the way it will be shipped.

The test copies the two executables from the `build` folder to the `test` folder when it starts. If they are not in the `build` folder, the copies already present in the `test` folder are used.

To test the libc-free stub, use `stub-free` (`stub-free.exe`) in place of `stub`: copy it over `stub` in the `build` folder, or run `make test-free`.

On Windows the test script adds the MinGW-w64 `bin` folder to the `PATH`, so that the normal stub (`stub.exe`) and the packages assembled from it find the GCC runtime DLLs. Edit the `mingw` path at the top of the script if your installation is elsewhere. `paq8sfx.exe` (statically linked), executables built with Visual Studio, and the libc-free stub do not need it.

The test uses compression level `-8` and therefore needs about 1 GB of free memory.

## Running

```
cd test
roundtrip_test_windows.cmd          (Windows)
bash roundtrip_test_linux.sh        (Linux)
```

Start the script with any argument (e.g. `roundtrip_test_windows.cmd nopause`) to skip the "press a key" prompt at the end.

## What the test does

| Step | Action |
| --- | --- |
| 0 | Copies `paq8sfx` and `stub` from the `build` folder. |
| 1-2 | Creates two small files, `test1.txt` and `test2.txt`. |
| 3-4 | Creates two assembly scripts: `script1.txt` for a first package (`sfx`) and `script2.txt` for a second one (`archive9`). |
| 5-6 | Creates two AutoRun scripts: `autorun` for the first package and `autoextract` for the second one. |
| 7 | Compresses `test2.txt` with `paq8sfx -8`. |
| 8 | Assembles the first package with `stub -a script1.txt`. It contains `autorun`, `autoextract`, the `paq8sfx` compressor, `test1.txt` and `test2.txt.paq8sfx`. |
| 9 | Runs the first package. It unpacks its files and runs `autorun`, which compresses `test1.txt` and assembles the second package with `sfx -a script2.txt`. The second package contains `autoextract` and the two compressed files. |
| 10 | Deletes everything except the second package, `archive9`. |
| 11 | Runs `archive9`. It unpacks its files and runs `autoextract`, which decompresses both files with `archive9 -d`. Finally the test verifies that `test1.txt` and `test2.txt` exist and have the original content. |

The test removes all the files it created when it finishes.

## Reading the result

Every check prints `[PASSED]` or `[FAILED]`, and the last lines summarize the result:

```
============================================================
 Results:  20 PASSED  /  0 FAILED
============================================================

All checks passed. Roundtrip OK.
```

The exit code of the test script is the number of failed checks (0 = success).

Besides checking that the expected files appear, the test checks the exit code of every program it starts and reports how a program ended if it did not end normally:

```
[FAILED] paq8sfx.exe -8 test2.txt - crashed: access violation [0xC0000005]
```

The programs started from the AutoRun scripts are checked as well. Their exit codes cannot reach the test script directly, so the AutoRun scripts record every failed command in `failures.log`, which the test reads and reports with the name of the AutoRun script:

```
[FAILED] autoextract.cmd: "archive9 -d test1.txt.paq8sfx test1.txt" - crashed: illegal instruction [0xC000001D] - CPU without AVX2? The stub was built with AVX2_ONLY - see Build settings in README.md
```

## When the test fails

| Reported | Likely cause |
| --- | --- |
| `crashed: illegal instruction` | The stub was built with `AVX2_ONLY` and the CPU has no AVX2. Rebuild the stub without it - see "Build settings" in the [main README](../README.md#build-settings). `paq8sfx` itself runs on any x64 CPU. |
| `crashed: access violation` (Windows), `crashed: segmentation fault` (Linux) | A bug in the program, or a damaged archive or package. |
| `could not start: DLL not found` (Windows) | The normal stub built with MinGW-w64 (or a package assembled from it) does not find the GCC runtime DLLs. Check the `mingw` path at the top of the test script. `paq8sfx.exe` is statically linked and does not need them. |
| `killed` (Linux) | Most likely not enough free memory. |
| `exit code 1` from a `-d` command, right after a failed compression | A follow-up error: the archive is incomplete because the compression before it failed. Look at the first failure. |
| A file is reported missing, but no program failed | The stub stays silent about its own errors by default. Comment out `#define SFX_SILENT` in `src/SystemDefines.hpp`, rebuild the stub and run the test again to see its messages. |

Failures usually come in groups, because the later steps depend on the earlier ones: the first `[FAILED]` line is the one to look at.
