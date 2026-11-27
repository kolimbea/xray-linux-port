#!/bin/bash
# Release packages from the portable build (tools/packaging/build_portable.sh), made inside the same
# Debian 13 container:
#   build/dist/xray-native-<tag>-linux-x86_64.tar.gz   self-contained package (the launcher's update unit)
#   build/dist/xray-native-<tag>-x86_64.AppImage         one file, runs the launcher
#   build/dist/xray-native_<ver>_amd64.deb               installs to /opt/xray-native
# plus .sha256 files. Self-contained: libraries the distributions name or version differently (SDL3,
# oneTBB, ICU, OpenSSL, OpenAL, Ogg/Vorbis/Theora, libjpeg, giflib) are bundled in lib/ and found through
# RUNPATH $ORIGIN/../lib; glibc, libstdc++, the Vulkan loader, GPU drivers and the X11/Wayland/audio
# client libraries stay the system's (they must match the system's drivers and servers).
#   tools/packaging/make_packages.sh <tag>        e.g. v0.1.0-alpha
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
TAG=${1:?usage: tools/packaging/make_packages.sh <tag>}
IMAGE=xray-native-build:debian13
B=$ROOT/build/portable
[ -x "$B/monolith/xray_vk" ] && [ -x "$B/launcher/xray-launcher" ] || { echo "run tools/packaging/build_portable.sh first" >&2; exit 1; }

# appimagetool 1.9.0 (MIT), pinned; sha256 recorded when first fetched (2026-11-27)
AIT=$ROOT/build/deps/appimagetool-1.9.0/appimagetool-x86_64.AppImage
if [ ! -f "$AIT" ]; then
    mkdir -p "$(dirname "$AIT")"
    curl -sSfL -o "$AIT" https://github.com/AppImage/appimagetool/releases/download/1.9.0/appimagetool-x86_64.AppImage
    chmod +x "$AIT"
fi
# type2 runtime for appimagetool (it would otherwise download "continuous" at build time)
RT=$ROOT/build/deps/appimagetool-1.9.0/runtime-x86_64
[ -f "$RT" ] || curl -sSfL -o "$RT" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64
sha256sum "$AIT" "$RT"

