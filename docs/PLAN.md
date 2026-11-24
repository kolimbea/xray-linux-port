# Plan: native Linux + Vulkan port of X-Ray Monolith (GAMMA engine)

Source of truth: themrdemonized/xray-monolith (`monolith/`, branch per stage).
Reference/donor only: OpenXRay (`openxray/`). Background numbers: [FEASIBILITY.md](FEASIBILITY.md).
Module layout: [ARCHITECTURE.md](ARCHITECTURE.md).

## Verified environment (2026-11-22)
- Fedora 44, Wayland, kernel 7.3.3.
- Vulkan 1.4 on NVIDIA GTX 1650 Ti (proprietary driver), plus RADV Vega 8 and llvmpipe.
  Runs must pin the NVIDIA device (`DXVK_FILTER_DEVICE_NAME="GTX 1650"`).
- gcc 16.2.1, clang 22.1.8, cmake 4.3.0, SDL3 3.4.20 devel installed.
- Missing (need sudo): `openal-soft-devel`, `libvkd3d-shader-devel libvkd3d-utils-devel` (1.17).
- dxvk-native: not packaged; build from source (meson) into `build/deps/` — user-local, no root.
- GAMMA install (read-only): `~/Games/gamma/anomaly` (base), `~/Games/gamma/gamma` (MO2 mods).
  Tests use a flattened copy or overlayfs mount under `build/`, never the install itself.

## Goal (revised 2026-11-23)
Final product: native Linux engine with **own Vulkan 1.3 renderer `xrRender_VK`** — no dxvk and no
D3D11 in the shipped build. GAMMA HLSL shaders are compiled unmodified (HLSL → SPIR-V via DXC).

## Decision (revised 2026-11-23, second time): dxvk-native as the temporary step
The first revision skipped dxvk-native. Measured in Stage 1c, that does not hold: skeletal
animation, bone queries and OGF model loading — which game logic needs even without pixels — live
in `Layers/xrRender`, and 72 of its 123 .cpp files call D3D directly (vertex/index buffers are created
while loading a model). A null backend would mean the full render split before the engine can boot.
So, as the user originally proposed:
- **Now**: R4 (D3D11) + Layers/xrRender build on Linux against dxvk-native 3.1.1 (headers + libs,
  pinned in tools/fetch_deps.sh). `D3DCompile`/`D3DReflect` → vkd3d-utils 1.17 (installed). D3DX11
  helpers get own replacements. This boots the engine, runs GAMMA logic and gives the reference image
  natively (Proton screenshots stay as a second reference).
- **Then** (stage 2a+): split backend-neutral code out of Layers/xrRender against a small backend
  interface and write xrRender_VK; dxvk is removed from the final build.
- dxvk's `windows_base.h` also supplies the Win32 base types for the whole engine (Windows-accurate
  widths), replacing OpenXRay's divergent typedefs.

## Stages
| stage | deliverable | check |
|---|---|---|
| 0 | plan, architecture | docs committed — done |
| 1a | xrCore on Linux, case-insensitive FS | `libxrCore.so` mounts Anomaly archives — done |
| 1b | xrCDB, xrSound (OpenAL Soft), xrPhysics/ODE, xrParticles, xrXMLParser, luajit+luabind | libs link |
| 1c | xrEngine + SDL3 window/input/timers, R4 on dxvk-native, xrGame links | everything links, boots to shader compilation — done 2026-11-24; main menu on dxvk dropped (see decision 2026-11-24) |
| 2a | render abstraction: split backend-neutral code out of Layers/xrRender (shader manager, resources, visibility) behind an interface; R4 keeps building on Windows | Windows build unchanged; Linux builds with null backend |
| 2b | HLSL pipeline: DXC HLSL→SPIR-V for GAMMA's .vs/.ps/.gs/.cs with a compat prelude (macros/wrappers for D3D register/sampler/cbuffer conventions), SPIR-V reflection replaces D3DReflect, on-disk SPIR-V + VkPipelineCache | all GAMMA shaders compile; reflection matches the R4 binding expectations |
| 2c | xrRender_VK core: Vulkan 1.3 device (dynamic rendering, synchronization2, timeline semaphores), VMA memory, bindless textures via descriptor indexing, swapchain, upload/streaming within 4 GB VRAM | main menu renders |
| 2d | G-buffer + geometry | screenshot diff vs Proton reference |
| 2e | lighting | screenshot diff |
| 2f | shadows | screenshot diff |
| 2g | post-processing incl. GAMMA screen-space shaders | screenshot diff |
| 2h | UI | screenshot diff; level playable |
| 3 | GAMMA compatibility (COMPAT.md), Windows save compatibility, FPS/frametime/load vs Proton | parity or better |

Each stage: branch in `monolith/`, small commits, tag, entry in PROGRESS.md.

