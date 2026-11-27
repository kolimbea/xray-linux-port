# Architecture audit

## 1. Structure as built
```
xray_vk (one executable, static libs in one link group)
├─ xrGame, xrServerEntities, xrPhysics(ODE), xrCDB, xrParticles, xrSound(OpenAL), xrNetServer
├─ xrEngine (device loop, SDL3 window/events, console)          ── unchanged upstream code + small fixes
├─ xrRender_VK = Layers/xrRender + xrRenderDX10 + xrRenderPC_R4 ── R4 D3D11 renderer, UNCHANGED source
│    └─ calls D3D11/DXGI C++ interfaces ──► Layers/xrRenderVK (xrVK_core): stand-in D3D11/DXGI classes
│         implemented on Vulkan 1.3 (shader objects, push descriptors, VMA, timeline, DXC→SPIR-V)
├─ Common/linux: Win32/D3DX/DInput shims (PlatformLinux.inl, win32_*.cpp, d3dx*_linux.cpp, dinput_sdl)
└─ xrCore (shared lib), luajit, luabind, cximage, imgui, crypto(OpenSSL 3)
```
Design decision (ADR 0001): keep the engine's D3D11 renderer source and implement D3D11 itself on
Vulkan, specialised to what R4 calls. Effect: ~99 % of engine lines unchanged, every GAMMA shader
mod keeps working as HLSL, upstream merges stay cheap.

## 2. Assessment by criterion

| Criterion | Finding | Evidence |
|---|---|---|
| Coupling | Engine ↔ backend coupling is exactly the D3D11 API surface — narrow, stable, documented by Microsoft. Generated headers (`vk_d3d_interfaces.gen.h`, `vk_d3d_types.gen.h`) define it. Good. | `tools/gen_vk_d3d_*.py`, empty `vk_pending.gen.cpp` |
| Separation of concerns | Backend files map 1:1 to D3D11 concepts (buffers, textures, views, states, context, queries, DXGI) plus Vulkan services (core, heap, shader, dxc). Clear. `vkContext.cpp` (1.5k lines) holds state tracking + recording + copies — largest unit, still coherent. | `wc -l` |
| Modularity | `xrVK_core` links standalone (tests use it without the engine). DXC isolated in `xrVK_shaderc` so its COM headers never meet the engine's Win32 stand-ins. | `CMakeLists.txt:198-203` |
| Scalability (CPU) | One immediate context, serialised with a recursive mutex on every call (D3D11 multithread protection); command recording is single-threaded; engine renders on the main thread. The R4 renderer issues ~4300 draws / 88 rendering scopes per frame; draw preparation 2.0 ms/frame after optimisation. The real CPU limit is the engine: ~35 % of main-thread samples in `CSheduler::Update` (AI + Lua), ~50 % in `CRender::Render` (culling, sorting, state). | profiles (CURRENT_STATE §3) |
| Scalability (GPU) | Coarse sync: one full ALL→ALL memory barrier per rendering scope and around transfers/dispatches; all images in `VK_IMAGE_LAYOUT_GENERAL`. Correct, simple, but prevents overlap of consecutive passes. Cost not measured. | `vkContext.cpp:108-119`, `begin_rendering` |
| Resource management | VMA for device memory; host heap with linear pages for per-frame data (constant buffers, dynamic geometry), virtual blocks for large slices; deferred destruction by timeline value; write-only staging textures in pageable process memory. No leak over 8-minute runs. | `vkHeap.cpp`, `vkTexture.cpp`, PROGRESS.md |
| Thread safety | Context lock covers engine worker threads (tbb texture loads call UpdateSubresource/Map); upload command buffers are separate from frame slots with their own lock; deferred list and heap are locked. Data races fixed after a validation THREADING error. ASan run clean after fixes. | `vkObjects.h` Context::lock, `vkCore.cpp` upload_* |
| Error handling | Fail-fast: 41 `XRVK_CHECK` + 17 `fail()` sites `abort()` with a logged reason. No `VK_ERROR_DEVICE_LOST` / OUT_OF_DEVICE_MEMORY recovery, no swapchain-loss path beyond recreation on OUT_OF_DATE. Engine-side errors go through xrDebug (log + message box). Logs are overwritten per run, cores have no stacks → post-mortem is weak. | `vkCore.cpp:77-85`, CURRENT_STATE |
| Testability | Backend: 5 test programs with pixel checks and validation; compat layer: hand-built tests; game: no automated scenario in the repo; no CTest, no CI. | tests/README |
| Maintainability | Upstream sources unchanged except small fixes; build reads upstream .vcxproj files, so upstream file additions flow in automatically. Risk: two renderer executables (dxvk path still built). Docs: ADR, PLAN, PROGRESS are current. | `CMakeLists.txt` |
| Portability | Hard requirements (shader objects, EDS3, vertex-input dynamic state, push descriptors) exclude old Intel (hasvk), Qualcomm and Mali drivers and older Mesa releases. | Mesa features.txt |

## 3. Architectural limits (ordered by impact)
1. **Engine main thread** — game logic (A-Life, AI, Lua) and render submission share one thread.
   This is upstream X-Ray design; the backend cannot remove it. Biggest FPS ceiling on 4-8 core CPUs.
2. **No temporal data in the renderer** — no motion vectors for all geometry, no projection jitter.
   Blocks TAA-class upscalers (FSR 2/3, DLSS, XeSS) and frame generation.
3. **Fail-fast without diagnostics** — crashes in the field are not diagnosable (no persistent
   logs/stacks).
4. **Hard extension requirements** — fine for RADV/ANV/NVK/NVIDIA, blocks older drivers.
5. **Coarse GPU synchronisation** — a ceiling on GPU efficiency at high resolutions (unmeasured).

## 4. What is *not* a limit
- The D3D11-on-Vulkan approach itself: the method surface R4 needs is fully implemented, GPU time at
  720p is ~10 ms on a GTX 1650 Ti, CPU time inside the backend is ~2 ms/frame. A native rewrite of
  the renderer would cost months and break GAMMA's shader mods for no measured gain (see
  COST_BENEFIT.md).
- Memory: fits a 4 GB GPU at 1440p with GAMMA's textures.
