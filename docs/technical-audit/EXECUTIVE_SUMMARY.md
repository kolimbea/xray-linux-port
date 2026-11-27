# Executive summary — technical audit 2026-11-27

Scope: the whole local project (`xray-linux-port` docs/tests/tools, `monolith/` engine on branch
`stage-2-vk`), built and tested on this machine; external technologies checked against primary
sources. No source code was changed for the audit. Details: CURRENT_STATE, ARCHITECTURE_AUDIT,
TECHNOLOGY_RESEARCH, FEATURE_MATRIX, TARGET_ARCHITECTURE, ROADMAP, COST_BENEFIT, RISKS,
BENCHMARK_PLAN, OPEN_QUESTIONS (this directory).

## What is the real state?
A **playable alpha**. The full GAMMA modpack runs natively on Linux on an own Vulkan 1.3 backend:
a Windows save loads, the Bar level renders with NPCs, lighting, shadows, HUD; the user played
sessions. Evidence: backend tests pass with 0 validation errors; 454/470 GAMMA shader files compile
to SPIR-V with 0 validation errors; no D3D11/DXGI method the renderer calls is unimplemented
(`vk_pending.gen.cpp` empty). It is **not release quality**: two unexplained aborts in the user's
sessions on 2026-11-26, a shutdown hang, one GAMMA mod (Useful Idiots) off because of Lua `require`
paths, no gamepad, gamma/brightness most likely ineffective, no packaging, no CI, tested on one
NVIDIA GPU only.

## What is done well?
- The architecture decision (ADR 0001): R4's D3D11 source unchanged, D3D11 implemented on Vulkan for
  exactly what R4 calls → ~99 % of engine lines untouched, every GAMMA HLSL mod works, upstream
  merges stay cheap.
- Backend engineering: shader objects + push descriptors, timeline semaphore, frames submitted in
  parts, linear host heap, deferred destruction, staging textures in pageable memory, frame and memory
  statistics in the log. CPU cost of the backend ≈ 2 ms of a 23.5 ms frame at 720p.
- Build reads upstream `.vcxproj` files; Windows build untouched.
- Measured, documented optimisation history (41.0 → 23.5 ms/frame at 720p) and an ADR/PLAN/PROGRESS
  trail.

## What is the main limit?
**The engine's main thread** (A-Life/AI/Lua ≈ 35 %, render traversal ≈ 50 % of samples): the game is
CPU-bound at 720p and GPU-bound only at high resolutions. Second: **no field diagnostics** — crashes
cannot be analysed after the fact. Third: **no temporal data** (motion vectors, jitter), which gates
every modern upscaler beyond FSR 1.

## What must be fixed first? (phase 1)
1. Crash reports (persistent logs + native stack + device info), then find the two aborts.
2. Lua `require` searcher (Useful Idiots).
3. Clean shutdown.
4. Gamma/brightness via a present-time LUT.
5. CI (build + lavapipe tests) and CTest.
6. Run on AMD/Intel drivers.

## Which technologies to adopt?
- **FSR 1** (MIT) with a proper native-size swapchain — now (phase 2).
- **Tracy** profiling and **ThinLTO** — now.
- **present_wait** pacing, VRR/tearing — phase 3.
- **FSR 2.2** (portable MIT, as in Godot) after building motion vectors + jitter — phase 4.
- **HDR10** (engine already has an HDR10 path) — phase 4.
- **DLSS** as an optional plug-in for RTX on the same inputs — phase 5, after licence review.
- Khronos **shader-object layer** as a fallback for drivers without the extension — experiment.

## What makes no sense?
A native renderer rewrite; XeSS (no Linux SDK); FSR 4 (RDNA 4 only); NIS (redundant with FSR 1);
frame generation at today's 30-45 fps base; mesh shaders, ray tracing, async compute, descriptor
buffers (no content or measured need); ReShade port; multiplayer.

## Keep / modernise / rewrite / replace
KEEP: engine game code, R4 renderer source, backend core, shader toolchain, compat layer, build from
`.vcxproj`. IMPROVE: backend sync and error handling, input (gamepad), Lua loader, build config.
REFACTOR: present path into its own module. ADD: diagnostics, CI/CTest, FSR 1, packaging, later
temporal upscaling and HDR. OPTIONAL/remove: legacy dxvk `xray` target, DLSS, Discord. REWRITE: none.
REPLACE: DXGI gamma ramp → shader LUT; compositor scaling → FSR.

## Target architecture
The same D3D11-on-Vulkan seam, with a separate present module (gamma LUT → upscaler strategy →
RCAS → swapchain, HDR, pacing), a cross-cutting diagnostics path, temporal inputs produced by the R4
code itself, console variables for player-facing options, and CI + packaging around it
(TARGET_ARCHITECTURE.md).

## What is realistically achievable?
- **2-4 weeks**: stable alpha for NVIDIA/AMD/Intel-Gen9+ Linux players — crash reports, fixes above,
  FSR 1, gamepad, CI; FPS at least as today (23.5 ms at 720p on this laptop; ~1.5x lower GPU cost at
  1440p with FSR 1 **[E]**).
- **2-3 months**: packaged beta with smoother level entry, pacing, measurable CPU gains from profiling.
- **4-6 months**: temporal upscaling (FSR 2, DLSS plug-in) and HDR — if the motion-vector work holds up
  on GAMMA's content.
- Not achievable without changing the engine's threading model: large CPU-bound FPS gains on 4-core
  CPUs; that is an upstream-scale project and stays a research item.
