#!/bin/bash
# run.sh "actions" app-args...   kills the previous instance (by PID), launches, runs actions
D="$(dirname "$0")"; . $D/env.sh
ACT="$1"; shift
[ -f $TMPD/pid ] && $D/kill.sh $(cat $TMPD/pid) >/dev/null 2>&1
PID=$($D/launch.sh "$@"); echo $PID > $TMPD/pid
$D/sh.sh $PID "win 0 0 1920 1040; w 800; $ACT"
