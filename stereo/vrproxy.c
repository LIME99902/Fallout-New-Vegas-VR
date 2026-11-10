/* FNVR Stereo - openvr_api.dll for the game, with OpenVR itself running in a separate 64-bit process.
   Copyright (C) 2026 FNVVRMC contributors. GPL-3.0-or-later.

   FNVR loads this instead of Valve's openvr_api.dll. It starts FNVR-VRHost.exe (64-bit), which loads the real OpenVR
   and SteamVR's client, so none of that lives in the game's 4 GB address space any more. Every OpenVR call FNVR makes
   is passed to the host through shared memory; pictures go across as shared D3D11 textures (the GPU copies them,
   nothing goes through the CPU). Only the calls FNVR makes are provided. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../src/openvr_capi.h"
#include "vripc.h"

static HANDLE g_shmMap, g_req, g_rsp, g_hostProc; static VrIpc *g_ipc; static CRITICAL_SECTION g_cs; static bool g_csInit, g_dead;
static FILE *g_log; static float g_sharpen = 0.0f;   /* (sharpening of the eye pictures in the host, 0 off .. 1 strongest) */
static void Log(const char *fmt, ...)
{
    if (!g_log) { char p[MAX_PATH]; GetModuleFileNameA(NULL, p, MAX_PATH); char *s = strrchr(p, '\\'); if (s) s[1] = 0; strcat(p, "FNVR_Stereo.log"); g_log = fopen(p, "w"); if (!g_log) return; }
    SYSTEMTIME t; GetLocalTime(&t); fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}

/* ---- the host process ---------------------------------------------------------------------------------------------- */
static bool Host_Start(void)
{
    if (g_ipc && !g_dead && g_hostProc && WaitForSingleObject(g_hostProc, 0) == WAIT_TIMEOUT) return true;
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    DWORD pid = GetCurrentProcessId(); char n[96];
    if (!g_ipc) {
        snprintf(n, sizeof n, VRIPC_SHM, (unsigned long)pid);
        g_shmMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(VrIpc), n);
        if (!g_shmMap) { Log("couldn't make the shared memory (%lu)", GetLastError()); return false; }
        g_ipc = (VrIpc *)MapViewOfFile(g_shmMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VrIpc));
        snprintf(n, sizeof n, VRIPC_REQ, (unsigned long)pid); g_req = CreateEventA(NULL, FALSE, FALSE, n);
        snprintf(n, sizeof n, VRIPC_RSP, (unsigned long)pid); g_rsp = CreateEventA(NULL, FALSE, FALSE, n);
        if (!g_ipc || !g_req || !g_rsp) { Log("couldn't set up talking to the host"); return false; }
    }
    memset(g_ipc, 0, sizeof *g_ipc); g_dead = false;
    char exe[MAX_PATH], dir[MAX_PATH]; HMODULE me = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Host_Start, &me);
    GetModuleFileNameA(me, dir, MAX_PATH); { char *s = strrchr(dir, '\\'); if (s) s[1] = 0; }
    snprintf(exe, sizeof exe, "%sx64\\FNVR-VRHost.exe", dir);
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) { Log("%s is missing", exe); return false; }
    char cmd[MAX_PATH + 64]; snprintf(cmd, sizeof cmd, "\"%s\" %lu", exe, (unsigned long)pid);
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si, 0, sizeof si); si.cb = sizeof si; memset(&pi, 0, sizeof pi);
    char wd[MAX_PATH]; snprintf(wd, sizeof wd, "%sx64", dir);
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, wd, &si, &pi)) { Log("couldn't start %s (%lu)", exe, GetLastError()); return false; }
    CloseHandle(pi.hThread); if (g_hostProc) CloseHandle(g_hostProc); g_hostProc = pi.hProcess;
    for (int i = 0; i < 500 && !g_ipc->hostReady; i++) { if (WaitForSingleObject(g_hostProc, 10) != WAIT_TIMEOUT) break; }
    if (!g_ipc->hostReady) { Log("the host didn't start"); return false; }
    Log("VR host running (process %lu, protocol %d) - OpenVR is out of the game's memory", (unsigned long)g_ipc->hostPid, g_ipc->version);
    return true;
}
/* one call to the host (the caller holds g_cs and has written ipc->in) */
static bool Call(int fn, DWORD timeoutMs)
{
    if (g_dead || !g_ipc) return false;
    g_ipc->fn = fn; SetEvent(g_req);
    HANDLE w[2] = { g_rsp, g_hostProc };
    DWORD r = WaitForMultipleObjects(2, w, FALSE, timeoutMs);
    if (r == WAIT_OBJECT_0) return true;
    if (r == WAIT_OBJECT_0 + 1) { g_dead = true; Log("the VR host closed"); }
    else { static int n = 0; if (n++ < 5) Log("call %d: no answer from the host in %lu ms", fn, timeoutMs); }
    return false;
}
#define BEGIN EnterCriticalSection(&g_cs); uint8_t *in = g_ipc ? g_ipc->in : NULL; uint8_t *out = g_ipc ? g_ipc->out : NULL; (void)in; (void)out; if (!g_ipc) { LeaveCriticalSection(&g_cs); goto fail; }
#define END   LeaveCriticalSection(&g_cs);

