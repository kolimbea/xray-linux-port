# X-Ray Monolith → native Linux: feasibility study

Date: 2026-11-22. Sources analysed (shallow clones, `HEAD` of default branch):

- `monolith/` — themrdemonized/xray-monolith (engine used by Anomaly/GAMMA), last push 2026-11-21
- `openxray/` — OpenXRay/xray-16 (X-Ray already ported to Linux), last push 2026-11-16

All numbers below were measured with `find`/`grep`/`diff -w` over `src/` (excluding `src/3rd party`).
They are heuristics (pattern counts per file), not exact dependency graphs.

## 1. Size

| | files (.cpp/.h/...) | lines |
|---|---|---|
| monolith/src (incl. 3rd party) | 5591 | 1.38 M |
| openxray/src | 4549 | 0.77 M |

Largest Monolith modules (own code): xrGame 457k, Layers/xrRender 62k, xrEngine 53k, xrCore 46k,
xrServerEntities 36k, xrPhysics 32k, xrRenderPC_R4 22k, xrCDB 20k.

## 2. Option A — rebase Monolith onto OpenXRay: **not viable**

Share of differing lines in files that exist in both trees (`diff -w`, changed lines / Monolith lines):

| module | common files | Monolith-only files | changed / total lines |
|---|---|---|---|
| xrGame | 2229 | 481 | 182k / 395k |
| xrEngine | 131 | 56 | 24.6k / 29.8k |
| xrCore | 89 | 84 | 17.5k / 21.5k |
| Layers/xrRender | 184 | 58 | 30.7k / 44.4k |
| xrRenderPC_R4 | 10 | 103 | — (almost fully separate) |
| xrPhysics | 145 | 5 | 11.7k / 31.4k (closest) |

The trees diverged too far: merging would amount to re-implementing every Monolith/Anomaly
change by hand, and would have to be redone on each Monolith update.
OpenXRay remains valuable as a **reference** (its `src/Common/Platform*.inl`, SDL input
mapping, CMake layout, GL renderer and case-insensitive FS work).

## 3. Option B — port Monolith in place: **viable, staged**

### Windows-specific surface (files matching each pattern group)

| module | files | Win32 API | Direct3D | DirectInput | MSVC-isms | inline asm/SSE |
|---|---|---|---|---|---|---|
| xrCore | 173 | 32 | 2 | 0 | 52 | 12 |
| xrEngine | 187 | 22 | 4 | 11 | 21 | 1 |
| xrGame | 2710 | 36 | 0 | 47 | 67 | 2 |
| xrServerEntities | 164 | 5 | 0 | 0 | 12 | 0 |
| xrPhysics / xrCDB / xrSound / xrParticles | — | ≤3 | 0 | 0 | ≤8 | ≤4 |
| Layers/xrRender | 242 | 5 | 48 | 0 | 16 | 3 |
| xrRenderDX10 | 55 | 1 | 32 | 0 | 1 | 0 |
| xrRenderPC_R4 | 113 | 4 | 22 | 0 | 4 | 1 |

Key observation: the gameplay code (xrGame, 457k lines) has only ~36 files touching Win32 and
DirectInput use is almost entirely `DIK_*` key constants (947 uses, 6 uses of
`DIDEVICEOBJECTDATA`) — a keycode table, not an API dependency. The Windows surface is
concentrated in xrCore/xrEngine and the renderer.

### Renderer strategy: keep D3D11, use DXVK-native

GAMMA ships ~297 shader files in its mods (HLSL for the DX11 path, e.g. screen-space shaders).
Porting the renderer to GL/Vulkan would mean rewriting all of them and every GAMMA shader
update afterwards. Instead:

- **dxvk-native** (doitsujin/dxvk, release v3.1.1 ships `dxvk-native-*.tar.gz`) provides the
  D3D11 API natively on Linux over Vulkan. The R4 renderer and GAMMA HLSL can stay as is.
- **Runtime shader compilation** is the main risk: R3/R4 call `D3DCompile` and `D3DReflect`.
  These are not part of DXVK. Options, to be evaluated:
  1. vkd3d-shader's HLSL→DXBC compiler (maturity for the SM5 features GAMMA uses: unverified);
  2. precompile a DXBC shader cache with the real `d3dcompiler_47` under Wine (already present
     in the GAMMA prefix) and load it natively; reflection then needs a DXBC parser
     (vkd3d-shader may also cover DXBC reflection — verify).
- **D3DX** is used in 39 files, mostly math types (`D3DXVECTOR3/MATRIX/PLANE`, matrix ops) plus
  a few texture/vertex-decl helpers. Replace math with a small own header or DirectXMath (MIT,
  header-only, builds on Linux); replace `D3DXCreateTexture`/decl helpers with own code.

### Other work items

| item | notes |
|---|---|
| Build system | Monolith has 39 `.vcxproj`, no CMake. New CMake tree needed (OpenXRay's as template). |
| Platform layer | Win32 threads/timers/files/dlopen → POSIX/SDL3; OpenXRay's `Platform*.inl` is a reference. |
| MSVC-isms | `__declspec`, `__forceinline`, `__int64`, `#pragma comment`, SEH `__try`; build with clang. |
| Window + input | SDL3; map SDL scancodes → `DIK_*` table (OpenXRay did the same). |
| Filesystem | backslash paths, **case sensitivity** of `gamedata` lookups — needs case-insensitive resolution in xrCore FS; a known pain point. |
| Sound | already OpenAL (3rd party `OpenAL-new`) — portable. |
| 3rd party | portable: luajit, luabind, ode, tbb, imgui, OpenAL, giflib, robin_hood, fastdelegate. Drop/replace: discord, reshade, stackwalker, DXERR, optick (optional), cximage/NVTT (check use). |
| Mod Organizer 2 | not needed natively: flatten enabled mods by MO2 load order into one tree, or mount with overlayfs. |

## 4. Licence

`License.txt` (both repos): GSC Game World proprietary code; redistribution of source/binaries
allowed with the notice retained, **non-commercial only**. A free, non-commercial Linux port
is compatible with these terms.

## 5. Estimate and risks

Order-of-magnitude only; no code has been compiled yet.

| stage | scope | rough effort |
|---|---|---|
| M0 | CMake + clang, xrCore compiles on Linux | 1–2 weeks |
| M1 | platform layer, xrCore/xrEngine/xrCDB/xrPhysics/xrSound link | 3–6 weeks |
| M2 | dxvk-native R4 renderer + shader compile/reflect solution, main menu renders | 4–8 weeks |
| M3 | xrGame + scripts, load a level, case-insensitive FS, SDL input | 4–8 weeks |
| M4 | GAMMA mod tree deploy, playable, stability | open-ended |

Top risks: (1) HLSL compile/reflect on Linux (M2); (2) case-insensitive asset lookup;
(3) keeping up with upstream Monolith (rebases); (4) undefined behaviour exposed by a
different compiler (MSVC-tolerated code under clang).

Payoff vs Proton+DXVK is mainly convenience, not FPS — Proton already runs the same D3D11
path through DXVK.

## 6. Recommended next step

M0 spike: generate CMake for xrCore only, build with clang on Fedora, and record the error
classes. This gives real numbers before committing to the port.
