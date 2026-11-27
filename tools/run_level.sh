#!/bin/bash
# Runs the native build on the GAMMA fixture, loads the autotest save, waits until N frame-stat windows
# (600 frames each) were logged in the level, stops the game and prints the stats.
#   tools/run_level.sh [windows=6] [timeout_s=400] [extra engine args...]
# Env: XR_BIN (default build/monolith/xray_vk), LTX (default user_bench.ltx; "" = player's user.ltx).
# The GAMMA install is mounted read-only. Output: one line per in-level stats window, then the median.
set -u
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
N=${1:-6}
TIMEOUT=${2:-400}
shift $(( $# >= 2 ? 2 : $# ))
BIN=${XR_BIN:-$ROOT/build/monolith/xray_vk}
LTX=${LTX-user_bench.ltx}
FIX=$ROOT/build/fixture-gamma
LOG=$FIX/appdata/logs/xray_$(id -un).log
cd "$FIX" || exit 1
rm -f "$LOG"
ARGS=(-nointro)
[ -n "$LTX" ] && ARGS+=(-ltx "$LTX")
ARGS+=("$@" -start "server(autotest/single/alife/load) client(localhost)")
XR_NO_MSGBOX=1 bwrap --die-with-parent --dev-bind / / --ro-bind "$HOME/Games/gamma" "$HOME/Games/gamma" \
    "$BIN" "${ARGS[@]}" < /dev/null > "$FIX/appdata/logs/run_level.out" 2>&1 &
PID=$!
deadline=$(( $(date +%s) + TIMEOUT ))
in_level() { grep -E "vk stats.* [0-9]{4,} draws" "$LOG" 2>/dev/null | wc -l; }
while kill -0 $PID 2>/dev/null && [ "$(in_level)" -lt "$N" ] && [ "$(date +%s)" -lt "$deadline" ]; do
    sleep 2
done
if kill -0 $PID 2>/dev/null; then
    kill $PID; sleep 3; kill -9 $PID 2>/dev/null
    status="stopped"
else
    wait $PID; status="exited ($?)"
fi
echo "game $status; in-level windows: $(in_level)"
grep -E "vk stats.* [0-9]{4,} draws" "$LOG" | sed -E 's/^\* vk stats: //' | cut -c1-150
grep -E "vk stats.* [0-9]{4,} draws" "$LOG" | tail -n +3 | awk '{print $4}' | sort -n |
    awk '{a[NR]=$1} END{if (NR) printf "median %.2f ms/frame over %d windows (first 2 skipped)\n", a[int((NR+1)/2)], NR}'
grep -cE "Failed to load script" "$LOG" | sed 's/^/failed scripts: /'
grep -E "FATAL|stack trace" "$LOG" | head -3
