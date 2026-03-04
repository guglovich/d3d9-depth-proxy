#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>

static FILE* g_log = nullptr;
static void log_init(HINSTANCE h) {
    char p[MAX_PATH];
    GetModuleFileNameA(h, p, MAX_PATH);
    char* dot = strrchr(p, '.');
    if (dot) strcpy(dot, "_log.txt"); else strcat(p, "_log.txt");
    g_log = fopen(p, "w");
    if (g_log) { fprintf(g_log, "=== d3d9 proxy log ===\nlog: %s\n", p); fflush(g_log); }
}
static void log_write(const char* m) { if (!g_log) return; fprintf(g_log, "%s\n", m); fflush(g_log); }
static void log_close() { if (!g_log) return; fprintf(g_log, "proxy unloaded\n"); fflush(g_log); fclose(g_log); g_log = nullptr; }

static HMODULE real_d3d9 = nullptr;
static IDirect3D9* (WINAPI* real_Create9)(UINT)                    = nullptr;
static HRESULT     (WINAPI* real_Create9Ex)(UINT, IDirect3D9Ex**)  = nullptr;

#define D3DFMT_INTZ ((D3DFORMAT)MAKEFOURCC('I','N','T','Z'))
#define D3DFMT_DF24 ((D3DFORMAT)MAKEFOURCC('D','F','2','4'))
#define D3DFMT_DF16 ((D3DFORMAT)MAKEFOURCC('D','F','1','6'))

static bool load_real_d3d9() {
    if (real_d3d9) return true;
    real_d3d9 = LoadLibraryA("d3d9_dxvk.dll");
    if (!real_d3d9) {
        char sys[MAX_PATH];
        GetSystemDirectoryA(sys, MAX_PATH);
        strcat(sys, "\\d3d9.dll");
        real_d3d9 = LoadLibraryA(sys);
    }
    if (!real_d3d9) { log_write("ERROR: cannot load real d3d9"); return false; }
    log_write("real d3d9 loaded");
    real_Create9   = (IDirect3D9*(WINAPI*)(UINT))             GetProcAddress(real_d3d9, "Direct3DCreate9");
    real_Create9Ex = (HRESULT(WINAPI*)(UINT, IDirect3D9Ex**)) GetProcAddress(real_d3d9, "Direct3DCreate9Ex");
    return true;
}

// D3D9DeviceProxy реализует IDirect3DDevice9Ex.
// real_ex — не-владеющий алиас на тот же COM-объект через Ex-интерфейс (может быть null).
struct D3D9DeviceProxy : IDirect3DDevice9Ex {
    IDirect3DDevice9*   real;
    IDirect3DDevice9Ex* real_ex;
    UINT sw = 1920, sh = 1080;
    IDirect3DSurface9*  depth_surface = nullptr;
    UINT depth_w = 0, depth_h = 0;
    IDirect3DTexture9*  depth_tex     = nullptr;
    IDirect3DSurface9*  depth_srf     = nullptr;
    bool depth_ready    = false;
    bool depth_captured = false;
    D3DFORMAT last_rt_fmt = D3DFMT_UNKNOWN;

    explicit D3D9DeviceProxy(IDirect3DDevice9* r) : real(r), real_ex(nullptr) {
        IDirect3DDevice9Ex* tmp = nullptr;
        if (SUCCEEDED(r->QueryInterface(IID_IDirect3DDevice9Ex, (void**)&tmp))) {
            tmp->Release();
            real_ex = tmp;
            log_write("Device9Ex available");
        }
    }
    ~D3D9DeviceProxy() { cleanup(); }

    void cleanup() {
        if (depth_srf)     { depth_srf->Release();     depth_srf     = nullptr; }
        if (depth_tex)     { depth_tex->Release();     depth_tex     = nullptr; }
        if (depth_surface) { depth_surface->Release(); depth_surface = nullptr; }
        depth_ready = false; depth_captured = false;
    }

