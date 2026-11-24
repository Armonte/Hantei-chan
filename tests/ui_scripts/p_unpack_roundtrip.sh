#!/bin/bash
# MBAACC .p unpack -> modify -> pack -> restore round trip through gonptechan.exe --headless (archive_gamepack.cpp), checked with fbarctool.
# Machine-specific (WSL): works on a THROWAWAY copy of the Steam .p files; it never touches a real install.
#   TEST=/mnt/c/dev/Hantei-chan/work/p_unpack_test   (the ui scripts p_unpack_[123]_*.txt hard-code C:\dev\Hantei-chan\work\p_unpack_test\game)
#   STEAM=the Steam MBAACC folder (read only: copied once)
# Usage: tests/ui_scripts/p_unpack_roundtrip.sh   (from the repo root, after ./build.sh). Delete $TEST afterwards (several GB).
set -u
REPO=$(cd "$(dirname "$0")/../.." && pwd)
TEST=${TEST:-/mnt/c/dev/Hantei-chan/work/p_unpack_test}
STEAM=${STEAM:-"/mnt/c/Program Files (x86)/Steam/steamapps/common/MELTY BLOOD Actress Again Current Code"}
GAME=$TEST/game
EXE=$REPO/build/gonptechan.exe
TOOL=$REPO/build/fbarctool.exe
fail=0
# fbarctool is a Windows exe: strip the CRs. lsent <archive>: one entry path per line.
lsent() { "$TOOL" ls "$1" | tr -d '\r' | tail -n +2 | awk '{ $1=$2=$3=""; sub(/^ +/, ""); print }'; }
check() { if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi; }
run() { local s=$REPO/tests/ui_scripts/$1; rm -f "$s.log"; (cd "$REPO/build" && ./gonptechan.exe --headless --ui-script "$(wslpath -w "$s")"); echo "exit $?"; cat "$s.log"; }

# 0. fresh copy
if [ ! -f "$TEST/orig.sha256" ]; then
	mkdir -p "$GAME" && (cd "$STEAM" && cp 000?.p MBAA.exe "$GAME"/ && sha256sum 000?.p > "$TEST/orig.sha256")
fi
(cd "$GAME" && sha256sum -c --quiet "$TEST/orig.sha256") && echo "copy matches the Steam archives"
cd "$GAME"
total=0; for p in 000?.p; do n=$("$TOOL" count "$p" | awk '{print $(NF-3)}'); total=$((total + n)); done
echo "archive entries total: $total"
# unique paths (case-insensitive) = what unpack writes
uniq=$(for p in 000?.p; do lsent "$p"; done | tr 'A-Z' 'a-z' | sort -u | wc -l)
echo "unique paths: $uniq"

# 1. unpack
run p_unpack_1_unpack.txt
check "no .p left in the game folder" '[ -z "$(ls "$GAME"/000?.p 2>/dev/null)" ]'
check "9 .p in _packed_backup" '[ "$(ls "$GAME"/_packed_backup/000?.p | wc -l)" = 9 ]'
files=$(find "$GAME" -type f ! -path "*/_packed_backup/*" ! -name MBAA.exe | wc -l)
echo "loose files written: $files"
check "loose file count == unique archive paths" '[ "$files" = "$uniq" ]'
# sample: 40 random entries of each archive whose path no higher-priority archive has, extracted with fbarctool and compared with the loose file
cd "$GAME/_packed_backup"
order="0008.p 0003.p 0005.p 0002.p 0007.p 0000.p 0004.p 0006.p 0001.p"; seen=$(mktemp); bad=0; n=0
for p in $order; do
	lsent "$p" > /tmp/.pl_$$
	shuf -n 40 --random-source=<(yes) /tmp/.pl_$$ | while IFS= read -r e; do
		grep -qixF "$e" "$seen" && continue
		out=$(mktemp -d); "$TOOL" extract "$p" "$e" "$(wslpath -w "$out")" > /dev/null
		cmp -s "$out/$e" "$GAME/$e" || echo "MISMATCH $p $e"
		rm -rf "$out"
	done | tee -a /tmp/.mm_$$
	cat /tmp/.pl_$$ >> "$seen"
done
check "sampled entries byte-identical to the loose files" '[ ! -s /tmp/.mm_$$ ]'
rm -f /tmp/.pl_$$ /tmp/.mm_$$ "$seen"

# 2. modify one loose file (the newest-priority copy of a 0002.p text entry), pack, check
cd "$GAME"
higher=$(for q in 0008.p 0003.p 0005.p; do lsent "_packed_backup/$q"; done)
target=$(lsent _packed_backup/0002.p | grep -i '\.txt$' | while IFS= read -r e; do grep -qixF "$e" <<< "$higher" || { echo "$e"; break; }; done)
[ -n "$target" ] || { echo "FAIL no entry to modify"; exit 1; }
echo "modified entry: $target"
printf '\r\n// p_unpack_roundtrip edit\r\n' >> "$GAME/$target"
cp "$GAME/$target" "$TEST/modified.bin"
run p_unpack_2_pack.txt
cd "$GAME/_packed_backup"
check "0002.p.bak = the original 0002.p" '[ "$(sha256sum < 0002.p.bak | cut -c1-64)" = "$(grep " 0002.p" "$TEST/orig.sha256" | cut -c1-64)" ]'
"$TOOL" diff 0002.p.bak 0002.p | tail -3
check "rebuilt 0002.p: exactly one entry differs" '[ "$("$TOOL" diff 0002.p.bak 0002.p | grep -c "^DIFF")" = 1 ] && "$TOOL" diff 0002.p.bak 0002.p | grep -q "identical $(( $("$TOOL" count 0002.p | awk "{print \$(NF-3)}") - 1 )), different 1, only in a 0, only in b 0"'
out=$(mktemp -d); "$TOOL" extract 0002.p "$target" "$(wslpath -w "$out")" > /dev/null
check "the changed entry holds the new bytes" 'cmp -s "$out/$target" "$TEST/modified.bin"'; rm -rf "$out"
for p in 0000.p 0001.p 0003.p 0004.p 0005.p 0006.p 0007.p 0008.p; do
	check "$p sha256 unchanged (4 = forced rebuild)" '[ "$(sha256sum < "$p" | cut -c1-64)" = "$(grep " $p" "$TEST/orig.sha256" | cut -c1-64)" ]'
done
check "forced rebuild of 0004.p kept 0004.p.bak" '[ -f 0004.p.bak ]'

# 3. restore
run p_unpack_3_restore.txt
cd "$GAME"
check "9 .p back in the game folder" '[ "$(ls 000?.p | wc -l)" = 9 ]'
check "only 0002.p differs from the original" '[ "$(sha256sum -c "$TEST/orig.sha256" 2>/dev/null | grep -c FAILED)" = 1 ] && sha256sum -c "$TEST/orig.sha256" 2>/dev/null | grep -q "0002.p: FAILED"'
[ $fail = 0 ] && echo "ROUND TRIP OK" || echo "ROUND TRIP FAILED"
exit $fail
