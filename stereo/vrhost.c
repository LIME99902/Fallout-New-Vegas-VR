/* FNVR Stereo - FNVR-VRHost.exe: the 64-bit process that runs OpenVR for the game.
   Copyright (C) 2026 FNVVRMC contributors. GPL-3.0-or-later.

   Started by the game's openvr_api.dll (vrproxy.c). It loads Valve's 64-bit openvr_api.dll, connects to SteamVR as the
   game's scene application, answers every call the game sends through shared memory, and shows the game's pictures:
   the game shares its D3D11 textures, this process opens them on its own D3D11 device and hands them to SteamVR.
   It closes by itself when the game does. */
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

typedef intptr_t (*VR_InitInternal2_t)(EVRInitError *err, EVRApplicationType type, const char *startupInfo);
typedef void (*VR_ShutdownInternal_t)(void);
typedef intptr_t (*VR_GetGenericInterface_t)(const char *ver, EVRInitError *err);
typedef const char *(*VR_ErrStr_t)(EVRInitError err);

static VrIpc *g_ipc; static FILE *g_log;
static struct VR_IVRSystem_FnTable *S; static struct VR_IVRCompositor_FnTable *C; static struct VR_IVROverlay_FnTable *O;
static VR_InitInternal2_t pInit; static VR_ShutdownInternal_t pShut; static VR_GetGenericInterface_t pGet; static VR_ErrStr_t pErr;
static ID3D11Device *g_dev; static ID3D11DeviceContext *g_ctx;
typedef struct { uint64_t h; ID3D11Texture2D *t; } Opened;
static Opened g_open[32]; static int g_openN;

static void Log(const char *fmt, ...)
{
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t); fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a; va_start(a, fmt); vfprintf(g_log, fmt, a); va_end(a); fputc('\n', g_log); fflush(g_log);
}
static bool Dev_Make(void)
{
    if (g_dev) return true;
    int32_t idx = -1; if (S) S->GetDXGIOutputInfo(&idx);
    IDXGIFactory1 *f = NULL; IDXGIAdapter1 *ad = NULL;
    if (idx >= 0 && SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&f)) && f) { IDXGIFactory1_EnumAdapters1(f, (UINT)idx, &ad); }
    D3D_FEATURE_LEVEL fl[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDevice((IDXGIAdapter *)ad, ad ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL, 0, fl, 3, D3D11_SDK_VERSION, &g_dev, NULL, &g_ctx);
    if (ad) IDXGIAdapter1_Release(ad); if (f) IDXGIFactory1_Release(f);
    if (FAILED(hr)) { Log("D3D11 device failed (%08lX)", (unsigned long)hr); g_dev = NULL; return false; }
    Log("D3D11 device on adapter %d", idx);
    return true;
}
static ID3D11Texture2D *Tex_Open(const IpcTex *it)
{
    if (!it->shared || !Dev_Make()) return NULL;
    for (int i = 0; i < g_openN; i++) if (g_open[i].h == it->shared) {
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(g_open[i].t, &d);
        if (d.Width == it->w && d.Height == it->h) return g_open[i].t;
        ID3D11Texture2D_Release(g_open[i].t); g_open[i] = g_open[--g_openN]; break;
    }
    ID3D11Texture2D *t = NULL;
    HRESULT hr = ID3D11Device_OpenSharedResource(g_dev, (HANDLE)(uintptr_t)it->shared, &IID_ID3D11Texture2D, (void **)&t);
    if (FAILED(hr) || !t) { static int n = 0; if (n++ < 10) Log("couldn't open the game's picture %llX (%08lX)", (unsigned long long)it->shared, (unsigned long)hr); return NULL; }
    if (g_openN >= 32) { ID3D11Texture2D_Release(g_open[0].t); memmove(g_open, g_open + 1, sizeof g_open[0] * 31); g_openN = 31; }
    g_open[g_openN].h = it->shared; g_open[g_openN].t = t; g_openN++;
    Log("opened the game's picture %ux%u", it->w, it->h);
    return t;
}


