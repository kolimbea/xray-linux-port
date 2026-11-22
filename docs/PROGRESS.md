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

## Stage 1a — done 2026-11-23 (`libxrCore.so` builds and runs)
- CMake (`monolith/CMakeLists.txt`, `src/xrCore/CMakeLists.txt`), clang 22, Ninja; source list checked
  against `xrCore.vcxproj`. Excluded on Linux: files excluded on Windows too, Borland files,
  `xrDebugNew.cpp` (replaced by `Common/linux/xrDebug_linux.cpp`).
- Only engine-provided symbols stay unresolved (build_id/date, printLuaStack, is_console_mark,
  modded-exes version) — resolved when the all-in-one executable links.
- Compat layer (`src/Common/linux/`): OpenXRay `PlatformLinux.inl` (3 buggy macros replaced) +
  `MonolithCompat.inl`, `win32_text` (iconv CP1251/UTF-8), `win32_file` (CreateFile/mmap views/ReadFile),
  `win32_system`, `io_compat` (_findfirst, stat, fopen…), `path_resolve`, `xrDebug_linux`
  (execinfo backtraces, signal handlers), stand-in headers in `include/`.
- **Case-insensitive FS**: `path_resolve` turns `\` into `/`, takes the fast path when the path exists,
  else matches components against a cached per-directory index (refreshed once on a miss).
  All compat file entry points use it.
- Smoke test (`tests/xrcore_smoke.cpp`) on the real Anomaly data: 68 archives, 51099 files mounted in
  0.43 s; `system.ltx` parsed with includes into 11871 sections. Install verified unmodified.
- Known gaps: gcc build not attempted (code needs clang's `__super`); `rename()` calls not yet routed
  through the resolver; `IsBadReadPtr` is a null check only; `LocatorAPI_Notifications` (dev file
  watcher) not ported.

## Next — Stage 1b
xrCDB, xrSound, xrPhysics (ODE), xrParticles, xrXMLParser, luajit + luabind.
