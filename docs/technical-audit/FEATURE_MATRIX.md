# Feature matrix

Status: ✅ works (verified) · 🟡 partial / unverified · ❌ missing or broken · — not applicable.
Decision codes as in COST_BENEFIT.md. Evidence in CURRENT_STATE.md.

| Area | Feature | Windows (upstream) | Linux `xray_vk` now | Decision |
|---|---|---|---|---|
| Boot | main menu, settings, console | ✅ | ✅ (~5 s warm) | KEEP |
| Saves | load Windows saves | ✅ | ✅ | KEEP |
| Saves | Linux save loads on Windows | ✅ | 🟡 untested | test (phase 1) |
| World | A-Life, AI, physics, levels | ✅ | ✅ Bar verified; other levels 🟡 | test matrix |
| Scripts | GAMMA Lua scripts | ✅ | ✅ except `require`-based mods ❌ (Useful Idiots) | **fix** (phase 1) |
| Input | keyboard / mouse | ✅ | ✅ | KEEP |
| Input | gamepad (XInput) | ✅ | ❌ | ADD (phase 2) |
| Audio | OpenAL Soft playback | ✅ | 🟡 initialises; audibility not verified | verify |
| Audio | EFX reverb | ✅ | 🟡 off in user config; untested on | verify |
| Render | R4 deferred pipeline, shadows, lights, HUD | ✅ | ✅ | KEEP |
| Render | GAMMA screen-space shaders | ✅ | ✅ except SSFX AO (shader bug, early-out) | investigate (phase 1) |
| Render | 454/470 GAMMA shader files compile | — | ✅ (16 failures, likely unused) | confirm unused |
| Render | F12 screenshots (PNG) | ✅ | ✅ | KEEP |
| Render | gamma / brightness / contrast sliders | ✅ (DXGI gamma ramp) | ❌ likely no effect | ADD shader gamma (phase 1-2) |
| Render | MSAA paths | ✅ | 🟡 not exercised in tests | test |
| Render | tessellation | ✅ (option) | ❌ patch constants unsupported | IMPROVE if used by GAMMA presets |
| Display | windowed / fullscreen | ✅ | ✅ | KEEP |
| Display | lower resolution in fullscreen | ✅ (mode switch) | 🟡 compositor bilinear scaling | replace by FSR 1 (phase 2) |
| Display | upscaling (FSR 1 / FSR 2 / DLSS) | partly via mods/ReShade | ❌ | FSR 1 ADD; temporal later |
| Display | HDR10 | ✅ (`r4_hdr10_*`) | ❌ SDR only | ADD later |
| Display | VRR / tearing | ✅ | ❌ reported unsupported | IMPROVE |
| Display | frame limiter / latency | ✅ engine limiter | 🟡 engine limiter only | present_wait (phase 3) |
| Perf | multithreaded render submission | ❌ (upstream too) | ❌ | research (phase 4) |
| Perf | shader cache on disk | ✅ | ✅ (SPIR-V blobs, 19 MB) | KEEP |
| Perf | no hitches on level entry | 🟡 | ❌ p99 2.9 s in first 600 frames | IMPROVE (phase 2) |
| Stability | crash reports | ✅ BugTrap/minidump | ❌ log overwritten, no stacks | ADD (phase 1) |
| Stability | clean shutdown | ✅ | ❌ SIGTERM hang (>30 s) | fix (phase 1) |
| Stability | GPU device-lost handling | ✅ (DXGI) | ❌ abort | IMPROVE |
| Platform | NVIDIA proprietary | — | ✅ | KEEP |
| Platform | AMD RADV (incl. Steam Deck) | — | 🟡 backend tests pass on RADV iGPU per PROGRESS; game not run on AMD | test (phase 1) |
| Platform | Intel ANV (Gen9+) | — | 🟡 extensions available per Mesa; untested | test |
| Platform | old Intel / ARM GPUs | — | ❌ no shader objects | shader-object layer (OPTIONAL) |
| Delivery | install from a package | ✅ (MO2 + exe) | ❌ build from source | ADD (phase 3) |
| Delivery | CI | ✅ Windows | ❌ | ADD (phase 1) |
| Integrations | Discord rich presence, ReShade | ✅ | ❌ stubs | SKIP (ReShade) / OPTIONAL (Discord) |
| Multiplayer | — | dormant code | dormant | SKIP |
