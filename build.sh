#!/bin/bash
# Build sonson_native -- a NORMAL SGDK project (standard makefile.gen, fast build,
# NOT the LTO-heavy transliteration build the sibling port uses). Run with the BASH
# tool (not PowerShell -- PS wraps benign make stderr as a fatal NativeCommandError).
set -e
GDK="D:/portForge/tools/sgdk"
PROJ="D:/genesis ports/_capcom/son son/sonson_native"
cd "$PROJ"
export GDK
export PATH="$GDK/bin:$PATH"
mkdir -p out
LOG="out/build.txt"
echo "=== build start $(date) ===" | tee "$LOG"
"$GDK/bin/make.exe" -f "$GDK/makefile.gen" release GDK="$GDK" "$@" >> "$LOG" 2>&1
echo "=== make exit $? ===" | tee -a "$LOG"
ls -la out/rom.bin | tee -a "$LOG"
