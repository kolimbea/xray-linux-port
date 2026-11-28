#!/bin/bash
# XR_BIN wrapper for tools/run_level.sh and tools/pgo.sh: the game in a headless gamescope (nothing on the
# desktop), at low CPU priority. XR_REAL_BIN = the game binary (default build/monolith/xray_vk);
# HL_W/HL_H = resolution (default 1920x1080).
# A private /tmp/.X11-unix: inside run_level's bwrap the real one belongs to an unmapped root, which
# gamescope's Xwayland refuses. Everything runs in a session of its own; a watcher kills that whole
# session once this script is gone (run_level stops the game with TERM, then KILL) or the game exited,
# otherwise gamescopereaper and the game survive the stop and keep the single-instance lock.
#   XR_BIN=tools/headless.sh tools/run_level.sh 4 400
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
command -v gamescope > /dev/null || { echo "headless.sh: gamescope not found" >&2; exit 1; }
nice -n 10 setsid bwrap --dev-bind / / --tmpfs /tmp/.X11-unix --die-with-parent \
    gamescope --backend headless -W "${HL_W:-1920}" -H "${HL_H:-1080}" -- \
    "${XR_REAL_BIN:-$ROOT/build/monolith/xray_vk}" "$@" &
child=$!
self=$$
(
    while kill -0 $self 2>/dev/null && kill -0 $child 2>/dev/null; do sleep 1; done
    pkill -TERM -s $child 2>/dev/null
    sleep 3
    pkill -KILL -s $child 2>/dev/null
) &
wait $child
