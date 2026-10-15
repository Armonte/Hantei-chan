#!/bin/bash
# Converts the whole ActCadenza roster with ha4tool convert (S8b) into $OUT (default roster_v3/).
# Layout matches roster/ and roster_v2/: characters in <char>/, CSEL_*/EFFECT/LASTLOAD in _system/<name>/.
#   AC_VERSION  1.03A (default; CTs from the extracted 06.p in $AC_PATCH) or 1.00
#   MBAACC      vanilla MBAACC data (0002.p extract) for the spawn-leak check and the giant donors
TOOL=${TOOL:-/mnt/c/dev/hantei-chan/wt/s8b-actcadenza-data/build/ha4tool.exe}
G=/mnt/c/dev/hantei-chan/gamedata
SRC=${SRC:-$G/mbac/02}
OUT=${OUT:-$G/mbac_to_cc/roster_v3}
AC_VERSION=${AC_VERSION:-1.03A}
AC_PATCH=${AC_PATCH:-$G/mbac_v11_english/06}
MBAACC=${MBAACC:-$G/mbaacc_ref/vanilla_0002}
W() { wslpath -w "$1"; }
cd "$SRC" || exit 1
fails=0
for d in *.DAT; do
  n=$(echo "${d%.DAT}" | tr A-Z a-z)
  extra=()
  case $n in
    csel_*|effect|lastload) o=$OUT/_system/$n ;;
    gakiha)  o=$OUT/$n; extra=(--giant-flag 2 --giant-donor "$(W $MBAACC/g_akiha.HA6)") ;;   # MBAACC G_AKIHA #16 layout
    g_chaos) o=$OUT/$n; extra=(--giant-flag 1 --giant-donor "$(W $MBAACC/Hermes.HA6)") ;;    # MBAACC HERMES #32 layout
    *) o=$OUT/$n; extra=(--mbaacc-data "$(W $MBAACC)") ;;
  esac
  mkdir -p "$o"
  in=$SRC/$d
  # 1.03A: a .DAT in the 06.p overlay replaces the 02.p one (only G_CHAOS.DAT: new parts texture)
  [ "$AC_VERSION" = 1.03A ] && [ -f "$AC_PATCH/$d" ] && in=$AC_PATCH/$d
  echo "=== $in -> $o"
  "$TOOL" convert "$(W $in)" -o "$(W $o)" --ac-version "$AC_VERSION" --ac-patch "$(W $AC_PATCH)" --ac-base "$(W $SRC)" "${extra[@]}" || fails=$((fails+1))
done
echo "FAILS $fails"
