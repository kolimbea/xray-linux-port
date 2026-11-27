# xray-linux-port — status

Goal: native Linux + Vulkan build of X-Ray Monolith (Anomaly/GAMMA engine).
Plan: [docs/PLAN.md](docs/PLAN.md) · Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) ·
Log: [docs/PROGRESS.md](docs/PROGRESS.md) · Technical audit and roadmap (2026-11-27):
[docs/technical-audit/EXECUTIVE_SUMMARY.md](docs/technical-audit/EXECUTIVE_SUMMARY.md).

- Stage 0 done. Stage 1a done: `libxrCore.so` builds with clang and mounts the Anomaly archives.
  Stage 1b built. Stage 1c: everything compiles and links (`build/monolith/xray`, tag `stage1c-linked`);
  first boot reaches shader compilation (D3D11 device on dxvk is up).
  Decided 2026-11-24: go to the own renderer now (vkd3d cannot compile all shaders). Current work:
  stage 2 (ADR 0001): M1–M4 done: the main menu renders on the own Vulkan renderer (0 validation errors,
  tag stage2-m4). Next: GAMMA data fixture (MO2 overlay), then levels.
  Engine work: `monolith/` branch `stage-2-vk`.
- Current work (2026-11-27): roadmap of docs/technical-audit/ROADMAP.md — phase 1 and most of phase 2
  done (status table at its end). Next: packaging, Tracy, AMD/Intel tests, SSFX AO, full-screen
  measurements with the display on (OPEN_QUESTIONS Q18), CI activation (branch ci-workflow).
- Black frames in the screenshot tour (2026-11-27) were NOT a renderer bug: run2 had a stale view of
  the recreated back buffer (fixed, monolith "present pass drops its view"); run3 ended with the player
  dying on Zaton (m_death sound, death screen) despite g_god. tools/settings_matrix.sh runs every
  graphics option once (frame + "! vk" lines + crash reports in build/matrix/).
- Memory note: "staging textures" reach 7-13 GB of process memory with GAMMA (the engine keeps every
  loaded-but-never-drawn texture as STAGING until its first Apply, dx10SH_Texture.cpp ProcessStaging);
  same engine design as on Windows, but worth an option to drop or compress them.
- Packaging (2026-11-27): tools/packaging/ builds in a Debian 13 container (glibc 2.41) and makes
  tar.gz, AppImage, .deb, Flatpak bundle and the AUR PKGBUILD (tools/package.sh <tag> [--publish]).
  Verified: game renders from the package and from the Flatpak; .deb installs on Ubuntu 25.04, AUR
  package on Arch. Player guide: docs/INSTALL.md / docs/INSTALL.ru.md. No release published yet.
- Fixed 2026-11-27: Body Health System flicker (std::sort -> stable_sort of HUD statics), crashes at
  exit (path cache, freeze watchdog). Sky "moving fast" on Truck Cemetery not reproduced in overcast
  weather (frames build/shots/skyc.*): need the user's weather/time or a video.
- Graphics settings matrix (2026-11-27, tools/settings_matrix.sh, 20 variants, GAMMA autotest save):
  all start and render after monolith acb7358. Fixed: HBAO and HDAO shader compile errors, null
  dereference with r2_ssao_blur (upstream), tessellation now disabled on Vulkan with a log line (hull /
  domain stages not implemented — open work). Still open: with MSAA the SSFX passes read multisampled
  targets declared as Texture2D (bound as null: TAA, motion vectors, SSS off under MSAA); "view
  dimension 8" render target views (Texture2DMS array) unsupported. FPS on GTX 1650 Ti, 1080p high,
  interior: 24-31.
- User-reported bugs to fix next (2026-11-27): the sky "lags" (stutters/flickers), the player health
  widget in the bottom-left corner (Body Health System, zzz_player_injuries.script): its grey health
  fill (vertical CUIProgressBar "bhs_health", scissor-clipped) is present on some frames and missing on
  others (build/shots/corners.png); 40 consecutive frames show no flicker. Need the user's description; then test every graphics setting (presets, SSAO modes,
  sun quality, MSAA, tessellation, SSFX options) and fix shader errors found.
- Screenshot tour (fixture only): gamedata/scripts/zz_shot_tour.script, active while
  appdata/shot_tour.txt exists; config appdata/user_shots.ltx; run with -dbg (g_god) and
  XR_VK_SCREENSHOT_EVERY; frames in build/shots/.
- `monolith/` and `openxray/` are upstream clones, excluded from this docs repo.
- Published 2026-11-25 (public): https://github.com/kolimbea/xray-linux-port (this repo, remote
  `origin`) and https://github.com/kolimbea/xray-monolith-linux branch `stage-2-vk` (remote `kolimbea` in
  monolith/). Author identity: kolimbea noreply; no Claude trailers in commits. Push over HTTPS with
  `git -c credential.helper= -c 'credential.helper=!gh auth git-credential' push ...` (no SSH key).
- GAMMA install `~/Games/gamma` is read-only for this project.
