#!/bin/bash
# Builds the release package of the native port (docs/LAUNCHER.md) and, with --publish, a GitHub
# pre-release of kolimbea/xray-linux-port with the package and its sha256.
#   tools/package.sh <tag> [--publish] [--notes <file>]
# Uses the existing builds: build/monolith (configure it with -DXR_THINLTO=ON) and build/launcher.
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
TAG=${1:?usage: tools/package.sh <tag> [--publish] [--notes <file>]}
shift
PUBLISH=0
NOTES=""
while [ $# -gt 0 ]; do
    case "$1" in
        --publish) PUBLISH=1 ;;
        --notes) NOTES=$2; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
B=$ROOT/build/monolith
grep -q "XR_THINLTO:BOOL=ON" "$B/CMakeCache.txt" || echo "note: build/monolith is not configured with XR_THINLTO=ON" >&2
ninja -C "$B" xray_vk
cmake -S "$ROOT/tools/launcher" -B "$ROOT/build/launcher" -G Ninja -DCMAKE_CXX_COMPILER=clang++ > /dev/null
ninja -C "$ROOT/build/launcher"

STAGE=$ROOT/build/package/$TAG
PKG=$STAGE/xray-native
mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/share/licenses"
cp "$B/xray_vk" "$ROOT/build/launcher/xray-launcher" "$PKG/bin/"
cp "$B/src/xrCore/libxrCore.so" "$PKG/lib/"
cp "$ROOT/build/deps/dxc-1.9.2609/lib/libdxcompiler.so" "$ROOT/build/deps/dxc-1.9.2609/lib/libdxil.so" "$PKG/lib/"
# debug info off (hundreds of MB), symbol tables kept: crash reports name the functions
strip --strip-debug "$PKG/bin/xray_vk" "$PKG/bin/xray-launcher" "$PKG/lib/libxrCore.so"
L=$PKG/share/licenses
cp "$ROOT/monolith/License.txt" "$L/X-Ray-GSC-License.txt"
cp "$ROOT/monolith/src/Layers/xrRenderVK/third_party/FidelityFX-FSR1-LICENSE.txt" "$L/"
cp "$ROOT/build/deps/imgui-sdl3-1.91.8/LICENSE.txt" "$L/DearImGui-LICENSE.txt"
cp "$ROOT/build/deps/dxc-1.9.2609/LICENSE-LLVM.txt" "$ROOT/build/deps/dxc-1.9.2609/LICENSE-MS.txt" "$L/"
echo "$TAG" > "$PKG/VERSION"
cat > "$PKG/README.txt" <<EOF
S.T.A.L.K.E.R. GAMMA — native Linux build of the X-Ray Monolith engine with a Vulkan renderer ($TAG).
Alpha. Needs your own installed Anomaly + GAMMA; it is never modified.
Start bin/xray-launcher, choose the GAMMA folder in Settings, press Play. Later updates install from
the launcher. Requirements: Vulkan 1.3 GPU driver with VK_EXT_shader_object (NVIDIA, Mesa RADV/ANV/NVK),
SDL3, OpenAL, oneTBB, ICU, OpenSSL, libvorbis, libtheora, libjpeg, giflib from your distribution.
Source: https://github.com/kolimbea/xray-linux-port
EOF

mkdir -p "$ROOT/build/dist"
ASSET=xray-native-$TAG-linux-x86_64.tar.gz
tar -C "$STAGE" -czf "$ROOT/build/dist/$ASSET" xray-native
(cd "$ROOT/build/dist" && sha256sum "$ASSET" > "$ASSET.sha256")
ls -la "$ROOT/build/dist/$ASSET" "$ROOT/build/dist/$ASSET.sha256"

if [ "$PUBLISH" = 1 ]; then
    ARGS=(release create "$TAG" "$ROOT/build/dist/$ASSET" "$ROOT/build/dist/$ASSET.sha256" -R kolimbea/xray-linux-port
          --prerelease --title "$TAG")
    if [ -n "$NOTES" ]; then ARGS+=(--notes-file "$NOTES"); else ARGS+=(--notes "Alpha build $TAG."); fi
    gh "${ARGS[@]}"
fi
