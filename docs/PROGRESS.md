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

## 2026-11-24 — xrGame compiles (tag `stage1c-game-built`)
All engine and game code now compiles on Linux (clang 22): xrCore, xrCDB, xrXMLParser, xrCPU_Pipe,
xrParticles, xrSound, ODE, xrPhysics, LuaJIT, luabind, lua_extensions, imgui, xrNetServer, xrEngine,
xrGame (1243 TUs incl. xrServerEntities). Not linked yet; renderer not compiled yet.
- AI navigation templates fixed in place (behaviour unchanged).
- Real bugs found along the way (present on Windows too): pointers to temporary attachment matrices
  (`&GetTransform()` returned by value) in script_attachment_manager; luabind friend scoping;
  LevelGraph friend scoping; OpenXRay shim bugs (S_FALSE, strncpy_s/_TRUNCATE, POINT/RECT width).
- Tooling lessons: apply_fixits keys edits by real path (headers reached through several `../`
  spellings were edited twice at stale columns once — that commit was reverted), and only inserts
  `this->` before the diagnosed identifier. vcxproj parser honours unconditional ExcludedFromBuild.
Next: renderer (xrRenderPC_R4 + Layers/xrRender + xrRenderDX10) against dxvk-native headers,
D3DCompile/D3DReflect via vkd3d-utils, D3DX11 replacements; then link the executable.

## 2026-11-24 — renderer compiles, executable links and boots to shader compilation
Tags in monolith/: `stage1c-all-compiles`, `stage1c-linked`.
- xrRender_R4 (+ Layers/xrRender, xrRenderDX10) compiles against dxvk-native 3.1.1 headers. The
  DirectX SDK D3DX9/10/11 headers in `sdk/include/dxsdk` are used (exposed case-insensitively through
  `build/monolith/dxsdk_ci`); mingw d3dx9 headers from dxvk do not coexist with D3DX11.
- Own replacements for Windows-only libraries (Common/linux): `d3dx9_linux.cpp` (math, half floats,
  FVF declarations), `d3dx11_tex_linux.cpp` + `dds_codec` (DDS load incl. legacy formats/cubes/volumes,
  texture copy with resize + BC1 for save thumbnails, DDS/BMP/PNG/JPG save, mip filtering),
  `d3dcompiler_linux.cpp` (D3DCompile on vkd3d-shader's C API, own D3DReflect from RDEF/ISGN/OSGN,
  D3DGetInputSignatureBlob, D3DDisassemble, D3DCreateBlob). crypto uses the system OpenSSL 3 with an
  own SHA-0 (OpenSSL dropped it; digests stay identical). ReShade add-on hooks: stub (no ReShade on
  Linux). NVAPI off. AVI (Video for Windows) textures: Load() fails like a missing file.
- Why not vkd3d-utils: on x86-64 Linux it declares WINAPI/COM as `ms_abi` (vkd3d_windows.h:193);
  dxvk-native and the engine use System V — the first D3DCompile crashed with garbage arguments.
- Tests: tests/compat_dds.cpp (all 15626 DDS files in the GAMMA tree: 15618 parse, the other 8 are
  PNGs renamed to .dds), compat_d3dx9.cpp, compat_sha0.cpp — all pass. tools/hlsl_check compiles one
  HLSL file through the same code path.
- First boot (fixture `fsgame.ltx` + `db`/`gamedata` symlinks to the Anomaly install, run under
  `bwrap --ro-bind ~/Games/gamma` so the install cannot be written): core, configs, sound (OpenAL),
  SDL3 window, D3D11 device on dxvk (GTX 1650 Ti, 4064 MB) all come up; ~100 shaders compile and land
  in the fixture's shaders_cache.
- `register(ps, t0)` (stage-specific reservation, E5017 in vkd3d) is rewritten on the preprocessed
  text to d3dcompiler semantics (kept for that stage, dropped for others) — sources untouched.

