/* FNVR Stereo - FNVRStereo.dll, an xNVSE plugin: the game's Direct3D 9 device becomes Direct3D 9Ex.
   Copyright (C) 2026 FNVVRMC contributors. GPL-3.0-or-later.

   Why: a D3D9Ex device can share its textures with other processes and APIs. FNVR then draws each eye straight into a
   texture the VR host (FNVR-VRHost.exe) opens on its own D3D11 device - no copy from the graphics card to the CPU and
   back, which is the slowest part of getting the game's picture to the headset.

   The catch: D3D9Ex doesn't allow the "managed" texture and buffer pool New Vegas loads nearly everything into. Each
   managed texture / vertex buffer / index buffer is quietly made in the default pool as a dynamic (lockable) one
   instead, so the game can still fill it. (It also means those resources no longer keep a second copy in the game's
   own memory - the same trick ENBoost uses to save address space.)

   Needs Windows' own d3d9.dll: with DXVK's or ENB's d3d9.dll in the game folder it stays off.
   [Stereo] bD3D9Ex=0 in FNVR.ini turns it off. Log: FNVRStereo.log in the game folder. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../src/nvse_api.h"

static FILE *g_log;
static void Log(const char *fmt, ...)
{
    if (!g_log) { char p[MAX_PATH]; GetModuleFileNameA(NULL, p, MAX_PATH); char *s = strrchr(p, '\\'); if (s) s[1] = 0; strcat(p, "FNVRStereo.log"); g_log = fopen(p, "w"); if (!g_log) return; }
    SYSTEMTIME t; GetLocalTime(&t); fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}

/* ---- hooking helpers ---- */
static int PrologueLen(const BYTE *p)
{
    int n = 0;
    while (n < 5) {
        const BYTE *q = p + n;
        if (q[0] == 0x8B && q[1] == 0xFF) n += 2; else if (q[0] == 0x55) n += 1; else if (q[0] == 0x8B && q[1] == 0xEC) n += 2;
        else if (q[0] == 0x83 && q[1] == 0xEC) n += 3; else if (q[0] == 0x81 && q[1] == 0xEC) n += 6; else if (q[0] == 0x6A) n += 2;
        else if (q[0] == 0x68) n += 5; else if (q[0] >= 0x50 && q[0] <= 0x57) n += 1; else if (q[0] == 0x83 && q[1] == 0xE4) n += 3;
        else return 0;
    }
    return n;
}
static void *HookFn(BYTE *fn, void *hook)
{
    if (!fn) return NULL;
    int len = fn[0] == 0xE9 ? 5 : PrologueLen(fn);
    if (!len) { Log("can't hook a function starting %02X %02X %02X %02X %02X", fn[0], fn[1], fn[2], fn[3], fn[4]); return NULL; }
    BYTE *tr = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); if (!tr) return NULL;
    if (fn[0] == 0xE9) { uintptr_t tg = (uintptr_t)fn + 5 + *(int32_t *)(fn + 1); tr[0] = 0xE9; *(int32_t *)(tr + 1) = (int32_t)(tg - ((uintptr_t)tr + 5)); }
    else { memcpy(tr, fn, len); tr[len] = 0xE9; *(int32_t *)(tr + len + 1) = (int32_t)(((uintptr_t)fn + len) - ((uintptr_t)tr + len + 5)); }
    FlushInstructionCache(GetCurrentProcess(), tr, 64);
    DWORD old; VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9; *(int32_t *)(fn + 1) = (int32_t)((uintptr_t)hook - ((uintptr_t)fn + 5)); for (int i = 5; i < len; i++) fn[i] = 0x90;
    VirtualProtect(fn, len, old, &old); FlushInstructionCache(GetCurrentProcess(), fn, len);
    return tr;
}
static void *VtPatch(void **slot, void *hook)
{
    void *orig = *slot; if (orig == hook) return NULL;
    DWORD old; VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old); *slot = hook; VirtualProtect(slot, sizeof(void *), old, &old);
    return orig;
}

