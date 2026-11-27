# Target architecture

Principle: **evolve, do not rewrite.** The D3D11-on-Vulkan seam (ADR 0001) is the project's main
asset: it keeps upstream and every GAMMA HLSL mod working. The target adds layers *around* it.

```
                        ┌──────────────────────── xray_vk ─────────────────────────┐
 Game / A-Life / Lua    │ xrGame · xrEngine (main thread)                           │
   + Linux Lua searcher │   └─ scripts: package.searchers → engine FS (require fix) │
   + SDL gamepad→XInput │                                                           │
                        │ R4 renderer source (unchanged)                            │
                        │   + temporal inputs (phase 4): jitter in projection,      │
                        │     motion-vector target written by geometry shaders,     │
                        │     UI pass marked                                        │
                        ├───────────────────────────────────────────────────────────┤
 Backend xrRenderVK     │ D3D11/DXGI stand-ins (KEEP)                               │
                        │ ├─ Context: state tracking, skip-redundant (done),        │
                        │ │   tracked barriers instead of ALL→ALL (IMPROVE, measured)│
                        │ ├─ Core: frames in parts, timeline, upload queue (KEEP)   │
                        │ ├─ Heap/VMA/staging (KEEP)                                │
                        │ ├─ Present pipeline (NEW module vkPresent):               │
                        │ │    gamma/brightness LUT → upscaler (FSR 1 | FSR 2 |      │
                        │ │    DLSS plug-in) → RCAS → swapchain; HDR10 colourspace; │
                        │ │    present_wait pacing, VRR/tearing                      │
                        │ └─ Diagnostics: device-lost handler, Vulkan error → crash │
                        │      report with stack, frame stats, Tracy zones          │
                        └───────────────────────────────────────────────────────────┘
 Platform               Common/linux shims (KEEP) · SDL3 · OpenAL Soft · system or bundled libs
 Delivery               CMake presets (Release+ThinLTO) · CTest · CI (build + lavapipe tests)
                        · relocatable package (runtime TBD) · launcher that builds the MO2 overlay
```

## Module boundaries
- **Engine ↔ backend**: D3D11/DXGI interfaces only. New engine-side needs (motion vectors, jitter,
  UI marker) go through the engine's own renderer code (R4 pipeline), not through backdoors into
  xrRenderVK — except one narrow, documented extension interface for the upscaler (inputs + jitter
  sequence), similar to how engines talk to FSR/DLSS.
- **Present pipeline** becomes its own module (today Present is part of `vkDXGI.cpp`): input = back
  buffer (+ depth/MV when temporal), output = swapchain image. Upscalers are strategies behind one
  interface: `none | fsr1 | fsr2 | dlss`.
- **Diagnostics** is cross-cutting: one crash path (signal handler, `fail()`, xrDebug) writes one
  report: log tail, native stack, Vulkan device info, last frame stats.

## Runtime configuration
Console variables (saved in user.ltx) instead of environment variables for anything a player should
change: `r__vk_upscaler`, `r__vk_render_scale`, `r__vk_sharpness`, `r__vk_hdr`, `r__vk_vrr`.
Environment variables stay for developer switches (validation, screenshots, GPU choice).

## Threading target
- Keep one immediate context (D3D11 semantics). Measure before splitting.
- First CPU wins come from the engine (profiling with Tracy), not from the backend: the backend is
  ~2 ms of a 23.5 ms frame at 720p.
- Optional later: a render thread that records the backend command buffers while the main thread
  runs the next frame's game logic — only with a measured benefit and determinism kept.

## Compatibility target
Vulkan 1.3 + current extension set (NVIDIA, RADV incl. Steam Deck, ANV Gen9+, NVK). Other drivers via
the Khronos shader-object layer if its overhead is acceptable (to be measured).