### Blocker — needs a decision (shader compiler for the temporary dxvk stage)
vkd3d-shader 1.17 rejects `SV_ClipDistance`/`SV_CullDistance` (E5013), used by Anomaly's
`accum_volumetric.vs`. That is a compiler feature gap, not syntax; it cannot be rewritten away.
More gaps of this kind are likely in GAMMA's ~300 shader files. Options:
1. **Skip the dxvk stage for shaders and go to xrRender_VK now** (DXC, HLSL -> SPIR-V; DXC supports
   all of SM5). This is the mandatory end goal anyway; dxvk would keep serving only as a reference.
   Cost: the first frame comes later (G-buffer path first), but no throw-away compiler work.
2. Build a newer vkd3d (git) locally — only helps if upstream has the missing features; unknown.
3. Compile shaders with the real d3dcompiler_47.dll under Wine/Proton into the engine's shader
   cache (offline tool). Works for any shader, but needs a Windows helper binary (cross-toolchain
   or clang-cl + import lib) and keeps a Windows component in the pipeline.
Recommendation: 1 (with 3 as a fallback if an early dxvk image is wanted for reference shots).
Not decided yet — it is hard to revert, so it is the user's call.
Decided 2026-11-24 by the user: option 1 — go to xrRender_VK now (PLAN.md "Decision 2026-11-24").
The notes in STATUS.md were confirmed as the user's: the performance track and the review suggestions
are in PLAN.md ("Track: performance & multithreading", "Process rules"); all adopted — the shader
de-risking is exactly the next step. The auto-resume timer is no longer wanted (user), not set up.
Side note: the engine log shows the GTX 1650 Ti with AMD vendor id 1002 — that is dxvk hiding NVIDIA
(its default without NVAPI), so the NVIDIA GPU was used.

## 2026-11-24 — stage 2b started: DXC over all Anomaly R4 shaders (offline)
- DXC 1.9.2609 (official Linux build, pinned + sha256 in tools/fetch_deps.sh) -> SPIR-V, Vulkan 1.3,
  `-HV 2018 -Zpr`, one representative define set (tools/dxc_scan.sh).
- Shader sources exported from the archives through the engine's own VFS: tools/extract_tree
  (`$game_shaders$`, 970 files) run in build/fixture-anomaly (tools/make_fixture.sh), install read-only.
- Result, unmodified sources: **329 / 368** r3 shader files compile. 23 of the failures were one
  construct: `Texture2D.Load(loc, non_literal_offset)` (shadow.h:183) — SPIR-V needs immediate offsets.
- Compat rule 1 (tools/hlsl_rewrite.py, prototype to be ported into the engine's shader pipeline):
  `Load(loc, off)` -> `Load(loc + int3(off, 0))` on Texture2D/Texture2DArray (exact: the offset is added
  to the integer coordinate). Not applied inside #define bodies (macro parameters have unknown type,
  e.g. SMAALoad on Texture2DMS). With it: **352 / 368**.
- The remaining 16: include spelled with different case (resolved by the engine's case-insensitive
  FS), include-only fragments/dead files (`*_old`, a file name with a space, ssao.ps/ssao_hdao.ps without
  `main`), files needing other define sets; real DXC-vs-FXC strictness only in two: lower-case
  `texture2D` (ssao_hbao.ps) and a duplicated input semantic (models_lfo_light_dot_weapons.ps).
- Conclusion: DXC is viable for the whole set; the next check is GAMMA's shader overlay (MO2 mods)
  and the define variants the engine actually requests.

### GAMMA shader set (same day)
- Overlay built like MO2 does: Anomaly export, then the 19 enabled shader mods of profile G.A.M.M.A
  from lowest to highest priority (modlist.txt: first line wins) -> build/shaders/gamma (662 r3 files).
- Engine's SSFX_* quality defines added to the scan set (values as r4.cpp passes them).
- Compat rule 2: `[unroll(expr)]`/`[loop(expr)]` with a non-literal count (Screen Space Shaders use
  `q_steps[SSFX_WATER_QUALITY].x`) -> `[unroll]`/`[loop]`; DXC only accepts literals; hint only.
