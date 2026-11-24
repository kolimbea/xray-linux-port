> **URGENT MESSAGE FROM THE USER (Kolimbea), relayed 2026-11-24 08:13 — please do this now, it is a small task.**
> The user is away and fears the usage limit will run out and the work will stop until they are back. Set up an automatic resume:
> 1. Write `tools/auto_resume.sh` + a **user** systemd timer (`systemctl --user`, no sudo) that fires **today at 11:00 local time** (Europe/Paris), and if that run fails because the limit is still active, retries every 30 min (give up after ~6 h).
> 2. It resumes THIS conversation in the project dir (`claude --continue`) with a prompt like: "Limit reset — continue the plan from docs/PROGRESS.md / STATUS.md." Run it inside a `tmux` session (e.g. `xray-port`) so the user can attach later; log to `logs/auto_resume.log`.
> 3. Do not start a second copy if a `claude` process for this project is still actively working; if the current interactive session is just waiting on the limit, make the new run take over cleanly (no two sessions editing the same tree).
> 4. Use the same permission settings this session already has — do not widen them (no --dangerously-skip-permissions unless already in use).
> 5. Test it with a dry run (timer listed in `systemctl --user list-timers`, script runs with a harmless prompt or `--help`), note it in PROGRESS.md, then remove this note and continue your work.

> **MESSAGE FROM THE USER (Kolimbea), relayed 2026-11-24 07:57 from their Claude chat — they are away from the PC.**
> Request: as far as possible, improve the game's performance/optimization, and make it use **all CPU cores** well.
> - Do not derail the current stage: finish getting the game running first; correctness and GAMMA behaviour come first.
> - Add a "Performance & multithreading" track to PLAN.md (when it fits: after the first frame, and in the own Vulkan renderer).
>   Ideas to evaluate: oneTBB task-based jobs; parallel visibility/culling and sector traversal; async resource/texture
>   streaming and shader cache; parallel A-Life/AI and physics updates where data allows; multithreaded Vulkan command
>   recording in xrRender_VK. Lua/luabind is not thread-safe — keep script calls on the main thread. Keep saves and
>   gameplay deterministic as on Windows.
> - Measure before/after (Tracy or similar, frametime + per-core load). Target CPU: Ryzen 5 3550H, 4 cores / 8 threads.
> - Once recorded in PLAN.md/PROGRESS.md, remove this note from STATUS.md.

> **SECOND MESSAGE FROM THE USER (Kolimbea), relayed 2026-11-24 08:01** — suggestions from their Claude chat reviewing the project, sent with the user's approval. Weigh them; adopt what makes sense and say in PROGRESS.md what you took and why.
> Process:
> 1. De-risk shaders now: before linking, run all ~300 GAMMA shader files through vkd3d (HLSL -> DXBC + reflection) offline. If coverage is bad, better to know today.
> 2. Behaviour regression tests: once it boots — load the user's save, tick A-Life headless for a few minutes, compare against the Windows build (Proton). The thousands of mechanical edits have no gameplay test yet.
> 3. STATUS.md still says "PAUSED" — update it so a restarted session isn't misled.
> 4. Keep mechanical edits (case, this->, typename) in separate commits from semantic ones, to keep future Monolith/GAMMA rebases manageable.
> Optimization:
> 5. Two configs: Debug (-O0, for crashes) and Release (-O2, ThinLTO, -march=znver1 for the Ryzen 5 3550H). Later PGO on a real play session.
> 6. In GAMMA the big CPU cost is Lua scripts, not only the engine: profile script callbacks and LuaJIT GC; evaluate LuaJIT 2.1 (check mod compatibility).
> 7. Frame pacing over average FPS: measure 1% lows/stutter. Disk pipeline cache, shader prewarm on load, async texture/resource streaming.
> 8. VRAM: GTX 1650 Ti has 4 GB; GAMMA HD textures can exceed it — texture streaming / budget control.
> 9. Hybrid laptop GPU: make sure it runs on NVIDIA (not Vega) and frame presentation doesn't stall on PRIME.
> - Once recorded in PLAN.md/PROGRESS.md, remove this note (and the one above) from STATUS.md.

# xray-linux-port — status

Goal: native Linux + Vulkan build of X-Ray Monolith (Anomaly/GAMMA engine).
Plan: [docs/PLAN.md](docs/PLAN.md) · Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) ·
Log: [docs/PROGRESS.md](docs/PROGRESS.md).

- Stage 0 done. Stage 1a done: `libxrCore.so` builds with clang and mounts the Anomaly archives.
  Stage 1b built. Stage 1c: all modules except xrGame compile; xrGame in progress.
  PAUSED 2026-11-23 — resume from the "Paused" section at the end of docs/PROGRESS.md. Engine work: `monolith/` branch `stage-1-linux-build`.
- `monolith/` and `openxray/` are upstream clones, excluded from this docs repo.
- GAMMA install `~/Games/gamma` is read-only for this project.
