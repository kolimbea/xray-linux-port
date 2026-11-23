#!/bin/bash
# Fetch pinned third-party binaries into build/deps (user-local, no root).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
d="$root/build/deps/dxvk-native-3.1.1"
if [ ! -f "$d/usr/include/dxvk/windows_base.h" ]; then
  mkdir -p "$d"
  curl -sSfL -o "$d/dxvk-native.tar.gz" \
    https://github.com/doitsujin/dxvk/releases/download/v3.1.1/dxvk-native-3.1.1-steamrt-sniper.tar.gz
  echo "506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7  $d/dxvk-native.tar.gz" | sha256sum -c -
  tar xzf "$d/dxvk-native.tar.gz" -C "$d"
fi
echo "dxvk-native 3.1.1: $d/usr"
