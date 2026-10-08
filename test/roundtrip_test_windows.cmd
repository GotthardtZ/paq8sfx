@echo off
setlocal enabledelayedexpansion

:: ============================================================
::  PAQ8SFX Roundtrip Test  -  Windows
:: ============================================================

:: Enable ANSI escape sequences
for /f "delims=" %%A in ('echo prompt $E^| cmd') do set "ESC=%%A"

set "GREEN=%ESC%[92m"
set "RED=%ESC%[91m"
set "RESET=%ESC%[0m"

rem Access MinGW-w64 C++ runtime and libraries:
rem - libgcc_s_seh-1.dll
rem - libstdc++-6.dll
rem - libwinpthread-1.dll
set "mingw=C:\mingw\winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r1"
set "path=%mingw%\bin;%path%"

set PASS=0
set FAIL=0

rem Programs started from autorun.cmd / autoextract.cmd cannot report their
rem exit code to this script directly (the stub does not pass it on), so
rem those scripts append a line to this file when a command fails:
rem   <script>;<command>;<exit code>
set "FAILLOG=failures.log"

goto :main

:: ------------------------------------------------------------
:check
::  Usage: call :check <label> <file_or_condition>
::  Checks existence of %2; prints PASSED/FAILED.
:: ------------------------------------------------------------
set "_label=%~1"
set "_file=%~2"
if exist "%_file%" (
    echo %GREEN%[PASSED]%RESET% %_label%
    set /a PASS+=1
) else (
    echo %RED%[FAILED]%RESET% %_label%
    set /a FAIL+=1
)
exit /b

:: ------------------------------------------------------------
:check_content
::  Usage: call :check_content <label> <file> <expected_string>
::  Checks that <file> contains <expected_string>.
:: ------------------------------------------------------------
set "_label=%~1"
set "_file=%~2"
set "_expected=%~3"
findstr /c:"%_expected%" "%_file%" >nul 2>&1
if !errorlevel! == 0 (
    echo %GREEN%[PASSED]%RESET% %_label%
    set /a PASS+=1
) else (
    echo %RED%[FAILED]%RESET% %_label%
    set /a FAIL+=1
)
exit /b

:: ------------------------------------------------------------
:describe_exit
::  Usage: call :describe_exit <exit_code>
::  Sets _desc to a readable description of a process exit code.
::  A crashed process exits with its NTSTATUS code, which cmd
::  shows as a negative number.
:: ------------------------------------------------------------
set "_desc=exit code %~1"
if "%~1"=="-1073741819" set "_desc=crashed: access violation [0xC0000005]"
if "%~1"=="-1073741795" set "_desc=crashed: illegal instruction [0xC000001D] - CPU without AVX2? The stub was built with AVX2_ONLY - see Build settings in README.md"
if "%~1"=="-1073741571" set "_desc=crashed: stack overflow [0xC00000FD]"
if "%~1"=="-1073740791" set "_desc=crashed: stack buffer overrun [0xC0000409]"
if "%~1"=="-1073740940" set "_desc=crashed: heap corruption [0xC0000374]"
if "%~1"=="-1073741515" set "_desc=could not start: DLL not found [0xC0000135] - MinGW-w64 runtime DLLs not on the path?"
if "%~1"=="9009"        set "_desc=command not found [9009]"
exit /b

:: ------------------------------------------------------------
:check_exit
::  Usage: call :check_exit <label> <exit_code>
::  Passes when the exit code is 0, otherwise reports why the
::  program ended (crash type or plain exit code).
:: ------------------------------------------------------------
set "_label=%~1"
if "%~2"=="0" (
    echo %GREEN%[PASSED]%RESET% !_label! exited normally
    set /a PASS+=1
) else (
    call :describe_exit %~2
    echo %RED%[FAILED]%RESET% !_label! - !_desc!
    set /a FAIL+=1
)
exit /b

:: ------------------------------------------------------------
:check_faillog
::  Usage: call :check_faillog <label>
::  Passes when the autorun script recorded no failed command.
::  Otherwise prints one FAILED line per recorded failure.
:: ------------------------------------------------------------
set "_label=%~1"
if not exist "%FAILLOG%" (
    echo %GREEN%[PASSED]%RESET% !_label!
    set /a PASS+=1
    exit /b
)
for /f "usebackq tokens=1,2,3 delims=;" %%A in ("%FAILLOG%") do (
    call :describe_exit %%C
    echo %RED%[FAILED]%RESET% %%A: "%%B" - !_desc!
    set /a FAIL+=1
)
del /f /q "%FAILLOG%" 2>nul
exit /b

:: ============================================================
:main
:: ============================================================

echo.
echo ============================================================
echo  PAQ8SFX Roundtrip Test  ^(Windows^)
echo ============================================================
echo.

:: --- Verify prerequisites ---
echo [Step 0] Checking prerequisites...

copy /Y ..\build\paq8sfx.exe . >nul
copy /Y ..\build\stub.exe . >nul
del /f /q "%FAILLOG%" 2>nul

if not exist paq8sfx.exe (
    echo %RED%[FAILED]%RESET% paq8sfx.exe not found - aborting.
    exit /b 1
)
if not exist stub.exe (
    echo %RED%[FAILED]%RESET% stub.exe not found - aborting.
    exit /b 1
)
echo %GREEN%[PASSED]%RESET% Prerequisites found (paq8sfx.exe, stub.exe)
echo.

:: ------------------------------------------------------------
echo [Step 1] Creating test1.txt ...
<nul set /p ="test1 test1 test1 " > test1.txt
call :check "test1.txt created" test1.txt