/* ---- the device: managed pool -> default pool, dynamic ---- */
typedef HRESULT (WINAPI *CreateTexture_t)(IDirect3DDevice9 *, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9 **, HANDLE *);
typedef HRESULT (WINAPI *CreateVolumeTexture_t)(IDirect3DDevice9 *, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9 **, HANDLE *);
typedef HRESULT (WINAPI *CreateCubeTexture_t)(IDirect3DDevice9 *, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9 **, HANDLE *);
typedef HRESULT (WINAPI *CreateVertexBuffer_t)(IDirect3DDevice9 *, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9 **, HANDLE *);
typedef HRESULT (WINAPI *CreateIndexBuffer_t)(IDirect3DDevice9 *, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9 **, HANDLE *);
static CreateTexture_t o_CreateTexture; static CreateVolumeTexture_t o_CreateVolumeTexture; static CreateCubeTexture_t o_CreateCubeTexture;
static CreateVertexBuffer_t o_CreateVertexBuffer; static CreateIndexBuffer_t o_CreateIndexBuffer;
static volatile LONG g_nConv, g_nFail, g_nStatic;
static void Stats(void)
{
    LONG n = g_nConv; if (n == 1 || n == 100 || n == 1000 || n == 5000 || (n % 20000) == 0)
        Log("managed resources moved to the default pool: %ld (%ld made without the dynamic flag, %ld failed)", n, g_nStatic, g_nFail);
}
static HRESULT WINAPI H_CreateTexture(IDirect3DDevice9 *d, UINT w, UINT h, UINT lv, DWORD us, D3DFORMAT f, D3DPOOL pool, IDirect3DTexture9 **t, HANDLE *sh)
{
    if (pool != D3DPOOL_MANAGED) return o_CreateTexture(d, w, h, lv, us, f, pool, t, sh);
    HRESULT hr = o_CreateTexture(d, w, h, lv, us | D3DUSAGE_DYNAMIC, f, D3DPOOL_DEFAULT, t, sh);
    if (FAILED(hr)) { hr = o_CreateTexture(d, w, h, lv, us & ~D3DUSAGE_AUTOGENMIPMAP, f, D3DPOOL_DEFAULT, t, sh); if (SUCCEEDED(hr)) InterlockedIncrement(&g_nStatic); }
    if (FAILED(hr)) { if (InterlockedIncrement(&g_nFail) < 20) Log("texture %ux%u (format %08X, %u levels, usage %08lX) couldn't be made: %08lX", w, h, (unsigned)f, lv, (unsigned long)us, (unsigned long)hr); }
    else { InterlockedIncrement(&g_nConv); Stats(); }
    return hr;
}
static HRESULT WINAPI H_CreateVolumeTexture(IDirect3DDevice9 *d, UINT w, UINT h, UINT dp, UINT lv, DWORD us, D3DFORMAT f, D3DPOOL pool, IDirect3DVolumeTexture9 **t, HANDLE *sh)
{
    if (pool != D3DPOOL_MANAGED) return o_CreateVolumeTexture(d, w, h, dp, lv, us, f, pool, t, sh);
    HRESULT hr = o_CreateVolumeTexture(d, w, h, dp, lv, us | D3DUSAGE_DYNAMIC, f, D3DPOOL_DEFAULT, t, sh);
    if (FAILED(hr)) { if (InterlockedIncrement(&g_nFail) < 20) Log("volume texture %ux%ux%u couldn't be made: %08lX", w, h, dp, (unsigned long)hr); } else { InterlockedIncrement(&g_nConv); Stats(); }
    return hr;
}
static HRESULT WINAPI H_CreateCubeTexture(IDirect3DDevice9 *d, UINT e, UINT lv, DWORD us, D3DFORMAT f, D3DPOOL pool, IDirect3DCubeTexture9 **t, HANDLE *sh)
{
    if (pool != D3DPOOL_MANAGED) return o_CreateCubeTexture(d, e, lv, us, f, pool, t, sh);
    HRESULT hr = o_CreateCubeTexture(d, e, lv, us | D3DUSAGE_DYNAMIC, f, D3DPOOL_DEFAULT, t, sh);
    if (FAILED(hr)) { hr = o_CreateCubeTexture(d, e, lv, us & ~D3DUSAGE_AUTOGENMIPMAP, f, D3DPOOL_DEFAULT, t, sh); if (SUCCEEDED(hr)) InterlockedIncrement(&g_nStatic); }
    if (FAILED(hr)) { if (InterlockedIncrement(&g_nFail) < 20) Log("cube texture %u couldn't be made: %08lX", e, (unsigned long)hr); } else { InterlockedIncrement(&g_nConv); Stats(); }
    return hr;
}
static HRESULT WINAPI H_CreateVertexBuffer(IDirect3DDevice9 *d, UINT len, DWORD us, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9 **b, HANDLE *sh)
{
    if (pool != D3DPOOL_MANAGED) return o_CreateVertexBuffer(d, len, us, fvf, pool, b, sh);
    HRESULT hr = o_CreateVertexBuffer(d, len, us | D3DUSAGE_DYNAMIC, fvf, D3DPOOL_DEFAULT, b, sh);
    if (FAILED(hr)) { if (InterlockedIncrement(&g_nFail) < 20) Log("vertex buffer %u bytes couldn't be made: %08lX", len, (unsigned long)hr); } else { InterlockedIncrement(&g_nConv); Stats(); }
    return hr;
}
static HRESULT WINAPI H_CreateIndexBuffer(IDirect3DDevice9 *d, UINT len, DWORD us, D3DFORMAT f, D3DPOOL pool, IDirect3DIndexBuffer9 **b, HANDLE *sh)
{
    if (pool != D3DPOOL_MANAGED) return o_CreateIndexBuffer(d, len, us, f, pool, b, sh);
    HRESULT hr = o_CreateIndexBuffer(d, len, us | D3DUSAGE_DYNAMIC, f, D3DPOOL_DEFAULT, b, sh);
    if (FAILED(hr)) { if (InterlockedIncrement(&g_nFail) < 20) Log("index buffer %u bytes couldn't be made: %08lX", len, (unsigned long)hr); } else { InterlockedIncrement(&g_nConv); Stats(); }
    return hr;
}
static void Device_Hook(IDirect3DDevice9 *dev)
{
    IDirect3DDevice9Vtbl *vt = (IDirect3DDevice9Vtbl *)dev->lpVtbl;
    void *o;
    if ((o = VtPatch((void **)&vt->CreateTexture, (void *)H_CreateTexture))) o_CreateTexture = (CreateTexture_t)o;
    if ((o = VtPatch((void **)&vt->CreateVolumeTexture, (void *)H_CreateVolumeTexture))) o_CreateVolumeTexture = (CreateVolumeTexture_t)o;
    if ((o = VtPatch((void **)&vt->CreateCubeTexture, (void *)H_CreateCubeTexture))) o_CreateCubeTexture = (CreateCubeTexture_t)o;
    if ((o = VtPatch((void **)&vt->CreateVertexBuffer, (void *)H_CreateVertexBuffer))) o_CreateVertexBuffer = (CreateVertexBuffer_t)o;
    if ((o = VtPatch((void **)&vt->CreateIndexBuffer, (void *)H_CreateIndexBuffer))) o_CreateIndexBuffer = (CreateIndexBuffer_t)o;
    Log("device hooked: managed textures and buffers go to the default pool");
}