- Result: **453 / 470** compile. The rest: the same non-standalone/case/define-variant files as in
  Anomaly, the two FXC-leniency cases, and one **DXC crash (SIGSEGV)** on `scope_color_write.ps`
  (Shader 3D Scopes) — DXC bug to reduce and work around (or report upstream).
- Next: engine-side shader pipeline (port rules 1-2 to C++, SPIR-V reflection replacing D3DReflect),
  then stage 2a (render abstraction) — design to be agreed with the user before the split.

## 2026-11-24 — stage 2, milestone M1 done (tag `stage2-m1` in monolith/, branch `stage-2-vk`)
- `xrRender_VK` compiles the R4 pipeline, the shared layer and xrRenderDX10's logic **unchanged**
  (0/227 TU errors, no edits to shared code): stand-in `d3d11.h`/`dxgi*.h`/`d3dcompiler.h`/`D3DX10*.h`
  in `Layers/xrRenderVK/include` lead to `vkCommonTypes.h` — data types generated from the WIDL
  headers (tools/gen_vk_d3d_types.py) and D3D11-named classes with non-virtual methods
  (tools/gen_vk_d3d_interfaces.py). imgui's D3D11 backend is compiled into each renderer.
- `xray_vk` links against libvulkan only (no dxvk/D3D/vkd3d libraries; ldd/nm checked). Remaining
  header-level dependency: the engine-wide Win32 base types still come from dxvk-native's
  windows_base.h (type-only; to be replaced by own header later).
- The link left exactly the backend work list: 133 symbols (docs/vk/backend_methods_M1.txt) —
  ID3D11DeviceContext 66, ID3D11Device 21, swapchain/DXGI ~10, shader reflection + compiler API,
  CHW device setup. Each is defined in `vk_pending.gen.cpp` (tools/gen_vk_pending.py) to stop with
  "vk: X not implemented"; implemented ones drop out when the file is regenerated.
- Check: `xray` (R4/dxvk) behaves as before (boots to shader compilation, stops at the known
  vkd3d gap); `xray_vk` boots through the engine to render-device creation and stops with
  "vk: CHW::CreateDevice not implemented".
Next: M2 — vkHW (Vulkan 1.3 instance/device on the NVIDIA GPU, SDL3 surface, swapchain, clear+present).

## 2026-11-24 — stage 2, milestone M2 done (tag `stage2-m2`)
- `xrVK_core` (Layers/xrRenderVK: vkCore, vkFormat, vkObjects, vkDevice, vkDXGI), engine-independent:
  Vulkan 1.3 instance (SDL3 surface extensions, optional `XR_VK_VALIDATION=1`), GPU choice (discrete
  with most VRAM first, `XR_VK_GPU` override), device with the ADR 0001 extensions/features checked,
  VMA 3.3.0 + memory budget, 3 frames in flight on a timeline semaphore, whole-image layout tracking
  with synchronization2 barriers.
- DXGI/D3D11 objects on top: factory (adapters = Vulkan GPUs, preferred first), outputs = SDL displays
  (mode lists from SDL), `D3D11CreateDevice`, swap chain (own back buffer blitted to the swapchain
  image on Present, FIFO for sync interval 1, mailbox/immediate for 0, resize/out-of-date handling),
  2D textures incl. typeless depth (D24S8, D32 fallback probed), RTV/DSV, clears, CopyResource incl.
  GPU->staging readback, Map/Unmap of staging textures, user annotations ("no tool attached").
- `dx10HW.cpp` now compiles unchanged in the VK build (device setup logic reused).
- Check: tests/vk_smoke.cpp (target `vk_smoke`) with validation on: NVIDIA GTX 1650 Ti chosen among
  3 Vulkan devices; cleared back buffer read back = (64,127,191,255) for (0.25,0.5,0.75,1); FIFO and
  mailbox presents, resize; **0 validation errors**. In the game (`xray_vk`, validation on): device +
  2560x1440 swapchain + base RT/ZB created, 0 validation errors; it stops at
  "vk: ID3D11Device::CreateBuffer not implemented" (milestone M4 work). 105 methods pending.
