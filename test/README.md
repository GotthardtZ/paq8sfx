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
| Windows, Visual Studio | Build twice in Release mode, once with `#define SFX` (rename the result to `stub.exe`) and once with `#define FULL`, then copy `stub.exe` and `paq8sfx.exe` to the `build` or the `test` folder. |
| Linux | Run `sh build-linux-with-gcc.sh` or `sh build-linux-with-clang.sh` from within the `build` folder. |

Optionally compress the stub with `upx --best --ultra-brute stub.exe` (Linux: `stub`) to test the stub the way it will be shipped.

The test copies the two executables from the `build` folder to the `test` folder when it starts. If they are not in the `build` folder, the copies already present in the `test` folder are used.

On Windows the test script adds the MinGW-w64 `bin` folder to the `PATH`, so that the executables find the GCC runtime DLLs. Edit the `mingw` path at the top of the script if your installation is elsewhere.

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
[FAILED] autoextract.cmd: "archive9 -d test1.txt.paq8sfx test1.txt" - crashed: illegal instruction [0xC000001D] - CPU without AVX2? See AVX2_ONLY in src\SystemDefines.hpp
```

## When the test fails

| Reported | Likely cause |
| --- | --- |
| `crashed: illegal instruction` | The executables were built with `AVX2_ONLY` and the CPU has no AVX2. Comment out `#define AVX2_ONLY` in `src/SystemDefines.hpp` and rebuild both executables. |
| `crashed: access violation` (Windows), `crashed: segmentation fault` (Linux) | A bug in the program, or a damaged archive or package. |
| `could not start: DLL not found` (Windows) | The MinGW-w64 runtime DLLs are not found. Check the `mingw` path at the top of the test script. |
| `killed` (Linux) | Most likely not enough free memory. |
| `exit code 1` from a `-d` command, right after a failed compression | A follow-up error: the archive is incomplete because the compression before it failed. Look at the first failure. |
| A file is reported missing, but no program failed | The stub stays silent about its own errors by default. Comment out `#define SFX_SILENT` in `src/SystemDefines.hpp`, rebuild the stub and run the test again to see its messages. |

Failures usually come in groups, because the later steps depend on the earlier ones: the first `[FAILED]` line is the one to look at.
