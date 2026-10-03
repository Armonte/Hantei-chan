#!/bin/bash
# Full han2 verification over every RBO / GOF2 archive (needs the game installs; read-only).
# Usage: tools/han2/run_roundtrip.sh [path/to/han2tool.exe]
T=${1:-$(dirname "$0")/../../build/han2tool.exe}
RBO='C:\games\rbo\DATA'
RARCS=("$RBO\\DATA01.PAC" "$RBO\\DATA02.PAC" "$RBO\\Update01.PAC" "$RBO\\Ex1Disc.PAC" "$RBO\\Ex2Disc.PAC" "$RBO\\Ex3Disc.pac")
GARCS=(C:/games/gof/Data/data0{0,1,2,3,4,5}.dat)
set -e
echo "== PAC entry counts";            "$T" count "${RARCS[@]}" "${GARCS[@]}"
echo "== PAC rebuild byte-identical";   "$T" pacrt "${RARCS[@]}" "${GARCS[@]}"
echo "== HAN2RBO container round trip"; "$T" roundtrip "${RARCS[@]}" "${GARCS[@]}" | tail -1
echo "== model round trip (load into Hantei-chan model, save) RBO + GOF2"; "$T" modelrt "${RARCS[@]}" "${GARCS[@]}" | tail -1
echo "== PAT v3/v4 parts reader+writer"; "$T" patrt "${RARCS[@]}" "${GARCS[@]}" | tail -1
echo "== IMG round trip";               "$T" imgrt "$RBO\\CG.PAC" | tail -1
