# Architecture of the port

Target (revised 2026-11-23): native Linux engine with its own Vulkan 1.3 renderer; no dxvk/D3D11
in the final build.

```
xrGame (gameplay, Lua via luabind/luajit)       portable, DIK_* keycodes only
xrEngine (device, input, console, level loop)   Win32 window/DirectInput -> SDL3
render interface (IRender_interface & co.)      unchanged contract used by engine/game
  Layers/xrRender — backend-neutral part         shader manager, resource/blender logic,
                                                 visibility, sorting (extracted in stage 2a)
  backend: xrRender_R4 (D3D11, Windows only)     kept building on Windows during the transition
  backend: xrRender_VK (Vulkan 1.3, Linux)       new
  backend: null (Linux, headless)                lets the engine run before pixels exist
xrSound (OpenAL)                                 OpenAL Soft
xrCore (FS, memory, threads, log, ini)           POSIX + case-insensitive path resolver
```

## Module boundaries
- `src/Common/linux/`: the only place for Win32-replacement code. Engine sources reach it through
  `xrCore_platform.h` under `#ifdef __linux__`; the Windows build stays untouched.
- All-in-one build as on Windows (Monolith's branch links every module into one executable); xrCore
  is a shared library for now so it can be tested alone.
- Data paths: the same `fsgame.ltx` layout. `path_resolve` maps `\` and letter case at the OS boundary;
  the MO2 load order is flattened into one tree (or overlayfs) by a script under `tools/`.

## Renderer split (stage 2a)
Layers/xrRender today mixes backend-neutral logic with D3D types (`ID3D11*`, D3DX math, DXBC
reflection). The split introduces a small backend layer — buffers, textures, samplers, render targets,
pipeline state, shader programs, draw/dispatch, queries — with R4 and VK implementations. Shader
manager, blenders, resource caching and visibility are written against that layer only. The R4 side is a
thin adapter over existing code so Windows keeps working and remains a behavioural reference.

## Shader pipeline (stage 2b)
- GAMMA's `.vs/.ps/.gs/.cs` and includes are compiled as-is by DXC to SPIR-V. A prepended compat
  header supplies macros/wrappers for FXC-era conventions; command-line remaps (`-fvk-*-shift`,
  `-fspv-*`) place D3D `register(bN/tN/sN/uN)` slots into fixed Vulkan descriptor sets.
- Reflection: SPIR-V (via SPIRV-Reflect or spirv-tools) replaces `D3DReflect`; it feeds the same
  constant-table structures the shader manager already uses.
- Cache: SPIR-V keyed by source hash + defines on disk; `VkPipelineCache` saved to disk.

## Vulkan backend (stage 2c+)
Vulkan 1.3 core: dynamic rendering (no render-pass objects), synchronization2, timeline semaphores
for frame pacing and uploads, descriptor indexing for bindless textures, VMA for memory with explicit
budgets for the 4 GB GTX 1650 Ti (streaming textures by mip, staging ring). Validation layers in debug.

## Testing
- Unit tests for the compat layer (`tests/compat_*`), FS smoke test on real Anomaly data.
- Screenshot autotest per render step: load a save, set camera pose, capture, compare against the
  Proton+DXVK reference of the same scene (perceptual diff with thresholds per scene).

## Decisions
- Port Monolith in place, not rebase onto OpenXRay (FEASIBILITY.md §2).
- Straight to Vulkan, no dxvk-native stage; references from Proton (PLAN.md, 2026-11-23).
- Compiler: clang with `-fms-extensions -fno-operator-names` (gcc lacks `__super`).
- Never edit GAMMA/Anomaly data; shader compatibility fixes live in the compat prelude / compile wrapper.
