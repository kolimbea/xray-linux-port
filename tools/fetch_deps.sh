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

# DXC (DirectX Shader Compiler) v1.9.2609, official Linux build: HLSL -> SPIR-V for xrRender_VK.
# sha256 as published by GitHub for the release asset.
d="$root/build/deps/dxc-1.9.2609"
if [ ! -x "$d/bin/dxc" ]; then
  mkdir -p "$d"
  curl -sSfL -o "$d/dxc.tar.gz" \
    https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz
  echo "96faadc7f5c282d2ffda49804beb4c3ee38127bc252b723234e3c5cdf7aa39a1  $d/dxc.tar.gz" | sha256sum -c -
  tar xzf "$d/dxc.tar.gz" -C "$d"
fi
echo "dxc 1.9.2609: $d/bin/dxc"