    void init_depth_resources(UINT w, UINT h) {
        if (depth_ready) return;
        char buf[128];
        IDirect3D9* d3d = nullptr; real->GetDirect3D(&d3d);
        D3DDEVICE_CREATION_PARAMETERS cp = {}; real->GetCreationParameters(&cp);
        D3DFORMAT fmt = D3DFMT_UNKNOWN; const char* name = "NONE";
        if (d3d) {
            auto chk = [&](D3DFORMAT f) {
                return SUCCEEDED(d3d->CheckDeviceFormat(cp.AdapterOrdinal, cp.DeviceType,
                    D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, f));
            };
            if      (chk(D3DFMT_DF24)) { fmt = D3DFMT_DF24; name = "DF24"; }
            else if (chk(D3DFMT_DF16)) { fmt = D3DFMT_DF16; name = "DF16"; }
            else if (chk(D3DFMT_INTZ)) { fmt = D3DFMT_INTZ; name = "INTZ"; }
            d3d->Release();
        }
        snprintf(buf, sizeof(buf), "Readable depth fmt: %s", name); log_write(buf);
        if (fmt != D3DFMT_UNKNOWN) {
            HRESULT hr = real->CreateTexture(w, h, 1, D3DUSAGE_DEPTHSTENCIL,
                fmt, D3DPOOL_DEFAULT, &depth_tex, nullptr);
            snprintf(buf, sizeof(buf), "CreateTexture %s: 0x%X", name, (unsigned)hr); log_write(buf);
            if (SUCCEEDED(hr) && depth_tex) {
                depth_tex->GetSurfaceLevel(0, &depth_srf);
                log_write("Depth texture ready");
            }
        }
        depth_ready = true;
        log_write("Resources ready");
    }

    void try_capture_depth() {
        if (!depth_srf || !depth_surface) return;
        HRESULT hr = real->StretchRect(depth_surface, nullptr, depth_srf, nullptr, D3DTEXF_NONE);
        if (SUCCEEDED(hr)) {
            if (!depth_captured) { depth_captured = true; log_write("Depth captured OK"); }
        } else {
            static bool logged = false;
            if (!logged) {
                logged = true;
                char buf[80];
                snprintf(buf, sizeof(buf), "Depth capture failed: 0x%X (expected under DXVK)", (unsigned)hr);
                log_write(buf);
            }
        }
    }

