# Technology research

Sources checked on 2026-11-27 (official repositories via the GitHub API, Mesa `docs/features.txt`,
local `vulkaninfo` on NVIDIA 620.70 / RADV / llvmpipe). Anything not checked against a primary source
is marked **[A]**. Target audience: every Linux player of Anomaly/GAMMA, not one GPU.

## 1. Upscaling and frame generation

| Technology | Licence / availability (verified) | GPU coverage | What the engine must provide | Verdict |
|---|---|---|---|---|
| **AMD FSR 1** (EASU + RCAS, spatial) | MIT; `GPUOpen-Effects/FidelityFX-FSR` @ a21ffb8, two headers, HLSL/GLSL; fetched and checksummed locally (sha256 b529a31d…) | every GPU | the final image only | **ADD** (phase 2). Fits the existing Present blit; ~1 day; measurable GPU win at 1440p/4K and on iGPUs/Steam Deck. Upscales the UI too unless placed before UI (later refinement). |
| NVIDIA Image Scaling (NIS) | MIT (`NVIDIAGameWorks/NVIDIAImageScaling`, last push 2022) | every GPU | final image | **SKIP** — same class as FSR 1, unmaintained since 2022; one spatial upscaler is enough. |
| **AMD FSR 2.2** (temporal) | MIT; portable Vulkan build exists in Godot (`thirdparty/amd-fsr2`, v2.2.1, 1680d1e) | every GPU | depth, **per-pixel motion vectors for all geometry**, sub-pixel projection jitter, reactive/transparency mask, UI composited after | **ADD later** (phase 4) — only after the motion-vector/jitter foundation exists. |
| AMD FSR 3.1 / FidelityFX SDK | `GPUOpen-LibrariesAndSDKs/FidelityFX-SDK`: "DirectX 12 or Vulkan"; tooling Windows-centric (prebuilt DLLs, shader compiler) **[F]**; Linux build untested **[E]** | every GPU (upscaler) | as FSR 2 | **OPTIONAL** — prefer FSR 2.2 portable path first; revisit if the SDK builds on Linux. |
| AMD FSR 4 (ML) | SDK lists FSR 4 components **[F]**; hardware limited to RDNA 4 per AMD **[A]** | RX 9000 only | as FSR 2 | **SKIP** for now (tiny audience on Linux, DX12-first). |
| **NVIDIA DLSS (SR)** | `NVIDIA/DLSS` ships `lib/Linux_x86_64` (static NGX + rel/dev libs) **[F]**; NVIDIA licence (not OSI) **[F]**; RTX-only (tensor cores) **[A, documented by NVIDIA]** | RTX 20+ | same inputs as FSR 2 | **OPTIONAL** (phase 5), plug-in on top of the FSR 2 inputs; licence review before shipping. |
| Intel XeSS | `intel/xess` `bin/` contains only Windows DLLs **[F]** | — on Linux | — | **SKIP** (no Linux SDK). |
| Frame generation (FSR 3 FG, DLSS FG) | FSR 3 FG in the SDK; DLSS FG RTX 40+ **[A]** | — | everything above + UI separation + pacing; vendors advise a high base frame rate **[A]** | **SKIP for now**: X-Ray is CPU-bound at 30-45 fps on mid CPUs; interpolation at that base adds latency and artefacts. Revisit after the CPU work. |

## 2. Vulkan platform features

