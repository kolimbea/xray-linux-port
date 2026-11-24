# xray-linux-port — status

Goal: native Linux + Vulkan build of X-Ray Monolith (Anomaly/GAMMA engine).
Plan: [docs/PLAN.md](docs/PLAN.md) · Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) ·
Log: [docs/PROGRESS.md](docs/PROGRESS.md).

- Stage 0 done. Stage 1a done: `libxrCore.so` builds with clang and mounts the Anomaly archives.
  Stage 1b built. Stage 1c: everything compiles and links (`build/monolith/xray`, tag `stage1c-linked`);
  first boot reaches shader compilation (D3D11 device on dxvk is up).
  Decided 2026-11-24: go to the own renderer now (vkd3d cannot compile all shaders). Current work:
  stage 2 (ADR 0001): M1+M2+M3 done (device, swapchain, clear+present; in-process DXC -> SPIR-V, reflection, shader
  objects: all 453 compilable GAMMA shaders, 0 validation errors on 3 GPUs); next M4 — buffers, states, draws -> main menu.
  Engine work: `monolith/` branch `stage-2-vk`.
- `monolith/` and `openxray/` are upstream clones, excluded from this docs repo.
- GAMMA install `~/Games/gamma` is read-only for this project.
