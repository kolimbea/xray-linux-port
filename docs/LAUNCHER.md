# Launcher and releases (design, 2026-11-27)

Goal: players install and update the native build without building it. Decided with the user:
a standalone, good-looking launcher; public releases on GitHub (pre-release while alpha).

## Release package (tools/package.sh)
`xray-native-<version>-linux-x86_64.tar.zst`:
```
xray-native/
  bin/xray_vk            game (Release, ThinLTO)
  bin/xray-launcher      launcher
  lib/libxrCore.so  lib/libdxcompiler.so   (found through LD_LIBRARY_PATH set by the launcher)
  share/licenses/        GSC License.txt, FidelityFX FSR 1 (MIT), Dear ImGui (MIT), DXC (LLVM/NCSA)
  VERSION
```
Published as a GitHub release of kolimbea/xray-linux-port with a `.sha256` file next to it.
System libraries stay system ones (SDL3, Vulkan loader, OpenAL, TBB, ICU, OpenSSL, vorbis, theora,
jpeg, gif): the first releases target current distributions (built on Fedora 44); a Steam Runtime
container build is the later answer for older ones.

## Launcher (tools/launcher, C++20, SDL3 + SDL_Renderer + Dear ImGui 1.91.8)
- No new dependencies for players: SDL3 is already needed by the game; ImGui is compiled in (its SDL3
  backends are fetched pinned by tools/fetch_deps.sh).
- Installs to `~/.local/share/xray-native/`: `versions/<tag>/` (unpacked releases, previous one kept
  for rollback), `current` symlink, `game/` (the MO2 overlay with the player's appdata: saves,
  settings, logs), `launcher.ini` (GAMMA path, chosen version).
- Update: GitHub API `releases` (curl CLI) → newest tag vs installed → download asset with progress →
  sha256 check (OpenSSL EVP, already a game dependency) → unpack (tar CLI) → switch `current`.
- First run: pick the GAMMA folder (contains `anomaly/` and `gamma/profiles/G.A.M.M.A/modlist.txt`);
  the overlay is built as in tools/make_fixture_gamma.py (symlinks to the winning file of each path,
  lower-case, copies of writable files, appdata copied once and never overwritten afterwards).
  Rebuild the overlay when modlist.txt changed.
- Play: starts `bin/xray_vk -nointro` in `game/` with LD_LIBRARY_PATH=<version>/lib; shows the last
  crash report (logs/crash_*.log) if the game ended with one.
- Screens: Play (big button, version, update badge), Updates (changelog from the release notes,
  install / roll back), Settings (GAMMA folder, open logs folder).
