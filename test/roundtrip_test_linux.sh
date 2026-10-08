#!/usr/bin/env bash

# ============================================================
#  PAQ8SFX Roundtrip Test  -  Linux
# ============================================================

GREEN='\033[0;32m'
RED='\033[0;31m'
RESET='\033[0m'

PASS=0
FAIL=0

# Programs started from autorun.sh / autoextract.sh cannot report their
# exit code to this script directly, so those scripts append a line to
# this file when a command fails:
#   <script>;<command>;<exit code>
FAILLOG=failures.log

# ------------------------------------------------------------
pass() { echo -e "${GREEN}[PASSED]${RESET} $1"; (( PASS++ )); }
fail() { echo -e "${RED}[FAILED]${RESET} $1"; (( FAIL++ )); }

check_file() {
    local label="$1"
    local file="$2"
    if [ -f "$file" ]; then pass "$label"; else fail "$label"; fi
}

check_content() {
    local label="$1"
    local file="$2"
    local expected="$3"
    if grep -qF "$expected" "$file" 2>/dev/null; then pass "$label"; else fail "$label"; fi
}

# Turns a process exit code into a readable description.
# A process killed by signal N exits with 128+N.
describe_exit() {
    case "$1" in
        132) echo "crashed: illegal instruction [SIGILL] - CPU without AVX2? The stub was built with AVX2_ONLY - see Build settings in README.md" ;;
        134) echo "crashed: aborted [SIGABRT]" ;;
        135) echo "crashed: bus error [SIGBUS]" ;;
        136) echo "crashed: arithmetic error [SIGFPE]" ;;
        137) echo "killed [SIGKILL] - out of memory?" ;;
        139) echo "crashed: segmentation fault [SIGSEGV]" ;;
        126) echo "not executable [126]" ;;
        127) echo "command not found [127]" ;;
        *)   echo "exit code $1" ;;
    esac
}

# Usage: check_exit <label> <exit_code>
check_exit() {
    local label="$1"
    local rc="$2"
    if [ "$rc" -eq 0 ]; then
        pass "$label exited normally"
    else
        fail "$label - $(describe_exit "$rc")"
    fi
}

# Usage: check_faillog <label>
# Passes when the autorun script recorded no failed command.
check_faillog() {
    local label="$1"
    if [ ! -f "$FAILLOG" ]; then
        pass "$label"
        return
    fi
    local script cmd rc
    while IFS=';' read -r script cmd rc; do
        fail "$script: \"$cmd\" - $(describe_exit "$rc")"
    done < "$FAILLOG"
    rm -f "$FAILLOG"
}

# ============================================================
echo ""
echo "============================================================"
echo " PAQ8SFX Roundtrip Test  (Linux)"
echo "============================================================"
echo ""

# --- Verify prerequisites ---
echo "[Step 0] Checking prerequisites..."
cp ../build/paq8sfx . 2>/dev/null
cp ../build/stub    . 2>/dev/null
rm -f "$FAILLOG"
PREREQ_OK=1
if [ ! -f ./paq8sfx ]; then
    echo -e "${RED}[FAILED]${RESET} ./paq8sfx not found - aborting."
    PREREQ_OK=0
fi
if [ ! -f ./stub ]; then
    echo -e "${RED}[FAILED]${RESET} ./stub not found - aborting."
    PREREQ_OK=0
fi
if [ "$PREREQ_OK" -eq 0 ]; then
    exit 1
fi
echo -e "${GREEN}[PASSED]${RESET} Prerequisites found (paq8sfx, stub)"
echo ""

# ------------------------------------------------------------
echo "[Step 1] Creating test1.txt ..."
printf 'test1 test1 test1 ' > test1.txt
check_file "test1.txt created" test1.txt

# ------------------------------------------------------------
echo "[Step 2] Creating test2.txt ..."
printf 'test2 test2 test2 ' > test2.txt
check_file "test2.txt created" test2.txt

# ------------------------------------------------------------
echo "[Step 3] Creating script1.txt ..."
cat > script1.txt << 'EOF'
sfx
autorun.sh
autoextract.sh
paq8sfx
test1.txt
test2.txt.paq8sfx
EOF
check_file "script1.txt created" script1.txt

