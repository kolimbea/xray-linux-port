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
- xrCore probe (`tools/probe.sh xrCore -DXRCORE_EXPORTS`): 9121 → 3478 → 58 error lines; the 57 left are the missing oneTBB headers (tbb-devel), so xrCore TUs are otherwise syntax-clean.
- Tried `-fms-compatibility`: breaks libstdc++ headers (`char16_t`, `__int128`, GCC atomics macros).
  Decision: standard clang + `-fms-extensions`; fix MSVC-permissive code in source.
- Open error classes (each repeats in ~57 TUs via headers):
  dependent-base lookup in `_stl_extensions.h` templates (needs `this->`/typedefs);
  `SRWLOCK` (xrSyncronize) → std::shared_mutex; `ppl.h` (Concurrency) → TBB/std;
  `errno_t` + `*_s` CRT functions; unqualified `is_polymorphic`;
  `_vector3d.h` uses engine `Device` from core (layering leak to resolve).
- Fixed since: dependent-base lookups in container wrappers; core-math `Device` calls go through
  `xr_device<T>()` (defined in xrEngine/device.h); latent bugs in never-instantiated templates
  (`_valid` for rect/obb/box, `Fsphere::` in `_sphere<T>`); PPL → oneTBB; Win32 locks → std mutexes;
  `or` used as a method name → build with `-fno-operator-names`.
- Blocked on: `sudo dnf install tbb-devel libogg-devel libvorbis-devel libtheora-devel giflib-devel libjpeg-turbo-devel meson glslang-devel`.
