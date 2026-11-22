# Progress

## Stage 0 — done 2026-11-22
- Environment verified (see PLAN.md): Vulkan on NVIDIA OK, clang 22 / gcc 16 / cmake 4.3, Wayland.
- PLAN.md, ARCHITECTURE.md written; feasibility numbers in FEASIBILITY.md.
- First xrCore compile probe: every TU blocked by headers (`_MT` guard, `<windows.h>`).
- Next: Stage 1a — compat header + CMake for xrCore.

## Stage 1a — in progress (branch `stage-1-linux-build` in monolith/)
- Commits: Linux shim routed from `xrCore_platform.h` (OpenXRay `PlatformLinux.inl` + own
  `MonolithCompat.inl`); `tools/fix_includes.py` normalised 3445 include paths in 1535 files
  (169 backslash, 3276 letter-case); xrCore header fixes.
- xrCore probe (`tools/probe.sh xrCore -DXRCORE_EXPORTS`): 9121 → 3478 error lines.
- Tried `-fms-compatibility`: breaks libstdc++ headers (`char16_t`, `__int128`, GCC atomics macros).
  Decision: standard clang + `-fms-extensions`; fix MSVC-permissive code in source.
- Open error classes (each repeats in ~57 TUs via headers):
  dependent-base lookup in `_stl_extensions.h` templates (needs `this->`/typedefs);
  `SRWLOCK` (xrSyncronize) → std::shared_mutex; `ppl.h` (Concurrency) → TBB/std;
  `errno_t` + `*_s` CRT functions; unqualified `is_polymorphic`;
  `_vector3d.h` uses engine `Device` from core (layering leak to resolve).
