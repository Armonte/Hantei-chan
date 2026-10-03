#!/bin/bash
# sh.sh PID "actions"  (see i18n_shoot.ps1)
. "$(dirname "$0")/env.sh"
cp $REPO/tools/i18n_shoot/i18n_shoot.ps1 /mnt/c/dev/hantei-chan/work/i18n_shoot.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\dev\hantei-chan\work\i18n_shoot.ps1' -ProcId "$1" -Out 'C:\dev\hantei-chan\work\i18nshots' -Actions "$2" 2>&1 | tr -d '\r'
