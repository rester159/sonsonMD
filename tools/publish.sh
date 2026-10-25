#!/usr/bin/env bash
# publish.sh -- push main to github.com/rester159/sonsonMD without the
# Son Son II HuCard dump. Early history checked in src/ss2_rom.c (raw HuCard
# banks); a scratch clone drops that path from every commit with
# git-filter-repo, checks no ROM data remains, then pushes. filter-repo is
# deterministic, so republishing a longer main fast-forwards the remote.
# Local history is left as it is.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE="${PUBLISH_REMOTE:-https://github.com/rester159/sonsonMD.git}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
git clone -q --no-local --single-branch --branch main "$ROOT" "$WORK/repo"
cd "$WORK/repo"
git filter-repo --quiet --force --invert-paths --path src/ss2_rom.c --path-glob '*.pce'
if git log --all --name-only --format= | grep -qE '(^|/)ss2_rom\.c$|\.pce$'; then
    echo "ROM data still in history; not pushing" >&2; exit 1
fi
git remote add origin "$REMOTE"
git push ${PUBLISH_FORCE:+--force} origin main:main