static const GUID kIID_D3D9Ex = { 0x02177241, 0x69fc, 0x400c, { 0x8f, 0xf1, 0x93, 0xa4, 0x4d, 0xf6, 0x86, 0x1d } };
/* ---- IDirect3D9Ex::CreateDevice -> CreateDeviceEx ---- */
typedef HRESULT (WINAPI *CreateDevice_t)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static CreateDevice_t o_CreateDevice;
static HRESULT WINAPI H_CreateDevice(IDirect3D9 *d3d, UINT ad, D3DDEVTYPE type, HWND wnd, DWORD fl, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    IDirect3D9Ex *ex = NULL;
    if (SUCCEEDED(IDirect3D9_QueryInterface(d3d, &kIID_D3D9Ex, (void **)&ex)) && ex) {
        D3DDISPLAYMODEEX m; memset(&m, 0, sizeof m); m.Size = sizeof m;
        if (pp && !pp->Windowed) { m.Width = pp->BackBufferWidth; m.Height = pp->BackBufferHeight; m.RefreshRate = pp->FullScreen_RefreshRateInHz; m.Format = pp->BackBufferFormat; m.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE; }
        IDirect3DDevice9Ex *dev = NULL;
        HRESULT hr = IDirect3D9Ex_CreateDeviceEx(ex, ad, type, wnd, fl, pp, (pp && !pp->Windowed) ? &m : NULL, &dev);
        IDirect3D9Ex_Release(ex);
        if (SUCCEEDED(hr) && dev) {
            Log("Direct3D 9Ex device made (%ux%u, %s, flags %08lX) - the game's pictures can go to the headset without a CPU copy",
                pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0, pp && pp->Windowed ? "windowed" : "full screen", (unsigned long)fl);
            Device_Hook((IDirect3DDevice9 *)dev);
            *out = (IDirect3DDevice9 *)dev; return hr;
        }
        Log("CreateDeviceEx failed (%08lX) - a normal Direct3D 9 device instead", (unsigned long)hr);
    }
    return o_CreateDevice(d3d, ad, type, wnd, fl, pp, out);
}

