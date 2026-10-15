#!/bin/bash
# ctest pcmod_cli: pcmodtool pack -> verify (folder + zip) -> digest -> session on the PovertyCaster fixture content.
# Asserts the four pinned schema-1 digests (PovertyCaster mods/pcmod-schema 96889671e) through the command line. Args: <pcmodtool.exe> <fixture dir> <scratch dir>.
set -u
TOOL="$1"; FIX="$2"; OUT="$3"
EXE=0000000000000000000000000000000000000000000000000000000000000001
PKG=eb3db61182ed3ee9875f68f6f668a1671ff3744340e073dffcbfc966998ddf27
SIM=16a83aa066f3a94620eed7219529cb3b6463d158e080c02190345a1d1ddc6041
SET=8c4fe7957fcc0be5912de92c093dcab0a47d812b6d080e6ab5517588df7341cc
VAN=f9956a1ae59ce26aa14e3c4d97597da9a665727f9b87539cd48432b10636c0db
fail=0
check() { if [ "$1" != "$2" ]; then echo "FAIL $3: got '$1' expected '$2'"; fail=1; else echo "ok   $3"; fi; }
rm -rf "$OUT"; mkdir -p "$OUT/src"
cp -r "$FIX/data" "$FIX/grp" "$OUT/src/"
# Windows exe from WSL: hand it Windows paths when wslpath exists.
W() { if command -v wslpath >/dev/null 2>&1; then wslpath -w "$1"; else echo "$1"; fi; }
"$TOOL" pack "$(W "$OUT/src")" --id pcmod-fixture --version 1.2.3 --exe $EXE --priority 100 --game "" --out "$(W "$OUT/pkg")" \
	--name "PovertyCaster pcmod fixture" | tr -d '\r' | tee "$OUT/pack.log"
check "$(grep '^class:' "$OUT/pack.log")" "class: sim (detected)" "class auto-detected"
check "$(grep '^package digest' "$OUT/pack.log" | awk '{print $3}')" "$PKG" "pack digest"
check "$(grep '^sim digest' "$OUT/pack.log" | awk '{print $3}')" "$SIM" "pack sim digest"
check "$(grep '^files:' "$OUT/pack.log")" "files: 3, 2 in the online sim digest" "per-file classes"
"$TOOL" verify "$(W "$OUT/pkg/pcmod-fixture")" >/dev/null; check "$?" 0 "verify folder"
"$TOOL" verify "$(W "$OUT/pkg/pcmod-fixture-1.2.3.zip")" >/dev/null; check "$?" 0 "verify zip"
"$TOOL" digest "$(W "$OUT/pkg/pcmod-fixture-1.2.3.zip")" | tr -d '\r' > "$OUT/digest.log"
check "$(awk '$1=="package"{print $2}' "$OUT/digest.log")" "$PKG" "digest zip (package)"
check "$(awk '$1=="sim"{print $2}' "$OUT/digest.log")" "$SIM" "digest zip (sim)"
check "$("$TOOL" session $EXE "$(W "$OUT/pkg/pcmod-fixture")" | tr -d '\r' | grep '^session digest' | awk '{print $3}')" "$SET" "session digest"
check "$("$TOOL" session $EXE | tr -d '\r' | grep '^session digest' | awk '{print $3}')" "$VAN" "vanilla set"
# a wrong exe makes the sim package broken: no digest
"$TOOL" session 0000000000000000000000000000000000000000000000000000000000000002 "$(W "$OUT/pkg/pcmod-fixture")" >/dev/null; check "$?" 1 "wrong exe refused"
# tamper -> broken
printf 'bas' > "$OUT/pkg/pcmod-fixture/data/_type.txt"
"$TOOL" verify "$(W "$OUT/pkg/pcmod-fixture")" >/dev/null; check "$?" 1 "tampered folder refused"
# cosmetic class with a data\ file is refused
"$TOOL" pack "$(W "$OUT/src")" --id cos --version 1.0.0 --class cosmetic --game "" --out "$(W "$OUT/pkg")" >/dev/null 2>&1; check "$?" 1 "cosmetic + data refused"
# class sim with no sim-class file is refused (grp\ only)
mkdir -p "$OUT/cosonly"; cp -r "$FIX/grp" "$OUT/cosonly/"
"$TOOL" pack "$(W "$OUT/cosonly")" --id simnone --version 1.0.0 --class sim --exe $EXE --game "" --out "$(W "$OUT/pkg")" >/dev/null 2>&1; check "$?" 1 "sim without a sim file refused"
# a refused root (PackDataVersion\) fails closed
mkdir -p "$OUT/refused/PackDataVersion"; printf '3' > "$OUT/refused/PackDataVersion/pack_0008_version.txt"; cp -r "$FIX/data" "$OUT/refused/"
"$TOOL" pack "$(W "$OUT/refused")" --id refused --version 1.0.0 --exe $EXE --game "" --out "$(W "$OUT/pkg")" >/dev/null 2>&1; check "$?" 1 "PackDataVersion refused"
exit $fail
