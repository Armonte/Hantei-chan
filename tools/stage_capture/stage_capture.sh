#!/usr/bin/env bash
# Ground-truth stage capture from the real MBAACC (docs/bg_research/STAGE_AUDIT.md, "Ground truth").
#
# Runs ONLY the separate test copy /mnt/c/games/mbaacc_winaspect (never mbaacc_tag, the user's play copy),
# kills only an MBAA.exe whose path is under that dir, and refuses to start if one is already running there
# (it never touches another agent's instance: every op, and the kill, use the pid this script launched).
#
#   stage_capture.sh start <stage_id>        launch MBAA.exe + pchost (PCHOST_STAGE=<id> forces the stage on
#                                            the offline VS boot), wait for InGame
#   stage_capture.sh stageonly               patch the running game (test copy only) so the scene draws the
#                                            stage alone: no fighters, effects, HUD; camera from our globals
#   stage_capture.sh cam <x> <y> [zoom]      set the working camera (world px; the game stores x*128)
#   stage_capture.sh freeze | unfreeze       stop / resume the stage animation (draw continues)
#   stage_capture.sh step <n>                while frozen: advance the stage exactly n ticks
#   stage_capture.sh shot <outdir> <name>    backbuffer BMP + a state dump (bg pool, particles, camera, rng)
#   stage_capture.sh stop
#
# Every patch is applied with pcmem.exe (tools/stage_capture/pcmem.c) to the test-copy process in memory; the
# exe on disk is never modified.
set -uo pipefail
DIR=/mnt/c/games/mbaacc_winaspect
WDIR='C:\games\mbaacc_winaspect'
HERE="$(cd "$(dirname "$0")" && pwd)"
PCMEM="$HERE/pcmem.exe"
TAG=stg
PIDF="$HERE/.game.pid"
# Every memory op targets the PID recorded at launch; pcmem also refuses any pid not under mbaacc_winaspect.
M() { [ -s "$PIDF" ] || { echo "!! no recorded pid (run start)"; return 1; }; "$PCMEM" "$(cat "$PIDF")" "$@"; }
# Kill ONLY the MBAA.exe this script launched (recorded pid, re-checked to be the test-copy image). The
# folder is shared with other agents: never kill by image name or folder.
ps_pids() {   # MBAA.exe pids whose path is under mbaacc_winaspect
    powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='MBAA.exe'\" | ? { \$_.ExecutablePath -like '*mbaacc_winaspect*' } | % { \$_.ProcessId }" 2>/dev/null | tr -d '\r' | grep -E '^[0-9]+$'
}
killours() {
    [ -s "$PIDF" ] || return 0
    local p; p=$(cat "$PIDF")
    powershell.exe -NoProfile -Command "\$x = Get-Process -Id $p -ErrorAction SilentlyContinue; if (\$x -and \$x.Path -like '*mbaacc_winaspect\\MBAA.exe') { Stop-Process -Id $p -Force }" >/dev/null 2>&1
}

case "${1:-}" in
start)
    STAGE="$2"
    # Other agents may run their own MBAA.exe from this folder; ours is told apart by pid (see below), so a
    # stray is reported, not fatal. Our previous instance (recorded pid) is stopped first.
    killours
    [ -z "$(ps_pids)" ] || echo "note: other MBAA.exe running under $DIR: $(ps_pids | tr '\n' ' ')"
    rm -f "$DIR"/pchost_${TAG}*.log "$PIDF"
    SETS="set PCHOST_GAME=mbaacc&& set PCHOST_LOG_TAG=$TAG&& set PCHOST_STAGE=$STAGE&& set PCHOST_BOOT_STOP=ingame&& "
    BEFORE=$(ps_pids | sort)
    # absolute dll path: the folder's shim loads PCHOST_DLL_PATH only for a path with a directory, a bare
    # name silently runs the folder's own pchost.dll
    ( cd "$DIR" && cmd.exe /c "${SETS}pc_inject.exe MBAA.exe ${WDIR}\\pchost_stage.dll" ) >/dev/null 2>&1 &
    for i in $(seq 1 120); do
        L=$(ls -t "$DIR"/pchost_${TAG}*.log 2>/dev/null | head -1)
        if [ -n "$L" ] && grep -q "SCENE -> InGame" "$L"; then
            # our pid = the MBAA.exe that appeared under this folder since launch
            P=$(comm -13 <(echo "$BEFORE") <(ps_pids | sort) | head -1); [ -n "$P" ] || { echo "!! no new pid"; exit 1; }
            echo "$P" > "$PIDF"; echo "InGame: $L pid=$P"; exit 0; fi
        sleep 1
    done
    echo "!! no InGame within 120 s"; exit 1 ;;
