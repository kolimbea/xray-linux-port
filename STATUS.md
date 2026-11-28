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
  move; suspected input-method capture). Not reproduced here (no IME on this machine); the tester
  confirmed the fix (2026-11-28).
- Fixed 2026-11-28 (monolith 89e81736): SIGSEGV on resolution change (tester report; also the earlier
  CUIWindow::(bool) crash from a CPHScriptCondition). Catspaw's Timed Tutorial Prompts attaches windows
  to ActorMenu.get_maingame() and keeps calling them from level.add_call timers; the HUD reload freed
  that window (use-after-free, latent on Windows). The old main HUD window is now retired, freed with
  the game UI. Reproduced with fixture zz_vidmode_test.script (active while appdata/vidmode_test.txt
  exists): crashed on the 2nd vid_mode in 3/3 runs, now 3 changes and a clean quit. Lua stack found by
  running the game under gdb and calling luaL_traceback at the fault. Loading screen: the progress bar
  only moves between load stages (8-18 s each with GAMMA), as in the original engine; left as is.
- Launcher (root 3d78469): language switch in Settings (auto/English/Russian); auto = Russian only for
  ru/uk/be/kk locales. 2026-11-28: command line for everything the window does (--gamma, --releases,
  --install [tag], --versions, --use, --overlay, --play; tester request "why isn't it just cli").
  Tested in a temporary XDG_DATA_HOME: install of three releases, overlay, --play to the main menu
  in headless gamescope.
- AMD (2026-11-28): the renderer runs on the laptop's Radeon Vega 8 (RADV, Mesa 26.2.6, XR_VK_GPU=0)
  with no "! vk" lines; frames match the GTX 1650 Ti side by side (build/shots/amd.*, nv.*). 9 fps at
  1080p high (GPU 111 ms, integrated GPU). Intel still untested.
- PGO (2026-11-28): tools/pgo.sh builds an instrumented release in the Debian container, trains it on
  the bench level (fixture script quits the game cleanly so the profile is written), merges
  build/pgo/release.profdata; tools/packaging/build_portable.sh uses it when present (not in git:
  run tools/pgo.sh before tools/package.sh). Interleaved headless runs: release with PGO 25.02 /
  25.19 / 25.27 ms vs v0.1.2 release 27.40 / 27.17 / 27.33 ms (~8 %). Training is one level (Bar);
  other levels are expected to gain less. Needed fix: CAI_Stalker::agent_manager out of line (ODR).
  tools/headless.sh = the headless gamescope wrapper for XR_BIN (no window on the desktop).
- Main-thread CPU (2026-11-28, perf on the bench level; CPU profiling now via the Fedora perf binary
  from the xray-perf image run on the host, `perf record -e cycles:u -g --user-callchains` with the
  game started under it inside tools/headless.sh; build/monolith-fp = frame-pointer build for call
  stacks). The renderer backend is ~10 % of the main thread (NVIDIA driver ~6 %), so a command-stream
  worker thread would gain at most ~6-8 %: postponed. Fixed instead: CScriptStorage::object scanned
  whole Lua namespaces with lua_next+strcmp on every engine->Lua functor lookup (monolith 0ef9e56e,
  lj_tab_next 4.5 % -> 0 %; old/new lookups compared on 65536 calls, 0 mismatches), and the
  fast_dynamic_cast cache was MSVC-only (monolith c04c34f4, dynamic_cast 2.2 % -> 1.0 %). Frame-time
  A/B not done yet (laptop hot, user gaming). Left: strcmp 2.9 % (spread out), Lua GC ~3 %,
  sun cascades ~10 %, Feel::Vision ray queries ~5 %.
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
  targets declared as Texture2D (bound as null: TAA, motion vectors, SSS off under MSAA). "View
  dimension 8" (TEXTURE3D render target views, volumetric smoke) supported since monolith
  2026-11-28; verified that r3_volumetric_smoke on loads and renders the bench level without errors. FPS on GTX 1650 Ti, 1080p high,
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
- Headless test wrapper (tools/headless.sh): gamescope under `setsid` plus a watcher that kills the
  whole session when the wrapper dies; before, gamescopereaper and the game survived run_level's stop
  and kept the single-instance lock.
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
