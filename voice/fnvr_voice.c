/* FNVR_Voice.exe - offline speech for FNVVRMC (64-bit helper; the game is 32-bit).
   Listens to the VR headset's microphone (WASAPI), recognises only the phrases FNVR sends (Vosk, offline),
   and writes what it heard back to FNVR.
   stdin:  "G <json array of phrases>\n"  (what to listen for; [] = nothing)   "Q\n" = quit
   stdout: "R <text>\n" (heard),  "L <message>\n" (for FNVR.log)
   args:   <folder with libvosk.dll and model\>  [mic name parts separated by ;] */
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

typedef void VoskModel; typedef void VoskRecognizer;
/* Vosk is linked normally (libvosk.dll sits next to this exe) */
__declspec(dllimport) void *vosk_model_new(const char *);
__declspec(dllimport) int vosk_model_find_word(void *, const char *);
__declspec(dllimport) void *vosk_recognizer_new_grm(void *, float, const char *);
__declspec(dllimport) void vosk_recognizer_set_grm(void *, const char *);
__declspec(dllimport) int vosk_recognizer_accept_waveform_s(void *, const short *, int);
__declspec(dllimport) const char *vosk_recognizer_result(void *);
__declspec(dllimport) void vosk_recognizer_reset(void *);
__declspec(dllimport) void vosk_set_log_level(int);
__declspec(dllimport) void vosk_recognizer_free(void *);
static VoskModel *(*p_model_new)(const char *);
static int (*p_find_word)(VoskModel *, const char *);
static VoskRecognizer *(*p_rec_new_grm)(VoskModel *, float, const char *);
static void (*p_set_grm)(VoskRecognizer *, const char *);
static int (*p_accept_s)(VoskRecognizer *, const short *, int);
static const char *(*p_result)(VoskRecognizer *);
static void (*p_reset)(VoskRecognizer *);
static void (*p_log_level)(int);
static void (*p_rec_free)(VoskRecognizer *);