Next: M3 (shaders: DXC in-process + SPIR-V reflection + shader objects), then M4 (buffers, textures,
states, draws -> main menu).

## 2026-11-24 — stage 2, milestone M3 (shaders) — backend done, in-game check moves to M4
- In-process DXC 1.9.2609 (`xrVK_shaderc`: `vkDxc.cpp`, isolated from the engine's Win32 stand-ins)
  implements `D3DCompile`: DXC preprocessor with the engine's ID3DInclude -> compat rules on the
  preprocessed text (`vkHlslRewrite.cpp`, the C++ port of tools/hlsl_rewrite.py; unit test
  `vk_hlsl_rewrite`) -> HLSL -> SPIR-V -> binding/location fix-ups. Flags: `-spirv
  -fspv-target-env=vulkan1.3 -HV 2018 -fvk-use-dx-layout -fspv-reflect -fvk-use-dx-position-w
  -fvk-auto-shift-bindings`, per-stage register shifts, `-Zpr` from D3DCOMPILE_PACK_MATRIX_ROW_MAJOR.
- Reflection from SPIR-V (SPIRV-Reflect vulkan-sdk-1.4.363.0, pinned in tools/fetch_deps.sh):
  `D3DReflect` (ID3D11ShaderReflection/ConstantBuffer/Variable/Type), `D3DGetInputSignatureBlob`
  (own blob format for CreateInputLayout), `D3DCreateBlob`; `D3DDisassemble` writes the reflection
  as text (no SPIR-V disassembler in the backend). Type names point into each reflection like
  d3dcompiler's, so dx10ConstantBuffer::Similar behaves as on Windows.
- `ID3D11Device::Create{Vertex,Hull,Domain,Geometry,Pixel,Compute}Shader` and programs (linked
  shader objects + push-descriptor layout per stage combination, cached per device). Binding and
  location scheme: ADR 0001 refinement (M3).
- Checks (`tests/vk_shaders.cpp`, GAMMA tree, r4.cpp's flags and a default define set):
  453/470 files compile (same as the dxc CLI scan; the 17 others are fragments without their own
  definitions, a case-differing include, and two FXC-only constructs: lowercase `texture2D` in
  ssao_hbao.ps, duplicate TEXCOORD0 in models_lfo_light_dot_weapons.ps), all 453 reflect, 0 spirv-val
  failures, and 515 programs (each shader alone + 62 linked VS+PS pairs) are created with
  **0 validation errors on NVIDIA 620.70, RADV 26.2 and llvmpipe**. 7.7 s for the tree on 8 threads.
- Reflection vs the R4 path (`tests/reflect_dump.cpp` built twice, `tools/reflect_compare.py`;
  reference = vkd3d DXBC, 435 files; 433 compared): every variable both report has the same size,
  class, type, rows, columns and elements, and the same offset in named cbuffers. Differences are
  DXC removing dead code that vkd3d keeps (37 VS signatures without unused inputs, 39 unused
  resources, 5 cbuffers of shaders whose uses are dead) — none inside a cbuffer that is present.
- Found in GAMMA's ssfx_ao.ps: the inner loop `for (int j = 1; j <= 2; i++)` never terminates by
  HLSL semantics, so DXC reduces the shader to its early-out `return float4(0,0,0,1)` (no SSFX AO).
  What FXC makes of it is to be compared on the Windows/Proton reference (screenshot test).
- vkd3d-shader (R4 path) hangs with >14 GB on deffer_tree_s_bump_d.vs; tests/reflect_dump_r4.sh runs
  the reference one file per process with a timeout and a memory limit.
- In the game nothing changed in behaviour: `xray_vk` still stops at
  "ID3D11Device::CreateBuffer not implemented" (CBackend::CreateQuadIB, before any shader load), so
  "every shader the engine requests" is checked once M4 gets the game to shader loading.
  84 backend methods pending (was 105).
Next: M4 — buffers, textures/SRVs, samplers and states, constants, input layouts -> vertex input,
draws with dynamic rendering -> main menu.
