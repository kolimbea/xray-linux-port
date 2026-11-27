# Current state (audit 2026-11-27)

Evidence legend: **[F]** confirmed fact (file/command/measurement given), **[A]** assumption,
**[E]** needs an experiment. Machine: Ryzen 5 3550H 4C/8T, GTX 1650 Ti 4 GB (NVIDIA 620.70.06),
Vega 8 iGPU (RADV), llvmpipe; Fedora 44, clang 22.

## 1. What the project is
- Root repo `xray-linux-port`: docs, tests, tools. Engine repo `monolith/` = fork of X-Ray Monolith
  (Anomaly/GAMMA engine), branch `stage-2-vk`. `openxray/` = OpenXRay clone used as a reference for
  Linux shims (not built). **[F]** (`ls`, `STATUS.md`)
- Size, C/C++ lines under `monolith/src` **[F]**: xrGame 457k, Layers 150k (xrRender 62k shared,
  R4 22k, R3 18k, DX10 15k, **xrRenderVK 14.7k incl. 6.2k generated**), xrEngine 53k, xrCore 46k,
  xrServerEntities 36k, xrPhysics 32k, xrCDB 20k, Common/linux 6.4k; 3rd party ~570k (ICU, imgui,
  OpenAL, LuaJIT, crypto, TBB, NVTT, ODE, luabind ...).
- Two executables **[F]** (`monolith/CMakeLists.txt:228-250`): `xray` = R4 on dxvk-native
  (stage-1c path, needs dxvk-native + vkd3d-shader) and `xray_vk` = R4 pipeline on the own Vulkan
  backend. Only the R4 pipeline is built; R1/R2/R3 renderers are not.
- Build: CMake + Ninja, sources taken from the upstream `.vcxproj` files by
  `cmake/vcxproj_sources.py` (Windows build untouched). RelWithDebInfo `-O2` for every module, no
  LTO, no `-march`, no PGO **[F]** (`compile_commands.json`; `CMakeLists.txt:12-13`).

## 2. Subsystems

| Subsystem | State | Evidence | Limits / problems |
|---|---|---|---|
| xrCore (FS, memory, strings, threads) | **Working** | mounts Anomaly archives (tests/README: 68 archives, 51099 files, 0.43 s); game runs | case-insensitive FS via shims; `vsnprintf_s` returns C99 values, not MSVC's -1 on truncation (`Common/linux/PlatformLinux.inl:337`) — low risk |
| Linux compat layer (`Common/linux`, 6.4k lines) | **Working, thin** | stub grep finds no silent stubs, only explicit "not supported" paths in D3DX texture helpers | XInput, ReShade, Discord are stubbed/absent (see input) |
| Window / input (SDL3) | **Working for keyboard+mouse** | user played sessions; `Common/linux/dinput_sdl.cpp` | no gamepad — **same as upstream**: `xrEngine/xr_input_xinput.cpp` and the XInput block in `Xr_input.cpp` are commented out on Windows too **[F]** (correction 2026-11-27); controllers work through Steam Input mappings |
| Sound (OpenAL Soft, system) | **Initialises** | log: device enumeration, `SOUND: OpenAL` lines **[F]** | audible output not verified by the auditor **[E]**; device names logged with wrong encoding (cosmetic) |
| Lua (LuaJIT + luabind) | **Working**, one gap | GAMMA scripts run; profiles show Lua inside the scheduler | **`require` with Windows-style `package.path` fails**: `a_idiots_config.script` adds `.\\gamedata\\scripts\\?.lua` and requires `illish.lib.*` → "Failed to load script" (log) — the whole "Useful Idiots" companion-AI mod is off on Linux **[F]**; 36 scripts in GAMMA use `require` |
| Physics (ODE) | **Working** | ASan fix of `ofs[-1]` (`ode/src/step.cpp`) | ODE "LCP internal error" crash seen before the fix; not seen since **[A]** |
| A-Life / AI / game logic | **Working** | level from a Windows save loads with NPCs | Windows saves load on Linux **[F]**; Linux→Windows save compatibility untested **[E]** |
| Renderer pipeline (R4 code, unchanged) | **Working** | Bar level, lighting, shadows, HUD, minimap; F12 screenshots | GAMMA shaders: **454/470 compile** to SPIR-V, 516 programs, 0 validation errors (`vk_shaders`, 2026-11-27) **[F]**; the 16 failures look like unused/legacy files (`*_old.ps`, missing includes) **[A, E]** |
| Vulkan backend (`Layers/xrRenderVK`) | **Working, complete for what R4 calls** | `vk_pending.gen.cpp` is empty — no unimplemented D3D11/DXGI method is referenced at link time **[F]**; tests pass with validation (below) | explicit gaps: typed buffer views, resource arrays, UAV counters, tessellation patch constants, RTV/DSV/UAV view dimensions not used by R4 (`vkContext.cpp:438,504,985`, `vkShader.cpp:461,546`, `vkViews.cpp:265,315,356,400,429`); coarse sync (full ALL→ALL barrier per rendering scope, `vkContext.cpp:108-119`); every `XRVK_CHECK`/`fail()` (41 + 17 sites) aborts, no device-lost path **[F]** |
| Shader toolchain (DXC in-process, SPIRV-Reflect) | **Working** | `vkDxc.cpp`, `vkShader.cpp`; engine shader cache on disk (`appdata/shaders_cache/r4`, 2280 files, 19 MB) **[F]** | GAMMA `ssfx_ao.ps` has an infinite loop → DXC keeps only the early-out, **SSFX AO is effectively off** (PROGRESS.md:307) **[F]**; FXC behaviour on Windows not compared **[E]** |
| Gamma / brightness | **Not working** **[A]** | `xr_effgamma.cpp` uses `IDXGIOutput::SetGammaControl`, which returns `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE` (`vkDXGI.cpp:753-760`) **[F]** | sliders likely have no effect **[E]** |
| HDR / VRR | **Absent** | `vkDXGI.cpp:380` (tearing FALSE), `:681` (SDR only) **[F]** | — |
| Fullscreen scaling | **Partial** | a lower `vid_mode` in fullscreen gives a swapchain of `vid_mode` size and the compositor scales it (measured: window 2560x1440 px, swapchain 1920x1080) **[F]** | no own upscaler; scaling quality = compositor's bilinear |
| Crash reporting | **Weak** | engine log has a fixed name and is overwritten per run; systemd-coredump kept two SIGABRT cores of the user's sessions (2026-11-26 00:02, 20:25) **without usable stacks** **[F]** | causes of these two aborts are unknown |
| Shutdown | **Faulty** | SIGTERM did not end the game within 30 s (2026-11-25, killed) **[F]** | menu "Quit" path not measured **[E]** |
| Packaging / distribution | **Absent** | only `tools/play_native.sh` + fixture generator | builds depend on the developer machine (system libs) |
| CI | **Absent for Linux** | upstream `.github/workflows/msbuild.yml` builds Windows only **[F]** | no automated Linux build/test |
| Tests | **Partial** | `vk_smoke`, `vk_draw`, `vk_heap`, `vk_hlsl_rewrite`: all pass, 0 validation errors (2026-11-27) **[F]**; `vk_shaders` (above) | `compat_*` and `xrcore_smoke` are built by hand (tests/README), not in CMake/CTest; no in-game automated test in the repo (autotest save + tour script live only in the local fixture); benchmark script only in a scratch directory |

