# xray-linux-port

A native Linux port of the X-Ray Monolith engine (the engine of S.T.A.L.K.E.R. Anomaly and the GAMMA
modpack) with its own Vulkan 1.3 renderer — no Wine, Proton or DXVK at run time.

**Status: experimental.** On the developer's machine (Fedora 44, NVIDIA GTX 1650 Ti, driver 620.70)
the main menu and a full GAMMA level (Bar, from a save) run with NPCs, lighting, HUD and in-game
screenshots, for 20+ minute sessions. Expect missing effects, crashes and settings that have never been
tried. Other GPUs and drivers have not been tested in the game.

## Install (players)

Download the AppImage, .deb, tar.gz or Flatpak from
[Releases](https://github.com/kolimbea/xray-linux-port/releases) (Arch: AUR `xray-native-bin`), start
the launcher, point it at your GAMMA folder and press Play. Step by step: [docs/INSTALL.md](docs/INSTALL.md)
· по-русски: [docs/INSTALL.ru.md](docs/INSTALL.ru.md).

Packages are built with `tools/packaging/` (Debian 13 container, glibc 2.41 baseline, bundled
libraries): `build_portable.sh`, `make_packages.sh <tag>`, `store_files.sh <tag>`.

## How it works

- The engine (`xray-monolith` fork, branch `stage-2-vk`) is built with clang on Linux. Its Windows APIs
  are replaced by small Linux shims; ~99 % of the engine's original lines are unchanged.
- The D3D11 renderer (R4) compiles unchanged against stand-in D3D11/DXGI classes implemented on Vulkan
  (`src/Layers/xrRenderVK`): shader objects, fully dynamic state, push descriptors, VMA, a timeline
  semaphore. HLSL is compiled in-process to SPIR-V with DXC. See
  [docs/adr/0001](docs/adr/0001-vulkan-backend-shader-objects.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- Game data is never modified: GAMMA's Mod Organizer 2 setup is reproduced as a symlink overlay
  (`tools/make_fixture_gamma.py`) and the install is mounted read-only (`bwrap`).

## Repositories

| | |
|---|---|
| this repository | docs, plan and progress log, tests, tools |
| [kolimbea/xray-monolith-linux](https://github.com/kolimbea/xray-monolith-linux/tree/stage-2-vk), branch `stage-2-vk` | the engine with the Linux port and the Vulkan renderer |

## Building (Linux)

Requirements: clang, CMake, Ninja, Vulkan headers/loader, SDL3, oneTBB, OpenAL, libvorbis/libogg,
libtheora, libjpeg, ICU, OpenSSL (development packages). The GPU needs Vulkan 1.3 with
`VK_EXT_shader_object`, `VK_EXT_extended_dynamic_state3`, `VK_EXT_vertex_input_dynamic_state` and
`VK_KHR_push_descriptor`.

```sh
git clone https://github.com/kolimbea/xray-linux-port
cd xray-linux-port
git clone -b stage-2-vk https://github.com/kolimbea/xray-monolith-linux monolith
tools/fetch_deps.sh            # pinned, checksummed third-party pieces (DXC, ...) into build/deps
cmake -S monolith -B build/monolith -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DXR_THINLTO=ON
ninja -C build/monolith xray_vk vk_smoke vk_draw vk_heap
build/monolith/vk_draw         # backend tests; XR_VK_VALIDATION=1 enables the validation layer
# XR_THINLTO (~4.5 % faster frames) needs clang's LLVMgold plugin (bfd-plugins) or lld; drop it otherwise
```

## Running with GAMMA

You need your own installed copy of Anomaly + GAMMA; no game data is in these repositories.

```sh
tools/make_fixture_gamma.py ~/Games/gamma build/fixture-gamma   # overlay of the MO2 profile
tools/play_native.sh                                             # starts the game on it
```

Saves, logs and screenshots go to `build/fixture-gamma/appdata`.

## Licence

- The engine repository (including the Vulkan renderer in `src/Layers/xrRenderVK`) is GSC Game World
  code under the terms in its `License.txt`: redistribution with the notice, **non-commercial use only**.
- The files of this repository (docs, tests, tools) are published without an open-source licence: you
  may read them here; any other use needs the author's permission.
- Third-party components keep their own licences.

Progress and open issues: [STATUS.md](STATUS.md), [docs/PROGRESS.md](docs/PROGRESS.md).
