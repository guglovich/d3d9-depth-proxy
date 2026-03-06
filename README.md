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

`Application → d3d9.dll (proxy) → Real D3D9 backend (System / DXVK / Gallium Nine) → GPU`

Two proxy classes are implemented:

- `D3D9Proxy` — wraps `IDirect3D9`
- `D3D9DeviceProxy` — wraps the full `IDirect3DDevice9` interface

## Backend Loading

This allows one build to work on:

- Native Windows
- Wine + DXVK
- Wine + Gallium Nine

## Capabilities

| Feature                   | Windows | DXVK | Gallium Nine |
|--------------------------|---------|------|--------------|
| Transparent passthrough  | ✅      | ✅   | ✅           |
| Render logging           | ✅      | ✅   | ✅           |
| Depth buffer capture     | ✅      | ❌   | ✅           |
| Color render target read | ✅      | ✅   | ✅           |
| Depth-based post effects | ✅      | ❌   | ✅           |

DXVK blocks depth `StretchRect` by design due to Vulkan translation constraints.

---

## Installation

Place the proxy `d3d9.dll` next to the game executable.

**Wine + DXVK:**
```
Rename DXVK:   d3d9.dll → d3d9_dxvk.dll
Place proxy:   d3d9.dll  (this project)
```

**Wine + Gallium Nine:**
```
Place proxy:   d3d9.dll  (this project)
Wine will use Gallium Nine automatically as the backend.
```

**Native Windows:**
```
Place proxy:   d3d9.dll  (this project)
The proxy loads C:\Windows\System32\d3d9.dll automatically.
```

---

## Integration Point

Post-processing hooks into the `Present()` method:

```cpp
HRESULT STDMETHODCALLTYPE Present(...) override {
    // When depth_captured == true, depth_tex holds the current frame's depth buffer.
    // Available on: native Windows, Gallium Nine.
    // Not available on: DXVK.

    return real->Present(a, b, c, d);
}
```

Depth capture happens automatically in `SetRenderTarget` when the offscreen → backbuffer transition is detected. At that moment depth contains world geometry and UI has not yet been drawn.

---

## Build

### Linux (CMake + MinGW cross-compilation)

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw32.cmake
make
```

### Linux (single command)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll src/d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

### Windows (CMake — Visual Studio)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32
cmake --build . --config Release
```

### Windows (MSVC — manual)

1. Open Visual Studio 2019 or 2022
2. Create a new **DLL** project
3. Add `src/d3d9_proxy.cpp`
4. Set platform to **x86 (Win32)**
5. Project Properties → Linker → Input → Additional Dependencies:

```
d3d9.lib
dxguid.lib
```

6. Build in **Release x86**

### Windows (MinGW)

```bash
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared \
    -o d3d9.dll src/d3d9_proxy.cpp \
    -ld3d9 -ldxguid \
    -static-libgcc -static-libstdc++ \
    -Wl,--kill-at
```

---

*Originally prototyped using AI-assisted development tools.*