/* ---- textures: a shared copy the host can open ------------------------------------------------------------------------ */
typedef struct { ID3D11Texture2D *src, *copy; HANDLE h; UINT w, hgt; DXGI_FORMAT f; ID3D11Query *q; } ShTex;
static ShTex g_sh[16]; static int g_shN;
static bool Tex_Share(void *handle, int type, int cs, IpcTex *o)
{
    memset(o, 0, sizeof *o); o->type = type; o->colorSpace = cs;
    if (!handle || type != ETextureType_TextureType_DirectX) return false;
    ID3D11Texture2D *t = (ID3D11Texture2D *)handle;
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(t, &d);
    ID3D11Device *dev = NULL; ID3D11Texture2D_GetDevice(t, &dev); if (!dev) return false;
    ID3D11DeviceContext *ctx = NULL; ID3D11Device_GetImmediateContext(dev, &ctx);
    ShTex *e = NULL;
    for (int i = 0; i < g_shN; i++) if (g_sh[i].src == t) { e = &g_sh[i]; break; }
    if (e && (e->w != d.Width || e->hgt != d.Height || e->f != d.Format)) { if (e->copy) ID3D11Texture2D_Release(e->copy); e->copy = NULL; e->h = NULL; }
    if (!e) { e = &g_sh[g_shN < 16 ? g_shN++ : 15]; memset(e, 0, sizeof *e); e->src = t; }
    if (!e->h) {
        e->w = d.Width; e->hgt = d.Height; e->f = d.Format;
        ID3D11Texture2D *src = t;
        if (!(d.MiscFlags & D3D11_RESOURCE_MISC_SHARED) || d.SampleDesc.Count > 1) {   /* (FNVR's own textures aren't shared: a shared copy of each) */
            D3D11_TEXTURE2D_DESC c = d; c.MipLevels = 1; c.ArraySize = 1; c.SampleDesc.Count = 1; c.SampleDesc.Quality = 0; c.Usage = D3D11_USAGE_DEFAULT;
            c.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; c.CPUAccessFlags = 0; c.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
            if (c.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS) c.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            if (c.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS) c.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            HRESULT hr = ID3D11Device_CreateTexture2D(dev, &c, NULL, &e->copy);
            if (FAILED(hr)) { Log("couldn't make a shared copy of a %ux%u picture (format %d, error %08lX)", d.Width, d.Height, (int)d.Format, (unsigned long)hr); e->copy = NULL; ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev); return false; }
            src = e->copy;
        }
        IDXGIResource *r = NULL;
        if (SUCCEEDED(ID3D11Texture2D_QueryInterface(src, &IID_IDXGIResource, (void **)&r)) && r) { IDXGIResource_GetSharedHandle(r, &e->h); IDXGIResource_Release(r); }
        if (!e->h) { Log("no shared handle for a picture"); ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev); return false; }
        D3D11_QUERY_DESC qd = { D3D11_QUERY_EVENT, 0 }; if (!e->q) ID3D11Device_CreateQuery(dev, &qd, &e->q);
        Log("picture %ux%u (format %d) shared with the host%s", d.Width, d.Height, (int)d.Format, e->copy ? " through a copy" : "");
    }
    if (e->copy) {
        if (d.SampleDesc.Count > 1) ID3D11DeviceContext_ResolveSubresource(ctx, (ID3D11Resource *)e->copy, 0, (ID3D11Resource *)t, 0, e->f);
        else ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)e->copy, 0, 0, 0, 0, (ID3D11Resource *)t, 0, NULL);
    }
    /* finished on the GPU before the host reads it */
    if (e->q) { ID3D11DeviceContext_End(ctx, (ID3D11Asynchronous *)e->q); ID3D11DeviceContext_Flush(ctx);
        LARGE_INTEGER f, a, b; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
        while (ID3D11DeviceContext_GetData(ctx, (ID3D11Asynchronous *)e->q, NULL, 0, 0) == S_FALSE) { QueryPerformanceCounter(&b); if ((b.QuadPart - a.QuadPart) * 1000 / f.QuadPart > 30) break; YieldProcessor(); } }
    else ID3D11DeviceContext_Flush(ctx);
    o->shared = (uint64_t)(uintptr_t)e->h; o->w = d.Width; o->h = d.Height; o->fmt = (uint32_t)d.Format;
    ID3D11DeviceContext_Release(ctx); ID3D11Device_Release(dev);
    return true;
}

