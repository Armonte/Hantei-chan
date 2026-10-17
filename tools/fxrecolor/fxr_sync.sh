#!/usr/bin/env bash
# The runtime recolour grammar + shader maths live in PovertyCaster (pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp).
# Hantei-chan vendors that header BYTE-IDENTICAL as src/cgm/fxr_spec.hpp so the editor previews with the very same code.
#   fxr_sync.sh check [pc-repo] [ref]   exit 1 when they differ
#   fxr_sync.sh pull  [pc-repo] [ref]   copy the runtime's header over ours
# The header is read from git (`git show <ref>:<path>`, default ref origin/main, fetched first), never from the
# PovertyCaster working tree: local checkouts are often on old branches that predate fxrecolor.
# Env: FXR_PC_REPO = PovertyCaster repo; FXR_SYNC_NOFETCH=1 skips the fetch (offline).
set -u
# Repo: arg 2, else $FXR_PC_REPO, else /home/teo/dev/PovertyCaster (the live clone; the /mnt/c/dev/castergroup checkouts are stale). Read only.
RT="${2:-${FXR_PC_REPO:-/home/teo/dev/PovertyCaster}}"
REF="${3:-origin/main}"
HDR="pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp"
DST="$(cd "$(dirname "$0")/../.." && pwd)/src/cgm/fxr_spec.hpp"
[ -n "$RT" ] && git -C "$RT" rev-parse --git-dir >/dev/null 2>&1 || { echo "no PovertyCaster git repo at $RT (arg 2 or FXR_PC_REPO)"; exit 2; }
if [ "${FXR_SYNC_NOFETCH:-0}" != 1 ] && [ "${REF#origin/}" != "$REF" ]; then
  git -C "$RT" fetch -q origin "${REF#origin/}" || echo "warning: fetch failed, using the cached $REF"
fi
TMP="$(mktemp)"; trap 'rm -f "$TMP"' EXIT
git -C "$RT" show "$REF:$HDR" >"$TMP" 2>/dev/null || { echo "no $REF:$HDR in $RT"; exit 2; }
REV="$(git -C "$RT" rev-parse --short "$REF")"
case "${1:-check}" in
  check) cmp -s "$TMP" "$DST" && echo "fxr_spec.hpp in sync with $REF ($REV)" || { echo "fxr_spec.hpp DIFFERS from $REF ($REV):$HDR"; exit 1; } ;;
  pull) cp "$TMP" "$DST" && echo "copied $REF ($REV):$HDR" ;;
  *) echo "usage: fxr_sync.sh check|pull [pc-repo] [ref]"; exit 2 ;;
esac
