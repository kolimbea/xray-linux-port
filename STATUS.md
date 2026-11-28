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
- Performance pass (2026-11-27, monolith f6f80123): profiling with perf (attach from outside bwrap;
  call chains need `-e cycles:u --user-callchains`) showed the frame is main-thread bound (GPU ~11 ms of
  ~35): backend + NVIDIA driver ~19 % of the main thread, the rest engine (Lua AI ~30 %, sun cascades
  ~17 %). Host heap moved from the PCIe BAR to system memory: 28.5-32.5 -> 27.1-27.8 ms/frame (laptop
  thermals make single runs noisy: interleave A/B runs, wait for Tctl < 60 C). Incremental push
  descriptors and a per-context program cache: ~1 %. Next candidates: a command-stream worker thread
  (DXVK-style) to take the ~19 % off the main thread, PGO, MSAA+SSFX, tessellation stages.
- Input (monolith 4dba8b49): SDL text input only while imgui edits text (tester report: A/S/D do not
  move; suspected input-method capture). Not reproduced here (no IME on this machine): needs the
  tester's confirmation. Open: one SIGSEGV in a Lua call of CUIWindow::(bool) from a CPHScriptCondition
  (crash_1795808520.log), not reproduced in two reruns.
- Launcher (root 3d78469): language switch in Settings (auto/English/Russian); auto = Russian only for
  ru/uk/be/kk locales. 2026-11-28: command line for everything the window does (--gamma, --releases,
  --install [tag], --versions, --use, --overlay, --play; tester request "why isn't it just cli").
  Tested in a temporary XDG_DATA_HOME: install of three releases, overlay, --play to the main menu
  in headless gamescope.
- Release vs dev build speed (2026-11-28): no gap. Packaged v0.1.2 binary (Debian clang 19, -O3, lld)
  29.53 / 29.86 ms, dev build (Fedora clang 22, -O2 -g) 29.68 / 27.45 ms, interleaved headless runs.
  The earlier 43.9 ms was a single hot run.
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
- Fixed 2026-11-28 (monolith 7722cec5): the sky "lags" — the cloud layer jumped back and forth with a
  5-frame period. Root cause: xrCore iFloor/iCeil bit tricks shift by negative counts and by >= 32 bits
  (undefined behaviour); clang folds them into garbage for compile-time constants, so the clouds' wind
  direction (color_rgba of iFloor'd constants, COLOR0 of v_clouds) was a random value per frame. Now
  floorf/ceilf under GCC/Clang. Verified headless with a still camera (fixture zz_sky_test.script,
  active while appdata/sky_test.txt names a weather): consecutive-frame sky difference 0.08-0.15
  (before: 0.1 / 1.0-1.7 alternating). Any other engine code calling iFloor/iCeil on constants was
  affected too. Interleaved headless benchmark (run_level 4, 2 rounds): fixed 27.04 / 27.13 ms,
  before 27.55 / 28.40 ms per frame (~2 %, close to noise).
- Headless test wrapper (scratchpad headless.sh pattern): run gamescope under `setsid` plus a watcher
  that kills the whole session when the wrapper dies; before, gamescopereaper and the game survived
  run_level's stop and kept the single-instance lock.
- User-reported bugs (2026-11-27): the player health
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