/* ---- IVRSystem ---------------------------------------------------------------------------------------------------------- */
#define CT OPENVR_FNTABLE_CALLTYPE
static void CT S_GetRecommendedRenderTargetSize(uint32_t *w, uint32_t *h)
{ BEGIN if (Call(S_RTSIZE, 3000)) { if (w) memcpy(w, out, 4); if (h) memcpy(h, out + 4, 4); } END return; fail: if (w) *w = 0; if (h) *h = 0; }
static struct HmdMatrix44_t CT S_GetProjectionMatrix(EVREye e, float n, float f)
{ struct HmdMatrix44_t m; memset(&m, 0, sizeof m); BEGIN memcpy(in, &e, 4); memcpy(in + 4, &n, 4); memcpy(in + 8, &f, 4); if (Call(S_PROJMAT, 3000)) memcpy(&m, out, sizeof m); END fail: return m; }
static void CT S_GetProjectionRaw(EVREye e, float *l, float *r, float *t, float *b)
{ float v[4] = { -1, 1, -1, 1 }; BEGIN memcpy(in, &e, 4); if (Call(S_PROJRAW, 3000)) memcpy(v, out, 16); END fail: if (l) *l = v[0]; if (r) *r = v[1]; if (t) *t = v[2]; if (b) *b = v[3]; }
static struct HmdMatrix34_t CT S_GetEyeToHeadTransform(EVREye e)
{ struct HmdMatrix34_t m; memset(&m, 0, sizeof m); m.m[0][0] = m.m[1][1] = m.m[2][2] = 1; BEGIN memcpy(in, &e, 4); if (Call(S_EYE2HEAD, 3000)) memcpy(&m, out, sizeof m); END fail: return m; }
static void CT S_GetDXGIOutputInfo(int32_t *idx)
{ int32_t v = -1; BEGIN if (Call(S_DXGI, 3000)) memcpy(&v, out, 4); END fail: if (idx) *idx = v; }
static void CT S_GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin o, float pred, struct TrackedDevicePose_t *p, uint32_t n)
{
    if (!p || !n) return; if (n > 64) n = 64; memset(p, 0, sizeof *p * n);
    BEGIN memcpy(in, &o, 4); memcpy(in + 4, &pred, 4); memcpy(in + 8, &n, 4);
    if (Call(S_POSES, 3000)) memcpy(p, out, sizeof *p * n); END fail: return;
}
static TrackedDeviceIndex_t CT S_GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole r)
{ uint32_t v = k_unTrackedDeviceIndexInvalid; BEGIN memcpy(in, &r, 4); if (Call(S_ROLEIDX, 3000)) memcpy(&v, out, 4); END fail: return v; }
static float CT S_GetFloatTrackedDeviceProperty(TrackedDeviceIndex_t d, ETrackedDeviceProperty pr, ETrackedPropertyError *err)
{ float v = 0; int32_t e = ETrackedPropertyError_TrackedProp_UnknownProperty; BEGIN memcpy(in, &d, 4); memcpy(in + 4, &pr, 4); if (Call(S_FLOATPROP, 3000)) { memcpy(&v, out, 4); memcpy(&e, out + 4, 4); } END fail: if (err) *err = (ETrackedPropertyError)e; return v; }
static int32_t CT S_GetInt32TrackedDeviceProperty(TrackedDeviceIndex_t d, ETrackedDeviceProperty pr, ETrackedPropertyError *err)
{ int32_t v = 0; int32_t e = ETrackedPropertyError_TrackedProp_UnknownProperty; BEGIN memcpy(in, &d, 4); memcpy(in + 4, &pr, 4); if (Call(S_INT32PROP, 3000)) { memcpy(&v, out, 4); memcpy(&e, out + 4, 4); } END fail: if (err) *err = (ETrackedPropertyError)e; return v; }
static bool CT S_GetBoolTrackedDeviceProperty(TrackedDeviceIndex_t d, ETrackedDeviceProperty pr, ETrackedPropertyError *err)
{ int32_t v = 0; int32_t e = ETrackedPropertyError_TrackedProp_UnknownProperty; BEGIN memcpy(in, &d, 4); memcpy(in + 4, &pr, 4); if (Call(S_BOOLPROP, 3000)) { memcpy(&v, out, 4); memcpy(&e, out + 4, 4); } END fail: if (err) *err = (ETrackedPropertyError)e; return v != 0; }
static uint32_t CT S_GetStringTrackedDeviceProperty(TrackedDeviceIndex_t d, ETrackedDeviceProperty pr, char *buf, uint32_t n, ETrackedPropertyError *err)
{
    uint32_t len = 0; int32_t e = ETrackedPropertyError_TrackedProp_UnknownProperty; if (n > 4000) n = 4000;
    BEGIN memcpy(in, &d, 4); memcpy(in + 4, &pr, 4); memcpy(in + 8, &n, 4);
    if (Call(S_STRINGPROP, 3000)) { memcpy(&len, out, 4); memcpy(&e, out + 4, 4); if (buf && n) { memcpy(buf, out + 8, n); buf[n - 1] = 0; } } END
fail: if (err) *err = (ETrackedPropertyError)e; return len;
}
static bool CT S_IsTrackedDeviceConnected(TrackedDeviceIndex_t d)
{ int32_t v = 0; BEGIN memcpy(in, &d, 4); if (Call(S_CONNECTED, 3000)) memcpy(&v, out, 4); END fail: return v != 0; }
static ETrackedDeviceClass CT S_GetTrackedDeviceClass(TrackedDeviceIndex_t d)
{ int32_t v = 0; BEGIN memcpy(in, &d, 4); if (Call(S_DEVCLASS, 3000)) memcpy(&v, out, 4); END fail: return (ETrackedDeviceClass)v; }
static bool CT S_PollNextEvent(struct VREvent_t *ev, uint32_t size)
{
    int32_t got = 0; if (!ev || size > 1024) return false;
    BEGIN memcpy(in, &size, 4); if (Call(S_EVENT, 3000)) { memcpy(&got, out, 4); if (got) memcpy(ev, out + 8, size); } END fail: return got != 0;
}
static bool CT S_GetControllerState(TrackedDeviceIndex_t d, VRControllerState_t *st, uint32_t size)
{
    int32_t ok = 0; if (!st || size > 512) return false;
    BEGIN memcpy(in, &d, 4); memcpy(in + 4, &size, 4); if (Call(S_CTRLSTATE, 3000)) { memcpy(&ok, out, 4); memcpy(st, out + 8, size); } END fail: return ok != 0;
}
static void CT S_TriggerHapticPulse(TrackedDeviceIndex_t d, uint32_t axis, unsigned short us)
{ BEGIN uint32_t u = us; memcpy(in, &d, 4); memcpy(in + 4, &axis, 4); memcpy(in + 8, &u, 4); Call(S_HAPTIC, 1000); END fail: return; }
static void CT S_AcknowledgeQuit_Exiting(void) { BEGIN Call(S_ACKQUIT, 3000); END fail: return; }
static bool CT S_GetTimeSinceLastVsync(float *s, uint64_t *fc)
{ int32_t ok = 0; BEGIN if (Call(S_VSYNC, 3000)) { memcpy(&ok, out, 4); if (s) memcpy(s, out + 4, 4); if (fc) memcpy(fc, out + 8, 8); } END fail: return ok != 0; }

