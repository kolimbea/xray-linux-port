#!/bin/bash
# AUR PKGBUILD and Flatpak manifest for a release made by make_packages.sh, written to build/dist/aur/ and
# build/dist/flatpak/. With --local the manifest points at the local tarball (for testing before the
# release is published), otherwise at the GitHub release URL.
#   tools/packaging/store_files.sh <tag> [--local]
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)
TAG=${1:?usage: tools/packaging/store_files.sh <tag> [--local]}
ASSET=xray-native-$TAG-linux-x86_64.tar.gz
TGZ=$ROOT/build/dist/$ASSET
[ -f "$TGZ" ] || { echo "missing $TGZ (run make_packages.sh)" >&2; exit 1; }
SHA=$(sha256sum "$TGZ" | cut -d' ' -f1)
VER=${TAG#v}; VER=${VER//-/}          # pacman versions have no '-': v0.1.0-alpha -> 0.1.0alpha
URL=https://github.com/kolimbea/xray-linux-port/releases/download/$TAG/$ASSET
if [ "${2:-}" = --local ]; then SOURCE="path: $TGZ"; else SOURCE="url: $URL"; fi

mkdir -p "$ROOT/build/dist/aur" "$ROOT/build/dist/flatpak"
sed -e "s|@TAG@|$TAG|g" -e "s|@VER@|$VER|g" -e "s|@SHA256@|$SHA|g" \
    "$ROOT/tools/packaging/aur/PKGBUILD.in" > "$ROOT/build/dist/aur/PKGBUILD"
sed -e "s|@SOURCE@|$SOURCE|" -e "s|@SHA256@|$SHA|" \
    "$ROOT/tools/packaging/flatpak/io.github.kolimbea.XrayNative.yml.in" \
    > "$ROOT/build/dist/flatpak/io.github.kolimbea.XrayNative.yml"
echo "build/dist/aur/PKGBUILD, build/dist/flatpak/io.github.kolimbea.XrayNative.yml ($SHA)"
