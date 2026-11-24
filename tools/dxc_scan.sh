#!/bin/bash
# Compiles every R4 shader file (r3/*.vs|ps|gs|hs|ds|cs) of an exported shader tree with DXC to SPIR-V
# and summarises the failures. Usage: tools/dxc_scan.sh <shader-root> [out-dir]
# The define set is a representative default of what r4.cpp passes (one variant per file).
set -u
cd "$(dirname "$0")/.."
ROOT=$(realpath "$1")
OUT=${2:-build/dxc_scan/$(basename "$ROOT")}
DXC=$PWD/build/deps/dxc-1.9.2609/bin/dxc
mkdir -p "$OUT"
find "$OUT" -maxdepth 1 \( -name '*.log' -o -name '*.ok' \) -delete
DEFS="-D SM_5=1 -D SM_4_1=1 -D SMAP_size=2048 -D SKIN_NONE=1 -D USE_HWSMAP=1 -D USE_HWSMAP_PCF=1 -D FP16_FILTER=1 \
-D FP16_BLEND=1 -D USE_BRANCHING=1 -D USE_VTF=1 -D SUN_QUALITY=2 -D VECTORIZED_CODE=1 -D USE_SJITTER=1 -D SSAO_QUALITY=2 \
-D SSFX_MODEXE=1 -D SSFX_RAIN_QUALITY=1 -D SSFX_SSR_QUALITY=1 -D SSFX_WATER_QUALITY=1 -D SSFX_WATER_PARALLAX=1 -D SSFX_IL_QUALITY=1 \
-D SSFX_AO_QUALITY=1 -D SSFX_POM_REFINE=1 -D SSFX_TERRA_POM_REFINE=1 -D SSFX_SSS_DIR_QUALITY=1 -D SSFX_SSS_OMNI_QUALITY=1"
export ROOT OUT DXC DEFS
compile() {
    f=$1
    ext=${f##*.}
    case $ext in vs|ps|gs|hs|ds|cs) ;; *) return ;; esac
    name=$(basename "$f")
    # shellcheck disable=SC2086
    if "$DXC" -spirv -fspv-target-env=vulkan1.3 -HV 2018 -Zpr -T ${ext}_5_0 -E main -I "$ROOT/r3" -I "$ROOT" $DEFS \
        -Fo "$OUT/$name.spv" "$f" > "$OUT/$name.log" 2>&1; then
        touch "$OUT/$name.ok"
        mv "$OUT/$name.log" "$OUT/$name.okwarn"
    fi
}
export -f compile
find "$ROOT/r3" -maxdepth 1 -type f -print0 | xargs -0 -n1 -P"$(nproc)" bash -c 'compile "$0"'
ok=$(find "$OUT" -maxdepth 1 -name '*.ok' | wc -l)
bad=$(find "$OUT" -maxdepth 1 -name '*.log' | wc -l)
echo "ok=$ok failed=$bad  (logs in $OUT)"
# first error per failed file, grouped
find "$OUT" -maxdepth 1 -name '*.log' -exec grep -h -m1 "error:" {} + 2>/dev/null | sed -E 's/^[^:]+:[0-9]+:[0-9]+: //' | sort | uniq -c | sort -rn | head -25
