# Plan: native Linux + Vulkan port of X-Ray Monolith (GAMMA engine)

Source of truth: themrdemonized/xray-monolith (`monolith/`, branch per stage).
Reference/donor only: OpenXRay (`openxray/`). Background numbers: [FEASIBILITY.md](FEASIBILITY.md).
Module layout: [ARCHITECTURE.md](ARCHITECTURE.md).

## Verified environment (2026-11-22)
- Fedora 44, Wayland, kernel 7.3.3.
- Vulkan 1.4 on NVIDIA GTX 1650 Ti (proprietary driver), plus RADV Vega 8 and llvmpipe.
  Runs must pin the NVIDIA device (`DXVK_FILTER_DEVICE_NAME="GTX 1650"`).
- gcc 16.2.1, clang 22.1.8, cmake 4.3.0, SDL3 3.4.20 devel installed.
- Missing (need sudo): `openal-soft-devel`, `vkd3d-devel` (vkd3d-shader/utils).
- dxvk-native: not packaged; build from source (meson) into `build/deps/` — user-local, no root.
- GAMMA install (read-only): `~/Games/gamma/anomaly` (base), `~/Games/gamma/gamma` (MO2 mods).
  Tests use a flattened copy or overlayfs mount under `build/`, never the install itself.

## Stages
| stage | deliverable | check |
|---|---|---|
| 0 | this plan, ARCHITECTURE.md | docs committed |
| 1a | CMake + xrCore builds with clang/gcc on Linux | `libxrCore.so` links |
| 1b | xrCDB, xrSound, xrPhysics, xrParticles, xrXMLParser, luabind/luajit | libs link |
| 1c | xrEngine with SDL3 window/input/timers, case-insensitive FS, null renderer | engine mounts Anomaly archives, parses `system.ltx`, writes log |
| 2 | R4 linked to dxvk-native, D3DCompile → vkd3d-utils + disk shader cache, OpenAL Soft, SDL3 input | main menu, then a level on Anomaly, then GAMMA |
| 3 | GAMMA compatibility (COMPAT.md), Windows save compatibility, FPS/load vs Proton | parity with Proton+DXVK |
| 4 | xrRender_VK (Vulkan 1.3, DXC → SPIR-V), screenshot-diff autotest | image parity, lower CPU frametime |

Each stage: branch `stage-N-*` in `monolith/`, small commits, tag `stageN-done`, entry in PROGRESS.md.

## Stage 1 approach
1. `src/Common/linux/` compat layer: minimal Win32 shims for what the engine actually uses,
   grown from compiler errors, not written up front.
2. MSVC-isms handled centrally: `__forceinline`, `__declspec(dllexport)` → visibility, `_MT` guard,
   `__int64`, SEH `__try`, `#pragma comment(lib)`.
3. Filesystem: `\` → `/` normaliser + case-insensitive lookup via a lowercase index built once per
   mounted root, refreshed on miss.
4. Threads/locks → std::thread / std::mutex; timers → `clock_gettime(CLOCK_MONOTONIC)`.

## Risks (ordered)
1. HLSL compile + reflection on Linux (vkd3d-shader SM5 coverage). Fallback: DXBC cache made once
   by real `d3dcompiler_47` under Wine.
2. Case-insensitive asset lookup cost and correctness.
3. MSVC-tolerated UB under clang/gcc.
4. Upstream Monolith churn — keep the port a rebasable patch series.

## First measurement (xrCore, clang 22 `-fsyntax-only -fms-extensions`)
All 69 translation units stop at the first header: 58 at the `_MT` guard in `xrCore.h`, 10 at
`<windows.h>`, 1 at a case-mismatched `StdAfx.h` include. So the first work item is the compat header.
