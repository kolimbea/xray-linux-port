# ADR 0001 — xrRender_VK: backend layer in the R3/R4 pattern, Vulkan 1.3 with shader objects

Date: 2026-11-24. Status: accepted (user).

## Context
The own Vulkan renderer is the mandatory goal. The shared render layer (`Layers/xrRender`) already
multiplexes backends with `#if defined(USE_DX10) || defined(USE_DX11)` (R1/R2 on D3D9 vs R3/R4);
OpenXRay added an OpenGL backend the same way (`USE_OGL`, ~37 shared files touched, a 22-file
`xrRenderGL` layer). The engine drives rendering through a D3D11-style state machine (`RCache`).

## Decision
- New backend layer `Layers/xrRenderVK` (counterpart of `xrRenderDX10`) and a target `xrRender_VK`
  that compiles the existing `xrRenderPC_R4` pipeline and `Layers/xrRender` sources with `USE_VK`
  (no copies of the pipeline). Direct `HW.pContext/pDevice` calls in shared/R4 code go through backend
  functions first (stage 2a), keeping the R4/dxvk build working as a reference.
- Under `USE_VK` the D3D type aliases (`DXCommonTypes.h`) map to own POD descriptors/handle classes
  (`VKCommonTypes.h`); no dxvk or D3D11 headers/libraries in `xrRender_VK`.
- Vulkan 1.3 core (dynamic rendering, synchronization2, timeline semaphores) plus
  `VK_EXT_shader_object`, `VK_EXT_vertex_input_dynamic_state`, `VK_EXT_extended_dynamic_state3`:
  shaders bind per stage and all fixed-function state is dynamic, mirroring D3D11/RCache, so no
  pipeline permutations. Shader binaries cached via `vkGetShaderBinaryDataEXT`.
- Descriptors: `VK_KHR_push_descriptor` first (binding = D3D register + per-class shift, one set per
  stage); bindless texture table later (performance track).
- Memory: VMA + `VK_EXT_memory_budget` (4 GB VRAM).
- Shaders: DXC in-process (`-spirv -fvk-use-dx-layout -Zpr -HV 2018`, register shifts), source rules
  from tools/hlsl_rewrite.py ported to C++, reflection with SPIRV-Reflect (pinned).

## Consequences
- Requires drivers with shader objects (NVIDIA 620.70 and Mesa RADV 26.2 on the target laptop have
  all three extensions). No pipeline-based fallback for now; adding one later means a pipeline cache
  keyed by RCache state.
- Y flip via negative viewport height; shaders unchanged.

## Refinement 2026-11-24 (during M1)
The shared layer reaches into `xrRenderDX10` directly (StateManager, dx10BufferUtils,
dx10ConstantBuffer, state caches); that code is D3D-agnostic logic over device calls. So the VK
build compiles all of `xrRenderDX10` except `dx10HW.cpp`, and the new code is the lowest layer only:
own `xrvk::Device` / `xrvk::DeviceContext` / resource, view, shader, state and query classes that
offer exactly the ~55 device/context methods the renderer calls (inventory in PROGRESS.md), named
as the D3D11 types they stand for, implemented on Vulkan (VMA, shader objects, automatic
layout/barrier tracking for the limited X-Ray usage). Not COM, no D3D11/dxvk code. `vkHW.cpp`
replaces `dx10HW.cpp` (instance, NVIDIA selection, SDL3 surface, swapchain).
