#!/bin/bash
# Profile-guided optimisation of the release build. tools/packaging/build_portable.sh compiles with
# build/pgo/release.profdata when it exists; this script makes it:
#  1. an instrumented build (-fprofile-generate) in the release container (Debian 13, clang 19), so the
#     profile matches the compiler and the code of the release;
#  2. a training run on this machine: the bench level of the GAMMA fixture (tools/run_level.sh, headless
#     with gamescope when available) until a fixture script quits the game, which writes the profile;
#  3. the merge with llvm-profdata in the container.
# Measured 2026-11-28 with the dev build (clang 22, same method): 27.6 -> 25.1 ms/frame on the bench level.
# Re-run it when the engine code changed a lot; a stale profile still builds (warnings off) but helps less.
#   tools/pgo.sh [training_seconds=150]
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
SECS=${1:-150}
IMAGE=xray-native-build:debian13
B=build/pgo
FIX=$ROOT/build/fixture-gamma
[ -d "$FIX/gamedata" ] || { echo "no GAMMA fixture: run tools/make_fixture_gamma.py first" >&2; exit 1; }
podman build -q -t "$IMAGE" -f "$ROOT/tools/packaging/Containerfile.debian13" "$ROOT/tools/packaging" > /dev/null
"$ROOT/tools/fetch_deps.sh" > /dev/null
mkdir -p "$ROOT/$B"
RAW=$(mktemp -d "$ROOT/$B/raw.XXXX")

echo "== instrumented build"
podman run --rm -v "$ROOT:$ROOT:z" -w "$ROOT" -e B=$B -e RAW="$RAW" "$IMAGE" bash -euo pipefail -c '
    F="-fprofile-generate=$RAW"
    cmake -S monolith -B $B/gen -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DXR_THINLTO=ON \
        "-DCMAKE_C_FLAGS=$F" "-DCMAKE_CXX_FLAGS=$F" \
        "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--no-dependent-libraries $F" \
        "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -Wl,--no-dependent-libraries $F" > /dev/null
    ninja -C $B/gen xray_vk > $B/gen.log 2>&1 || { tail -30 $B/gen.log; exit 1; }
'
rm -f "$RAW"/*.profraw # the build runs instrumented tools (code generators)
# Libraries this host lacks (the release bundles them, e.g. ICU 76), copied from the container. Only
# those: LD_LIBRARY_PATH also reaches gamescope and must not swap host libraries for older ones.
rm -rf "$ROOT/$B/lib" && mkdir -p "$ROOT/$B/lib"
MISSING=$(ldd "$ROOT/$B/gen/xray_vk" | awk '/not found/ {print $1}' | tr '\n' ' ')
if [ -n "$MISSING" ]; then
    podman run --rm -v "$ROOT:$ROOT:z" -w "$ROOT" -e MISSING="$MISSING" -e B=$B "$IMAGE" bash -euo pipefail -c '
        for l in $MISSING; do cp -L "$(ldd $B/gen/xray_vk | awk -v l=$l "\$1 == l {print \$3}")" $B/lib/; done'
fi

echo "== training run (${SECS} s in the level)"
cat > "$FIX/gamedata/scripts/zz_pgo_quit.script" <<'LUA'
-- Fixture-only (tools/pgo.sh): while appdata/pgo_quit.txt exists (first line: seconds), quits the game that
-- long after the level started, so an instrumented build exits cleanly and writes its profile.
local quit_at = nil
local function update()
	if quit_at == false then return end
	if quit_at == nil then
		local f = io.open(getFS():update_path("$app_data_root$", "") .. "pgo_quit.txt", "r")
		if not f then quit_at = false return end
		local s = tonumber(f:read("*l")) or 150
		f:close()
		quit_at = time_global() + s * 1000
	end
	if time_global() >= quit_at then
		quit_at = false
		get_console():execute("quit")
	end
end
function on_game_start()
	RegisterScriptCallback("actor_on_update", update)
end
LUA
echo "$SECS" > "$FIX/appdata/pgo_quit.txt"
trap 'rm -f "$FIX/appdata/pgo_quit.txt"' EXIT
WRAP=""
command -v gamescope > /dev/null && WRAP=$ROOT/tools/headless.sh
LD_LIBRARY_PATH="$ROOT/$B/lib" LLVM_PROFILE_FILE="$RAW/%m_%p.profraw" XR_REAL_BIN="$ROOT/$B/gen/xray_vk" \
    XR_BIN="${WRAP:-$ROOT/$B/gen/xray_vk}" "$ROOT/tools/run_level.sh" 1000 $(( SECS + 400 ))
ls "$RAW"/*.profraw > /dev/null 2>&1 || { echo "no profile written (did the game quit cleanly?)" >&2; exit 1; }

echo "== merge"
podman run --rm -v "$ROOT:$ROOT:z" -w "$ROOT" "$IMAGE" llvm-profdata-19 merge -o $B/release.profdata "$RAW"/*.profraw
ls -la "$ROOT/$B/release.profdata"
