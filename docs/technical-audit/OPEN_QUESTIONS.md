# Open questions (need an experiment or information)

| # | Question | How to answer |
|---|---|---|
| Q1 | What caused the two SIGABRTs on 2026-11-26 (00:02, 20:25)? | crash reports (roadmap 1.1), then reproduce |
| Q2 | Is sound audible and correct (3D positioning, EFX on)? | play test with headphones; `snd_efx on` |
| Q3 | Are the 16 GAMMA shader files that fail to compile ever loaded by the game? | log every D3DCompile failure in game; run levels |
| Q4 | What does FXC/Windows render for `ssfx_ao.ps` with its non-terminating loop? | screenshot on Windows/Proton vs Linux |
| Q5 | Do the gamma/brightness sliders change anything today? | screenshot histogram with two settings |
| Q6 | Does a save made on Linux load on Windows? | round-trip test |
| Q7 | Behaviour on AMD RADV (iGPU here, Steam Deck) and Intel ANV in-game | run B1 on each |
| Q8 | Minimum Mesa / NVIDIA versions with every required extension | driver release notes; test on an older distro (container won't do: needs GPU) |
| Q9 | Cost of the ALL→ALL barriers at 1440p | GPU capture (Nsight/RenderDoc) — **neither is installed on this machine** (checked 2026-11-27); alternative: per-scope timestamp queries in the backend |
| Q10 | Cause of the level-entry hitches (texture conversion vs shader compiles vs Lua) | **answered 2026-11-27**: pmp sampling (400 samples from "Spawning artefacts" on) shows game logic — CSheduler 162/400, CAI_Stalker::shedule_Update 106, Lua pcall 121, visibility ray queries; no shader compiles, no ProcessStaging uploads. The hitches are the engine waking every NPC at once, not the renderer port |
| Q11 | Gain of ThinLTO/PGO on the CPU-bound scene | A/B (BENCHMARK_PLAN) |
| Q12 | Does the FidelityFX SDK (FSR 3.1) build on Linux? | try the SDK's CMake on Linux |
| Q13 | Which packaging works across target distros (AppImage, Flatpak, Steam Linux Runtime)? | distro matrix |
| Q14 | Upstream activity and merge cadence of xray-monolith | not checked in this audit (instruction: no repository history research) |
| Q15 | Is the shader-object emulation layer fast enough for drivers without the extension? | B1 on RADV with the layer forced |
| Q16 | MSAA and tessellation settings in GAMMA presets: used by players? | GAMMA default presets; test both options |
| Q17 | The kernel marked the TSC unstable at boot on 2026-11-26 ("TSC found unstable after boot, most likely due to broken BIOS") and switched to HPET: every clock read is a system call. How much does it cost the engine (timers per frame), and is the kernel parameter `tsc=reliable` safe on this Ryzen 5 3550H? | A/B with the kernel parameter (a system change: the user decides); 720p was 23.5 ms on TSC, 29.0 ms on HPET with the same code |
| Q18 | Full-screen GPU time in the 2026-11-27 night runs: 60 ms for both native 2560x1440 and 1080p + FSR, ~1450 draws, while windowed 720p shows 11 ms / 4400 draws and the 27th day run at 1440p showed 32.5 ms / 4384 draws. Hypothesis: with the display off (DPMS) the compositor does not consume full-screen frames and the wait lands in the measurement | repeat with the display on |
