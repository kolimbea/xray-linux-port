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

## Stage 1c — first probe 2026-11-23
- xrEngine: all 105 TUs stop at `xrEngine/stdafx.h:28 #include <d3d9.h>` — the engine precompiled
  header pulls D3D9 types in everywhere. First 1c task: find which D3D types engine code really uses
  (D3DCOLOR, D3DXVECTOR/MATRIX, D3DVIEWPORT, caps) and give them backend-neutral definitions in a
  stand-in header, which also serves the render split in 2a.
- xrGame: 23460 error lines in 1356 TUs, nearly all cascading from the same engine header
  (CGameFont, Device, ENGINE_API undefined after the d3d9.h failure). Real xrGame-specific errors
  will appear only after xrEngine/stdafx.h is fixed.
- xrServerEntities has no own .vcxproj (its sources are compiled as part of xrGame).
- Tooling fixed: probe/vcxproj parser handle subdirectory sources, case-mismatched directories
  (`AI/` vs `ai/`), projects without a plain Release config.

## Stage 1c — in progress 2026-11-23
- xrEngine builds through CMake except 14 TUs: Device_create/Initialize/wndproc + device.cpp
  (Win32 window & message loop -> SDL3), EngineAPI.cpp (renderer selection: needs the null
  backend), x_ray.cpp, console line editor (keyboard-layout APIs), imgui input, fmesh (d3dx9).
- Input: `Common/linux/dinput_sdl.cpp` emulates DirectInput 8 over SDL3 so Xr_input.cpp runs
  unchanged (buffered events, acquire, coop level -> relative mouse, key names). DIK values from
  mingw-w64 dinput.h; SDL->DIK table generated from names in both headers (117 keys). Unit test
  `tests/compat_dinput.cpp` passes.
- Window: `win32_window.cpp` maps HWND to SDL_Window* (cursor, clip, rects, monitors, display
  mode). Constants from mingw-w64 winuser.h. MonitorList over SDL displays.
- Bugs fixed on the way: OpenXRay's POINT/RECT used 64-bit long (engine casts int pairs to them),
  OpenXRay strncpy_s wiped the buffer instead of honouring _TRUNCATE.
- Not ported (noted): Text_Console (dedicated server only), tntQAVI (AVI textures, renderer
  stage), keyboard layout switching from the console (ActivateKeyboardLayout).

## Decision 2026-11-23 (later) — dxvk-native back as the temporary step
Measured: 72/123 Layers/xrRender sources call D3D directly and model loading (needed by game
logic) creates GPU buffers, so a null renderer would require the full render split first. Switched to
the user's original plan: R4 on dxvk-native 3.1.1 now, own Vulkan renderer after (details PLAN.md).
Engine-wide Win32 types now come from dxvk's windows_base.h; verified no change in the xrCore
smoke test against a stage1a-done build on the same fixture (50992 files, 11871 sections).
- 2026-11-23 later: xrEngine builds except x_ray.cpp. SDL3 window (SDL_WINDOW_VULKAN, dxvk WSI
  "SDL3"), event loop + on_sdl_event replacing WndProc (focus -> OnWM_Activate, close -> quit, text
  input -> imgui as Unicode), D3DX9 replaced by an FVF-size helper and a 4x4 inverse (tested:
  1000 random matrices, A*A^-1 = I within float precision; singular input leaves output untouched).
- Next: xrNetServer without DirectPlay. OpenXRay ships "empty" NET_Client/NET_Server for Linux on
  which single-player works; port that to Monolith's headers (they differ ~120-180 lines each).

## Stage 1c — 2026-11-23 (continued)
- Everything except xrGame now compiles: xrCore, xrCDB, xrXMLParser, xrCPU_Pipe, xrParticles,
  xrSound, ODE, xrPhysics, LuaJIT, luabind, lua_extensions, imgui, xrNetServer, xrEngine.
  - xrEngine: x_ray.cpp `main()` for Linux, single-instance lock via flock, dlopen-based LoadLibrary.
  - xrNetServer: DirectPlay8 headers from mingw-w64; single-player uses psNET_direct_connect and never
    creates DirectPlay objects; COM activation fails cleanly (REGDB_E_CLASSNOTREG) for multiplayer.
- xrGame (1247 TUs, includes xrServerEntities): sampled check of 120 TUs went 974 -> 381 error lines.
  Fixed: precompiled-header case (xrServerEntities/xrGame sources found xrCore's stdafx.h instead of
  xrGame/StdAfx.h), template parameters re-typedef'd to their own name, names inside template template
  parameters, dependent-base members, `typename const`, several MSVC-only declarations.
- **Open decision — AI navigation templates.** vertex_manager_*, data_storage_*, dijkstra, a_star,
  graph_engine (~60 headers) use MSVC-only template constructs heavily (class templates as default
  *type* arguments, nested parameters shadowing outer ones). Options:
  (a) fix Monolith's files in place — keeps exact behaviour, slow, error-prone;
  (b) adopt OpenXRay's rewritten xrAICore navigation (portable, used on Linux for years) and adapt
      callers — faster, but a different implementation of A-Life/level pathfinding, so GAMMA behaviour
      must be checked (paths, NPC movement, saves).
  Recommendation: (a) for now, file by file, guided by OpenXRay's fixes; revisit if it exceeds a few days.

## Paused 2026-11-23 (second time) — where to resume
State: everything except xrGame compiles. xrGame sampled check (150 TUs): 157 error lines, down from
974. Remaining are mostly dependent-base members in AI navigation inline headers (a_star, path_manager_*,
action_planner: data_storage, graph, solution, m_failed, ...) plus a few one-offs.
Resume with:
1. Opt-in `this->` for those headers, then fixit passes to a fixpoint:
   `export XR_THIS_FILES='/xrGame/(a_star|dijkstra|path_manager|action_planner|graph_edge|problem_solver|data_storage|vertex_manager|operator_abstract)[a-z_]*\.h$'`
   `tools/fixit_pass.sh /src/xrGame/`   (repeat until it reports 0; it writes files only at the end)
2. `TOP=45 tools/check.sh /src/xrGame/ 150`. Known one-offs: IKLimb.h:15 (nested-name forward
   declaration), HUDTarget.cpp:106 / HUDCrosshair.cpp:68 (declaration vs using-declaration),
   script_game_object_inventory_owner.cpp:2044 (returns bool as LPCSTR), script_imgui_inline.h
   (address of temporary ImVec2 at 386/416), alife_registry_wrapper.h:69.
3. Full xrGame build (~11 min), link the all-in-one executable, boot headless on the Anomaly fixture.
Tools: fix_includes.py, fix_opaque_enums.py, fix_template_param_typedefs.py,
fix_template_template_names.py, fixit_pass.sh + apply_fixits.py (XR_THIS_FILES opt-in), check.sh,
probe.sh, fetch_deps.sh.
