[🇷🇺 Русская версия](README_RU.md)

# D3D9 Depth Proxy

A proxy implementation of `d3d9.dll` for Direct3D 9 applications.

The module wraps the `IDirect3D9` and `IDirect3DDevice9` interfaces, intercepting calls between the application and the real D3D9 backend.  
Application behavior remains unchanged — the proxy acts as a transparent intermediate layer.

## Project Goals

- D3D9 render research  
- Graphics call logging  
- Post-processing integration  
- Depth buffer access  
- Graphics pipeline analysis  

## Architecture

The proxy acts as a transparent intermediate layer:

`Application → d3d9.dll (proxy) → Real D3D9 backend (System / DXVK / Gallium Nine) → GPU`

Two proxy classes are implemented:

- `D3D9Proxy` — wraps `IDirect3D9`
- `D3D9DeviceProxy` — wraps the full `IDirect3DDevice9` interface

## Backend Loading

When `Direct3DCreate9` is called:

1. Attempts to load `d3d9_dxvk.dll` (Wine + DXVK)
2. Falls back to system `d3d9.dll`

This allows one build to work on:

- Native Windows  
- Wine + DXVK  
- Wine + Gallium Nine  

## Limitations

| Feature                   | Windows | DXVK | Gallium Nine |
|--------------------------|---------|------|--------------|
| Transparent passthrough  | ✅      | ✅   | ✅           |
| Render logging           | ✅      | ✅   | ✅           |
| Depth buffer capture     | ✅      | ❌   | ✅           |
| Color render target read | ✅      | ✅   | ✅           |
| Depth-based post effects | ✅      | ❌   | ✅           |

DXVK blocks depth `StretchRect` by design due to Vulkan translation constraints.

---

## Build

### Linux (MinGW → Windows DLL)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

### Windows (MSVC — Visual Studio)

1. Open Visual Studio 2019 or 2022  
2. Create a new **DLL** project  
3. Add `d3d9_proxy.cpp`  
4. Set platform to **x86 (Win32)**  
5. Project Properties → Linker → Input  
6. Add to *Additional Dependencies*:

```
d3d9.lib
dxguid.lib
```

7. Build in **Release x86**

### Windows (CMake — Visual Studio)

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

### Windows (MinGW)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```