# Architecture of the port

```
xrGame (gameplay, Lua via luabind/luajit)       portable, DIK_* keycodes only
xrEngine (device, input, console, level loop)   Win32 window/DirectInput -> SDL3
xrRender_R4 + Layers/xrRender (D3D11)           unchanged API; linked to dxvk-native
   D3DCompile/D3DReflect                         -> vkd3d-utils (+ on-disk DXBC cache)
xrSound (OpenAL)                                 OpenAL Soft
xrCore (FS, memory, threads, log, ini)           POSIX + case-insensitive path index
```

## Module boundaries
- `src/Common/linux/`: the only place for Win32-replacement code. Engine sources include it via
  `xrCore_platform.h` under `#ifdef __linux__`; Windows build stays untouched.
- Modules are shared libraries as on Windows (`libxrCore.so`, ...), loaded with `dlopen` where the
  engine expects `LoadLibrary` (renderers, xrGame).
- Data paths: engine sees the same `fsgame.ltx` layout; the FS layer resolves `\`, case, and the
  MO2 load order (flattened tree or overlayfs built by a script under `tools/`).

## Decisions
- Port Monolith in place, not rebase onto OpenXRay (see FEASIBILITY.md §2).
- Keep D3D11 + dxvk-native for Stage 2 so GAMMA HLSL works unmodified; own Vulkan renderer only in Stage 4.
- Primary compiler clang (closest MSVC compatibility via `-fms-extensions`); gcc kept building in CI later.
