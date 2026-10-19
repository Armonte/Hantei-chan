#!/usr/bin/env bash
# Regenerates the PovertyCaster-side schema-2 digests pinned in tests/pcmod_s2_test.cpp. Read-only on PovertyCaster.
# usage: tools/pcmod/s2_vectors_from_pc.sh [/home/teo/dev/PovertyCaster] [mods/pcmod-schema2]
set -euo pipefail
PC=${1:-/home/teo/dev/PovertyCaster}; REF=${2:-mods/pcmod-schema2}
here=$(cd "$(dirname "$0")/../.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
git -C "$PC" archive "$REF" pc-core/include pc-core/src/Sha256.cpp | tar -x -C "$tmp"
g++ -std=c++17 -O1 -I"$tmp/pc-core/include" "$here/tools/pcmod/s2_vectors_from_pc.cpp" "$tmp/pc-core/src/Sha256.cpp" -o "$tmp/s2v"
"$tmp/s2v" "$here/tests/fixtures/pcmod_s2"
