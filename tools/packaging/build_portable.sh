#!/bin/bash
# Portable build of the game and the launcher inside the Debian 13 container (Containerfile.debian13):
# the result needs glibc >= 2.41 instead of the build host's. Output: build/portable/monolith/xray_vk,
# build/portable/launcher/xray-launcher. Rootless podman; the project is mounted at the same path so
# CMake paths (build/deps, monolith/) stay valid. lld with --no-dependent-libraries: the sources carry
# MSVC "#pragma comment(lib, ...)" lines (winmm.lib, ...) that lld would otherwise try to link.
#   tools/packaging/build_portable.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
IMAGE=xray-native-build:debian13
podman build -q -t "$IMAGE" -f "$ROOT/tools/packaging/Containerfile.debian13" "$ROOT/tools/packaging" > /dev/null
"$ROOT/tools/fetch_deps.sh" > /dev/null
# build/pgo/release.profdata (tools/pgo.sh): profile-guided optimisation, ~9 % less frame time
PROF=$ROOT/build/pgo/release.profdata
[ -f "$PROF" ] && echo "using the profile $PROF ($(date -r "$PROF" +%F))" || PROF=""
podman run --rm -v "$ROOT:$ROOT:z" -w "$ROOT" -e PROF="$PROF" "$IMAGE" bash -euo pipefail -c '
    B=build/portable
    F="" L=""
    [ -n "$PROF" ] && F="-fprofile-use=$PROF -Wno-profile-instr-out-of-date -Wno-profile-instr-unprofiled -Wno-backend-plugin" \
        && L="-fprofile-use=$PROF"
    cmake -S monolith -B $B/monolith -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DXR_THINLTO=ON "-DCMAKE_C_FLAGS=$F" "-DCMAKE_CXX_FLAGS=$F" \
        "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -Wl,--no-dependent-libraries $L" \
        "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -Wl,--no-dependent-libraries $L" > /dev/null
    ninja -C $B/monolith xray_vk
    cmake -S tools/launcher -B $B/launcher -G Ninja -DCMAKE_CXX_COMPILER=clang++ > /dev/null
    ninja -C $B/launcher
    ldd --version | sed -n 1p
'
ls -la "$ROOT/build/portable/monolith/xray_vk" "$ROOT/build/portable/launcher/xray-launcher"
