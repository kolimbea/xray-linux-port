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
- `compat_overlay_write.cpp` — writing through an overlay symlink (fopen "w"/"a") leaves the mod file
  unchanged and creates the overlay's own copy; pass an empty temporary directory.
- `compat_sha0.cpp` — SHA-0 known answers: `clang++ -I"monolith/src/3rd party/crypto" tests/compat_sha0.cpp`

Results 2026-11-24: compat_dds (15618/15626 GAMMA DDS parse; the 8 others are PNG files), compat_d3dx9
and compat_sha0 pass.

Vulkan backend (CMake targets in build/monolith):
- `vk_smoke` — M2: DXGI/D3D11 device, swap chain, clear, present, readback (`XR_VK_VALIDATION=1`).
- `vk_hlsl_rewrite` — unit test of the HLSL compat rules (`xrRenderVK/vkHlslRewrite.cpp`).
- `tools/settings_matrix.sh [variant...]` — every graphics option once on the GAMMA fixture: frame,
  "! vk" lines and crash reports in build/matrix/report.txt (2026-11-27: 20/20 pass; vk_shaders 454/470).
- `vk_draw` — M4: draw path through the D3D11 interface with pixel checks (`XR_VK_VALIDATION=1`).
- `vk_shaders <shader-root> [out-dir]` — M3: every r3 shader through D3DCompile/D3DReflect, spirv-val,
  shader objects/programs on the GPU (`XR_VK_VALIDATION=1`, `XR_VK_GPU=<n>` to pick a GPU).
- `reflect_dump_vk <shader-root>` and `tests/reflect_dump_r4.sh build/reflect_dump_r4 <shader-root>`
  — the same reflection dump from both backends; compare with `tools/reflect_compare.py`.
Results 2026-11-24: vk_smoke, vk_hlsl_rewrite pass; vk_shaders on GAMMA: 453/470 compile, 0 reflection,
spirv-val or validation failures on NVIDIA, RADV and llvmpipe; reflect_compare: no type/offset mismatch.
