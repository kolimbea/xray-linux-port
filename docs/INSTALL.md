# Install and launcher (native Linux build of S.T.A.L.K.E.R. GAMMA)

Alpha. Needs an installed Anomaly + GAMMA (MO2 under Wine/Proton, or stalker-gamma-cli). The native
build only reads that installation and never modifies it. Russian: [INSTALL.ru.md](INSTALL.ru.md).

## Requirements
- 64-bit Linux with glibc 2.41 or newer: Debian 13, Ubuntu 25.04, Fedora 42, Arch and newer
  (`ldd --version`).
- A Vulkan 1.3 driver with `VK_EXT_shader_object`: NVIDIA proprietary, Mesa RADV/ANV for AMD/Intel
  (`vulkaninfo --summary`).
- Every other library ships inside the package.

## Packages (from [Releases](https://github.com/kolimbea/xray-linux-port/releases))
| Format | Install / run |
|---|---|
| AppImage | `chmod +x xray-native-*-x86_64.AppImage && ./xray-native-*-x86_64.AppImage` |
| tar.gz | `tar xf xray-native-<ver>-linux-x86_64.tar.gz && ./xray-native/bin/xray-launcher` |
| Debian/Ubuntu | `sudo apt install ./xray-native_<ver>_amd64.deb`, then `xray-launcher` or the menu entry |
| Arch (AUR) | `yay -S xray-native-bin` |
| Flatpak | `flatpak install --user xray-native-<ver>.flatpak && flatpak run io.github.kolimbea.XrayNative` |

The Flatpak sees home, `/mnt`, `/media` and `/run/media` read-only; its data is in
`~/.var/app/io.github.kolimbea.XrayNative/`.

## First start
1. **Settings**: choose the GAMMA folder (the one with `anomaly/` and `gamma/`, e.g. `~/Games/gamma`).
2. Optionally **Add to applications menu**.
3. **Play**. The first start builds the game folder from your mod list the way MO2 does (order of
   `gamma/profiles/G.A.M.M.A/modlist.txt`); saves and settings are copied once and then kept apart from
   the GAMMA installation. A changed mod list is picked up before the next start.

## Updates
The launcher checks GitHub releases. **Updates** shows the changelog, installs (download, sha256 check,
unpack) and rolls back to the previous version, which is kept.

## Files
| What | Where |
|---|---|
| Game versions, game folder, launcher settings | `~/.local/share/xray-native/` |
| Saves and game settings | `~/.local/share/xray-native/game/appdata/` |
| Logs and crash reports | `~/.local/share/xray-native/game/appdata/logs/` |

## Problems
Attach the crash report the launcher shows (`crash_*.log`) and `xray_<user>.log` to an
[issue](https://github.com/kolimbea/xray-linux-port/issues).

## Known alpha limitations
Tessellation is off in the Vulkan renderer; with MSAA the SSFX passes (TAA, motion vectors, SSS) are
off (keep MSAA off); Windows ReShade does not apply — vkBasalt (`ENABLE_VKBASALT=1`) works as a Vulkan
layer.
