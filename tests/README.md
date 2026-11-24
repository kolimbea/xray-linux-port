# Tests

- `compat_*.cpp` — standalone unit tests of the Linux compat layer (`monolith/src/Common/linux`).
  `@SCRATCH@` marks a temporary directory the test expects to exist (see each file's fixtures).
- `xrcore_smoke.cpp` — links `libxrCore.so`, mounts a real Anomaly tree through `fsgame.ltx`,
  parses `system.ltx`. Run it from a directory holding `fsgame.ltx`, `appdata/`, and symlinks
  `db` and `gamedata` to the (read-only) Anomaly install. Set `XR_NO_MSGBOX=1`.

Last results (2026-11-23): all compat tests pass; smoke test mounts 68 archives / 51099 files in
0.43 s and parses 11871 LTX sections.
- `compat_dds.cpp` — DDS codec; pass a list of .dds paths to also parse real files (read-only):
  `clang++ -std=c++20 -Imonolith/src/Common/linux tests/compat_dds.cpp monolith/src/Common/linux/dds_codec.cpp`
- `compat_d3dx9.cpp` — D3DX9 math/declarations (`d3dx9_linux.cpp`); build with the engine include
  paths (see the d3dx9_linux.cpp entry in build/monolith/compile_commands.json).
- `compat_sha0.cpp` — SHA-0 known answers: `clang++ -I"monolith/src/3rd party/crypto" tests/compat_sha0.cpp`

Results 2026-11-24: compat_dds (15618/15626 GAMMA DDS parse; the 8 others are PNG files), compat_d3dx9
and compat_sha0 pass.
