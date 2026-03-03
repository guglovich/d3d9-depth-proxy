[🇷🇺 Читать на русском](README_RU.md)

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

---

## Architecture

Application  
   
 ↓  
   
 d3d9.dll (proxy)  
   
 ↓  
   
 Real D3D9 backend (System / DXVK / Gallium Nine)  
   
 ↓  
   
 GPU  
Two proxy classes are implemented:  
- D3D9Proxy --- wraps IDirect3D9  
- D3D9DeviceProxy --- wraps IDirect3DDevice9 (full interface)  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OMQ2AABAAsSNBACP6MMH6NpGACyywEZJWQZeZ2aszAAD+4l6rrTq+ngAA8Nr1AL+6BElk4wV6AAAAAElFTkSuQmCC)  
**Backend Loading**  
When Direct3DCreate9 is called:  
1. Attempts to load d3d9_dxvk.dll (Wine + DXVK)  
2. If not found --- loads the system d3d9.dll  
The same module can be used in:  
- Windows (native D3D9)  
- Wine + DXVK  
- Wine + Gallium Nine  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNBCkJfE1pYGfHAiAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrse4dwF6o2O55YAAAAASUVORK5CYII=)  
**Features**  
**Windows (native D3D9)**  
- Transparent interception of all D3D9 calls\  
- Logging of PresentationParameters and render passes\  
- Depth buffer capture via StretchRect\  
- Support for DF24 / DF16 / INTZ formats\  
- Access to depth texture before Present()\  
- Post-processing integration point\  
- Overlay rendering on top of the frame\  
- Interception of CreateTexture and shader methods\  
- Color render target readback via GetRenderTargetData  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANElEQVR4nO3OQQmAUBBAwSf8GGLWDWFDY3ixgjcRZhLMNjNHdQYAwF9cq1rV/vUEAIDX7gcRXAQ2s/16gwAAAABJRU5ErkJggg==)  
**Wine + DXVK**  
- Full passthrough\  
- Render target switch logging\  
- Creation of compatible depth texture\  
- Color render target read access  
⚠ Depth capture via StretchRect is not available.  
Reason: DXVK translates D3D9 to Vulkan. Copying depth surfaces using  
   
 StretchRect is not implemented due to architectural constraints.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OMQ2AABAAsSNBACPq8MH2NpGACyywEZJWQZeZ2aszAAD+4l6rrTq+ngAA8Nr1AL/KBEe6dElaAAAAAElFTkSuQmCC)  
**Wine + Gallium Nine**  
- Depth capture works\  
- depth_captured == true after first geometry pass\  
- Depth texture available for shader usage\  
- DF24 (AMD) and INTZ (NVIDIA) supported  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSPBCj5fFyM6mJHAjAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrsexOEF35f1aEgAAAAASUVORK5CYII=)  
**Limitations**  
Feature               Windows   DXVK   Gallium Nine  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AUBBAsUeCE4yeIiT9CRVMWGAjJK2CbjNzVGcAAPzF2qu7Wl9PAAB47XoA/vcF8exqpY4AAAAASUVORK5CYII=)  
Passthrough           ✅        ✅     ✅  
   
 Logging               ✅        ✅     ✅  
   
 Depth capture         ✅        ❌     ✅  
   
 Color screenshot      ✅        ✅     ✅  
   
 Depth-based post FX   ✅        ❌     ✅  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAM0lEQVR4nO3OUQmAABBAsaeI2MKqV8RyJrGCfyJsCbbMzFldAQDwF/dWrdXx9QQAgNf2B/NkAzRb7P0YAAAAAElFTkSuQmCC)  
**Installation**  
**Windows**  
Place d3d9.dll **next to the game's .exe file**.  
The proxy automatically loads the system D3D9 backend.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNhQAQ60PcrIhnxgQU2QtIq6DIze3UGAMBf3Gu1VcfXEwAAXrseS14EKxPCORkAAAAASUVORK5CYII=)  
**Linux + Wine + DXVK**  
1. Rename the original DXVK d3d9.dll to d3d9_dxvk.dll  
2. Place the proxy d3d9.dll next to the game's .exe file  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSNhZscYahheJwqQgQU2QtIq6DIze3UGAMBf3Gu1VcfXEwAAXrseoqcEQXyAWBgAAAAASUVORK5CYII=)  
**Linux + Wine + Gallium Nine**  
Place the proxy d3d9.dll next to the game's .exe file.  
Wine will automatically use Gallium Nine as the backend.  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANklEQVR4nO3OQQmAABRAsSfYxZo/jzlMYQLPJrCCNxG2BFtmZquOAAD4i3Ot7mr/egIAwGvXA4q7Bc870TqdAAAAAElFTkSuQmCC)  
**Build**  
**Linux (MinGW → Windows DLL)**  
i686-w64-mingw32-g++ -std=c++17 -O2 -m32 -shared     -o d3d9.dll d3d9_proxy.cpp     -ld3d9 -ldxguid     -static-libgcc -static-libstdc++     -Wl,--kill-at  
   
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OMQ2AABAAsSPBCUbfEm6YmFDBhAU2QtIq6DIzW7UHAMBfnGt1V8fXEwAAXrse/w8F7pbTa1oAAAAASUVORK5CYII=)  
**Windows (MSVC)**  
Platform: **x86**  
Additional dependencies:  
d3d9.lib  
   
 dxguid.lib  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANElEQVR4nO3OQQmAABRAsad4EEtY9QcxnUms4E2ELcGWmTmrKwAA/uLeqrU6vp4AAPDa/gDzXgM37EF77AAAAABJRU5ErkJggg==)  
**Post-Processing Integration**  
The integration point is located in:  
HRESULT STDMETHODCALLTYPE Present(...) override  
   
If depth_captured == true, depth_tex contains the depth buffer of  
   
 the current frame  
   
 (Windows / Gallium Nine).  
![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAnEAAAACCAYAAAA3pIp+AAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAANUlEQVR4nO3OQQmAABRAsSd4NIGRTPXNaQBrWMGbCFuCLTOzV2cAAPzFvVZbdXw9AQDgtesBhZQEOYZGgUEAAAAASUVORK5CYII=)  
**Project Scope**  
This project provides a helper proxy module for analyzing and extending  
   
 the behavior of Direct3D 9 applications.  