/* ---- IVRCompositor ------------------------------------------------------------------------------------------------------ */
static void CT C_SetTrackingSpace(ETrackingUniverseOrigin o) { BEGIN memcpy(in, &o, 4); Call(C_TRACKSPACE, 3000); END fail: return; }
static EVRCompositorError CT C_WaitGetPoses(struct TrackedDevicePose_t *rp, uint32_t rn, struct TrackedDevicePose_t *gp, uint32_t gn)
{
    int32_t err = EVRCompositorError_VRCompositorError_DoNotHaveFocus; if (rn > 64) rn = 64; if (gn > 64) gn = 64;
    BEGIN memcpy(in, &rn, 4); memcpy(in + 4, &gn, 4);
    if (Call(C_WAITPOSES, 2000)) { memcpy(&err, out, 4); if (rp && rn) memcpy(rp, out + 8, sizeof *rp * rn); if (gp && gn) memcpy(gp, out + 8 + sizeof *rp * 64, sizeof *gp * gn); } END
fail: return (EVRCompositorError)err;
}
static EVRCompositorError CT C_Submit(EVREye eye, struct Texture_t *t, struct VRTextureBounds_t *b, EVRSubmitFlags fl)
{
    int32_t err = EVRCompositorError_VRCompositorError_InvalidTexture;
    if (!t) return (EVRCompositorError)err;
    IpcTex it; HmdMatrix34_t pose; memset(&pose, 0, sizeof pose);
    if (fl & EVRSubmitFlags_Submit_TextureWithPose) pose = ((VRTextureWithPose_t *)t)->mDeviceToAbsoluteTracking;
    BEGIN
    if (!Tex_Share(t->handle, t->eType, t->eColorSpace, &it)) { END return (EVRCompositorError)err; }
    int32_t hasB = b ? 1 : 0, e32 = eye, f32 = fl;
    memcpy(in, &e32, 4); memcpy(in + 4, &f32, 4); memcpy(in + 8, &it, sizeof it); memcpy(in + 8 + sizeof it, &hasB, 4);
    if (b) memcpy(in + 12 + sizeof it, b, sizeof *b);
    memcpy(in + 12 + sizeof it + sizeof(VRTextureBounds_t), &pose, sizeof pose);
    { float z = 0; memcpy(in + 12 + sizeof it + sizeof(VRTextureBounds_t) + sizeof pose, &z, 4); }
    if (Call(C_SUBMIT, 2000)) memcpy(&err, out, 4);
    END
fail: return (EVRCompositorError)err;
}
static uint32_t CT C_GetCurrentSceneFocusProcess(void)
{ uint32_t v = 0; BEGIN if (Call(C_FOCUS, 3000)) memcpy(&v, out, 4); END fail: if (v && g_ipc && v == g_ipc->hostPid) v = GetCurrentProcessId(); return v; }   /* (the host is the game, as far as SteamVR knows) */
static void CT C_PostPresentHandoff(void) { BEGIN Call(C_POSTPRESENT, 2000); END fail: return; }