static const GUID kFloatFmt = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static CRITICAL_SECTION g_cs; static char *g_pendingGrm = NULL; static volatile LONG g_quit = 0;
static HANDLE g_out;
static void Out(const char *fmt, ...)
{
    char b[2048]; va_list ap; va_start(ap, fmt); int n = vsnprintf(b, sizeof b - 2, fmt, ap); va_end(ap);
    if (n < 0) return; if (n > (int)sizeof b - 2) n = sizeof b - 2;
    b[n++] = '\n'; DWORD w; WriteFile(g_out, b, (DWORD)n, &w, NULL);
}
static DWORD WINAPI InThread(LPVOID u)
{
    (void)u; HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    static char buf[65536]; int len = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(in, buf + len, (DWORD)(sizeof buf - 1 - len), &got, NULL) || got == 0) { InterlockedExchange(&g_quit, 1); return 0; }
        len += (int)got; buf[len] = 0;
        char *nl;
        while ((nl = memchr(buf, '\n', (size_t)len))) {
            *nl = 0;
            if (buf[0] == 'Q') { InterlockedExchange(&g_quit, 1); return 0; }
            if (buf[0] == 'G' && buf[1] == ' ') { EnterCriticalSection(&g_cs); free(g_pendingGrm); g_pendingGrm = _strdup(buf + 2); LeaveCriticalSection(&g_cs); }
            int used = (int)(nl - buf) + 1; memmove(buf, buf + used, (size_t)(len - used)); len -= used; buf[len] = 0;
        }
        if (len >= (int)sizeof buf - 1) len = 0;
    }
}
/* keep only phrases whose words the model knows (a grammar word it doesn't know breaks the grammar) */
static char *FilterGrammar(VoskModel *m, const char *json, int *kept, int *dropped)
{
    size_t cap = strlen(json) + 64; char *o = (char *)malloc(cap); size_t ol = 0; o[ol++] = '[';
    *kept = 0; *dropped = 0;
    const char *p = json;
    while ((p = strchr(p, '"'))) {
        const char *e = strchr(p + 1, '"'); if (!e) break;
        char ph[512]; size_t L = (size_t)(e - p - 1); if (L >= sizeof ph) L = sizeof ph - 1; memcpy(ph, p + 1, L); ph[L] = 0;
        bool ok = L > 0;
        char tmp[512]; strcpy(tmp, ph);
        for (char *w = strtok(tmp, " "); w && ok; w = strtok(NULL, " ")) if (p_find_word(m, w) < 0) ok = false;
        if (ok) { if (ol + L + 4 >= cap) { cap = cap * 2 + L; o = (char *)realloc(o, cap); } if (*kept) o[ol++] = ','; o[ol++] = '"'; memcpy(o + ol, ph, L); ol += L; o[ol++] = '"'; (*kept)++; }
        else (*dropped)++;
        p = e + 1;
    }
    const char *unk = ",\"[unk]\"]"; if (!*kept) unk = "\"[unk]\"]";
    size_t ul = strlen(unk); if (ol + ul + 1 >= cap) { cap += ul + 2; o = (char *)realloc(o, cap); }
    memcpy(o + ol, unk, ul); ol += ul; o[ol] = 0;
    return o;
}
static IMMDevice *PickMic(IMMDeviceEnumerator *en, const char *want, char *name, size_t cap)
{
    const char *pats[8] = { want, "oculus", "headset microphone", "virtual desktop", "steam streaming microphone", "quest", "index", "vive" };
    IMMDeviceCollection *col = NULL; UINT n = 0; IMMDevice *best = NULL; int bestRank = 99;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eCapture, DEVICE_STATE_ACTIVE, &col)) && col && SUCCEEDED(IMMDeviceCollection_GetCount(col, &n)))
        for (UINT i = 0; i < n; i++) {
            IMMDevice *d = NULL; IPropertyStore *ps = NULL; PROPVARIANT v; PropVariantInit(&v); char nm[256] = "";
            if (FAILED(IMMDeviceCollection_Item(col, i, &d)) || !d) continue;
            if (SUCCEEDED(IMMDevice_OpenPropertyStore(d, STGM_READ, &ps)) && ps && SUCCEEDED(IPropertyStore_GetValue(ps, &PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                WideCharToMultiByte(CP_ACP, 0, v.pwszVal, -1, nm, sizeof nm, NULL, NULL);
            PropVariantClear(&v); if (ps) IPropertyStore_Release(ps);
            char low[256]; size_t k; for (k = 0; nm[k] && k < 255; k++) low[k] = (char)tolower((unsigned char)nm[k]); low[k] = 0;
            int rank = 50;
            for (int q = 0; q < 8 && rank == 50; q++) {
                if (!pats[q] || !pats[q][0]) continue;
                char pl[256]; strncpy(pl, pats[q], 255); pl[255] = 0;
                for (char *t = strtok(pl, ";"); t; t = strtok(NULL, ";")) { char tl[128]; size_t j; for (j = 0; t[j] && j < 127; j++) tl[j] = (char)tolower((unsigned char)t[j]); tl[j] = 0; if (tl[0] && strstr(low, tl)) { rank = q; break; } }
            }
            if (rank < bestRank) { if (best) IMMDevice_Release(best); best = d; bestRank = rank; snprintf(name, cap, "%s", nm); }
            else IMMDevice_Release(d);
        }
    if (col) IMMDeviceCollection_Release(col);
    if (!best || bestRank == 50) {
        if (best) IMMDevice_Release(best); best = NULL;
        if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eCapture, eConsole, &best))) snprintf(name, cap, "Windows' default microphone");
    }
    return best;
}
int main(int argc, char **argv)
{
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    InitializeCriticalSection(&g_cs);
    if (argc < 2) { Out("L usage: FNVR_Voice <folder> [mic]"); return 1; }
    char model[MAX_PATH];
    snprintf(model, sizeof model, "%s\\model", argv[1]);
    p_model_new = vosk_model_new; p_find_word = vosk_model_find_word; p_rec_new_grm = vosk_recognizer_new_grm;
    p_set_grm = vosk_recognizer_set_grm; p_accept_s = vosk_recognizer_accept_waveform_s; p_result = vosk_recognizer_result;
    p_reset = vosk_recognizer_reset; p_rec_free = vosk_recognizer_free; p_log_level = vosk_set_log_level;
    if (p_log_level) p_log_level(-1);
    VoskModel *m = p_model_new(model);
    if (!m) { Out("L offline speech: the speech model in %s didn't load", model); return 4; }
    VoskRecognizer *rec = p_rec_new_grm(m, 16000.0f, "[\"[unk]\"]");
    if (!rec) { Out("L offline speech: recognizer didn't start"); return 5; }

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&en))) { Out("L offline speech: no audio devices"); return 6; }
    char micName[256] = "?";
    IMMDevice *dev = PickMic(en, argc > 2 ? argv[2] : "", micName, sizeof micName);
    if (!dev) { Out("L offline speech: no microphone found"); return 7; }
    IAudioClient *ac = NULL;
    if (FAILED(IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&ac))) { Out("L offline speech: microphone \"%s\" couldn't be opened", micName); return 8; }
    WAVEFORMATEX *wf = NULL; IAudioClient_GetMixFormat(ac, &wf);
    int ch = wf->nChannels, rate = (int)wf->nSamplesPerSec, bits = wf->wBitsPerSample;
    bool isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && IsEqualGUID(&((WAVEFORMATEXTENSIBLE *)wf)->SubFormat, &kFloatFmt));
    HRESULT hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED, 0, 2000000, 0, wf, NULL);
    if (FAILED(hr)) { Out("L offline speech: microphone \"%s\" wouldn't start (0x%08lX)", micName, (unsigned long)hr); return 9; }
    IAudioCaptureClient *cc = NULL; IAudioClient_GetService(ac, &IID_IAudioCaptureClient, (void **)&cc);
    IAudioClient_Start(ac);
    Out("L offline speech: listening on \"%s\" (%d Hz, %d ch, %s)", micName, rate, ch, isFloat ? "float" : "pcm");
    CreateThread(NULL, 0, InThread, NULL, 0, NULL);

    static short out16[64000]; double pos = 0.0, step = (double)rate / 16000.0; float prev = 0.0f;
    bool active = false;
    while (!g_quit) {
        char *g = NULL; EnterCriticalSection(&g_cs); g = g_pendingGrm; g_pendingGrm = NULL; LeaveCriticalSection(&g_cs);
        if (g) {
            int kept, dropped; char *fg = FilterGrammar(m, g, &kept, &dropped);
            active = kept > 0;
            if (active) {   /* a fresh recognizer for each phrase list (changing it on a running one can stop the engine) */
                VoskRecognizer *nr = p_rec_new_grm(m, 16000.0f, fg);
                if (nr) { if (p_rec_free) p_rec_free(rec); rec = nr; } else { Out("L offline speech: phrase list refused"); active = false; }
            }
            Out("L offline speech: listening for %d phrases%s", kept, dropped ? " (some had words it doesn't know)" : "");
            free(fg); free(g);
        }
        Sleep(20);
        UINT32 pk = 0;
        while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(cc, &pk)) && pk > 0) {
            BYTE *data; UINT32 frames; DWORD flags;
            if (FAILED(IAudioCaptureClient_GetBuffer(cc, &data, &frames, &flags, NULL, NULL))) break;
            int n16 = 0;
            for (UINT32 f = 0; f < frames; f++) {
                float sm = 0.0f;
                for (int c = 0; c < ch; c++) {
                    float x;
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) x = 0.0f;
                    else if (isFloat) x = ((float *)data)[f * ch + c];
                    else if (bits == 16) x = ((short *)data)[f * ch + c] / 32768.0f;
                    else if (bits == 32) x = ((int *)data)[f * ch + c] / 2147483648.0f;
                    else x = 0.0f;
                    sm += x;
                }
                sm /= (float)ch;
                /* to 16 kHz: sample at each step position (linear between neighbours) */
                while (pos <= 1.0 && n16 < (int)(sizeof out16 / sizeof out16[0])) {
                    float y = prev + (sm - prev) * (float)pos;
                    int iv = (int)(y * 32767.0f); if (iv > 32767) iv = 32767; if (iv < -32768) iv = -32768;
                    out16[n16++] = (short)iv; pos += step;
                }
                pos -= 1.0; prev = sm;
            }
            IAudioCaptureClient_ReleaseBuffer(cc, frames);
            if (active && n16 > 0 && p_accept_s(rec, out16, n16)) {
                const char *r = p_result(rec);
                const char *t = r ? strstr(r, "\"text\"") : NULL;
                if (t) { t = strchr(t + 6, '"'); }
                if (t) { t++; const char *e = strchr(t, '"'); if (e && e > t) { char txt[512]; size_t L = (size_t)(e - t); if (L >= sizeof txt) L = sizeof txt - 1; memcpy(txt, t, L); txt[L] = 0;
                    if (strcmp(txt, "[unk]")) Out("R %s", txt); } }
            }
        }
    }
    IAudioClient_Stop(ac);
    return 0;
}
