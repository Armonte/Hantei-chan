#!/bin/bash
# launch.sh app-args...  -> prints PID
ARGS=""; for a in "$@"; do ARGS="$ARGS,'${a//\'/\'\'}'"; done; ARGS=${ARGS#,}
powershell.exe -NoProfile -Command "\$p=Start-Process -FilePath C:\\dev\\hantei-chan\\work\\h2shots\\gonptechan.exe -WorkingDirectory C:\\dev\\hantei-chan\\work\\h2shots -ArgumentList @($ARGS) -PassThru; Start-Sleep 5; \$p.Id" | tr -d '\r'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\hantei-chan\work\hidecon.ps1' -ProcId 0 >&2