## Decision 2026-11-24 (user): go to xrRender_VK now
vkd3d-shader 1.17 cannot compile all of Anomaly's HLSL (`SV_ClipDistance`, PROGRESS.md "Blocker").
Instead of more work on the temporary dxvk path, stage 2 starts now; order 2b (DXC pipeline,
offline over all shaders) -> 2a (render abstraction) -> 2c. The R4/dxvk build stays compilable as a
reference for API usage, not as a milestone.

## Stage 2 milestones to the main menu (approved 2026-11-24, ADR 0001)
| milestone | content | check |
|---|---|---|
| M1 | stage 2a seam + `xrRender_VK` target builds; unimplemented backend functions fail loudly (R_ASSERT/log), no silent stubs; `STATIC_RENDERER_VK` selection | R4/dxvk still builds and boots as before; `xrRender_VK` links without dxvk |
| M2 | vkHW: NVIDIA selection, device, SDL3 swapchain, clear + present | validation layers: 0 errors; screenshot |
| M3 | every shader the engine requests compiles via in-process DXC, reflects, becomes a shader object | 0 failures in the log; SPIR-V constant tables match D3DReflect on shaders vkd3d also compiles; rewrite-rule unit tests |
| M4 | buffers, textures, RTs, constants, states, UI pass -> main menu | screenshot test vs Proton reference; validation clean |

## Track: performance & multithreading (user request 2026-11-24)
Correctness and GAMMA behaviour first; this track starts after the first frame and is designed into
xrRender_VK from the start. Target CPU: Ryzen 5 3550H (4C/8T); GPU GTX 1650 Ti 4 GB (hybrid with Vega).
- Measure first: Tracy (or similar) zones, frametime with 1% lows / stutter, per-core load; before
  and after every change, same save and route.
- Build configs: Debug (-O0) and Release (-O2, ThinLTO, `-march=znver1`); PGO from a real session later.
- Candidates: oneTBB task jobs; parallel visibility/culling and sector traversal; async texture and
  resource streaming; shader prewarm on load + VkPipelineCache on disk; multithreaded command
  recording in xrRender_VK; parallel A-Life/AI and physics where data allows.
- Lua/luabind is not thread-safe: script callbacks stay on the main thread. In GAMMA scripts are a
  major CPU cost: profile callbacks and LuaJIT GC; evaluate LuaJIT 2.1 (mod compatibility first).
- Saves and gameplay stay deterministic as on Windows.
- VRAM budget for GAMMA HD textures (streaming, budget control); make sure the NVIDIA GPU is used
  (not Vega) and presentation does not stall on PRIME.

## Process rules (adopted 2026-11-24 from the user's review)
- De-risk shaders offline over the whole GAMMA set before relying on a compiler (doing it now with DXC).
- Behaviour regression test once it boots: load the user's save, tick A-Life headless for minutes,
  compare with the Windows build under Proton.
- Mechanical edits (case, `this->`, `typename`) in separate commits from semantic ones, to keep
  rebases on Monolith/GAMMA updates manageable.

## Estimate (order of magnitude, one engineer-equivalent, revised 2026-11-23)
| stage | estimate |
|---|---|
| 1b | 1–2 weeks |
| 1c | 3–5 weeks |
| 2a | 2–4 weeks |
| 2b | 2–4 weeks (risk: GAMMA HLSL quirks under DXC) |
| 2c | 3–5 weeks |
| 2d–2h | 8–14 weeks |
| 3 | open-ended, 4+ weeks |
Total to a playable GAMMA on xrRender_VK: roughly 6–9 months. Stage 1a took one session, faster
than the earlier 1–2 week guess, but the renderer dominates and is less predictable.

## Stage 1 approach
1. `src/Common/linux/` compat layer: minimal Win32 shims for what the engine actually uses,
   grown from compiler errors, not written up front.
2. MSVC-isms handled centrally: `__forceinline`, `__declspec(dllexport)` → visibility, `_MT` guard,
   `__int64`, SEH `__try`, `#pragma comment(lib)`.
3. Filesystem: `\` → `/` normaliser + case-insensitive lookup via a lowercase index built once per
   mounted root, refreshed on miss.
4. Threads/locks → std::thread / std::mutex; timers → `clock_gettime(CLOCK_MONOTONIC)`.

## Risks (ordered)
1. GAMMA HLSL under DXC: SM4/5 legacy syntax, implicit conversions FXC tolerated, `register()` layouts.
   Mitigation: compat prelude + per-shader error log; DXC `-fspv-*` binding remaps; never hand-edit
   GAMMA files (fixes go into the prelude or the compile wrapper).
2. Case-insensitive asset lookup cost and correctness.
3. MSVC-tolerated UB under clang/gcc.
4. Upstream Monolith churn — keep the port a rebasable patch series.

## First measurement (xrCore, clang 22 `-fsyntax-only -fms-extensions`)
All 69 translation units stop at the first header: 58 at the `_MT` guard in `xrCore.h`, 10 at
`<windows.h>`, 1 at a case-mismatched `StdAfx.h` include. So the first work item is the compat header.
