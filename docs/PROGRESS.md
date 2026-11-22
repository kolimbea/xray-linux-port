# Progress

## Stage 0 — done 2026-11-22
- Environment verified (see PLAN.md): Vulkan on NVIDIA OK, clang 22 / gcc 16 / cmake 4.3, Wayland.
- PLAN.md, ARCHITECTURE.md written; feasibility numbers in FEASIBILITY.md.
- First xrCore compile probe: every TU blocked by headers (`_MT` guard, `<windows.h>`).
- Next: Stage 1a — compat header + CMake for xrCore.
