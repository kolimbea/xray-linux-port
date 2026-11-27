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
- **Top bug (2026-11-27): black world after several level changes** (HUD still drawn). Screenshot tour
  (Bar → Cordon → Agroprom → Dark Valley → ...): after monolith commit "present pass drops its view"
  the first three loads render, from about the fourth on the scene is black. Frames in
  build/shots/run3 (contact sheet build/shots/sheet3.jpg). Suspects: state cached across device or
  level resets in the backend (occlusion queries culling everything, program cache keyed by reused
  ids, null resources), or a GAMMA effect. Reproduce: echo 1 > build/fixture-gamma/appdata/shot_tour.txt,
  run with -dbg -ltx user_shots.ltx and XR_VK_SCREENSHOT_EVERY=200.
- User-reported bugs to fix next (2026-11-27): the sky "lags" (stutters/flickers), the player health
  widget in the bottom-left corner renders wrong (the body silhouette is one flat dark red while the
  limb bars show different values — per-part colour possibly lost: vertex colour channel order or
  blend state; crop in build/shots/corner.png); then test every graphics setting (presets, SSAO modes,
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