## 3. Measured performance (Bar, GAMMA save; `* vk stats` in the engine log)
| config | frame | GPU | notes |
|---|---|---|---|
| 1280x720 windowed, start of the optimisation pass | 41.0 ms | ~10.5 ms | CPU-bound; GetData spin on light occlusion queries |
| after partial submits + linear host heap | 26.3 ms | ~10.5 ms | 2026-11-25 |
| after draw-path skips (commit 26c5746) | 23.5 ms median (n=14) | 9.9 ms | **[F]** |
| 2560x1440 fullscreen (player settings) | 34 ms | 32-33 ms | GPU-bound, ~4x pixels of 720p **[F]** |
- Main thread profile (eu-stack sampling, `tools/pmp.sh`): ~35-37 % of samples in `CSheduler::Update`
  (AI + Lua callbacks), ~45-53 % in `CRender::Render`; ~4300 draws, 88 rendering scopes, ~310 program
  switches per frame **[F]**.
- Startup to main loop (warm shader cache): ~5 s; save → 600 in-level frames: 93 s, and that first
  window has **p99 2.9 s hitches** **[F]** (cause: first-use texture uploads / shader compiles **[A, E]**).
- Memory at 1440p: VRAM ~3.3 GB (textures 2.3-2.5 GB, targets 0.74 GB, buffers 0.17 GB); ~5.5 GB of
  GAMMA's not-yet-used staging textures in process memory (`r__no_ram_textures off`); no growth over
  8-minute runs **[F]** (PROGRESS.md).

## 4. Driver coverage of the hard requirements
`VK_EXT_shader_object`, `VK_EXT_extended_dynamic_state3`, `VK_EXT_vertex_input_dynamic_state`,
`VK_KHR_push_descriptor`, Vulkan 1.3 (ADR 0001; `vkCore.cpp` `required[]`). Mesa `docs/features.txt`
(main branch, fetched 2026-11-27): shader_object = anv, lvp, hk, nvk, radv; **not** hasvk (old Intel),
tu (Qualcomm), panvk (Mali), pvr **[F]**. NVIDIA proprietary: present on 620.70 **[F]**. Minimum
Mesa/NVIDIA versions per distro **[E]**.
