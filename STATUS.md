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
- `monolith/` and `openxray/` are upstream clones, excluded from this docs repo.
- Published 2026-11-25 (public): https://github.com/kolimbea/xray-linux-port (this repo, remote
  `origin`) and https://github.com/kolimbea/xray-monolith-linux branch `stage-2-vk` (remote `kolimbea` in
  monolith/). Author identity: kolimbea noreply; no Claude trailers in commits. Push over HTTPS with
  `git -c credential.helper= -c 'credential.helper=!gh auth git-credential' push ...` (no SSH key).
- GAMMA install `~/Games/gamma` is read-only for this project.
