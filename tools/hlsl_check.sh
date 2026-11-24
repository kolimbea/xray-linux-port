#!/bin/sh
# Builds tools/hlsl_check into $1 (default build/hlsl_check).
set -e
cd "$(dirname "$0")/.."
OUT=${1:-build/hlsl_check}
M=monolith
clang++ -std=gnu++20 -O1 -fms-extensions -fno-operator-names -Wno-register -msse4.2 -w \
  -I$M/src/Common/linux/include -I$M/src -I$M/sdk/include -Ibuild/deps/dxvk-native-3.1.1/usr/include/dxvk \
  $(pkg-config --cflags libvkd3d-shader) tools/hlsl_check.cpp $M/src/Common/linux/d3dcompiler_linux.cpp \
  $M/src/Common/linux/io_compat.cpp $M/src/Common/linux/path_resolve.cpp $(pkg-config --libs libvkd3d-shader) -o "$OUT"