/* ---- IVROverlay --------------------------------------------------------------------------------------------------------- */
static EVROverlayError CT O_CreateOverlay(char *key, char *name, VROverlayHandle_t *h)
{
    int32_t err = EVROverlayError_VROverlayError_RequestFailed; uint64_t hv = 0;
    BEGIN snprintf((char *)in, 256, "%s", key ? key : ""); snprintf((char *)in + 256, 256, "%s", name ? name : "");
    if (Call(O_CREATE, 3000)) { memcpy(&err, out, 4); memcpy(&hv, out + 8, 8); } END
fail: if (h) *h = hv; return (EVROverlayError)err;
}
#define OV1(NAME, ID) static EVROverlayError CT NAME(VROverlayHandle_t h) { int32_t err = EVROverlayError_VROverlayError_RequestFailed; BEGIN memcpy(in, &h, 8); if (Call(ID, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
OV1(O_HideOverlay, O_HIDE) OV1(O_ShowOverlay, O_SHOW) OV1(O_DestroyOverlay, O_DESTROY)
#define OVF(NAME, ID) static EVROverlayError CT NAME(VROverlayHandle_t h, float v) { int32_t err = EVROverlayError_VROverlayError_RequestFailed; BEGIN memcpy(in, &h, 8); memcpy(in + 8, &v, 4); if (Call(ID, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
OVF(O_SetOverlayCurvature, O_CURVE) OVF(O_SetOverlayWidthInMeters, O_WIDTH) OVF(O_SetOverlayAlpha, O_ALPHA)
static EVROverlayError CT O_SetOverlayFlag(VROverlayHandle_t h, VROverlayFlags f, bool on)
{ int32_t err = EVROverlayError_VROverlayError_RequestFailed; BEGIN int32_t a = f, b = on; memcpy(in, &h, 8); memcpy(in + 8, &a, 4); memcpy(in + 12, &b, 4); if (Call(O_FLAG, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
static EVROverlayError CT O_SetOverlaySortOrder(VROverlayHandle_t h, uint32_t s)
{ int32_t err = EVROverlayError_VROverlayError_RequestFailed; BEGIN memcpy(in, &h, 8); memcpy(in + 8, &s, 4); if (Call(O_SORT, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
static EVROverlayError CT O_SetOverlayColor(VROverlayHandle_t h, float r, float g, float b)
{ int32_t err = EVROverlayError_VROverlayError_RequestFailed; BEGIN memcpy(in, &h, 8); memcpy(in + 8, &r, 4); memcpy(in + 12, &g, 4); memcpy(in + 16, &b, 4); if (Call(O_COLOR, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
static EVROverlayError CT O_SetOverlayTexture(VROverlayHandle_t h, struct Texture_t *t)
{
    int32_t err = EVROverlayError_VROverlayError_InvalidTexture; IpcTex it;
    if (!t) return (EVROverlayError)err;
    BEGIN
    if (!Tex_Share(t->handle, t->eType, t->eColorSpace, &it)) { END return (EVROverlayError)err; }
    memcpy(in, &h, 8); memcpy(in + 8, &it, sizeof it);
    if (Call(O_TEXTURE, 3000)) memcpy(&err, out, 4);
    END
fail: return (EVROverlayError)err;
}
static EVROverlayError CT O_SetOverlayTransformAbsolute(VROverlayHandle_t h, ETrackingUniverseOrigin o, struct HmdMatrix34_t *m)
{ int32_t err = EVROverlayError_VROverlayError_RequestFailed; if (!m) return (EVROverlayError)err; BEGIN memcpy(in, &h, 8); memcpy(in + 8, &o, 4); memcpy(in + 12, m, sizeof *m); if (Call(O_XFORMABS, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }
static EVROverlayError CT O_SetOverlayTransformTrackedDeviceRelative(VROverlayHandle_t h, TrackedDeviceIndex_t d, struct HmdMatrix34_t *m)
{ int32_t err = EVROverlayError_VROverlayError_RequestFailed; if (!m) return (EVROverlayError)err; BEGIN memcpy(in, &h, 8); memcpy(in + 8, &d, 4); memcpy(in + 12, m, sizeof *m); if (Call(O_XFORMDEV, 3000)) memcpy(&err, out, 4); END fail: return (EVROverlayError)err; }

/* ---- the tables ---------------------------------------------------------------------------------------------------------- */
static struct VR_IVRSystem_FnTable g_sys; static struct VR_IVRCompositor_FnTable g_comp; static struct VR_IVROverlay_FnTable g_ov; static bool g_tables;
static void Tables(void)
{
    if (g_tables) return; g_tables = true;
    g_sys.GetRecommendedRenderTargetSize = S_GetRecommendedRenderTargetSize; g_sys.GetProjectionMatrix = S_GetProjectionMatrix; g_sys.GetProjectionRaw = S_GetProjectionRaw;
    g_sys.GetEyeToHeadTransform = S_GetEyeToHeadTransform; g_sys.GetDXGIOutputInfo = S_GetDXGIOutputInfo; g_sys.GetDeviceToAbsoluteTrackingPose = S_GetDeviceToAbsoluteTrackingPose;
    g_sys.GetTrackedDeviceIndexForControllerRole = S_GetTrackedDeviceIndexForControllerRole; g_sys.GetFloatTrackedDeviceProperty = S_GetFloatTrackedDeviceProperty;
    g_sys.GetInt32TrackedDeviceProperty = S_GetInt32TrackedDeviceProperty; g_sys.GetBoolTrackedDeviceProperty = S_GetBoolTrackedDeviceProperty; g_sys.GetStringTrackedDeviceProperty = S_GetStringTrackedDeviceProperty;
    g_sys.IsTrackedDeviceConnected = S_IsTrackedDeviceConnected; g_sys.GetTrackedDeviceClass = S_GetTrackedDeviceClass;
    g_sys.PollNextEvent = S_PollNextEvent; g_sys.GetControllerState = S_GetControllerState; g_sys.TriggerHapticPulse = S_TriggerHapticPulse;
    g_sys.AcknowledgeQuit_Exiting = S_AcknowledgeQuit_Exiting; g_sys.GetTimeSinceLastVsync = S_GetTimeSinceLastVsync;
    g_comp.SetTrackingSpace = C_SetTrackingSpace; g_comp.WaitGetPoses = C_WaitGetPoses; g_comp.Submit = C_Submit; g_comp.GetCurrentSceneFocusProcess = C_GetCurrentSceneFocusProcess; g_comp.PostPresentHandoff = C_PostPresentHandoff;
    g_ov.CreateOverlay = O_CreateOverlay; g_ov.HideOverlay = O_HideOverlay; g_ov.ShowOverlay = O_ShowOverlay; g_ov.DestroyOverlay = O_DestroyOverlay;
    g_ov.SetOverlayCurvature = O_SetOverlayCurvature; g_ov.SetOverlayWidthInMeters = O_SetOverlayWidthInMeters; g_ov.SetOverlayAlpha = O_SetOverlayAlpha;
    g_ov.SetOverlayFlag = O_SetOverlayFlag; g_ov.SetOverlaySortOrder = O_SetOverlaySortOrder; g_ov.SetOverlayColor = O_SetOverlayColor; g_ov.SetOverlayTexture = O_SetOverlayTexture;
    g_ov.SetOverlayTransformAbsolute = O_SetOverlayTransformAbsolute; g_ov.SetOverlayTransformTrackedDeviceRelative = O_SetOverlayTransformTrackedDeviceRelative;
}

/* ---- the exports (same as Valve's openvr_api.dll) ------------------------------------------------------------------------ */
__declspec(dllexport) intptr_t VR_InitInternal2(EVRInitError *err, EVRApplicationType type, const char *startup)
{
    (void)startup; EVRInitError e = EVRInitError_VRInitError_Init_InstallationNotFound;
    if (!Host_Start()) { Log("VR host unavailable - FNVR will use OpenVR inside the game instead"); e = (EVRInitError)1001; if (err) *err = e; return 0; }
    EnterCriticalSection(&g_cs);
    int32_t t = type; memcpy(g_ipc->in, &t, 4);
    if (Call(F_INIT, 15000)) memcpy(&e, g_ipc->out, 4);
    LeaveCriticalSection(&g_cs);
    Log("SteamVR connect (app type %d): %s%s", (int)type, e == EVRInitError_VRInitError_None ? "ok" : "error ", e == EVRInitError_VRInitError_None ? "" : g_ipc->hostLog);
    Tables();
    if (err) *err = e; return e == EVRInitError_VRInitError_None ? 1 : 0;
}
__declspec(dllexport) intptr_t VR_InitInternal(EVRInitError *err, EVRApplicationType type) { return VR_InitInternal2(err, type, ""); }
__declspec(dllexport) void VR_ShutdownInternal(void)
{
    if (!g_ipc || g_dead) return;
    EnterCriticalSection(&g_cs); Call(F_SHUTDOWN, 5000); LeaveCriticalSection(&g_cs);
    for (int i = 0; i < g_shN; i++) { if (g_sh[i].copy) ID3D11Texture2D_Release(g_sh[i].copy); if (g_sh[i].q) ID3D11Query_Release(g_sh[i].q); }
    g_shN = 0; Log("SteamVR disconnected");
}
__declspec(dllexport) intptr_t VR_GetGenericInterface(const char *ver, EVRInitError *err)
{
    Tables(); if (err) *err = EVRInitError_VRInitError_None;
    if (ver && !strncmp(ver, "FnTable:IVRSystem_", 18)) return (intptr_t)&g_sys;
    if (ver && !strncmp(ver, "FnTable:IVRCompositor_", 22)) return (intptr_t)&g_comp;
    if (ver && !strncmp(ver, "FnTable:IVROverlay_", 19)) return (intptr_t)&g_ov;
    Log("interface %s isn't passed to the host", ver ? ver : "?");
    if (err) *err = EVRInitError_VRInitError_Init_InterfaceNotFound; return 0;
}
/* FNVR's zero copy: a Direct3D 9Ex shared picture (already finished on the GPU) for one eye */
__declspec(dllexport) uint32_t FNVRStereo_SubmitShared9(int eye, uint64_t handle, uint32_t w, uint32_t h, const VRTextureBounds_t *b, const HmdMatrix34_t *pose)
{
    int32_t err = EVRCompositorError_VRCompositorError_InvalidTexture;
    if (!g_ipc || g_dead || !handle) return (uint32_t)err;
    IpcTex it; memset(&it, 0, sizeof it); it.shared = handle; it.type = ETextureType_TextureType_DirectX; it.colorSpace = EColorSpace_ColorSpace_Gamma; it.w = w; it.h = h; it.fmt = 87;
    HmdMatrix34_t p; memset(&p, 0, sizeof p); if (pose) p = *pose;
    EnterCriticalSection(&g_cs);
    int32_t e32 = eye, f32 = pose ? EVRSubmitFlags_Submit_TextureWithPose : 0, hasB = b ? 1 : 0; uint8_t *in = g_ipc->in;
    memcpy(in, &e32, 4); memcpy(in + 4, &f32, 4); memcpy(in + 8, &it, sizeof it); memcpy(in + 8 + sizeof it, &hasB, 4);
    if (b) memcpy(in + 12 + sizeof it, b, sizeof *b);
    memcpy(in + 12 + sizeof it + sizeof(VRTextureBounds_t), &p, sizeof p);
    memcpy(in + 12 + sizeof it + sizeof(VRTextureBounds_t) + sizeof p, &g_sharpen, 4);
    if (Call(C_SUBMIT, 2000)) memcpy(&err, g_ipc->out, 4);
    LeaveCriticalSection(&g_cs);
    static bool said = false; if (!said) { said = true; Log("zero copy: Direct3D 9Ex pictures submitted straight from the game's GPU memory"); }
    return (uint32_t)err;
}
/* FNVR's zero copy for overlays: a Direct3D 9Ex shared picture (finished on the GPU) for an overlay */
__declspec(dllexport) uint32_t FNVRStereo_SetOverlayShared9(uint64_t overlay, uint64_t handle, uint32_t w, uint32_t h, int colorSpace)
{
    int32_t err = EVROverlayError_VROverlayError_InvalidTexture;
    if (!g_ipc || g_dead || !handle) return (uint32_t)err;
    IpcTex it; memset(&it, 0, sizeof it); it.shared = handle; it.type = ETextureType_TextureType_DirectX; it.colorSpace = colorSpace; it.w = w; it.h = h;
    EnterCriticalSection(&g_cs);
    memcpy(g_ipc->in, &overlay, 8); memcpy(g_ipc->in + 8, &it, sizeof it);
    if (Call(O_TEXTURE, 2000)) memcpy(&err, g_ipc->out, 4);
    LeaveCriticalSection(&g_cs);
    return (uint32_t)err;
}
__declspec(dllexport) void FNVRStereo_SetSharpen(float s) { g_sharpen = s < 0 ? 0 : s > 1 ? 1 : s; }
__declspec(dllexport) bool VR_IsRuntimeInstalled(void) { return true; }
__declspec(dllexport) bool VR_IsHmdPresent(void) { return true; }
__declspec(dllexport) bool VR_IsInterfaceVersionValid(const char *v) { (void)v; return true; }
__declspec(dllexport) uint32_t VR_GetInitToken(void) { return 1; }
__declspec(dllexport) const char *VR_GetVRInitErrorAsEnglishDescription(EVRInitError e)
{ static char b[300]; if ((int)e == 1001) return "the VR host (FNVR-VRHost.exe) couldn't start"; snprintf(b, sizeof b, "error %d %s", (int)e, g_ipc ? g_ipc->hostLog : ""); return b; }
__declspec(dllexport) const char *VR_GetVRInitErrorAsSymbol(EVRInitError e) { return VR_GetVRInitErrorAsEnglishDescription(e); }
__declspec(dllexport) const char *VR_RuntimePath(void) { return ""; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p)
{
    (void)h; (void)p;
    if (r == DLL_PROCESS_DETACH && g_hostProc && g_ipc && !g_dead) { g_ipc->fn = F_SHUTDOWN; SetEvent(g_req); }   /* (the host also closes when the game does) */
    return TRUE;
}