:: ------------------------------------------------------------
echo [Step 2] Creating test2.txt ...
<nul set /p ="test2 test2 test2 " > test2.txt
call :check "test2.txt created" test2.txt

:: ------------------------------------------------------------
echo [Step 3] Creating script1.txt ...
(
    echo sfx.exe
    echo autorun.cmd
    echo autoextract.cmd
    echo paq8sfx.exe
    echo test1.txt
    echo test2.txt.paq8sfx
) > script1.txt
call :check "script1.txt created" script1.txt

:: ------------------------------------------------------------
echo [Step 4] Creating script2.txt ...
(
    echo archive9.exe
    echo autoextract.cmd
    echo test1.txt.paq8sfx
    echo test2.txt.paq8sfx
) > script2.txt
call :check "script2.txt created" script2.txt

:: ------------------------------------------------------------
:: Each command in the generated scripts is followed by a line that
:: records a non-zero exit code in %FAILLOG% (see :check_faillog).
:: The redirection is written first so that the exit code's last
:: digit cannot be mistaken for a file handle number.
echo [Step 5] Creating autorun.cmd ...
(
    echo @echo off
    echo paq8sfx -8 test1.txt
    echo if not %%errorlevel%%==0 ^>^>%FAILLOG% echo autorun.cmd;paq8sfx -8 test1.txt;%%errorlevel%%
    echo sfx -a script2.txt
    echo if not %%errorlevel%%==0 ^>^>%FAILLOG% echo autorun.cmd;sfx -a script2.txt;%%errorlevel%%
) > autorun.cmd
call :check "autorun.cmd created" autorun.cmd

:: ------------------------------------------------------------
echo [Step 6] Creating autoextract.cmd ...
(
    echo @echo off
    echo archive9 -d test1.txt.paq8sfx test1.txt
    echo if not %%errorlevel%%==0 ^>^>%FAILLOG% echo autoextract.cmd;archive9 -d test1.txt.paq8sfx test1.txt;%%errorlevel%%
    echo archive9 -d test2.txt.paq8sfx test2.txt
    echo if not %%errorlevel%%==0 ^>^>%FAILLOG% echo autoextract.cmd;archive9 -d test2.txt.paq8sfx test2.txt;%%errorlevel%%
) > autoextract.cmd
call :check "autoextract.cmd created" autoextract.cmd

echo.

:: ------------------------------------------------------------
echo [Step 7] Compressing test2.txt ...
paq8sfx.exe -8 test2.txt
call :check_exit "paq8sfx.exe -8 test2.txt" %errorlevel%
call :check "test2.txt.paq8sfx created" test2.txt.paq8sfx

:: ------------------------------------------------------------
echo [Step 8] Assembling sfx.exe from stub ...
stub.exe -a script1.txt
call :check_exit "stub.exe -a script1.txt" %errorlevel%
call :check "sfx.exe assembled" sfx.exe

:: ------------------------------------------------------------
echo [Step 9] Running sfx.exe (should create archive9.exe) ...
sfx.exe
call :check_exit "sfx.exe" %errorlevel%
call :check_faillog "autorun.cmd: all commands succeeded"
call :check "archive9.exe created" archive9.exe

:: ------------------------------------------------------------
echo [Step 10] Cleaning up - keeping only archive9.exe ...
del /f /q test1.txt      2>nul
del /f /q test2.txt      2>nul
del /f /q test1.txt.paq8sfx 2>nul
del /f /q test2.txt.paq8sfx 2>nul
del /f /q script1.txt    2>nul
del /f /q script2.txt    2>nul
del /f /q autorun.cmd    2>nul
del /f /q autoextract.cmd 2>nul
del /f /q sfx.exe        2>nul
del /f /q paq8sfx.exe    2>nul
del /f /q stub.exe       2>nul

:: Confirm cleanup (these should NOT exist)
if exist test1.txt (
    echo %RED%[FAILED]%RESET% Cleanup: test1.txt still present
    set /a FAIL+=1
) else (
    echo %GREEN%[PASSED]%RESET% Cleanup complete
    set /a PASS+=1
)

echo.

:: ------------------------------------------------------------
echo [Step 11] Running archive9.exe (self-extracting decompression) ...
archive9.exe
set "_rc=%errorlevel%"
echo.

call :check_exit "archive9.exe" %_rc%
call :check_faillog "autoextract.cmd: all commands succeeded"
call :check "test1.txt extracted" test1.txt
call :check "test2.txt extracted" test2.txt

:: Verify content
call :check_content "test1.txt content correct" test1.txt "test1 test1 test1"
call :check_content "test2.txt content correct" test2.txt "test2 test2 test2"

:: Cleanup
del /f /q test1.txt         2>nul
del /f /q test2.txt         2>nul
del /f /q test1.txt.paq8sfx 2>nul
del /f /q test2.txt.paq8sfx 2>nul
del /f /q archive9.exe      2>nul
del /f /q autoextract.cmd   2>nul
del /f /q "%FAILLOG%"       2>nul

:: ------------------------------------------------------------
echo.
echo ============================================================
echo  Results:  %GREEN%!PASS! PASSED%RESET%  /  %RED%!FAIL! FAILED%RESET%
echo ============================================================
echo.

if !FAIL! == 0 (
    echo %GREEN%All checks passed. Roundtrip OK.%RESET%
) else (
    echo %RED%One or more checks failed.%RESET%
)
echo.

rem Pass any argument to skip the pause (for unattended runs).
if [%1]==[] pause

rem The exit code is the number of failed checks.
exit /b %FAIL%