/* ---- Sharpening (AMD's Contrast Adaptive Sharpening, MIT licence) -------------------------------------------------------
   When the game draws below the headset's resolution, its picture is scaled up and comes out soft; this pass sharpens it
   on the way to SteamVR, on this process's own D3D11 device. Strength from the game (0 = off). */
typedef HRESULT (WINAPI *D3DCompile_t)(const void *, SIZE_T, const char *, const void *, void *, const char *, const char *, UINT, UINT, ID3DBlob **, ID3DBlob **);
static ID3D11VertexShader *g_vs; static ID3D11PixelShader *g_ps; static ID3D11Buffer *g_cb; static bool g_shTried, g_shOk;
static struct { ID3D11Texture2D *t[2]; ID3D11RenderTargetView *rtv[2]; UINT w, h; DXGI_FORMAT f; int k; } g_sharp[2];
static struct { ID3D11Texture2D *t; ID3D11ShaderResourceView *srv; } g_srv[8]; static int g_srvN;
static const char *k_sharpHlsl =
"cbuffer C : register(b0) { float peak; float3 pad; };\n"
"Texture2D T : register(t0);\n"
"float4 VS(uint id : SV_VertexID) : SV_Position { float2 uv = float2((id << 1) & 2, id & 2); return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); }\n"
"float4 PS(float4 pos : SV_Position) : SV_Target {\n"
"  int2 p = int2(pos.xy);\n"
"  float4 cc = T.Load(int3(p, 0)); float3 c = cc.rgb;\n"
"  float3 a = T.Load(int3(p + int2(0, -1), 0)).rgb, b = T.Load(int3(p + int2(-1, 0), 0)).rgb;\n"
"  float3 d = T.Load(int3(p + int2(1, 0), 0)).rgb, e = T.Load(int3(p + int2(0, 1), 0)).rgb;\n"
"  float3 mn = min(min(min(a, b), min(d, e)), c), mx = max(max(max(a, b), max(d, e)), c);\n"
"  float3 amp = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-5)));\n"
"  float3 w = amp * peak;\n"
"  float3 r = (c + (a + b + d + e) * w) / (1.0 + 4.0 * w);\n"
"  return float4(saturate(r), cc.a);\n"
"}\n";
static bool Sharp_Init(void)
{
    if (g_shTried) return g_shOk; g_shTried = true;
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll"); D3DCompile_t comp = dc ? (D3DCompile_t)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!comp) { Log("sharpening off: d3dcompiler_47.dll missing"); return false; }
    ID3DBlob *vb = NULL, *pb = NULL, *er = NULL;
    if (FAILED(comp(k_sharpHlsl, strlen(k_sharpHlsl), "sharp", NULL, NULL, "VS", "vs_4_0", 0, 0, &vb, &er)) ||
        FAILED(comp(k_sharpHlsl, strlen(k_sharpHlsl), "sharp", NULL, NULL, "PS", "ps_4_0", 0, 0, &pb, &er))) {
        Log("sharpening off: shader didn't compile (%s)", er ? (const char *)ID3D10Blob_GetBufferPointer(er) : "?"); return false; }
    ID3D11Device_CreateVertexShader(g_dev, ID3D10Blob_GetBufferPointer(vb), ID3D10Blob_GetBufferSize(vb), NULL, &g_vs);
    ID3D11Device_CreatePixelShader(g_dev, ID3D10Blob_GetBufferPointer(pb), ID3D10Blob_GetBufferSize(pb), NULL, &g_ps);
    D3D11_BUFFER_DESC bd = { 16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
    ID3D11Device_CreateBuffer(g_dev, &bd, NULL, &g_cb);
    g_shOk = g_vs && g_ps && g_cb; Log("sharpening %s", g_shOk ? "ready" : "off (shader setup failed)");
    return g_shOk;
}
static ID3D11Texture2D *Sharp_Apply(ID3D11Texture2D *src, int eye, float amount)
{
    if (amount <= 0.001f || eye < 0 || eye > 1 || !Sharp_Init()) return src;
    D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(src, &d);
    ID3D11ShaderResourceView *srv = NULL;
    for (int i = 0; i < g_srvN; i++) if (g_srv[i].t == src) srv = g_srv[i].srv;
    if (!srv) { if (FAILED(ID3D11Device_CreateShaderResourceView(g_dev, (ID3D11Resource *)src, NULL, &srv))) return src;
        int k = g_srvN < 8 ? g_srvN++ : 7; if (g_srv[k].srv && k == 7) ID3D11ShaderResourceView_Release(g_srv[k].srv); g_srv[k].t = src; g_srv[k].srv = srv; }
    if (g_sharp[eye].w != d.Width || g_sharp[eye].h != d.Height || g_sharp[eye].f != d.Format || !g_sharp[eye].t[0]) {
        for (int k = 0; k < 2; k++) { if (g_sharp[eye].rtv[k]) ID3D11RenderTargetView_Release(g_sharp[eye].rtv[k]); if (g_sharp[eye].t[k]) ID3D11Texture2D_Release(g_sharp[eye].t[k]); g_sharp[eye].t[k] = NULL; g_sharp[eye].rtv[k] = NULL; }
        D3D11_TEXTURE2D_DESC o = d; o.MipLevels = 1; o.ArraySize = 1; o.SampleDesc.Count = 1; o.SampleDesc.Quality = 0; o.Usage = D3D11_USAGE_DEFAULT;
        o.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; o.CPUAccessFlags = 0; o.MiscFlags = 0;
        for (int k = 0; k < 2; k++) { if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &o, NULL, &g_sharp[eye].t[k]))) return src; ID3D11Device_CreateRenderTargetView(g_dev, (ID3D11Resource *)g_sharp[eye].t[k], NULL, &g_sharp[eye].rtv[k]); }
        g_sharp[eye].w = d.Width; g_sharp[eye].h = d.Height; g_sharp[eye].f = d.Format;
        Log("sharpening eye %d: %ux%u", eye, d.Width, d.Height);
    }
    int k = g_sharp[eye].k ^= 1;
    D3D11_MAPPED_SUBRESOURCE m; if (SUCCEEDED(ID3D11DeviceContext_Map(g_ctx, (ID3D11Resource *)g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        float v[4] = { -1.0f / (8.0f - 3.0f * amount), 0, 0, 0 }; memcpy(m.pData, v, 16); ID3D11DeviceContext_Unmap(g_ctx, (ID3D11Resource *)g_cb, 0); }
    D3D11_VIEWPORT vp = { 0, 0, (float)d.Width, (float)d.Height, 0, 1 };
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 1, &g_sharp[eye].rtv[k], NULL);
    ID3D11DeviceContext_RSSetViewports(g_ctx, 1, &vp);
    ID3D11DeviceContext_IASetInputLayout(g_ctx, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(g_ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(g_ctx, g_vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(g_ctx, g_ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(g_ctx, 0, 1, &g_cb);
    ID3D11DeviceContext_PSSetShaderResources(g_ctx, 0, 1, &srv);
    ID3D11DeviceContext_Draw(g_ctx, 3, 0);
    ID3D11ShaderResourceView *none = NULL; ID3D11DeviceContext_PSSetShaderResources(g_ctx, 0, 1, &none);
    ID3D11DeviceContext_OMSetRenderTargets(g_ctx, 0, NULL, NULL);
    ID3D11DeviceContext_Flush(g_ctx);
    return g_sharp[eye].t[k];
}
static void Handle(int fn)
{
    uint8_t *in = g_ipc->in, *out = g_ipc->out; int32_t i32; float f;
    switch (fn) {
    case F_INIT: {
        int32_t type; memcpy(&type, in, 4); EVRInitError e = EVRInitError_VRInitError_None;
        pInit(&e, (EVRApplicationType)type, "");
        if (e == EVRInitError_VRInitError_None) {
            char v[64]; EVRInitError e2;
            snprintf(v, sizeof v, "FnTable:%s", IVRSystem_Version); S = (struct VR_IVRSystem_FnTable *)pGet(v, &e2);
            snprintf(v, sizeof v, "FnTable:%s", IVRCompositor_Version); C = (struct VR_IVRCompositor_FnTable *)pGet(v, &e2);
            snprintf(v, sizeof v, "FnTable:%s", IVROverlay_Version); O = (struct VR_IVROverlay_FnTable *)pGet(v, &e2);
            Log("SteamVR connected (app type %d) - system %p, compositor %p, overlays %p", type, (void *)S, (void *)C, (void *)O);
            if (!S) e = EVRInitError_VRInitError_Init_InterfaceNotFound;
        } else { snprintf(g_ipc->hostLog, sizeof g_ipc->hostLog, "(%s)", pErr ? pErr(e) : "?"); Log("SteamVR connect failed: %d %s", (int)e, g_ipc->hostLog); }
        memcpy(out, &e, 4); break; }
    case F_SHUTDOWN:
        for (int i = 0; i < g_openN; i++) ID3D11Texture2D_Release(g_open[i].t); g_openN = 0;
        if (S && pShut) { pShut(); Log("SteamVR disconnected"); } S = NULL; C = NULL; O = NULL; break;
    case S_RTSIZE: { uint32_t w = 0, h = 0; if (S) S->GetRecommendedRenderTargetSize(&w, &h); memcpy(out, &w, 4); memcpy(out + 4, &h, 4); break; }
    case S_PROJMAT: { int32_t e; float n, fr; memcpy(&e, in, 4); memcpy(&n, in + 4, 4); memcpy(&fr, in + 8, 4); HmdMatrix44_t m; memset(&m, 0, sizeof m); if (S) m = S->GetProjectionMatrix((EVREye)e, n, fr); memcpy(out, &m, sizeof m); break; }
    case S_PROJRAW: { int32_t e; memcpy(&e, in, 4); float v[4] = { -1, 1, -1, 1 }; if (S) S->GetProjectionRaw((EVREye)e, &v[0], &v[1], &v[2], &v[3]); memcpy(out, v, 16); break; }
    case S_EYE2HEAD: { int32_t e; memcpy(&e, in, 4); HmdMatrix34_t m; memset(&m, 0, sizeof m); if (S) m = S->GetEyeToHeadTransform((EVREye)e); memcpy(out, &m, sizeof m); break; }
    case S_DXGI: { int32_t v = -1; if (S) S->GetDXGIOutputInfo(&v); memcpy(out, &v, 4); break; }
    case S_POSES: { int32_t o; float pr; uint32_t n; memcpy(&o, in, 4); memcpy(&pr, in + 4, 4); memcpy(&n, in + 8, 4); if (n > 64) n = 64;
        TrackedDevicePose_t p[64]; memset(p, 0, sizeof p); if (S) S->GetDeviceToAbsoluteTrackingPose((ETrackingUniverseOrigin)o, pr, p, n); memcpy(out, p, sizeof p[0] * n); break; }
    case S_ROLEIDX: { int32_t r; memcpy(&r, in, 4); uint32_t v = S ? S->GetTrackedDeviceIndexForControllerRole((ETrackedControllerRole)r) : k_unTrackedDeviceIndexInvalid; memcpy(out, &v, 4); break; }
    case S_FLOATPROP: case S_INT32PROP: case S_BOOLPROP: {
        uint32_t d; int32_t pr; memcpy(&d, in, 4); memcpy(&pr, in + 4, 4); ETrackedPropertyError e = ETrackedPropertyError_TrackedProp_UnknownProperty;
        if (fn == S_FLOATPROP) { f = S ? S->GetFloatTrackedDeviceProperty(d, (ETrackedDeviceProperty)pr, &e) : 0; memcpy(out, &f, 4); }
        else if (fn == S_INT32PROP) { i32 = S ? S->GetInt32TrackedDeviceProperty(d, (ETrackedDeviceProperty)pr, &e) : 0; memcpy(out, &i32, 4); }
        else { i32 = S ? S->GetBoolTrackedDeviceProperty(d, (ETrackedDeviceProperty)pr, &e) : 0; memcpy(out, &i32, 4); }
        i32 = e; memcpy(out + 4, &i32, 4); break; }
    case S_STRINGPROP: { uint32_t d, n; int32_t pr; memcpy(&d, in, 4); memcpy(&pr, in + 4, 4); memcpy(&n, in + 8, 4); if (n > 4000) n = 4000;
        ETrackedPropertyError e = ETrackedPropertyError_TrackedProp_UnknownProperty; char *buf = (char *)out + 8; memset(buf, 0, n ? n : 1);
        uint32_t len = S ? S->GetStringTrackedDeviceProperty(d, (ETrackedDeviceProperty)pr, buf, n, &e) : 0; memcpy(out, &len, 4); i32 = e; memcpy(out + 4, &i32, 4); break; }
    case S_CONNECTED: { uint32_t d; memcpy(&d, in, 4); i32 = S && S->IsTrackedDeviceConnected(d); memcpy(out, &i32, 4); break; }
    case S_DEVCLASS: { uint32_t d; memcpy(&d, in, 4); i32 = S ? S->GetTrackedDeviceClass(d) : 0; memcpy(out, &i32, 4); break; }
    case S_EVENT: { uint32_t sz; memcpy(&sz, in, 4); if (sz > sizeof(struct VREvent_t)) sz = sizeof(struct VREvent_t);
        struct VREvent_t ev; memset(&ev, 0, sizeof ev); i32 = S && S->PollNextEvent(&ev, sizeof ev); memcpy(out, &i32, 4); memcpy(out + 8, &ev, sz);
        if (i32 && ev.eventType == EVREventType_VREvent_Quit) Log("SteamVR asks the game to quit");
        break; }
    case S_CTRLSTATE: { uint32_t d, sz; memcpy(&d, in, 4); memcpy(&sz, in + 4, 4); VRControllerState_t st; memset(&st, 0, sizeof st);
        i32 = S && S->GetControllerState(d, &st, sizeof st); memcpy(out, &i32, 4); memcpy(out + 8, &st, sz < sizeof st ? sz : sizeof st); break; }
    case S_HAPTIC: { uint32_t d, ax, us; memcpy(&d, in, 4); memcpy(&ax, in + 4, 4); memcpy(&us, in + 8, 4); if (S) S->TriggerHapticPulse(d, ax, (unsigned short)us); break; }
    case S_ACKQUIT: if (S) S->AcknowledgeQuit_Exiting(); break;
    case S_VSYNC: { float s = 0; uint64_t fc = 0; i32 = S && S->GetTimeSinceLastVsync(&s, &fc); memcpy(out, &i32, 4); memcpy(out + 4, &s, 4); memcpy(out + 8, &fc, 8); break; }
    case C_TRACKSPACE: { int32_t o; memcpy(&o, in, 4); if (C) C->SetTrackingSpace((ETrackingUniverseOrigin)o); break; }
    case C_WAITPOSES: { uint32_t rn, gn; memcpy(&rn, in, 4); memcpy(&gn, in + 4, 4); if (rn > 64) rn = 64; if (gn > 64) gn = 64;
        TrackedDevicePose_t rp[64], gp[64]; memset(rp, 0, sizeof rp); memset(gp, 0, sizeof gp);
        i32 = C ? C->WaitGetPoses(rp, rn, gn ? gp : NULL, gn) : EVRCompositorError_VRCompositorError_DoNotHaveFocus;
        memcpy(out, &i32, 4); memcpy(out + 8, rp, sizeof rp[0] * 64); memcpy(out + 8 + sizeof rp[0] * 64, gp, sizeof gp[0] * gn); break; }
    case C_SUBMIT: {
        int32_t eye, fl, hasB; IpcTex it; VRTextureBounds_t b; HmdMatrix34_t pose;
        memcpy(&eye, in, 4); memcpy(&fl, in + 4, 4); memcpy(&it, in + 8, sizeof it); memcpy(&hasB, in + 8 + sizeof it, 4);
        memcpy(&b, in + 12 + sizeof it, sizeof b); memcpy(&pose, in + 12 + sizeof it + sizeof b, sizeof pose);
        float sharpen = 0; memcpy(&sharpen, in + 12 + sizeof it + sizeof b + sizeof pose, 4); if (!(sharpen >= 0 && sharpen <= 1)) sharpen = 0;
        ID3D11Texture2D *t = Tex_Open(&it);
        if (t) t = Sharp_Apply(t, eye, sharpen);
        i32 = EVRCompositorError_VRCompositorError_InvalidTexture;
        if (t && C) {
            VRTextureWithPose_t tp; memset(&tp, 0, sizeof tp);
            tp.handle = t; tp.eType = ETextureType_TextureType_DirectX; tp.eColorSpace = (EColorSpace)it.colorSpace; tp.mDeviceToAbsoluteTracking = pose;
            i32 = C->Submit((EVREye)eye, (Texture_t *)&tp, hasB ? &b : NULL, (EVRSubmitFlags)fl);
            static int n = 0; if (i32 && n++ < 10) Log("submit eye %d: compositor error %d", eye, i32);
        }
        memcpy(out, &i32, 4); break; }
    case C_FOCUS: { uint32_t v = C ? C->GetCurrentSceneFocusProcess() : 0; memcpy(out, &v, 4); break; }
    case C_POSTPRESENT: if (C) C->PostPresentHandoff(); break;
    case O_CREATE: { uint64_t h = 0; i32 = O ? O->CreateOverlay((char *)in, (char *)in + 256, &h) : EVROverlayError_VROverlayError_RequestFailed; memcpy(out, &i32, 4); memcpy(out + 8, &h, 8); break; }
    case O_HIDE: case O_SHOW: case O_DESTROY: { uint64_t h; memcpy(&h, in, 8); i32 = EVROverlayError_VROverlayError_RequestFailed;
        if (O) i32 = fn == O_HIDE ? O->HideOverlay(h) : fn == O_SHOW ? O->ShowOverlay(h) : O->DestroyOverlay(h); memcpy(out, &i32, 4); break; }
    case O_CURVE: case O_WIDTH: case O_ALPHA: { uint64_t h; memcpy(&h, in, 8); memcpy(&f, in + 8, 4); i32 = EVROverlayError_VROverlayError_RequestFailed;
        if (O) i32 = fn == O_CURVE ? O->SetOverlayCurvature(h, f) : fn == O_WIDTH ? O->SetOverlayWidthInMeters(h, f) : O->SetOverlayAlpha(h, f); memcpy(out, &i32, 4); break; }
    case O_FLAG: { uint64_t h; int32_t a, b2; memcpy(&h, in, 8); memcpy(&a, in + 8, 4); memcpy(&b2, in + 12, 4); i32 = O ? O->SetOverlayFlag(h, (VROverlayFlags)a, b2 != 0) : 1; memcpy(out, &i32, 4); break; }
    case O_SORT: { uint64_t h; uint32_t so; memcpy(&h, in, 8); memcpy(&so, in + 8, 4); i32 = O ? O->SetOverlaySortOrder(h, so) : 1; memcpy(out, &i32, 4); break; }
    case O_COLOR: { uint64_t h; float c[3]; memcpy(&h, in, 8); memcpy(c, in + 8, 12); i32 = O ? O->SetOverlayColor(h, c[0], c[1], c[2]) : 1; memcpy(out, &i32, 4); break; }
    case O_TEXTURE: { uint64_t h; IpcTex it; memcpy(&h, in, 8); memcpy(&it, in + 8, sizeof it); ID3D11Texture2D *t = Tex_Open(&it);
        i32 = EVROverlayError_VROverlayError_InvalidTexture;
        if (t && O) { Texture_t tx = { t, ETextureType_TextureType_DirectX, (EColorSpace)it.colorSpace }; i32 = O->SetOverlayTexture(h, &tx); }
        memcpy(out, &i32, 4); break; }
    case O_XFORMABS: { uint64_t h; int32_t o; HmdMatrix34_t m; memcpy(&h, in, 8); memcpy(&o, in + 8, 4); memcpy(&m, in + 12, sizeof m); i32 = O ? O->SetOverlayTransformAbsolute(h, (ETrackingUniverseOrigin)o, &m) : 1; memcpy(out, &i32, 4); break; }
    case O_XFORMDEV: { uint64_t h; uint32_t d; HmdMatrix34_t m; memcpy(&h, in, 8); memcpy(&d, in + 8, 4); memcpy(&m, in + 12, sizeof m); i32 = O ? O->SetOverlayTransformTrackedDeviceRelative(h, d, &m) : 1; memcpy(out, &i32, 4); break; }
    default: { static int n = 0; if (n++ < 10) Log("unknown request %d", fn); }
    }
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show)
{
    (void)hi; (void)hp; (void)show;
    unsigned long pid = strtoul(cmd ? cmd : "0", NULL, 10);
    char dir[MAX_PATH]; GetModuleFileNameA(NULL, dir, MAX_PATH); { char *s = strrchr(dir, '\\'); if (s) s[1] = 0; }
    { char lp[MAX_PATH]; snprintf(lp, sizeof lp, "%sFNVR-VRHost.log", dir); g_log = fopen(lp, "w"); }
    Log("FNVR VR host (64-bit) for game process %lu", pid);
    HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    char n[96];
    snprintf(n, sizeof n, VRIPC_SHM, pid); HANDLE map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, n);
    snprintf(n, sizeof n, VRIPC_REQ, pid); HANDLE req = OpenEventA(EVENT_ALL_ACCESS, FALSE, n);
    snprintf(n, sizeof n, VRIPC_RSP, pid); HANDLE rsp = OpenEventA(EVENT_ALL_ACCESS, FALSE, n);
    if (!game || !map || !req || !rsp) { Log("couldn't find the game (%p %p %p %p)", game, map, req, rsp); return 1; }
    g_ipc = (VrIpc *)MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VrIpc));
    char dll[MAX_PATH]; snprintf(dll, sizeof dll, "%sopenvr_api.dll", dir);
    HMODULE ovr = LoadLibraryA(dll);
    if (ovr) { pInit = (VR_InitInternal2_t)GetProcAddress(ovr, "VR_InitInternal2"); pShut = (VR_ShutdownInternal_t)GetProcAddress(ovr, "VR_ShutdownInternal");
               pGet = (VR_GetGenericInterface_t)GetProcAddress(ovr, "VR_GetGenericInterface"); pErr = (VR_ErrStr_t)GetProcAddress(ovr, "VR_GetVRInitErrorAsEnglishDescription"); }
    if (!ovr || !pInit || !pShut || !pGet) { Log("couldn't load %s", dll); snprintf(g_ipc->hostLog, sizeof g_ipc->hostLog, "(64-bit openvr_api.dll missing)"); }
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS); SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    g_ipc->hostPid = GetCurrentProcessId(); g_ipc->version = VRIPC_VERSION; g_ipc->hostReady = 1;
    HANDLE w[2] = { req, game };
    for (;;) {
        DWORD r = WaitForMultipleObjects(2, w, FALSE, INFINITE);
        if (r != WAIT_OBJECT_0) { Log("the game closed"); break; }
        int fn = g_ipc->fn;
        if (fn == F_INIT && (!pInit || !pGet)) { EVRInitError e = EVRInitError_VRInitError_Init_InstallationNotFound; memcpy(g_ipc->out, &e, 4); }
        else Handle(fn);
        g_ipc->calls++; g_ipc->fn = 0;
        SetEvent(rsp);
        if (fn == F_SHUTDOWN && WaitForSingleObject(game, 0) != WAIT_TIMEOUT) break;
    }
    if (S && pShut) pShut();
    Log("closing");
    return 0;
}
