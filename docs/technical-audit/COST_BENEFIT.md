# Cost / benefit and decisions

## 1. Decision per subsystem
| Subsystem | Decision | Reason |
|---|---|---|
| Engine game code (xrGame, A-Life, AI, physics) | **KEEP** | upstream code, works, Windows saves load; changes only for portability bugs |
| Build from upstream `.vcxproj` (`vcxproj_sources.py`) | **KEEP** | upstream file changes flow in automatically; Windows build untouched |
| Build configuration | **IMPROVE** | add Release preset with ThinLTO; CTest |
| Linux compat layer (`Common/linux`) | **KEEP** | thin, no silent stubs |
| Input (SDL3) | **KEEP** | gamepad is a new feature (none upstream), optional |
| Sound (OpenAL Soft) | **KEEP** | initialises; verify audibly |
| Lua runtime | **IMPROVE** | `require`/`package.path` searcher |
| R4 renderer source | **KEEP** | unchanged upstream code = all GAMMA shader mods work |
| Vulkan backend (D3D11 stand-ins) | **KEEP + IMPROVE** | complete for R4; improve sync, errors, present path |
| Present path in `vkDXGI.cpp` | **REFACTOR** | into a present module with gamma LUT, upscalers, pacing |
| Shader toolchain (DXC + compat rules) | **KEEP** | 454/470 GAMMA files, 0 validation errors |
| Diagnostics (logs, crash reports) | **ADD** | today field crashes cannot be analysed |
| Legacy `xray` target on dxvk-native | **OPTIONAL → remove** | extra build deps (dxvk-native, vkd3d-shader) for a path nobody runs |
| Upscaling | **ADD** FSR 1 now; FSR 2 after temporal inputs; DLSS **OPTIONAL** | see TECHNOLOGY_RESEARCH.md |
| HDR, VRR, present_wait | **ADD** (later phases) | engine already has an HDR10 path; small backend work |
| Packaging + CI | **ADD** | needed to serve players and to keep the port working |
| Multithreaded rendering | **OPTIONAL (research)** | only after profiling shows the main thread render part dominates |
| Discord rich presence | **OPTIONAL** | cosmetic |
| ReShade | **SKIP** | Windows DLL injection model; FSR/CAS cover the common use |
| XeSS, FSR 4, NIS, frame generation now | **SKIP** | no Linux SDK / tiny audience / redundant / base frame rate too low |
| Mesh shaders, ray tracing, async compute, descriptor buffers | **SKIP** | no engine content or measured need |
| Multiplayer | **SKIP** | dormant upstream |

## 2. Incremental modernisation vs. rewrite of the renderer
| | Incremental (this roadmap) | Native Vulkan renderer rewrite |
|---|---|---|
| Effort | phase 1-3 ≈ 2-3 months | 6-12+ months to parity (R4 is 62k+22k+15k lines of shared/pipeline code plus GAMMA's 470 shader files) |
| Mod compatibility | every GAMMA HLSL shader works as on Windows | each shader mod has to be ported or wrapped; GAMMA updates break it |
| Upstream merges | cheap (renderer source unchanged) | renderer diverges permanently |
| Measured gain | backend costs ~2 ms of a 23.5 ms frame; GPU ~10 ms at 720p | at most the backend's 2 ms CPU + barrier overhead (unmeasured) |
| Risk | low, each step measurable | high, long period without a playable build |
**Decision: no rewrite.** The bottleneck is the engine's main thread (AI + Lua + render traversal),
which a renderer rewrite would not remove.

## 3. Value / cost of the main candidates
| Item | Player value | Cost | Maintenance | Score |
|---|---|---|---|---|
| Crash reports | high (makes every other fix possible) | 2-3 d | low | ★★★★★ |
| `require` fix | high for GAMMA (a whole mod) | 1 d | low | ★★★★★ |
| Gamma/brightness | medium-high | 1 d | low | ★★★★ |
| CI + CTest | indirect, high | 2 d | low | ★★★★ |
| FSR 1 | high at 1440p/4K, iGPU, Deck | 2-3 d | low (frozen MIT code) | ★★★★ |
| Level-entry hitches | high | 3-5 d | medium | ★★★★ |
| Gamepad (new feature; none upstream) | medium (Deck: covered by Steam Input) | 2-4 d | low | ★★ |
| ThinLTO | small-medium **[E]** | 1 d | low | ★★★ |
| Packaging | high for reach | 1-2 w | medium | ★★★ |
| Temporal inputs + FSR 2 | high image quality | 2-3 months | medium-high | ★★ |
| DLSS | RTX owners only | +1-2 w on top of FSR 2 inputs | medium (vendor SDK) | ★★ |
| HDR10 | niche today | 1-2 w | medium | ★★ |
| Frame generation | negative at 30-45 fps base | months | high | ★ (skip now) |
