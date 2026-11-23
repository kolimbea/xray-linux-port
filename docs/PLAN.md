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
| 1c | xrEngine + SDL3 window/input/timers, R4 on dxvk-native, xrGame links | engine boots to the main menu, scripts load, log clean |
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