# ------------------------------------------------------------
echo "[Step 4] Creating script2.txt ..."
cat > script2.txt << 'EOF'
archive9
autoextract.sh
test1.txt.paq8sfx
test2.txt.paq8sfx
EOF
check_file "script2.txt created" script2.txt

# ------------------------------------------------------------
# Each command in the generated scripts records a non-zero exit code
# in failures.log (see check_faillog).
echo "[Step 5] Creating autorun.sh ..."
cat > autorun.sh << 'EOF'
#!/bin/sh
./paq8sfx -8 test1.txt || echo "autorun.sh;./paq8sfx -8 test1.txt;$?" >> failures.log
./sfx -a script2.txt   || echo "autorun.sh;./sfx -a script2.txt;$?" >> failures.log
EOF
chmod +x autorun.sh
check_file "autorun.sh created" autorun.sh

# ------------------------------------------------------------
echo "[Step 6] Creating autoextract.sh ..."
cat > autoextract.sh << 'EOF'
#!/bin/sh
./archive9 -d test1.txt.paq8sfx test1.txt || echo "autoextract.sh;./archive9 -d test1.txt.paq8sfx test1.txt;$?" >> failures.log
./archive9 -d test2.txt.paq8sfx test2.txt || echo "autoextract.sh;./archive9 -d test2.txt.paq8sfx test2.txt;$?" >> failures.log
EOF
chmod +x autoextract.sh
check_file "autoextract.sh created" autoextract.sh

echo ""

# ------------------------------------------------------------
echo "[Step 7] Compressing test2.txt ..."
./paq8sfx -8 test2.txt
check_exit "./paq8sfx -8 test2.txt" $?
check_file "test2.txt.paq8sfx created" test2.txt.paq8sfx

# ------------------------------------------------------------
echo "[Step 8] Assembling sfx from stub ..."
./stub -a script1.txt
check_exit "./stub -a script1.txt" $?
check_file "sfx assembled" sfx

# ------------------------------------------------------------
echo "[Step 9] Running sfx (should create archive9) ..."
./sfx
check_exit "./sfx" $?
check_faillog "autorun.sh: all commands succeeded"
check_file "archive9 created" archive9

# ------------------------------------------------------------
echo "[Step 10] Cleaning up - keeping only archive9 ..."
rm -f test1.txt
rm -f test2.txt
rm -f test1.txt.paq8sfx
rm -f test2.txt.paq8sfx
rm -f script1.txt
rm -f script2.txt
rm -f autorun.sh
rm -f autoextract.sh
rm -f sfx
rm -f paq8sfx
rm -f stub

if [ ! -f test1.txt ]; then
    pass "Cleanup complete"
else
    fail "Cleanup: test1.txt still present"
fi

echo ""

# ------------------------------------------------------------
echo "[Step 11] Running archive9 (self-extracting decompression) ..."
./archive9
RC=$?
echo ""

check_exit    "./archive9" "$RC"
check_faillog "autoextract.sh: all commands succeeded"
check_file    "test1.txt extracted"          test1.txt
check_file    "test2.txt extracted"          test2.txt
check_content "test1.txt content correct"    test1.txt "test1 test1 test1"
check_content "test2.txt content correct"    test2.txt "test2 test2 test2"

# Cleanup
rm -f test1.txt
rm -f test2.txt
rm -f test1.txt.paq8sfx
rm -f test2.txt.paq8sfx
rm -f archive9
rm -f autoextract.sh
rm -f "$FAILLOG"

# ------------------------------------------------------------
echo ""
echo "============================================================"
echo -e " Results:  ${GREEN}${PASS} PASSED${RESET}  /  ${RED}${FAIL} FAILED${RESET}"
echo "============================================================"
echo ""

if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}All checks passed. Roundtrip OK.${RESET}"
else
    echo -e "${RED}One or more checks failed.${RESET}"
fi
echo ""

# Pass any argument to skip the pause (for unattended runs).
if [ -z "$1" ] && [ -t 0 ]; then
    read -rp "Press Enter to continue..." _
fi

# The exit code is the number of failed checks.
exit "$FAIL"
