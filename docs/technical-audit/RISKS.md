# Risks

| # | Risk | Likelihood | Impact | Evidence / reason | Mitigation |
|---|---|---|---|---|---|
| R1 | Undiagnosed crashes in long sessions | high (2 aborts on 2026-11-26) | high | cores without stacks, log overwritten | phase 1.1 crash reports, soak tests |
| R2 | Driver coverage: shader objects missing on older Mesa / some GPUs | medium | high for reach | Mesa features.txt | document minimum versions; Khronos layer fallback; test matrix |
| R3 | GAMMA updates change scripts/shaders and break Linux-specific assumptions | high (GAMMA updates often) | medium | mods use Windows paths (`require`), shader bugs (SSFX AO) | CI shader compile of the current GAMMA tree; script loader logs as test |
| R4 | Upstream engine merges conflict with Linux fixes | medium | medium | fork of an active upstream **[A]** | keep fixes small and isolated; merge often |
| R5 | Temporal upscaling (FSR 2/DLSS) artefacts from incomplete motion vectors (particles, foliage, skinned, HUD) | high | medium | X-Ray has no MV pass today | phase 4 with per-category MVs and reactive mask; ship FSR 1 first |
| R6 | Licence issues | low-medium | high | engine under GSC non-commercial terms; DLSS SDK under NVIDIA terms | keep MIT code (FSR) in separate files with notices; review DLSS licence before shipping |
| R7 | Performance work changes game determinism (saves, A-Life) | medium | high | multithreading AI/Lua | change one subsystem at a time; save/load comparisons |
| R8 | Single maintainer / knowledge in session logs | high | medium | docs exist (ADR/PLAN/PROGRESS) | keep ADRs, CI, tests; this audit |
| R9 | VRAM budget on 4 GB GPUs with HD texture packs | medium | medium | 3.3 GB at 1440p measured | budget tracking (already logged), texture_lod guidance |
| R10 | Packaging choices fragment support (AppImage vs Flatpak vs Steam runtime) | medium | low | not tested | decide after a distro matrix; ship one |
| R11 | Shutdown hang corrupts user settings/saves when killed | medium | medium | SIGTERM >30 s | phase 1.4 |
