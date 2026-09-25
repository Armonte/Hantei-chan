#!/usr/bin/env bash
# Stage Link verification launcher (docs/HANTEI_STAGE_LINK.md §6). Runs ONLY in /mnt/c/games/mbaacc_winaspect — a
# folder other agents (the stage audit) also launch games in, so this script:
#   * injects its OWN dll (pchost_stagelink.dll), never overwriting pchost.dll / pchost_stage.dll. The dll is
#     passed by ABSOLUTE path: pc-launch stages PCHOST_DLL_PATH only for a path with a directory, and the
#     game folder's winmm/d3d9 shim loads that, else pchost.dll beside the game. A bare name silently ran the
#     folder's pchost.dll (measured 2026-09-24: build 5ccfb3dd ran instead of ours);
#   * uses its own PCHOST_LOG_TAG, and remembers the PID it launched (diff of the MBAA.exe list before/after);
#   * only ever stops THAT pid (and only if its path is under mbaacc_winaspect);
#   * moves the borderless window to the secondary 640x480 display at x=-640.
#
#   run_stage_game.sh start <tag> <dll>    copy <dll> -> pchost_stagelink.dll, launch, print the new pid, wait InGame
#   run_stage_game.sh place <pid>          move that pid's window to (-640,0) 640x480
#   run_stage_game.sh shot <tag> <name>    backbuffer screenshot via pchost_shot_<tag>.req -> docs/stage_link/<name>.bmp
#   run_stage_game.sh stop <pid>           stop ONLY that pid (refuses a pid outside mbaacc_winaspect)
#   run_stage_game.sh pack-aside | pack-restore   park/restore 0000.p (the .\bg archive; its 70 files are
#                                          byte-identical to the loose Bg\ copies, measured 2026-09-24), so edits of
#                                          loose Bg\ files are what the game reads. pack-aside refuses while any
#                                          other MBAA.exe runs from this folder.
#   PARK=1 run_stage_game.sh start ...     park 0000.p only for OUR boot: MBAA registers its packs once, in
#                                          LoadAllPackFiles (0x41E471.., fixed names ./0000.p..), so a game that
#                                          booted without it never reads it; 0000.p is restored as soon as our
#                                          game is InGame. Anyone else booting inside that window reads the
#                                          identical loose copies.
set -uo pipefail
DIR=/mnt/c/games/mbaacc_winaspect
WDIR='C:\games\mbaacc_winaspect'
HERE="$(cd "$(dirname "$0")" && pwd)"
ps_pids() {   # MBAA.exe pids whose path is under mbaacc_winaspect
    powershell.exe -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='MBAA.exe'\" | ? { \$_.ExecutablePath -like '*mbaacc_winaspect*' } | % { \$_.ProcessId }" 2>/dev/null | tr -d '\r' | grep -E '^[0-9]+$'
}
case "${1:-}" in
start)
    TAG="$2"; DLL="$3"
    cp -f "$DLL" "$DIR/pchost_stagelink.dll" || exit 1
    export PCHOST_MBAACC_LINK=1
    SETS="set PCHOST_GAME=mbaacc&& set PCHOST_LOG_TAG=$TAG&& "
    while IFS= read -r n; do
        [ -n "$n" ] || continue
        eval "v=\${$n}"; SETS="${SETS}set $n=${v}&& "
    done <<< "$(env | sed -n 's/^\(PCHOST_[A-Za-z0-9_]*\)=.*/\1/p' | grep -v '^PCHOST_GAME$\|^PCHOST_LOG_TAG$' | sort)"
    if [ "${PARK:-0}" = 1 ]; then "$0" pack-aside || exit 1; fi
    # THE FOLDER'S winmm.dll SHIM IS OLD (2026-09-23): it has no PCHOST_DLL_PATH support and always loads
    # pchost.dll beside the game. So OUR dll is swapped in as pchost.dll for OUR boot only, and the original is
    # put back (byte-identical, sha1-checked) the moment the game is InGame -- a mapped DLL can be renamed.
    others="$(ps_pids | tr '\n' ' ')"
    [ -z "${others// /}" ] || { [ "${PARK:-0}" = 1 ] && "$0" pack-restore; echo "!! MBAA.exe running here ($others); not swapping pchost.dll"; exit 1; }
    ORIG_SHA=$(sha1sum "$DIR/pchost.dll" | cut -d' ' -f1)
    mv "$DIR/pchost.dll" "$DIR/pchost.dll.stagelink_orig" && cp -f "$DLL" "$DIR/pchost.dll"
    swapback() {
        [ -f "$DIR/pchost.dll.stagelink_orig" ] || return 0
        mv -f "$DIR/pchost.dll" "$DIR/pchost_stagelink_running_$$.dll" 2>/dev/null
        mv -f "$DIR/pchost.dll.stagelink_orig" "$DIR/pchost.dll"
        [ "$(sha1sum "$DIR/pchost.dll" | cut -d' ' -f1)" = "$ORIG_SHA" ] && echo "pchost.dll restored ($ORIG_SHA)" || echo "!! pchost.dll restore MISMATCH"
    }
    before=" $(ps_pids | tr '\n' ' ') "
    echo ">> other MBAA.exe in this folder before launch:${before}"
    ( cd "$DIR" && cmd.exe /c "${SETS}pc_inject.exe MBAA.exe ${WDIR}\\pchost_stagelink.dll" ) >/dev/null 2>&1 &
    PID=""
    for i in $(seq 1 40); do
        for p in $(ps_pids); do case "$before" in *" $p "*) ;; *) PID=$p ;; esac; done
        [ -n "$PID" ] && break; sleep 0.5
    done
    [ -n "$PID" ] || { swapback; [ "${PARK:-0}" = 1 ] && "$0" pack-restore; echo "!! no new MBAA.exe appeared"; exit 1; }
    echo "PID=$PID"
    for i in $(seq 1 90); do
        L="$DIR/pchost_${TAG}.log"
        if [ -f "$L" ] && grep -q "SCENE -> InGame" "$L"; then
            echo "InGame: $L"; swapback; [ "${PARK:-0}" = 1 ] && "$0" pack-restore; "$0" place "$PID"; exit 0; fi
        sleep 1
    done
    swapback; [ "${PARK:-0}" = 1 ] && "$0" pack-restore
    echo "!! no InGame within 90 s"; exit 1 ;;
