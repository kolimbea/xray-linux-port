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

## Decision 2026-11-23 — own Vulkan renderer is the goal
User changed the plan: final build uses `xrRender_VK` (Vulkan 1.3), no dxvk/D3D11. Chosen: skip the
dxvk-native stage entirely; reference screenshots come from the existing Proton+DXVK GAMMA install,
engine logic runs on a null backend until VK draws. Reasoning and revised stages/estimate in PLAN.md,
render split and shader pipeline in ARCHITECTURE.md. Stage 1 (1b, 1c) is finished first.

## Stage 1b — built 2026-11-23 (tag `stage1b-built`)
Static libraries build with clang: xrCDB, xrXMLParser, xrCPU_Pipe, xrParticles, xrSound (system
OpenAL Soft 1.24 + vorbis/ogg), ODE, xrPhysics, LuaJIT, luabind, lua_extensions — plus libxrCore.so.
They are not linked into an executable yet, so unresolved cross-module symbols show up only in 1c.
- **Build flags**: added `-fdelayed-template-parsing` (MSVC-style lookup into dependent bases;
  xrPhysics went from 20956 to 103 errors with it). It is deprecated in clang but present in 22;
  if it disappears, `tools/fixit_pass.sh` + `this->` edits are the fallback.
- **Mechanical fixes** (tools in repo): `fix_opaque_enums.py` — 34 forward-declared enums get
  `: int` (MSVC's implicit type) in declarations and definitions, their `u32(-1)` sentinels become
  `-1` (value MSVC stored); `fixit_pass.sh`/`apply_fixits.py` — inserts missing `typename` at clang's
  reported spot only (no guessed fix-its).
- **LuaJIT**: Monolith ships a patched 2.0.4 (xr_alloc, lj_allocprof) without Makefiles; CMake now
  runs minilua → DynASM (vm_x86.dasc, -D P64) → buildvm (elfasm, bcdef/ffdef/libdef/recdef/vmdef/folddef).
  The Windows low-address pool (xr_alloc.c) is compiled out; LuaJIT's own MAP_32BIT path is used.
  `tests/luajit_smoke.c`: 1e7-iteration loop, JIT on, FFI call into libc — pass.
- Linux-specific swaps: ttapi thread pool over pthreads (CreateThread/WaitForMultipleObjects shims),
  audio device notifications dropped (OpenAL Soft follows default device), Winsock luasocket → POSIX.
- Fixed real bugs exposed by clang: luabind `friend struct scope` befriended the wrong class,
  invalid `##` token pastes in dTriBox.cpp, a local shadowing a template parameter in
  dSortTriPrimitive.h, `ov_seek_func` signature vs `ogg_int64_t`.

## Next — Stage 1c
xrEngine (SDL3 window/input, x_ray.cpp entry, XR_EARLY_INIT guard), xrServerEntities, xrGame,
null render backend, link the all-in-one executable, boot headless with Anomaly data.
