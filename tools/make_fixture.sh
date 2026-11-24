#!/bin/sh
# Creates a run directory for the engine on top of the (read-only) Anomaly install:
#   build/fixture-anomaly/{fsgame.ltx, appdata/, db -> install/db, gamedata -> install/gamedata}
# Writes go to appdata/ only; run the engine under bwrap --ro-bind on the install for safety.
set -e
cd "$(dirname "$0")/.."
SRC=${ANOMALY_DIR:-$HOME/Games/gamma/anomaly}
F=build/fixture-anomaly
mkdir -p "$F/appdata/logs"
cp "$SRC/fsgame.ltx" "$F/fsgame.ltx"
ln -sfn "$SRC/db" "$F/db"
ln -sfn "$SRC/gamedata" "$F/gamedata"
echo "$PWD/$F"