place)
    PID="$2"
    powershell.exe -NoProfile -Command "
      Add-Type -Namespace W -Name U -MemberDefinition '[DllImport(\"user32.dll\")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);';
      \$p = Get-Process -Id $PID; [W.U]::SetWindowPos(\$p.MainWindowHandle, [IntPtr]::Zero, -640, 0, 640, 480, 0x0044) | Out-Null" ;;
shot)
    TAG="$2"; NAME="$3"
    rm -f "$DIR/_stagelink_${NAME}.bmp"
    printf '%s\\_stagelink_%s.bmp\n' "$WDIR" "$NAME" > "$DIR/pchost_shot_${TAG}.req"
    for i in $(seq 1 40); do [ -s "$DIR/_stagelink_${NAME}.bmp" ] && break; sleep 0.25; done
    sleep 0.5
    if [ -s "$DIR/_stagelink_${NAME}.bmp" ]; then mv -f "$DIR/_stagelink_${NAME}.bmp" "$HERE/${NAME}.bmp"; echo "$HERE/${NAME}.bmp"; else echo "!! no shot"; exit 1; fi ;;
stop)
    PID="$2"
    case " $(ps_pids | tr '\n' ' ') " in *" $PID "*) ;; *) echo "!! $PID is not an MBAA.exe under mbaacc_winaspect; not touching it"; exit 1 ;; esac
    powershell.exe -NoProfile -Command "Stop-Process -Id $PID -Force" ;;
pack-aside)
    others="$(ps_pids | tr '\n' ' ')"
    [ -z "${others// /}" ] || { echo "!! MBAA.exe running from this folder ($others): not moving 0000.p"; exit 1; }
    [ -f "$DIR/0000.p" ] && mv "$DIR/0000.p" "$DIR/0000.p.stagelink_aside" && echo "0000.p parked" ;;
pack-restore)
    [ -f "$DIR/0000.p.stagelink_aside" ] && mv "$DIR/0000.p.stagelink_aside" "$DIR/0000.p" && echo "0000.p restored" ;;
*) echo "usage: see banner"; exit 1 ;;
esac
