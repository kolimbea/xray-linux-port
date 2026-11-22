# Tests

- `compat_*.cpp` — standalone unit tests of the Linux compat layer (`monolith/src/Common/linux`).
  `@SCRATCH@` marks a temporary directory the test expects to exist (see each file's fixtures).
- `xrcore_smoke.cpp` — links `libxrCore.so`, mounts a real Anomaly tree through `fsgame.ltx`,
  parses `system.ltx`. Run it from a directory holding `fsgame.ltx`, `appdata/`, and symlinks
  `db` and `gamedata` to the (read-only) Anomaly install. Set `XR_NO_MSGBOX=1`.

Last results (2026-11-23): all compat tests pass; smoke test mounts 68 archives / 51099 files in
0.43 s and parses 11871 LTX sections.
