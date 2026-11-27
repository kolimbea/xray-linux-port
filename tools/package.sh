#!/bin/bash
# Release of the native port (docs/LAUNCHER.md, docs/INSTALL.md): portable build in the Debian 13
# container, the packages (tar.gz, AppImage, .deb, Flatpak bundle), the AUR PKGBUILD, and with --publish a
# GitHub pre-release of kolimbea/xray-linux-port carrying them.
#   tools/package.sh <tag> [--publish] [--notes <file>] [--no-build]
set -euo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
TAG=${1:?usage: tools/package.sh <tag> [--publish] [--notes <file>] [--no-build]}
shift
PUBLISH=0
BUILD=1
NOTES=""
while [ $# -gt 0 ]; do
    case "$1" in
        --publish) PUBLISH=1 ;;
        --no-build) BUILD=0 ;;
        --notes) NOTES=$2; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
P=$ROOT/tools/packaging
[ $BUILD = 1 ] && "$P/build_portable.sh"
"$P/make_packages.sh" "$TAG"

# Flatpak bundle (needs org.flatpak.Builder and org.freedesktop.Sdk//25.08 in the user installation)
D=$ROOT/build/dist
"$P/store_files.sh" "$TAG" --local > /dev/null
W=$(mktemp -d "$ROOT/build/flatpak.XXXX")
flatpak run --user org.flatpak.Builder --user --force-clean --disable-rofiles-fuse --repo="$W/repo" "$W/app" \
    "$D/flatpak/io.github.kolimbea.XrayNative.yml" > "$W/build.log" 2>&1 || { tail -20 "$W/build.log"; exit 1; }
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo "$W/repo" \
    "$D/xray-native-$TAG.flatpak" io.github.kolimbea.XrayNative
(cd "$D" && sha256sum "xray-native-$TAG.flatpak" > "xray-native-$TAG.flatpak.sha256")
# the published manifest and PKGBUILD point at the release URL
"$P/store_files.sh" "$TAG"

VER=${TAG#v}; VER=${VER//-/\~}
ASSETS=("$D/xray-native-$TAG-linux-x86_64.tar.gz" "$D/xray-native-$TAG-x86_64.AppImage"
        "$D/xray-native_${VER}_amd64.deb" "$D/xray-native-$TAG.flatpak")
for a in "${ASSETS[@]}"; do ls -la "$a" "$a.sha256"; done

if [ "$PUBLISH" = 1 ]; then
    FILES=()
    for a in "${ASSETS[@]}"; do FILES+=("$a" "$a.sha256"); done
    ARGS=(release create "$TAG" "${FILES[@]}" -R kolimbea/xray-linux-port --prerelease --title "$TAG")
    if [ -n "$NOTES" ]; then ARGS+=(--notes-file "$NOTES"); else ARGS+=(--notes "Alpha build $TAG. Install: docs/INSTALL.md"); fi
    gh "${ARGS[@]}"
    echo "AUR: push build/dist/aur/PKGBUILD (+ makepkg --printsrcinfo > .SRCINFO) to aur.archlinux.org/xray-native-bin.git"
fi
