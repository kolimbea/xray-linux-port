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

# SPIRV-Reflect, tag vulkan-sdk-1.4.363.0 (commit f3e261fda74ca81b44d1941931c6b7b5fe236d0e), Apache-2.0:
# reflection of the SPIR-V the Vulkan renderer compiles (replaces D3DReflect). Source only; built in.
# sha256 of the GitHub tag tarball, recorded when first fetched (2026-11-24).
d="$root/build/deps/spirv-reflect-1.4.363.0"
if [ ! -f "$d/spirv_reflect.c" ]; then
  mkdir -p "$d"
  curl -sSfL -o "$d/src.tar.gz" \
    https://github.com/KhronosGroup/SPIRV-Reflect/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz
  echo "e0b2914648ab2eac1c83851911199e73f868d966e183531e4c227a258ea07bc6  $d/src.tar.gz" | sha256sum -c -
  tar xzf "$d/src.tar.gz" -C "$d" --strip-components=1
fi
echo "spirv-reflect 1.4.363.0: $d"

# AMD FidelityFX Super Resolution 1.0 (commit a21ffb8f6c13233ba336352bdff293894c706575, MIT): EASU/RCAS
# upscaling of the presented frame (xrRenderVK/vkPresent.cpp). Two headers, compiled in. sha256 of the
# GitHub commit tarball, recorded when first fetched (2026-11-27).
d="$root/build/deps/fsr1-a21ffb8"
if [ ! -f "$d/ffx-fsr/ffx_fsr1.h" ]; then
  mkdir -p "$d"
  curl -sSfL -o "$d/src.tar.gz" \
    https://github.com/GPUOpen-Effects/FidelityFX-FSR/archive/a21ffb8f6c13233ba336352bdff293894c706575.tar.gz
  echo "b529a31d21971ee9f2a3231bbe17e31d183ccbb7fb6e3f7b0508b2624f61c2d9  $d/src.tar.gz" | sha256sum -c -
  tar xzf "$d/src.tar.gz" -C "$d" --strip-components=1
fi
echo "fsr1 a21ffb8: $d"
