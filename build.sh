#!/usr/bin/env bash
# Native host build on macOS/Linux; retain SGDK's Windows make workflow.
set -euo pipefail
cd "$(dirname "$0")"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    export GDK="${GDK:-D:/portForge/tools/sgdk}"
    export PATH="$GDK/bin:$PATH"
    mkdir -p out
    "$GDK/bin/make.exe" -f "$GDK/makefile.gen" release GDK="$GDK" "$@" > out/build.txt 2>&1
    ;;
  *) exec python3 tools/build_native.py "$@" ;;
esac