    // --- IUnknown ---
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void** p) override {
        if (!p) return E_POINTER;
        if (r == IID_IUnknown || r == IID_IDirect3DDevice9) {
            *p = static_cast<IDirect3DDevice9*>(this); AddRef(); return S_OK;
        }
        if (r == IID_IDirect3DDevice9Ex) {
            if (!real_ex) return E_NOINTERFACE;
            *p = static_cast<IDirect3DDevice9Ex*>(this); AddRef(); return S_OK;
        }
        return real->QueryInterface(r, p);
    }
    ULONG STDMETHODCALLTYPE AddRef()  override { return real->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = real->Release();
        if (!r) { real_ex = nullptr; delete this; }
        return r;
    }

    // --- IDirect3DDevice9 ---
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override {
        HRESULT hr = real->TestCooperativeLevel();
        if (hr == D3DERR_DEVICELOST) cleanup();
        return hr;
    }
    UINT    STDMETHODCALLTYPE GetAvailableTextureMem() override { return real->GetAvailableTextureMem(); }
    HRESULT STDMETHODCALLTYPE EvictManagedResources() override { return real->EvictManagedResources(); }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** p) override { return real->GetDirect3D(p); }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* p) override { return real->GetDeviceCaps(p); }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT i, D3DDISPLAYMODE* p) override { return real->GetDisplayMode(i, p); }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* p) override { return real->GetCreationParameters(p); }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT x, UINT y, IDirect3DSurface9* s) override { return real->SetCursorProperties(x, y, s); }
    void    STDMETHODCALLTYPE SetCursorPosition(int x, int y, DWORD f) override { real->SetCursorPosition(x, y, f); }
    BOOL    STDMETHODCALLTYPE ShowCursor(BOOL b) override { return real->ShowCursor(b); }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* p, IDirect3DSwapChain9** s) override { return real->CreateAdditionalSwapChain(p, s); }
    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT i, IDirect3DSwapChain9** s) override { return real->GetSwapChain(i, s); }
    UINT    STDMETHODCALLTYPE GetNumberOfSwapChains() override { return real->GetNumberOfSwapChains(); }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* p) override {
        log_write("Reset"); cleanup();
        HRESULT hr = real->Reset(p);
        if (SUCCEEDED(hr) && p) { sw = p->BackBufferWidth ? p->BackBufferWidth : sw; sh = p->BackBufferHeight ? p->BackBufferHeight : sh; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Present(const RECT* a, const RECT* b, HWND c, const RGNDATA* d) override { return real->Present(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT a, UINT b, D3DBACKBUFFER_TYPE c, IDirect3DSurface9** d) override { return real->GetBackBuffer(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(UINT a, D3DRASTER_STATUS* b) override { return real->GetRasterStatus(a, b); }
    HRESULT STDMETHODCALLTYPE SetDialogBoxMode(BOOL b) override { return real->SetDialogBoxMode(b); }
    void    STDMETHODCALLTYPE SetGammaRamp(UINT a, DWORD b, const D3DGAMMARAMP* c) override { real->SetGammaRamp(a, b, c); }
    void    STDMETHODCALLTYPE GetGammaRamp(UINT a, D3DGAMMARAMP* b) override { real->GetGammaRamp(a, b); }
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT w, UINT h, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DTexture9** t, HANDLE* s) override { return real->CreateTexture(w,h,l,u,f,p,t,s); }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT w, UINT h, UINT d, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DVolumeTexture9** t, HANDLE* s) override { return real->CreateVolumeTexture(w,h,d,l,u,f,p,t,s); }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT s, UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DCubeTexture9** t, HANDLE* sh) override { return real->CreateCubeTexture(s,l,u,f,p,t,sh); }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT l, DWORD u, DWORD f, D3DPOOL p, IDirect3DVertexBuffer9** v, HANDLE* s) override { return real->CreateVertexBuffer(l,u,f,p,v,s); }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT l, DWORD u, D3DFORMAT f, D3DPOOL p, IDirect3DIndexBuffer9** i, HANDLE* s) override { return real->CreateIndexBuffer(l,u,f,p,i,s); }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE m, DWORD q, BOOL l, IDirect3DSurface9** s, HANDLE* sh) override { return real->CreateRenderTarget(w,h,f,m,q,l,s,sh); }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE m, DWORD q, BOOL d, IDirect3DSurface9** s, HANDLE* sh) override {
        HRESULT hr = real->CreateDepthStencilSurface(w,h,f,m,q,d,s,sh);
        if (SUCCEEDED(hr) && s && *s && w > 256 && h > 256 && !depth_ready) { depth_w = w; depth_h = h; init_depth_resources(w, h); }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE UpdateSurface(IDirect3DSurface9* a, const RECT* b, IDirect3DSurface9* c, const POINT* d) override { return real->UpdateSurface(a,b,c,d); }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture9* a, IDirect3DBaseTexture9* b) override { return real->UpdateTexture(a,b); }
    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* a, IDirect3DSurface9* b) override { return real->GetRenderTargetData(a,b); }
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(UINT a, IDirect3DSurface9* b) override { return real->GetFrontBufferData(a,b); }
    HRESULT STDMETHODCALLTYPE StretchRect(IDirect3DSurface9* a, const RECT* b, IDirect3DSurface9* c, const RECT* d, D3DTEXTUREFILTERTYPE f) override { return real->StretchRect(a,b,c,d,f); }
    HRESULT STDMETHODCALLTYPE ColorFill(IDirect3DSurface9* a, const RECT* b, D3DCOLOR c) override { return real->ColorFill(a,b,c); }
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurface(UINT w, UINT h, D3DFORMAT f, D3DPOOL p, IDirect3DSurface9** s, HANDLE* sh) override { return real->CreateOffscreenPlainSurface(w,h,f,p,s,sh); }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD i, IDirect3DSurface9* s) override {
        if (i == 0 && s && depth_srf && depth_surface) {
            D3DSURFACE_DESC desc;
            if (SUCCEEDED(s->GetDesc(&desc))) {
                if (last_rt_fmt == (D3DFORMAT)0x15 && desc.Format == (D3DFORMAT)0x16 && desc.Width == sw)
                    try_capture_depth();
                last_rt_fmt = desc.Format;
            }
        }
        return real->SetRenderTarget(i, s);
    }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD i, IDirect3DSurface9** s) override { return real->GetRenderTarget(i, s); }
    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* s) override {
        if (depth_surface) depth_surface->Release();
        depth_surface = s;
        if (s) { s->AddRef(); if (!depth_w) { D3DSURFACE_DESC d; if (SUCCEEDED(s->GetDesc(&d))) { depth_w=d.Width; depth_h=d.Height; } } }
        return real->SetDepthStencilSurface(s);
    }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** s) override { return real->GetDepthStencilSurface(s); }
    HRESULT STDMETHODCALLTYPE BeginScene() override { return real->BeginScene(); }
    HRESULT STDMETHODCALLTYPE EndScene()   override { return real->EndScene(); }
    HRESULT STDMETHODCALLTYPE Clear(DWORD a, const D3DRECT* b, DWORD c, D3DCOLOR d, float e, DWORD f) override { return real->Clear(a,b,c,d,e,f); }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE a, const D3DMATRIX* b) override { return real->SetTransform(a,b); }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE a, D3DMATRIX* b) override { return real->GetTransform(a,b); }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE a, const D3DMATRIX* b) override { return real->MultiplyTransform(a,b); }
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* p) override { return real->SetViewport(p); }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* p) override { return real->GetViewport(p); }
    HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL9* m) override { return real->SetMaterial(m); }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL9* m) override { return real->GetMaterial(m); }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD i, const D3DLIGHT9* l) override { return real->SetLight(i,l); }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD i, D3DLIGHT9* l) override { return real->GetLight(i,l); }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD i, BOOL b) override { return real->LightEnable(i,b); }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD i, BOOL* b) override { return real->GetLightEnable(i,b); }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD i, const float* p) override { return real->SetClipPlane(i,p); }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD i, float* p) override { return real->GetClipPlane(i,p); }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE a, DWORD b) override { return real->SetRenderState(a,b); }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE a, DWORD* b) override { return real->GetRenderState(a,b); }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE t, IDirect3DStateBlock9** s) override { return real->CreateStateBlock(t,s); }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override { return real->BeginStateBlock(); }
    HRESULT STDMETHODCALLTYPE EndStateBlock(IDirect3DStateBlock9** s) override { return real->EndStateBlock(s); }
    HRESULT STDMETHODCALLTYPE SetClipStatus(const D3DCLIPSTATUS9* c) override { return real->SetClipStatus(c); }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS9* c) override { return real->GetClipStatus(c); }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD s, IDirect3DBaseTexture9** t) override { return real->GetTexture(s,t); }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD s, IDirect3DBaseTexture9* t) override { return real->SetTexture(s,t); }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD s, D3DTEXTURESTAGESTATETYPE t, DWORD* v) override { return real->GetTextureStageState(s,t,v); }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD s, D3DTEXTURESTAGESTATETYPE t, DWORD v) override { return real->SetTextureStageState(s,t,v); }
    HRESULT STDMETHODCALLTYPE GetSamplerState(DWORD s, D3DSAMPLERSTATETYPE t, DWORD* v) override { return real->GetSamplerState(s,t,v); }
    HRESULT STDMETHODCALLTYPE SetSamplerState(DWORD s, D3DSAMPLERSTATETYPE t, DWORD v) override { return real->SetSamplerState(s,t,v); }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* p) override { return real->ValidateDevice(p); }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT p, const PALETTEENTRY* e) override { return real->SetPaletteEntries(p,e); }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT p, PALETTEENTRY* e) override { return real->GetPaletteEntries(p,e); }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT p) override { return real->SetCurrentTexturePalette(p); }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* p) override { return real->GetCurrentTexturePalette(p); }
    HRESULT STDMETHODCALLTYPE SetScissorRect(const RECT* r) override { return real->SetScissorRect(r); }
    HRESULT STDMETHODCALLTYPE GetScissorRect(RECT* r) override { return real->GetScissorRect(r); }
    HRESULT STDMETHODCALLTYPE SetSoftwareVertexProcessing(BOOL b) override { return real->SetSoftwareVertexProcessing(b); }
    BOOL    STDMETHODCALLTYPE GetSoftwareVertexProcessing() override { return real->GetSoftwareVertexProcessing(); }
    HRESULT STDMETHODCALLTYPE SetNPatchMode(float n) override { return real->SetNPatchMode(n); }
    float   STDMETHODCALLTYPE GetNPatchMode() override { return real->GetNPatchMode(); }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE t, UINT a, UINT b) override { return real->DrawPrimitive(t,a,b); }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE t, INT a, UINT b, UINT c, UINT d, UINT e) override { return real->DrawIndexedPrimitive(t,a,b,c,d,e); }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE t, UINT c, const void* d, UINT s) override { return real->DrawPrimitiveUP(t,c,d,s); }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t, UINT a, UINT b, UINT c, const void* d, D3DFORMAT e, const void* f, UINT g) override { return real->DrawIndexedPrimitiveUP(t,a,b,c,d,e,f,g); }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT a, UINT b, UINT c, IDirect3DVertexBuffer9* d, IDirect3DVertexDeclaration9* e, DWORD f) override { return real->ProcessVertices(a,b,c,d,e,f); }
    HRESULT STDMETHODCALLTYPE CreateVertexDeclaration(const D3DVERTEXELEMENT9* e, IDirect3DVertexDeclaration9** d) override { return real->CreateVertexDeclaration(e,d); }
    HRESULT STDMETHODCALLTYPE SetVertexDeclaration(IDirect3DVertexDeclaration9* d) override { return real->SetVertexDeclaration(d); }
    HRESULT STDMETHODCALLTYPE GetVertexDeclaration(IDirect3DVertexDeclaration9** d) override { return real->GetVertexDeclaration(d); }
    HRESULT STDMETHODCALLTYPE SetFVF(DWORD f) override { return real->SetFVF(f); }
    HRESULT STDMETHODCALLTYPE GetFVF(DWORD* f) override { return real->GetFVF(f); }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* f, IDirect3DVertexShader9** s) override { return real->CreateVertexShader(f,s); }
    HRESULT STDMETHODCALLTYPE SetVertexShader(IDirect3DVertexShader9* s) override { return real->SetVertexShader(s); }
    HRESULT STDMETHODCALLTYPE GetVertexShader(IDirect3DVertexShader9** s) override { return real->GetVertexShader(s); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantF(UINT r, const float* d, UINT c) override { return real->SetVertexShaderConstantF(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantF(UINT r, float* d, UINT c) override { return real->GetVertexShaderConstantF(r,d,c); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantI(UINT r, const int* d, UINT c) override { return real->SetVertexShaderConstantI(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantI(UINT r, int* d, UINT c) override { return real->GetVertexShaderConstantI(r,d,c); }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstantB(UINT r, const BOOL* d, UINT c) override { return real->SetVertexShaderConstantB(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstantB(UINT r, BOOL* d, UINT c) override { return real->GetVertexShaderConstantB(r,d,c); }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT n, IDirect3DVertexBuffer9* v, UINT o, UINT s) override { return real->SetStreamSource(n,v,o,s); }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT n, IDirect3DVertexBuffer9** v, UINT* o, UINT* s) override { return real->GetStreamSource(n,v,o,s); }
    HRESULT STDMETHODCALLTYPE SetStreamSourceFreq(UINT n, UINT d) override { return real->SetStreamSourceFreq(n,d); }
    HRESULT STDMETHODCALLTYPE GetStreamSourceFreq(UINT n, UINT* d) override { return real->GetStreamSourceFreq(n,d); }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer9* i) override { return real->SetIndices(i); }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer9** i) override { return real->GetIndices(i); }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* f, IDirect3DPixelShader9** s) override { return real->CreatePixelShader(f,s); }
    HRESULT STDMETHODCALLTYPE SetPixelShader(IDirect3DPixelShader9* s) override { return real->SetPixelShader(s); }
    HRESULT STDMETHODCALLTYPE GetPixelShader(IDirect3DPixelShader9** s) override { return real->GetPixelShader(s); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantF(UINT r, const float* d, UINT c) override { return real->SetPixelShaderConstantF(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantF(UINT r, float* d, UINT c) override { return real->GetPixelShaderConstantF(r,d,c); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantI(UINT r, const int* d, UINT c) override { return real->SetPixelShaderConstantI(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantI(UINT r, int* d, UINT c) override { return real->GetPixelShaderConstantI(r,d,c); }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstantB(UINT r, const BOOL* d, UINT c) override { return real->SetPixelShaderConstantB(r,d,c); }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstantB(UINT r, BOOL* d, UINT c) override { return real->GetPixelShaderConstantB(r,d,c); }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT h, const float* s, const D3DRECTPATCH_INFO* i) override { return real->DrawRectPatch(h,s,i); }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT h, const float* s, const D3DTRIPATCH_INFO* i) override { return real->DrawTriPatch(h,s,i); }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT h) override { return real->DeletePatch(h); }
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE t, IDirect3DQuery9** q) override { return real->CreateQuery(t,q); }

    // --- IDirect3DDevice9Ex ---
    HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(UINT w, UINT h, float* rows, float* cols) override {
        return real_ex ? real_ex->SetConvolutionMonoKernel(w, h, rows, cols) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE ComposeRects(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst,
        IDirect3DVertexBuffer9* pSrcRects, UINT nRects, IDirect3DVertexBuffer9* pDstRects,
        D3DCOMPOSERECTSOP op, int xOff, int yOff) override {
        return real_ex ? real_ex->ComposeRects(pSrc, pDst, pSrcRects, nRects, pDstRects, op, xOff, yOff) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE PresentEx(const RECT* pSrc, const RECT* pDst, HWND hWnd,
        const RGNDATA* pDirty, DWORD flags) override {
        return real_ex ? real_ex->PresentEx(pSrc, pDst, hWnd, pDirty, flags) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT* p) override {
        return real_ex ? real_ex->GetGPUThreadPriority(p) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT p) override {
        return real_ex ? real_ex->SetGPUThreadPriority(p) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE WaitForVBlank(UINT i) override {
        return real_ex ? real_ex->WaitForVBlank(i) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DResource9** arr, UINT32 n) override {
        return real_ex ? real_ex->CheckResourceResidency(arr, n) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT n) override {
        return real_ex ? real_ex->SetMaximumFrameLatency(n) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* n) override {
        return real_ex ? real_ex->GetMaximumFrameLatency(n) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceState(HWND h) override {
        return real_ex ? real_ex->CheckDeviceState(h) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(UINT w, UINT h, D3DFORMAT f,
        D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL l, IDirect3DSurface9** s, HANDLE* sh, DWORD u) override {
        return real_ex ? real_ex->CreateRenderTargetEx(w, h, f, ms, q, l, s, sh, u) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(UINT w, UINT h, D3DFORMAT f,
        D3DPOOL p, IDirect3DSurface9** s, HANDLE* sh, DWORD u) override {
        return real_ex ? real_ex->CreateOffscreenPlainSurfaceEx(w, h, f, p, s, sh, u) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(UINT w, UINT h, D3DFORMAT f,
        D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL d, IDirect3DSurface9** s, HANDLE* sh, DWORD u) override {
        HRESULT hr = real_ex ? real_ex->CreateDepthStencilSurfaceEx(w, h, f, ms, q, d, s, sh, u) : E_NOINTERFACE;
        if (SUCCEEDED(hr) && s && *s && w > 256 && h > 256 && !depth_ready)
            { depth_w = w; depth_h = h; init_depth_resources(w, h); }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE ResetEx(D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* dm) override {
        log_write("ResetEx"); cleanup();
        if (!real_ex) return E_NOINTERFACE;
        HRESULT hr = real_ex->ResetEx(pp, dm);
        if (SUCCEEDED(hr) && pp) {
            sw = pp->BackBufferWidth  ? pp->BackBufferWidth  : sw;
            sh = pp->BackBufferHeight ? pp->BackBufferHeight : sh;
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayModeEx(UINT i, D3DDISPLAYMODEEX* m, D3DDISPLAYROTATION* r) override {
        return real_ex ? real_ex->GetDisplayModeEx(i, m, r) : E_NOINTERFACE;
    }
};

// D3D9Proxy реализует IDirect3D9Ex.
// real_ex — не-владеющий алиас (может быть null если бэкенд не поддерживает Ex).
struct D3D9Proxy : IDirect3D9Ex {
    IDirect3D9*   real;
    IDirect3D9Ex* real_ex;

    explicit D3D9Proxy(IDirect3D9* r) : real(r), real_ex(nullptr) {
        IDirect3D9Ex* tmp = nullptr;
        if (SUCCEEDED(r->QueryInterface(IID_IDirect3D9Ex, (void**)&tmp))) {
            tmp->Release();
            real_ex = tmp;
        }
        log_write("D3D9 created");
    }
    ~D3D9Proxy() { log_write("D3D9 destroyed"); }

    HRESULT  STDMETHODCALLTYPE QueryInterface(REFIID r, void** p) override {
        if (!p) return E_POINTER;
        if (r == IID_IUnknown || r == IID_IDirect3D9) {
            *p = static_cast<IDirect3D9*>(this); AddRef(); return S_OK;
        }
        if (r == IID_IDirect3D9Ex) {
            if (!real_ex) return E_NOINTERFACE;
            *p = static_cast<IDirect3D9Ex*>(this); AddRef(); return S_OK;
        }
        return real->QueryInterface(r, p);
    }
    ULONG    STDMETHODCALLTYPE AddRef()  override { return real->AddRef(); }
    ULONG    STDMETHODCALLTYPE Release() override {
        ULONG r = real->Release();
        if (!r) { real_ex = nullptr; delete this; }
        return r;
    }
    HRESULT  STDMETHODCALLTYPE RegisterSoftwareDevice(void* p) override { return real->RegisterSoftwareDevice(p); }
    UINT     STDMETHODCALLTYPE GetAdapterCount() override { return real->GetAdapterCount(); }
    HRESULT  STDMETHODCALLTYPE GetAdapterIdentifier(UINT a, DWORD b, D3DADAPTER_IDENTIFIER9* c) override { return real->GetAdapterIdentifier(a,b,c); }
    UINT     STDMETHODCALLTYPE GetAdapterModeCount(UINT a, D3DFORMAT b) override { return real->GetAdapterModeCount(a,b); }
    HRESULT  STDMETHODCALLTYPE EnumAdapterModes(UINT a, D3DFORMAT b, UINT c, D3DDISPLAYMODE* d) override { return real->EnumAdapterModes(a,b,c,d); }
    HRESULT  STDMETHODCALLTYPE GetAdapterDisplayMode(UINT a, D3DDISPLAYMODE* b) override { return real->GetAdapterDisplayMode(a,b); }
    HRESULT  STDMETHODCALLTYPE CheckDeviceType(UINT a, D3DDEVTYPE b, D3DFORMAT c, D3DFORMAT d, BOOL e) override { return real->CheckDeviceType(a,b,c,d,e); }
    HRESULT  STDMETHODCALLTYPE CheckDeviceFormat(UINT a, D3DDEVTYPE b, D3DFORMAT c, DWORD d, D3DRESOURCETYPE e, D3DFORMAT f) override { return real->CheckDeviceFormat(a,b,c,d,e,f); }
    HRESULT  STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT a, D3DDEVTYPE b, D3DFORMAT c, BOOL d, D3DMULTISAMPLE_TYPE e, DWORD* f) override { return real->CheckDeviceMultiSampleType(a,b,c,d,e,f); }
    HRESULT  STDMETHODCALLTYPE CheckDepthStencilMatch(UINT a, D3DDEVTYPE b, D3DFORMAT c, D3DFORMAT d, D3DFORMAT e) override { return real->CheckDepthStencilMatch(a,b,c,d,e); }
    HRESULT  STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT a, D3DDEVTYPE b, D3DFORMAT c, D3DFORMAT d) override { return real->CheckDeviceFormatConversion(a,b,c,d); }
    HRESULT  STDMETHODCALLTYPE GetDeviceCaps(UINT a, D3DDEVTYPE b, D3DCAPS9* c) override { return real->GetDeviceCaps(a,b,c); }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT a) override { return real->GetAdapterMonitor(a); }

    HRESULT STDMETHODCALLTYPE CreateDevice(UINT a, D3DDEVTYPE b, HWND c, DWORD d,
        D3DPRESENT_PARAMETERS* e, IDirect3DDevice9** f) override {
        log_write("CreateDevice");
        if (e) {
            char buf[256];
            snprintf(buf, sizeof(buf), "PP: %ux%u fmt=0x%X ds=%d dsfmt=0x%X",
                e->BackBufferWidth, e->BackBufferHeight,
                (unsigned)e->BackBufferFormat,
                (int)e->EnableAutoDepthStencil,
                (unsigned)e->AutoDepthStencilFormat);
            log_write(buf);
        }
        HRESULT hr = real->CreateDevice(a,b,c,d,e,f);
        if (SUCCEEDED(hr) && f && *f) {
            auto* px = new D3D9DeviceProxy(*f);
            if (e) {
                px->sw = e->BackBufferWidth  ? e->BackBufferWidth  : 1920;
                px->sh = e->BackBufferHeight ? e->BackBufferHeight : 1080;
                char buf[64];
                snprintf(buf, sizeof(buf), "Screen: %ux%u", px->sw, px->sh);
                log_write(buf);
            }
            *f = px;
            log_write("Device hooks installed");
        } else {
            log_write("CreateDevice FAILED");
        }
        return hr;
    }

    // --- IDirect3D9Ex ---
    UINT STDMETHODCALLTYPE GetAdapterModeCountEx(UINT a, const D3DDISPLAYMODEFILTER* f) override {
        return real_ex ? real_ex->GetAdapterModeCountEx(a, f) : 0;
    }
    HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(UINT a, const D3DDISPLAYMODEFILTER* f, UINT m, D3DDISPLAYMODEEX* mode) override {
        return real_ex ? real_ex->EnumAdapterModesEx(a, f, m, mode) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(UINT a, D3DDISPLAYMODEEX* m, D3DDISPLAYROTATION* r) override {
        return real_ex ? real_ex->GetAdapterDisplayModeEx(a, m, r) : E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE CreateDeviceEx(UINT a, D3DDEVTYPE t, HWND w, DWORD f,
        D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* dm, IDirect3DDevice9Ex** d) override {
        log_write("CreateDeviceEx");
        if (!real_ex) { log_write("CreateDeviceEx: Ex not available"); return E_NOINTERFACE; }
        HRESULT hr = real_ex->CreateDeviceEx(a, t, w, f, pp, dm, d);
        if (SUCCEEDED(hr) && d && *d) {
            auto* px = new D3D9DeviceProxy(*d);
            if (pp) {
                px->sw = pp->BackBufferWidth  ? pp->BackBufferWidth  : 1920;
                px->sh = pp->BackBufferHeight ? pp->BackBufferHeight : 1080;
                char buf[64];
                snprintf(buf, sizeof(buf), "Screen: %ux%u", px->sw, px->sh);
                log_write(buf);
            }
            *d = static_cast<IDirect3DDevice9Ex*>(px);
            log_write("DeviceEx hooks installed");
        } else {
            log_write("CreateDeviceEx FAILED");
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT a, LUID* l) override {
        return real_ex ? real_ex->GetAdapterLUID(a, l) : E_NOINTERFACE;
    }
};

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { log_init(h); log_write("proxy loaded"); }
    else if (reason == DLL_PROCESS_DETACH) { log_close(); if (real_d3d9) { FreeLibrary(real_d3d9); real_d3d9 = nullptr; } }
    return TRUE;
}

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdk) {
    log_write("Direct3DCreate9");
    if (!load_real_d3d9()) return nullptr;
    if (!real_Create9) { log_write("ERROR: Direct3DCreate9 not found"); return nullptr; }
    IDirect3D9* r = real_Create9(sdk);
    if (!r) { log_write("ERROR: null IDirect3D9"); return nullptr; }
    return new D3D9Proxy(r);
}

extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT sdk, IDirect3D9Ex** ppD3D) {
    log_write("Direct3DCreate9Ex");
    if (!ppD3D) return D3DERR_INVALIDCALL;
    if (!load_real_d3d9()) return D3DERR_NOTAVAILABLE;
    if (!real_Create9Ex) { log_write("ERROR: Direct3DCreate9Ex not found"); return D3DERR_NOTAVAILABLE; }
    IDirect3D9Ex* r = nullptr;
    HRESULT hr = real_Create9Ex(sdk, &r);
    if (FAILED(hr) || !r) { log_write("ERROR: Direct3DCreate9Ex failed"); return hr; }
    auto* proxy = new D3D9Proxy(r);
    *ppD3D = static_cast<IDirect3D9Ex*>(proxy);
    log_write("D3D9Ex proxy created");
    return S_OK;
}
