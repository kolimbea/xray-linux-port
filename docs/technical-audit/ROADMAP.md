# Roadmap

Effort = one engineer-equivalent (sessions of focused work); ranges, not promises. Every item ends
with a test and a measurement where it can be measured (BENCHMARK_PLAN.md).

## Phase 1 — critical fixes (≈1-2 weeks)
| # | Task | Components | Effort | Risk | Done when | Tests |
|---|---|---|---|---|---|---|
| 1.1 | Crash diagnostics: per-run log names (keep last N), crash file with native stack + last log lines + device info from signal handler, `fail()` and xrDebug | Common/linux/xrDebug_linux.cpp, xrCore log, vkCore `fail` | 2-3 d | low | a forced crash (debug console command) leaves a readable report | unit test for rotation; manual crash test |
| 1.2 | Find the two unexplained SIGABRTs (2026-11-26) with 1.1 in place; long play sessions | engine | open-ended | medium | 2 h session without crash on the Bar→Garbage route | soak test |
| 1.3 | Lua `require` on Linux: searcher mapping module names and `\\`/`.` paths through the engine FS | xrServerEntities script_storage / luabind setup | 1 d | low | Useful Idiots loads; no "Failed to load script" in the log | script fixture test with a module in a subfolder |
| 1.4 | Clean shutdown on SIGTERM and menu Quit within 5 s | xrEngine, backend teardown | 1-2 d | medium | both paths exit 0, no abort | automated start→quit test |
| 1.5 | Gamma/brightness/contrast through a Present-time LUT (replaces DXGI gamma ramp) | xrRenderVK present | 1 d | low | slider changes the screenshot histogram | pixel test in vk_draw |
| 1.6 | CI: build + `vk_*` tests on lavapipe; CTest registration of compat tests | CMake, .github | 2 d | low | green CI on push | CI itself |
| 1.7 | Test on AMD (RADV iGPU here, Steam Deck if available) and Intel ANV | — | 1 d | medium (unknown bugs) | Bar loads on RADV | screenshots + stats |
| 1.8 | SSFX AO: compare with Windows/Proton; decide shader fix (mod-side) vs. FXC-compatible behaviour | shader compat rules | 1 d | low | documented decision; AO visible if FXC renders it | screenshot diff |

## Phase 2 — highest return (≈2-3 weeks)
| # | Task | Effort | Done when | Measure |
|---|---|---|---|---|
| 2.1 | Swapchain at window size in fullscreen + **FSR 1** (EASU+RCAS) for render < window; console vars | 2-3 d | 1440p window rendered at 1080p looks sharper than bilinear | GPU ms at 1440p native vs 1080p+FSR |
| 2.2 | Level-entry hitches: prewarm shaders of the level, convert staging textures progressively instead of on first draw | 3-5 d | p99 of the first 600 in-level frames < 100 ms | first-window p99 |
| 2.3 | Gamepad via SDL3 → XInput state | 1-2 d | menu + gameplay with a controller | manual + input unit test |
| 2.4 | ThinLTO release preset (+ `-march=x86-64-v2` option) | 1 d | builds, tests pass | ms/frame CPU-bound scene |
| 2.5 | Tracy integration (optional build flag) + engine zones on scheduler/AI/render | 2 d | captures show per-subsystem time | — |
| 2.6 | Benchmark harness in repo (tools/bench.sh, fixed save, user_bench.ltx) | 0.5 d | repeatable medians | itself |

## Phase 3 — architectural modernisation (≈1-2 months)
- 3.1 Present module (vkPresent) with upscaler interface; present_wait pacing; VRR/tearing; frame limiter in the backend.
- 3.2 Tracked barriers (read/write sets per resource) replacing ALL→ALL per scope — **only if** a GPU
  capture shows barrier stalls > ~5 % of GPU time.
- 3.3 Device-lost / OOM handling: save-and-exit with a report instead of `abort()`.
- 3.4 Packaging: relocatable build + launcher that builds the MO2 overlay for any GAMMA install
  (generalised `tools/make_fixture_gamma.py`); decide AppImage vs Flatpak vs Steam Linux Runtime
  after a distro test matrix.
- 3.5 CPU: act on Tracy data — candidates: parallel visibility/culling in R4, A-Life update batching,
  LuaJIT GC tuning. Each change keeps saves deterministic.

## Phase 4 — new capabilities (≈2-3 months)
- 4.1 Temporal foundation: projection jitter, motion vectors for static/skinned/particle geometry,
  reactive mask for transparency, UI after upscale.
- 4.2 FSR 2.2 on that foundation (portable MIT code).
- 4.3 HDR10 output on Wayland (engine HDR10 path + swapchain colourspace + metadata).

## Phase 5 — complex / vendor technologies
- 5.1 DLSS SR plug-in (RTX) on the 4.1 inputs; licence review.
- 5.2 Render thread / parallel command recording — only with Tracy evidence that the main thread is
  the limit after phase 3.5.

## Phase 6 — experimental
- Frame generation (FSR 3 FG) once base frame rates exceed ~60 fps on mid CPUs.
- Khronos shader-object layer as the path for drivers without `VK_EXT_shader_object`; measure cost.
- PGO from recorded sessions.

## Phase 7 — not worth doing (see COST_BENEFIT.md)
Native renderer rewrite; XeSS (no Linux SDK); FSR 4; NIS; mesh shaders / ray tracing / async compute;
descriptor buffers; multiplayer; ReShade port.
