# xray-linux-port — status

Goal: native Linux + Vulkan build of X-Ray Monolith (Anomaly/GAMMA engine).
Plan: [docs/PLAN.md](docs/PLAN.md) · Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) ·
Log: [docs/PROGRESS.md](docs/PROGRESS.md).

- Stage 0 done. Stage 1a done: `libxrCore.so` builds with clang and mounts the Anomaly archives.
  Stage 1b built. Stage 1c: all modules except xrGame compile; xrGame in progress.
  PAUSED 2026-11-23 — resume from the "Paused" section at the end of docs/PROGRESS.md. Engine work: `monolith/` branch `stage-1-linux-build`.
- `monolith/` and `openxray/` are upstream clones, excluded from this docs repo.
- GAMMA install `~/Games/gamma` is read-only for this project.