| Feature | Availability (local vulkaninfo / Mesa) | Use in this project | Verdict |
|---|---|---|---|
| `VK_EXT_shader_object` (current hard requirement) | NVIDIA ✓, RADV ✓, llvmpipe ✓; Mesa: anv, nvk, radv, lvp, hk; not hasvk/tu/panvk | pipeline-free state model of the backend | **KEEP**; add fallback below |
| Khronos `VK_LAYER_KHRONOS_shader_object` | exists in `KhronosGroup/Vulkan-ExtensionLayer` (`layers/shader_object`, docs) **[F]** | emulates shader objects on drivers without them | **ADD** as documented/packaged fallback (zero code) **[E: performance]** |
| `VK_EXT_graphics_pipeline_library` | all 3 local devices; Mesa anv, radv, nvk, tu, panvk… | alternative to shader objects for drivers lacking them | **SKIP** unless the layer proves too slow — a second state model doubles maintenance |
| `VK_EXT_descriptor_buffer` | all 3 local devices | could replace push descriptors | **SKIP** — push descriptors cost ~nothing measurable now (prepare 2 ms total incl. all state) |
| `VK_KHR_present_wait` / `present_id` (2) | NVIDIA, RADV | frame pacing, latency control, accurate frame limiter | **ADD** (phase 3, small) |
| `VK_NV_low_latency2` / `VK_AMD_anti_lag` | NVIDIA / all | Reflex-like latency reduction | **OPTIONAL** after present_wait |
| HDR (`VK_EXT_swapchain_colorspace`, `VK_EXT_hdr_metadata`) | colorspace on NVIDIA; metadata on all | the engine already has an HDR10 output path (`r4_hdr10_*` console variables in user.ltx) | **ADD later** (phase 4); needs an HDR desktop (KDE/Gamescope) to test **[E]** |
| VRR / tearing (`IMMEDIATE`/`MAILBOX`) | surface-dependent | `DXGI_FEATURE_PRESENT_ALLOW_TEARING` reported FALSE (`vkDXGI.cpp:380`) | **IMPROVE** (small) |
| Async compute, mesh shaders, ray tracing | partly available | no engine content/passes that use them | **SKIP** |
| Finer barriers / image layouts | core Vulkan | replace ALL→ALL per scope by tracked read/write sets | **IMPROVE** after measuring the barrier cost **[E]** |

## 3. Engine-level technology

| Item | Finding | Verdict |
|---|---|---|
| Lua `require` / `package.path` | Windows-style paths fail on Linux (Useful Idiots mod) | **ADD** a Linux searcher that maps `a.b.c` / `\\` paths through the engine's case-insensitive FS |
| Gamepad | none upstream either (XInput code commented out in `xr_input_xinput.cpp`, `Xr_input.cpp`) | **OPTIONAL** new feature (SDL3 gamepad → engine actions); Steam Input mappings work today |
| Gamma / brightness | DXGI gamma ramp unsupported | **ADD** a shader-based gamma/brightness/contrast at Present (portable; also fixes windowed mode) |
| Multithreading | main thread does AI+Lua+render | **IMPROVE** step by step: profile with Tracy; candidates: parallel culling/visibility, render-thread split, A-Life parts — each needs determinism checks (PLAN.md performance track) |
| LTO / PGO / `-march` | not used; Windows build uses whole-program optimisation (branch name `…-wpo`) | **ADD** ThinLTO (low risk); PGO later **[E: gain]** |
| Profiling (Tracy, BSD-3, active) | `tools/pmp.sh` sampling only | **ADD** Tracy zones in engine + backend (optional build flag) |

## 4. Tooling, delivery, quality

| Item | Finding | Verdict |
|---|---|---|
| CI | none for Linux | **ADD** GitHub Actions: build + `vk_*` tests on llvmpipe (lavapipe works locally: PROGRESS.md) |
| CTest | tests built by hand | **ADD** register compat/vk tests in CMake + CTest |
| Crash diagnostics | logs overwritten, cores without stacks | **ADD** per-run log names/rotation, write the engine's own stack trace to a crash file, keep last N |
| Packaging | none; binary depends on system libs | **ADD**: Steam Linux Runtime / AppImage / Flatpak **[A: choice needs a test matrix]**; first a relocatable tarball with bundled non-system libs |
| Sanitizers | ASan build exists (`build/asan`) | **KEEP**, run periodically / in CI nightly |
| Legacy dxvk path (`xray` target) | still built by default, needs dxvk-native + vkd3d-shader | **OPTIONAL** (off by default) or remove |
