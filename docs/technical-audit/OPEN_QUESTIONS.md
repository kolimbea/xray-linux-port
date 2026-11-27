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
| Q10 | Cause of the level-entry hitches (texture conversion vs shader compiles vs Lua) | Tracy / pmp sampling during level entry |
| Q11 | Gain of ThinLTO/PGO on the CPU-bound scene | A/B (BENCHMARK_PLAN) |
| Q12 | Does the FidelityFX SDK (FSR 3.1) build on Linux? | try the SDK's CMake on Linux |
| Q13 | Which packaging works across target distros (AppImage, Flatpak, Steam Linux Runtime)? | distro matrix |
| Q14 | Upstream activity and merge cadence of xray-monolith | not checked in this audit (instruction: no repository history research) |
| Q15 | Is the shader-object emulation layer fast enough for drivers without the extension? | B1 on RADV with the layer forced |
| Q16 | MSAA and tessellation settings in GAMMA presets: used by players? | GAMMA default presets; test both options |