stageonly)
    # Battle_TickCameraEffectsAndQueueDraws 0x423860:
    #   0x423869 call Camera_UpdateAndPushFightersInStage -> call Camera_UpdateMatrices (0x44BCB0), so the
    #            matrices come from the working camera 0x55DEC4/0x55DEC8 and zoom 0x54EB70 that we write
    #   0x42388D call Battle_RenderAllCharacters, 0x423892 Battle_DrawAllEffectEntities,
    #   0x423897 SysEffectPool_TickAndDraw, 0x42389C Hud_UpdateAndDrawComboDisplays -> NOP
    #   0x4238A1 mov eax,[esp+4]; push eax; call Hud_DrawAll (10 bytes) -> NOP
    M w 423869 e842840200 && M w 42388d 9090909090 && M w 423892 9090909090 && M w 423897 9090909090 \
      && M w 42389c 9090909090 && M w 4238a1 90909090909090909090 && echo patched ;;
chars)
    # restore the fighter + effect draw (for light-shadow captures), keep the HUD off
    M w 42388d e84eaa0400 && M w 423892 e829080300 && echo chars-on ;;
cam)
    X=$(( ${2} * 128 )); Y=$(( ${3} * 128 ))
    hx=$(printf '%08x' $(( X & 0xffffffff )) | sed 's/\(..\)\(..\)\(..\)\(..\)/\4\3\2\1/')
    hy=$(printf '%08x' $(( Y & 0xffffffff )) | sed 's/\(..\)\(..\)\(..\)\(..\)/\4\3\2\1/')
    M w 55dec4 "$hx$hy"
    if [ -n "${4:-}" ]; then
        hz=$(python3 -c "import struct,sys;print(struct.pack('<f',float(sys.argv[1])).hex())" "$4"); M w 54eb70 "$hz"
    fi ;;
step)     M step "$2" ;;   # exact n-tick stage advance while frozen (pcmem remote call of the 6 updaters)
freeze|unfreeze)
    # pchost pins the game's "stage animation" option every frame and advances the stage from its Present
    # hook, so the option cannot freeze it. Instead the six updaters are RET-patched (pcmem freeze).
    M "$1" ;;
shot)
    OUT="$2"; NAME="$3"; mkdir -p "$OUT"
    rm -f "$DIR/_stg_${NAME}.bmp"
    printf '%s\\_stg_%s.bmp\n' "$WDIR" "$NAME" > "$DIR/pchost_shot_${TAG}.req"
    for i in $(seq 1 60); do [ -s "$DIR/_stg_${NAME}.bmp" ] && break; sleep 0.25; done
    sleep 0.5
    [ -s "$DIR/_stg_${NAME}.bmp" ] || { echo "!! no shot"; exit 1; }
    mv -f "$DIR/_stg_${NAME}.bmp" "$OUT/${NAME}.bmp"
    python3 -c "import sys;from PIL import Image;Image.open(sys.argv[1]).convert('RGB').save(sys.argv[2])" "$OUT/${NAME}.bmp" "$OUT/${NAME}.png" && rm -f "$OUT/${NAME}.bmp"
    W=$(wslpath -w "$OUT")
    cat > "$HERE/.dump.txt" <<EOF
r 750840 88000 $W\\${NAME}.pool
r 766000 4500 $W\\${NAME}.drop
r 55dec4 8 $W\\${NAME}.cam
r 54eb70 4 $W\\${NAME}.zoom
r 76e79c 4 $W\\${NAME}.bgframe
r 74fd98 4 $W\\${NAME}.stage
r 563780 2508 $W\\${NAME}.rng
EOF
    M script "$(wslpath -w "$HERE/.dump.txt")" && echo "$OUT/${NAME}.png" ;;
stop) killours; rm -f "$PIDF" ;;
*) sed -n 2,20p "$0"; exit 1 ;;
esac
