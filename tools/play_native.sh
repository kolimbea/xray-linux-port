#!/bin/bash
# Starts the native Linux build (own Vulkan renderer) on the GAMMA test fixture. The GAMMA install is
# mounted read-only; the game writes only into build/fixture-gamma (saves, logs, screenshots).
set -u
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
BIN="$ROOT/build/monolith/xray_vk"
FIX="$ROOT/build/fixture-gamma"
GAMMA="$HOME/Games/gamma"
err() { notify-send -a "X-Ray native" "$1" 2>/dev/null; echo "$1" >&2; exit 1; }
[ -x "$BIN" ] || err "No build at $BIN"
[ -d "$FIX/gamedata" ] || err "No fixture at $FIX (tools/make_fixture_gamma.py)"
[ -d "$GAMMA" ] || err "No GAMMA install at $GAMMA"
pgrep -f "^$BIN" >/dev/null && err "The game is already running"
cd "$FIX" || exit 1
exec bwrap --die-with-parent --dev-bind / / --ro-bind "$GAMMA" "$GAMMA" "$BIN" -nointro "$@" \
    > "$FIX/appdata/logs/launcher.out" 2>&1
