#!/bin/bash
# Milestone 1+2 check: PAC entry counts, PAC rebuild byte-identity, HAN2RBO container round trip over every RBO/GOF2 archive.
# Usage: tools/han2/run_roundtrip.sh [path/to/han2tool.exe]
T=${1:-$(dirname "$0")/../../build/han2tool.exe}
RBO='C:\games\rbo\DATA'
ARCS=("$RBO\\DATA01.PAC" "$RBO\\DATA02.PAC" "$RBO\\Update01.PAC" "$RBO\\Ex1Disc.PAC" "$RBO\\Ex2Disc.PAC" "$RBO\\Ex3Disc.pac"
      C:/games/gof/Data/data0{0,1,2,3,4,5}.dat)
"$T" count "${ARCS[@]}" && "$T" pacrt "${ARCS[@]}" && "$T" roundtrip "${ARCS[@]}" | tail -1