/* ---- Direct3DCreate9 -> Direct3DCreate9Ex ---- */
typedef IDirect3D9 *(WINAPI *Direct3DCreate9_t)(UINT);
typedef HRESULT (WINAPI *Direct3DCreate9Ex_t)(UINT, IDirect3D9Ex **);
static Direct3DCreate9_t o_Direct3DCreate9; static Direct3DCreate9Ex_t p_Direct3DCreate9Ex;
static IDirect3D9 *WINAPI H_Direct3DCreate9(UINT sdk)
{
    IDirect3D9Ex *ex = NULL;
    if (p_Direct3DCreate9Ex && SUCCEEDED(p_Direct3DCreate9Ex(sdk, &ex)) && ex) {
        IDirect3D9Vtbl *vt = (IDirect3D9Vtbl *)((IDirect3D9 *)ex)->lpVtbl;
        void *o = VtPatch((void **)&vt->CreateDevice, (void *)H_CreateDevice); if (o) o_CreateDevice = (CreateDevice_t)o;
        static int n = 0; if (n++ < 3) Log("Direct3DCreate9 -> Direct3D 9Ex");
        return (IDirect3D9 *)ex;
    }
    Log("Direct3DCreate9Ex unavailable - normal Direct3D 9");
    return o_Direct3DCreate9(sdk);
}

static bool Install(void)
{
    char ini[MAX_PATH]; GetModuleFileNameA(NULL, ini, MAX_PATH); { char *s = strrchr(ini, '\\'); if (s) s[1] = 0; } strcat(ini, "Data\\NVSE\\Plugins\\FNVR.ini");
    if (!GetPrivateProfileIntA("Stereo", "bD3D9Ex", 1, ini)) { Log("off ([Stereo] bD3D9Ex=0)"); return false; }
    HMODULE d9 = GetModuleHandleA("d3d9.dll"); if (!d9) d9 = LoadLibraryA("d3d9.dll");
    if (!d9) { Log("d3d9.dll not loaded"); return false; }
    char path[MAX_PATH], sys[MAX_PATH]; GetModuleFileNameA(d9, path, MAX_PATH); GetSystemDirectoryA(sys, MAX_PATH);
    char sysW[MAX_PATH]; GetSystemWow64DirectoryA(sysW, MAX_PATH);
    if (_strnicmp(path, sys, strlen(sys)) && (!sysW[0] || _strnicmp(path, sysW, strlen(sysW)))) {
        Log("the game uses %s - not Windows' own d3d9.dll (DXVK or ENB?). Direct3D 9Ex stays off: remove that d3d9.dll to use it", path);
        return false;
    }
    p_Direct3DCreate9Ex = (Direct3DCreate9Ex_t)GetProcAddress(d9, "Direct3DCreate9Ex");
    if (!p_Direct3DCreate9Ex) { Log("no Direct3DCreate9Ex in %s", path); return false; }
    o_Direct3DCreate9 = (Direct3DCreate9_t)HookFn((BYTE *)GetProcAddress(d9, "Direct3DCreate9"), (void *)H_Direct3DCreate9);
    if (!o_Direct3DCreate9) { Log("couldn't hook Direct3DCreate9"); return false; }
    Log("ready: the game's Direct3D 9 device will be made as Direct3D 9Ex (%s)", path);
    return true;
}

__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface *nvse, PluginInfo *info)
{
    info->infoVersion = 1; info->name = "FNVRStereo"; info->version = 1;
    return !nvse->isEditor;
}
__declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface *nvse)
{
    (void)nvse;
    Log("FNVR Stereo plugin (Direct3D 9Ex)");
    Install();
    return true;
}
