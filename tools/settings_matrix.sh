#!/bin/bash
# Graphics settings matrix: one game start per variant of build/fixture-gamma/appdata/user_shots.ltx,
# the autotest save loaded, one frame captured at present FRAME, then the game is stopped. Per variant:
# the frame (build/matrix/<name>.png), the renderer warnings ("! vk" lines), crash reports, frame stats.
#   tools/settings_matrix.sh [variant ...]      (default: all)
# Read-only for the GAMMA install (bwrap --ro-bind), like tools/run_level.sh.
set -uo pipefail
ROOT=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
FIX=$ROOT/build/fixture-gamma
APP=$FIX/appdata
OUT=$ROOT/build/matrix
FRAME=${FRAME:-1500}
mkdir -p "$OUT"

# name|overrides ("key value" pairs separated by ';')
VARIANTS=(
    "base|"
    "ssao_off|r2_ssao st_opt_off"
    "ssao_ultra|r2_ssao st_opt_ultra"
    "ssao_hdao|r2_ssao_mode hdao"
    "ssao_hbao|r2_ssao_mode hbao"
    "sun_low|r2_sun_quality st_opt_low"
    "sun_extreme|r2_sun_quality st_opt_extreme"
    "sun_off|r2_sun off"
    "msaa_2x|r3_msaa 2x"
    "msaa_4x|r3_msaa 4x"
    "smaa|r2_smaa st_opt_high"
    "tessellation|r4_enable_tessellation on"
    "volumetrics_off|r2_volumetric_lights off;r3_volumetric_smoke off"
    "shafts_screen|r2_sunshafts_mode screen_space"
    "shafts_combined|r2_sunshafts_mode combined;r2_sunshafts_quality st_opt_high"
    "mblur|r2_mblur_enabled on;r2_mblur 0.5"
    "dof_off|r2_dof_enable off"
    "parallax|r2_steep_parallax on;r2_detail_bump on"
    "wet_off|r3_dynamic_wet_surfaces off"
    "ssao_blur_half|r2_ssao_blur on;r2_ssao_half_data on"
)

want=("$@")
for v in "${VARIANTS[@]}"; do
    name=${v%%|*}
    over=${v#*|}
    if [ ${#want[@]} -gt 0 ] && [[ ! " ${want[*]} " =~ " $name " ]]; then continue; fi
    ltx=user_m_$name.ltx
    cp "$APP/user_shots.ltx" "$APP/$ltx"
    IFS=';' read -ra pairs <<< "$over"
    for p in "${pairs[@]}"; do
        [ -z "$p" ] && continue
        key=${p%% *}
        if grep -q "^$key " "$APP/$ltx"; then
            sed -i "s|^$key .*|$p|" "$APP/$ltx"
        else
            echo "$p" >> "$APP/$ltx"
        fi
    done
    log=$APP/logs/xray_$(id -un).log
    rm -f "$log" "$OUT/$name.png"
    before=$(ls "$APP"/logs/crash_*.log 2>/dev/null | wc -l)
    start=$(date +%s)
    XR_VK_SCREENSHOT="$OUT/$name.png" XR_VK_SCREENSHOT_EVERY=1 XR_VK_SCREENSHOT_FROM=$FRAME XR_VK_SCREENSHOT_COUNT=1 \
    XR_NO_MSGBOX=1 timeout 420 bwrap --die-with-parent --dev-bind / / --ro-bind "$HOME/Games/gamma" "$HOME/Games/gamma" \
        "$ROOT/build/monolith/xray_vk" -nointro -ltx "$ltx" \
        -start "server(autotest/single/alife/load) client(localhost)" < /dev/null > /dev/null 2>&1 &
    pid=$!
    while kill -0 $pid 2>/dev/null; do
        if [ -f "$OUT/$name.png" ]; then sleep 8; kill -TERM $pid 2>/dev/null; break; fi
        sleep 2
    done
    wait $pid 2>/dev/null
    rc=$?
    after=$(ls "$APP"/logs/crash_*.log 2>/dev/null | wc -l)
    {
        echo "== $name ($over): exit $rc, $(( $(date +%s) - start )) s, frame $( [ -f "$OUT/$name.png" ] && echo yes || echo NO ), new crash reports $((after - before))"
        grep -E "^\* vk stats" "$log" | tail -1 | cut -c1-150
        grep -E "^! vk|SHADER|shader.*(fail|error)" "$log" | sort | uniq -c | sort -rn | head -12
    } | tee -a "$OUT/report.txt"
    rm -f "$APP/$ltx"
done
