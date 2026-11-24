#!/bin/sh
# Builds tools/extract_tree against build/monolith's libxrCore.so into $1 (default build/extract_tree).
set -e
cd "$(dirname "$0")/.."
M=$PWD/monolith
clang++ -std=gnu++20 -O1 -w -DNDEBUG -DPURE_ALLOC -fms-extensions -fno-operator-names -fno-strict-aliasing -msse4.2 \
  -I$M/src/xrCore -I$M/src/Common/linux/include -I$M/src -I$M/sdk/include -I"$M/src/3rd party" \
  -Ibuild/deps/dxvk-native-3.1.1/usr/include/dxvk tools/extract_tree.cpp \
  -Lbuild/monolith/src/xrCore -lxrCore -Wl,-rpath,$PWD/build/monolith/src/xrCore -o "${1:-build/extract_tree}"
