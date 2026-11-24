#!/bin/sh
# Builds tests/reflect_dump.cpp against the R4 path (Common/linux/d3dcompiler_linux.cpp: vkd3d-shader ->
# DXBC, RDEF reflection) into $1 (default build/reflect_dump_r4). The Vulkan build is the CMake target
# reflect_dump_vk. With a shader root as $2 it also writes the reference dump to $3 (default
# build/refl_r4.txt): one process per file, 60 s and 4 GB each (vkd3d-shader hangs on a few shaders).
set -e
cd "$(dirname "$0")/.."
OUT=${1:-build/reflect_dump_r4}
M=monolith
clang++ -std=gnu++20 -O1 -fms-extensions -fno-operator-names -Wno-register -msse4.2 -w \
  -I$M/src/Common/linux/include -I$M/src -I$M/sdk/include -Ibuild/deps/dxvk-native-3.1.1/usr/include/dxvk \
  $(pkg-config --cflags libvkd3d-shader) tests/reflect_dump.cpp $M/src/Common/linux/d3dcompiler_linux.cpp \
  $M/src/Common/linux/io_compat.cpp $M/src/Common/linux/path_resolve.cpp $(pkg-config --libs libvkd3d-shader) -lpthread -o "$OUT"

[ -n "$2" ] || exit 0
ROOT=$2
DUMP=${3:-build/refl_r4.txt}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/refl_r4.XXXXXX")
export OUT ROOT TMP
ls "$ROOT/r3" | grep -E '\.(vs|ps|gs|hs|ds|cs)$' | tr '\n' '\0' | xargs -0 -P"$(nproc)" -I{} sh -c \
  'ulimit -v 4000000; timeout 60 "$OUT" "$ROOT" "{}" > "$TMP/{}.txt" 2>/dev/null || echo "FILE {} timeout" > "$TMP/{}.txt"'
# file blocks in name order, as reflect_dump prints them
ls "$ROOT/r3" | grep -E '\.(vs|ps|gs|hs|ds|cs)$' | LC_ALL=C sort | while IFS= read -r f; do cat "$TMP/$f.txt"; done > "$DUMP"
rm -rf "$TMP"
echo "reference dump: $DUMP ($(grep -c ' ok$' "$DUMP") ok, $(grep -c ' timeout$' "$DUMP") timeouts)"