podman run --rm -v "$ROOT:$ROOT:z" -w "$ROOT" -e TAG="$TAG" "$IMAGE" bash -euo pipefail -c '
B=build/portable
STAGE=build/package/$TAG
rm -rf "$STAGE"
P=$STAGE/xray-native
mkdir -p $P/bin $P/lib $P/share/licenses $P/share/applications $P/share/icons/hicolor/scalable/apps
cp $B/monolith/xray_vk $B/launcher/xray-launcher $P/bin/
cp $B/monolith/src/xrCore/libxrCore.so $P/lib/
cp build/deps/dxc-1.9.2609/lib/libdxcompiler.so build/deps/dxc-1.9.2609/lib/libdxil.so $P/lib/
strip --strip-debug $P/bin/* $P/lib/libxrCore.so

# bundle the libraries the binaries need, following direct dependencies (DT_NEEDED) and stopping at
# the system set: glibc, libstdc++/libgcc, the Vulkan loader, display, audio and IPC client libraries
# (they must match the running system), zlib.
SYSTEM="^(linux-vdso|ld-linux-x86-64|libc|libm|libdl|libpthread|librt|libresolv|libutil|libstdc\+\+|libgcc_s|libz|libvulkan|libGL|libEGL|libGLX|libGLdispatch|libOpenGL|libdrm|libgbm|libasound|libdbus-1|libudev|libsystemd|libdecor-0)\.so|^(libX|libxcb|libwayland|libxkbcommon|libpulse|libpipewire|libjack)"
queue="$(ls $P/bin/* $P/lib/*.so)"
while [ -n "$queue" ]; do
    next=""
    for f in $queue; do
        for name in $(objdump -p "$f" | awk "/NEEDED/{print \$2}"); do
            echo "$name" | grep -qE "$SYSTEM" && continue
            [ -e "$P/lib/$name" ] && continue
            path=$(ldd "$f" | awk -v n="$name" "\$1==n{print \$3}")
            [ -n "$path" ] || { echo "cannot resolve $name for $f"; exit 1; }
            cp -L "$path" "$P/lib/$name"
            next="$next $P/lib/$name"
        done
    done
    queue=$next
done
for f in $P/bin/*; do patchelf --set-rpath "\$ORIGIN/../lib" "$f"; done
for f in $P/lib/*.so*; do patchelf --set-rpath "\$ORIGIN" "$f"; done
echo "bundled:"; ls $P/lib | tr "\n" " "; echo
# nothing may resolve outside lib/ except the system set
missing=$(for f in $P/bin/* $P/lib/*.so*; do ldd "$f"; done | grep "not found" || true)
[ -z "$missing" ] || { echo "unresolved: $missing"; exit 1; }

L=$P/share/licenses
cp monolith/License.txt $L/X-Ray-GSC-License.txt
cp monolith/src/Layers/xrRenderVK/third_party/FidelityFX-FSR1-LICENSE.txt $L/
cp build/deps/imgui-sdl3-1.91.8/LICENSE.txt $L/DearImGui-LICENSE.txt
cp build/deps/dxc-1.9.2609/LICENSE-LLVM.txt build/deps/dxc-1.9.2609/LICENSE-MS.txt $L/
for d in /usr/share/doc/libsdl3-0 /usr/share/doc/libtbb12 /usr/share/doc/libopenal1 /usr/share/doc/libvorbis0a \
         /usr/share/doc/libogg0 /usr/share/doc/libtheora1 /usr/share/doc/libjpeg62-turbo /usr/share/doc/libicu76 \
         /usr/share/doc/libssl3t64 /usr/share/doc/libgif7; do
    [ -f $d/copyright ] && cp $d/copyright $L/$(basename $d).copyright
done
echo "$TAG" > $P/VERSION
cp tools/packaging/xray-native.svg $P/share/icons/hicolor/scalable/apps/xray-native.svg
cat > $P/share/applications/xray-native.desktop <<EOF
[Desktop Entry]
Type=Application
Name=S.T.A.L.K.E.R. GAMMA (native)
Comment=Native Linux build of the X-Ray Monolith engine with a Vulkan renderer
Exec=xray-launcher
Icon=xray-native
Terminal=false
Categories=Game;
EOF
cat > $P/README.txt <<EOF
S.T.A.L.K.E.R. GAMMA — native Linux build of the X-Ray Monolith engine with a Vulkan renderer ($TAG).
Alpha. Needs your own installed Anomaly + GAMMA; it is never modified.
Start bin/xray-launcher, choose the GAMMA folder in Settings, press Play. Later updates install from
the launcher. Requirements: glibc 2.41+ (Debian 13, Ubuntu 25.04, Fedora 42, Arch or newer), a Vulkan
1.3 driver with VK_EXT_shader_object (NVIDIA, Mesa RADV/ANV/NVK). Other libraries are included.
Source: https://github.com/kolimbea/xray-linux-port
EOF

mkdir -p build/dist
ASSET=xray-native-$TAG-linux-x86_64.tar.gz
tar -C $STAGE -czf build/dist/$ASSET xray-native

# .deb: /opt/xray-native, launcher on PATH, menu entry
VER=${TAG#v}; VER=${VER//-/\~}
D=$STAGE/deb
mkdir -p $D/DEBIAN $D/opt $D/usr/bin $D/usr/share/applications $D/usr/share/icons/hicolor/scalable/apps
cp -a $P $D/opt/xray-native
ln -s /opt/xray-native/bin/xray-launcher $D/usr/bin/xray-launcher
cp $P/share/applications/xray-native.desktop $D/usr/share/applications/
cp $P/share/icons/hicolor/scalable/apps/xray-native.svg $D/usr/share/icons/hicolor/scalable/apps/
cat > $D/DEBIAN/control <<EOF
Package: xray-native
Version: $VER
Architecture: amd64
Maintainer: kolimbea <244228398+kolimbea@users.noreply.github.com>
Depends: libc6 (>= 2.41), libstdc++6 (>= 14), libvulkan1, libx11-6, libxext6, libxcursor1, libxfixes3, libxi6,
 libxrandr2, libxrender1, libxss1, libxcb-render0, libxkbcommon0, libwayland-client0, libwayland-cursor0,
 libwayland-egl1, libdecor-0-0, libgbm1, libpipewire-0.3-0t64 | libpipewire-0.3-0, libpulse0,
 libasound2t64 | libasound2
Recommends: mesa-vulkan-drivers, curl, fontconfig
Section: games
Priority: optional
Homepage: https://github.com/kolimbea/xray-linux-port
Description: Native Linux build of the X-Ray Monolith engine (S.T.A.L.K.E.R. Anomaly/GAMMA)
 Vulkan renderer, launcher with updates and a Mod Organizer 2 overlay builder. Needs an installed
 Anomaly + GAMMA, which it never modifies.
EOF
fakeroot dpkg-deb --root-owner-group -Zxz --build $D build/dist/xray-native_${VER}_amd64.deb > /dev/null

# AppImage: AppDir = the package + AppRun + desktop entry + icon
A=$STAGE/AppDir
cp -a $P $A
cat > $A/AppRun <<"EOF"
#!/bin/sh
# the launcher adopts this package (copies it to ~/.local/share/xray-native/versions/) and runs the game
HERE=$(dirname "$(readlink -f "$0")")
exec "$HERE/bin/xray-launcher" "$@"
EOF
chmod +x $A/AppRun
cp $P/share/applications/xray-native.desktop $A/
cp tools/packaging/xray-native.svg $A/xray-native.svg
ARCH=x86_64 build/deps/appimagetool-1.9.0/appimagetool-x86_64.AppImage --appimage-extract-and-run \
    --runtime-file build/deps/appimagetool-1.9.0/runtime-x86_64 -n $A build/dist/xray-native-$TAG-x86_64.AppImage 2>&1 | tail -15 \
    || { echo "appimagetool failed"; exit 1; }

cd build/dist
for f in $ASSET xray-native-$TAG-x86_64.AppImage xray-native_${VER}_amd64.deb; do sha256sum $f > $f.sha256; done
ls -la $ASSET xray-native-$TAG-x86_64.AppImage xray-native_${VER}_amd64.deb
'
