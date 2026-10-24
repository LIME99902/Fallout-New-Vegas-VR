/*
 * FNVR - Fallout: New Vegas native VR plugin
 *
 * Milestone 3a: show the game in the headset (mono, correct scale) via the SteamVR compositor.
 *
 * Pipeline per frame:
 *   MainGameLoop  -> take the head pose SteamVR predicted for this frame, point the camera there
 *   D3D9 Present  -> copy the finished frame into a D3D11 texture laid out in the headset's
 *                    field of view, submit it to SteamVR together with the pose it was rendered
 *                    from (so SteamVR can correct for any remaining head movement), then wait
 *                    for SteamVR's next frame (WaitGetPoses) which paces the game to the headset.
 *
 * Game addresses are for FalloutNV.exe 1.4.0.525 and come from xNVSE / JIP LN NVSE sources.
 */
#define COBJMACROS
#define CINTERFACE
#include <windows.h>
#include <shlobj.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "nvse_api.h"
#include "openvr_capi.h"

#define FNVR_VERSION        210
#define FNVR_VERSION_STR    "2.1.0 (startup setup screen: profile, standing or seated, calibration before the main menu; seated play with right-stick crouch; arms, hands and Pip-Boy repair themselves when the game rebuilds them - no more holstering to fix them; 2.0.4: climbing and grapple moves run from the main loop, not mid-frame; body hidden while climbing; hand turn saved; loading screens / new locations: hands let go of everything and physics rests 2 s - fixes a crash leaving a building; no controls repair on a new game; menu revolver: the Ranger Sequoia's own textures (found in each part's material list), loaded on the main menu too; barrel end found from the gun's shape - faces forward with any revolver model; intro skip works during the movie (checks your location, not your body); 2.0.3: log: which app SteamVR is showing, refused menu pictures; 2.0.2: new game intro movie: hold the right grip 6 s to skip it, with a ring that fills up; 2.0.1: message boxes (tutorial prompts) are normal menus again - OK can be pressed; left X never opens the pause menu over a menu; 2.0.0: new game: VR features wait until you are in the world - fixes a crash on starting a new game; menu revolver faces forward by default; menu shot volume setting, quieter by default; 1.9.9: vendor menu: left X accepts the trade, takes all in containers; test photos only with [Debug] bSavePhotos=1; 1.9.8: holstered weapons shown on your hip and over your shoulder; 1.9.7: body hidden while grappling; 1.9.6: body-moving fix from 1.9.2-1.9.5 removed - possible cause of a crash in combat; 1.9.5: body-under-head fix only at grapple/launch speeds, reads the body's position for this frame; 1.9.4: leaving a conversation no longer flashes the flat screen; 1.9.3: melee weapon angle in the hand, per weapon; 1.9.2: your body stays under your head at any speed - grapple, launches; 1.9.1: Pip-Boy text back - the clip fix only touches the screen picture; 1.9.0: smaller menus: lists clipped in the right place again - dialogue answers and trade lists show; looking away presses Tab to close; dialogue shrinks again; 1.8.9: look away to close menus uses the frame's head pose, logs the angle; 1.8.8: fire rate and Sequoia scripts fixed; menus lowered to eye level (Menu height); dialogue full size by default; 1.8.7: more revolvers recognised: That Gun, Mysterious Magnum, Schofield, .500 S&W; 1.8.6: revolvers fire as fast as you pull the trigger; 1.8.5: fire rate setting; Ranger Sequoia back to 5 rounds; 1.8.4: scopes without a lens part start at the gun's highest part; grapple grip-then-trigger again; 1.8.3: Pip-Boy no longer switches controls (right grip activates again); scope settings shared by guns of the same model; Dialogue size setting; 1.8.2: bullet time fires at once again; 1.8.1: grapple only fires grip-then-trigger; bullet time never fires while making a grapple fist; 1.8.0: scope picture height in cm (Marksman, Bozar, AMR start 11 cm up) and size per gun; melee weapons can be held with both hands; 1.7.9: menu size also applies to the dialogue topics (second interface camera); HUD switches matched to the real HUD pieces (health, AP, sneak, compass, ammo, crosshair); 1.7.8: new HUD & menus settings page; HUD and pop-up menus drawn smaller towards the middle of your view (HUD size / Menu size); HUD parts found by name, listed in the log; 1.7.7: pop-up menus further away and smaller; HUD smaller with distance/width settings; HUD parts can be hidden; Pip-Boy A works again; 1.7.6: live menus: talking, trading, containers and messages keep the world running, you stay in it, look away to close; 1.7.5: automatic melee weapons use the trigger; right-trigger melee option; scope picture height per gun (Marksman, Bozar, AMR raised); character held still while the Pip-Boy is open - A never jumps; 1.7.4: Pip-Boy map: right stick at a third of its strength, so turning is gentle; no more snapping back; 1.7.3: Pip-Boy map: the right stick moves the map cursor again; facing and tilt held still while the Pip-Boy is open; 1.7.2: 1.7.1: the right stick never turns you while the Pip-Boy or the quick-loot window is up; 1.7.0: quick loot: right stick scrolls the list (as mouse wheel clicks); 1.6.9: Pip-Boy: right stick moves the map cursor only on the map page - no more turning you elsewhere; 1.6.8: quick loot: A takes the item (the loot mod's E key), grip opens the full menu; 1.6.7: quick loot: also recognised from what you look at - a container; right grip loot check written to the log; 1.6.6: Pip-Boy: the right stick works as the game's right stick again - moves the map cursor; 1.6.5: F10 only switches the real-time Pip-Boy - 3D is Ctrl+F10; 1.6.4: Pip-Boy menu stays on your saved spot - the game's own screen shape is shrunk away while FNVR draws, so following it made the menu vanish; 1.6.3: Pip-Boy menu drawn on every frame - its spot is worked out again at the end of each frame; 1.6.2: Pip-Boy menu taken from the end of the game's Pip-Boy draw step every frame, placed on the game's own Pip-Boy screen; quick loot keys sent as real keystrokes; 1.6.1: quick loot: right grip presses the loot mod's R key, B its take-all key; Pip-Boy drawing trace; black box stays on the Pip-Boy; 1.6.0: one Pip-Boy menu for paused and live: the menu picture is caught as the game draws the Pip-Boy screen with it, then FNVR draws it on your arm over everything; 1.5.9: menu copied from where the game draws it, screen box behind the menu, loot: grip takes, Y opens; 1.5.8: frozen-mode Pip-Boy screen laid over the live world; 1.5.7: quick-loot window controls for motion controllers; 1.5.6: quick-loot window centred in your view; 1.5.5: live world drawn first, Pip-Boy menu on top of it; 1.5.4: menu picture read from its transparency and coloured like the Pip-Boy; 1.5.3: Pip-Boy menu copied straight from the Pip-Boy's own draw step and drawn on your arm over everything; menu on top of the glass as in 1.3.5; slide it on your arm; both Pip-Boy black boxes switchable; view stays with your body at grapple speed; run by swinging your arms; no fall damage in power armor; grapple in either hand + more grapple settings; rope drawing fixed, Pip-Boy menu shown with its own screen over the live world; grapple rope, stronger kid friendly options; grappling hook, Pip-Boy menu shown over the live world, held objects can't lift you, kid friendly options; live world behind the Pip-Boy, gentler grip, sneak throws)"
#define FNVR_VERSION_SHORT  "2.1.0"

#define PI_F 3.14159265f
#define RAD2DEG 57.2957795f

/* ---- performance timing (ms accumulated since the last status line) ---- */
static int g_nearCount;
static unsigned g_shHits = 0, g_shMisses = 0; static double g_shMissMs = 0;
static double g_tGrab, g_tArms, g_tVis, g_tGame, g_tTick, g_tHead; static unsigned g_tFrames;
static double QpcMs(void) { static LARGE_INTEGER f; LARGE_INTEGER c; if (!f.QuadPart) QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
/* ---- small vector / 3x3 matrix helpers ---- */
typedef float M3[3][3];
static void M3Mul(M3 out, M3 a, M3 b)
{
    M3 t;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
        t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
    memcpy(out, t, sizeof t);
}
static void M3T(M3 out, M3 a) { M3 t; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] = a[j][i]; memcpy(out, t, sizeof t); }
/* How far a matrix is from a signed permutation (entries 0 or +-1). */
static void V3Sub(float *o, const float *a, const float *b) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
static float V3Dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static float V3Len(const float *a) { return sqrtf(V3Dot(a, a)); }
static void V3Cross(float *o, const float *a, const float *b)
{
    float t[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
}
/* Rotation matrix turning direction a onto direction b. */
static void RotBetween(M3 R, const float *a, const float *b)
{
    float la = V3Len(a), lb = V3Len(b);
    float ua[3] = { a[0] / la, a[1] / la, a[2] / la }, ub[3] = { b[0] / lb, b[1] / lb, b[2] / lb };
    float ax[3]; V3Cross(ax, ua, ub);
    float sn = V3Len(ax), cs = V3Dot(ua, ub);
    if (sn < 1e-6f) {
        if (cs > 0.0f) { M3 I = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }; memcpy(R, I, sizeof I); return; }
        /* exactly opposite: half a turn around any axis square to a */
        float p[3] = { 0, 0, 0 }; int k = fabsf(ua[0]) < 0.6f ? 0 : fabsf(ua[1]) < 0.6f ? 1 : 2; p[k] = 1.0f;
        V3Cross(ax, ua, p); sn = V3Len(ax); ax[0] /= sn; ax[1] /= sn; ax[2] /= sn;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[i][j] = 2.0f * ax[i] * ax[j] - (i == j ? 1.0f : 0.0f);
        return;
    }
    ax[0] /= sn; ax[1] /= sn; ax[2] /= sn;
    float t = 1.0f - cs, x = ax[0], y = ax[1], z = ax[2];
    R[0][0] = t * x * x + cs;     R[0][1] = t * x * y - sn * z; R[0][2] = t * x * z + sn * y;
    R[1][0] = t * x * y + sn * z; R[1][1] = t * y * y + cs;     R[1][2] = t * y * z - sn * x;
    R[2][0] = t * x * z - sn * y; R[2][1] = t * y * z + sn * x; R[2][2] = t * z * z + cs;
}
/* The twist part of rotation D around coordinate axis ax (swing-twist split): the angle, -pi..pi, that a
   rotation about that axis alone contributes. Bending around the other axes doesn't change it. */
static float TwistAbout(M3 D, int ax)
{
    float w = sqrtf(fmaxf(0.0f, 1.0f + D[0][0] + D[1][1] + D[2][2])) * 0.5f;
    float q[3];
    q[0] = copysignf(sqrtf(fmaxf(0.0f, 1.0f + D[0][0] - D[1][1] - D[2][2])) * 0.5f, D[2][1] - D[1][2]);
    q[1] = copysignf(sqrtf(fmaxf(0.0f, 1.0f - D[0][0] + D[1][1] - D[2][2])) * 0.5f, D[0][2] - D[2][0]);
    q[2] = copysignf(sqrtf(fmaxf(0.0f, 1.0f - D[0][0] - D[1][1] + D[2][2])) * 0.5f, D[1][0] - D[0][1]);
    float a = q[ax];
    if (w < 1e-6f && fabsf(a) < 1e-6f) return 0.0f;   /* a pure half-turn swing: no defined twist */
    return 2.0f * atan2f(a, w);                       /* w >= 0 here, so this stays within -pi..pi */
}


/* ======================================================================= */
/* Game access                                                             */
/* ======================================================================= */

typedef bool (__cdecl *QueueUIMessage_t)(const char *msg, uint32_t emotion, const char *ddsPath,
                                         const char *soundName, float msgTime, bool maybeNextToDisplay);
static const QueueUIMessage_t QueueUIMessage = (QueueUIMessage_t)0x007052F0;

typedef bool (__cdecl *IsMenuMode_t)(void);
static const IsMenuMode_t IsMenuMode = (IsMenuMode_t)0x00702360;

#define PLAYER_PTR_ADDR      0x011DEA3C  /* PlayerCharacter*            */
#define REFR_ROTX_OFFSET     0x24        /* pitch, radians, + = look down */
#define REFR_ROTZ_OFFSET     0x2C        /* heading, radians, + = turn right */
#define SCENEGRAPH_PTR_ADDR  0x011DEB7C  /* SceneGraph* (world)          */
#define SCENEGRAPH_CAMERA    0xAC        /* NiCamera*                    */
#define SCENEGRAPH_FOV       0xBC        /* float, degrees (horizontal)  */
#define NICAMERA_FRUSTUM     0xDC        /* float l, r, t, b, near, far  */
#define NIDX9RENDERER_ADDR   0x011F4748  /* NiDX9Renderer*               */
#define NIDX9_DEVICE         0x288       /* IDirect3DDevice9*            */

#define PLAYER_WORLD_FOV     0x670       /* float, degrees (4:3-based)   */
#define PLAYER_1ST_FOV       0x674       /* float, degrees, weapon model */

/* SceneGraph::SetCameraFOV(fov, force, camera, setLODAdjust) - same call JIP LN uses with (fov, 0, 0, 0). */
typedef void (__attribute__((thiscall)) *SetCameraFOV_t)(void *sceneGraph, float fov, bool force, void *camera, bool lod);
static const SetCameraFOV_t SetCameraFOV = (SetCameraFOV_t)0x00C52020;

static void *GetPlayer(void) { return *(void **)PLAYER_PTR_ADDR; }
static float *PlayerRotX(void *p) { return (float *)((char *)p + REFR_ROTX_OFFSET); }
static float *PlayerRotZ(void *p) { return (float *)((char *)p + REFR_ROTZ_OFFSET); }
/* While the Pip-Boy or the quick-loot window is up, the right stick works those menus - so the character's
   facing is held still, and the running world never spins around you. */
static bool g_headingLockWant = false;
static float g_pipStickScale = 0.35f;   /* INI [Controls] fPipBoyStickStrength: right stick strength on the Pip-Boy map */
static void HeadingLock_Apply(void)
{
    static bool held = false; static float hz = 0;
    void *pl = GetPlayer();
    if (!g_headingLockWant || !pl) { held = false; return; }
    float *rz = PlayerRotZ(pl), *rx = (float *)((char *)pl + REFR_ROTX_OFFSET); static float hx = 0;
    if (!held) { held = true; hz = *rz; hx = *rx; } else { *rz = hz; *rx = hx; }
}

static IDirect3DDevice9 *GetGameDevice(void)
{
    char *renderer = *(char **)NIDX9RENDERER_ADDR;
    if (!renderer) return NULL;
    return *(IDirect3DDevice9 **)(renderer + NIDX9_DEVICE);
}

/* ======================================================================= */
/* Logging / settings / UI                                                 */
/* ======================================================================= */

static FILE *g_log = NULL;
static volatile LONG g_inMainLoopWork = 0;   /* what FNVR is doing on the main thread (watchdog) */
static HMODULE g_selfModule = NULL;
static bool g_gameOk = false;
static char g_pluginDir[MAX_PATH] = "";   /* ...\Data\NVSE\plugins\ with trailing slash */
static char g_iniPath[MAX_PATH] = "";

static void Log(const char *fmt, ...)
{
    if (!g_log) return;
    SYSTEMTIME t; GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static void LogMem(const char *when)
{
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms))
        Log("Memory %s: %u MB address space used", when, (unsigned)((ms.ullTotalVirtual - ms.ullAvailVirtual) >> 20));
}
static void DirOf(char *path)
{
    char *slash = NULL;
    for (char *c = path; *c; c++) if (*c == '\\' || *c == '/') slash = c;
    if (slash) slash[1] = 0; else path[0] = 0;
}

static void LogOpen(void)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    DirOf(path);
    strcat(path, "FNVR.log");
    g_log = fopen(path, "w");
}

static void UIMessage(const char *text)
{
    if (g_gameOk) QueueUIMessage(text, 0, NULL, NULL, 3.0f, false);
}

/* Settings (Data\NVSE\plugins\FNVR\FNVR.ini) */
static int  g_predictionMs = 40;   /* used only when the headset display isn't active */
static bool g_headsetDisplay = true;
static int  g_pixelDensityPct = 0;    /* 0 = auto: match the game resolution */
static bool g_matchHeadsetFov = true;
static bool g_asyncCapture;
static bool  g_stereo = true;
static float g_unitsPerMeter = 70.0f;   /* game units per real-world meter (world scale) */
static float g_menuScreenFov = 90.0f;
static bool  g_motionControls = true;   /* Touch controllers act as an Xbox controller */
static bool  g_handAim = true;          /* right hand aims, head only looks */
static float g_aimPitchOffsetDeg = -60.0f;/* tilt of the aiming direction relative to the controller */
static bool  g_leftHanded = false;
static WORD  g_runScancode = 0x2A;      /* keyboard Run key (Left Shift) */
static bool  g_meleeSwings = true;
static bool  g_meleeTrigger = false;   /* INI [Controls] bMeleeTrigger: the right trigger attacks with melee weapons and fists too */
static bool  g_fingerTracking = true;
static int   g_fingerAxis = 2;           /* local axis fingers bend around (0 X, 1 Y, 2 Z) */
static float g_fingerSign = 1.0f;        /* bend direction */
static float g_swingSpeed = 2.2f;       /* hand speed (m/s) that counts as a melee swing */
static float g_shotYawTrimDeg = -0.75f; /* fine correction of shot direction: + = right */
static float g_shotPitchTrimDeg = 0.75f;/* + = down */
static bool  g_inGameplay = false;      /* a save is loaded (not at the main menu) */
/* Per-frame VR body data (for the arms). VR space = SteamVR standing space, metres. */
static bool  g_bodyValid = false;
static float g_vrHeadPos[3];
static float g_vrYawOffset = 0.0f;        /* game heading = offset - VR yaw */
static bool  g_vrHandOk[2] = { false, false };
static float g_vrHandPos[2][3];
static float g_handHeading[2], g_handPitch[2];   /* game convention */
static float g_handSpeed[2] = { 0, 0 };   /* m/s */
static float g_vrHandRot[2][3][3];
static float g_fingerCurl[2][3];         /* per hand: thumb, index, other three (0 = open, 1 = closed) */

/* Hand grabbing (no weapon out): grip near a loose object picks it up with that hand. */
static float g_sprintMult = 1.5f;        /* left stick click: movement speed multiplier */
static bool  g_grabEnabled = true;
static float g_grabReach = 12.0f;        /* game units between the hand and the object's surface */
static float g_grabMaxMass = 200.0f;
static float g_throwStrength = 1.0f;
static char *g_grabCand[2] = { NULL, NULL };   /* object each physical hand could grab right now */
static bool  g_grabWant[2] = { false, false }; /* grip is held for grabbing (set by the controller poll) */
static bool  g_gripHeld[2] = { false, false }, g_gripEdge[2] = { false, false };   /* raw grips (for climbing) */
static bool  g_climb[2] = { false, false };     /* this hand is holding on to a wall */
static bool  g_climbEnabled = true;             /* INI [Grab] bClimbing */
static bool  g_flatMenus = false;               /* INI [Display] bFlatMenus: old flat screen for menus/main menu */
static bool  g_curvedUI = true;          /* INI [Display] bCurvedUI: menus/HUD on a curved panel in front of you */
static float g_uiDist = 0.45f;           /* INI [Display] fUIDistance (metres) */
static float g_mmDist = 1.8f;            /* INI [Display] fMainMenuDistance: main menu & loading screens (metres) */
static float g_mmWidth = 2.8f;           /* INI [Display] fMainMenuWidth (metres) */
static bool  g_farMenu = true;           /* INI [Display] bMainMenuScreen: main menu/loading on a far curved screen */
static bool  g_mmPointer = true;         /* INI [Controls] bMenuPointer: point with the right hand, trigger selects */
static bool  g_mmShot = true;            /* INI [Controls] bMenuPointerShot: Ranger Sequoia shot on each select */
static float g_mmShotVol = 0.25f;        /* INI [Controls] fMenuShotVolume: how loud that shot is (0-1) */
static bool  g_farActive = false;
static bool  g_farGameMenus = true;
static float g_pipFitIni = 1.0f;
static bool  g_pipNoAnim = true;
static float g_pipShift[3] = { 0, 0, 0 };
static float g_pipMenuAbove = 0.0f;   /* INI [Controls] fPipMenuAboveBox */
static float g_pipMenuShift[3] = { 0, 0, 0 };
static float g_pipMenuDeltaW[3]; static bool g_pipHideBoxIni = true;
static void PP_Load(void);
static void PipScr_Load(void);
static void PipScr_Draw(IDirect3DDevice9 *dev);
static void PipGrab_Release(void);
static void PM_Release(void);
static IDirect3DDevice9 *g_hookedDevice;
static void *PatchVtable(void **vtbl, int index, void *hook);
static void Rope_Draw(IDirect3DDevice9 *dev);
static float g_txt[3] = { 0, 0, 1 }; static bool g_txtFollow = false; static char *g_pipMenuCamRestore = NULL; static float g_pipMenuCamF[4];   /* INI [PipParts] fTextX/fTextY/fTextScale, bTextFollow */
static char *g_pipAnchor = NULL, *g_menuTextNode = NULL; static float g_pipMenuScale = 1.0f;   /* INI [Controls] fPipMenuScale */   /* how far the menu picture was moved this frame (world) - the black box ignores it */   /* INI [Controls] fPipMenuShiftX/Y/Z: moves only the lit menu picture */
static bool  g_seqFlip = false;
static float g_holdAlong = 0.62f, g_holdGap = 0.9f, g_holdCurl = 1.0f;   /* INI [Grab] fHoldPalmAlong / fHoldPalmGap / fHoldFingerWrap */ static float g_seqRoll = 0.0f;   /* INI [Controls] bMenuGunFlip, fMenuGunRoll */
static bool  g_ltBullet = true; static int g_bulletKey = 46;   /* INI [Controls] bLeftTriggerBulletTime, iBulletTimeKey (46 = C) */
static bool  g_momentumOnIni = true;   /* INI [Controls] bJumpMomentum */
static bool  g_armSwing = true; static float g_armSwingSens = 1.0f; static bool g_armSwingSprint = true;   /* INI [Controls] bArmSwing, fArmSwingSensitivity, bArmSwingSprint */
static bool  g_grapOn = true, g_grapSafe = true;   /* INI [Grapple] bEnabled, bSafeLanding */
static bool  g_grapHand[2] = { true, false };      /* INI [Grapple] bLeftHand, bRightHand */
static bool  g_grapRope = true, g_grapRumble = true, g_grapActors = true;   /* INI [Grapple] bShowRope, bRumble, bHookActors */
static bool  g_grapYank = true;   /* INI [Grapple] bPullPeople: hooking a person pulls THEM to you (and knocks them down on arrival) */
static float g_grapRange = 40.0f, g_grapSpeed = 28.0f, g_grapLaunch = 1.0f;   /* INI [Grapple] fRange (m), fTopSpeed (m/s), fLaunch (x) */
static float g_grapRamp = 0.6f, g_grapStart = 0.35f, g_grapDrag = 0.35f, g_grapHop = 3.5f;   /* INI [Grapple] fTimeToTop (s), fStartSpeed (0-1), fAirDrag, fEndHop (m/s) */
static bool  g_weaponOutThisFrame = true;
static bool  g_lootCenter = true; static float g_lootOffX = 0.0f;
static bool  g_lootDiag = false;   /* (right grip was just squeezed: write down what the loot check saw) */
static bool  g_lootShown = false, g_lootControls = true;   /* the quick-loot window is up; INI [Controls] bLootMenuControls */
static WORD  g_jlmTake = 0x1000, g_jlmEquip = 0x4000, g_jlmAll = 0x0002; static int g_jlmKey1 = 18, g_jlmKey2 = 19, g_jlmKeyAll = 42;   /* its buttons, from JustMods.ini */   /* INI [Display] bCenterLootMenu, fLootMenuOffset */
static bool  g_pipWorldFirst = false;     /* INI [Controls] bPipBoyWorldFirst */
static bool  g_pipBoxShow = true, g_pipMenuBg = true;   /* INI [PipScreen] bShowScreenBox, bMenuBackground */
static bool  g_paNoFall = true;          /* INI [Controls] bPowerArmorNoFallDamage */
static bool g_grapPulling = false;
static bool g_hideBodyGrap = true;
static void Hol_Load(void);   /* INI [Grapple] bHideBody: no body below you while grappling */
static char *g_bodyHidden = NULL;
static float g_grapTrigH[2] = { 0, 0 };  /* each hand's trigger, 0..1 (0 = left, 1 = right) */
static bool  g_chestPause = true; static float g_chestDown = 0.32f, g_chestFwd = 0.10f, g_chestR = 0.14f;   /* INI [Controls] bChestPause etc. */
/* Stimpaks: a free hand grabbing at your chest pulls one out (Stimpak first, else Super Stimpak); stab it into your other
   forearm or your chest to use it. Let go without stabbing and it goes back. */
static bool  g_stimOn = true;                   /* INI [Stimpak] bChestStimpak */
static int   g_stimHand = -1, g_stimType = 0; static char *g_stimRef = NULL; static uint32_t g_stimRefID = 0;
static double g_stimAt = 0; static bool g_stimHeld = false;
static volatile int g_stimDo = 0; static char *g_stimDoRef = NULL; static uint32_t g_stimDoID = 0; static int g_stimDoType = 0;   /* 1 use, 2 put back (run from the main loop) */
static WORD  g_chestPress = 0; static double g_chestPressUntil = 0;
static uint32_t g_holsterId[3] = { 0, 0, 0 };
static bool  g_holstersOn = true;                    /* INI [Controls] bHolsters */
static float g_hipDown = 0.62f, g_hipSide = 0.20f, g_shDown = 0.12f, g_shSide = 0.16f, g_shBack = 0.12f, g_holsterR = 0.16f;            /* INI [Controls] bMenuGunFlip */
static float g_pipLift = 1.0f;             /* INI [Controls] fPipBoyScreenLift (game units above the glass) */
static float g_twShare = 1.0f;             /* INI [Controls] fForearmTwistShare */
static float g_twRoll = 0.0f; static int g_twAx = 0; static bool g_twOk = false;   /* INI [Controls] fPipBoyScreenShiftX/Y/Z */
static bool g_pipOnBoxIni = true; static float g_pipBoxFitIni = 1.0f, g_pipBoxLiftIni = 0.3f;
static bool  g_pipOvEnabledIni = true; static float g_pipOvIni[4] = { 0.10f, 0.045f, 0.0f, 0.11f };        /* INI [Controls] bPipBoyNoAnimation */     /* INI [Display] bMenusOnScreen: in-game menus & prompts on the curved screen too */       /* the far main-menu screen is showing */
static float g_mmGunUpIni = 0.045f, g_mmGunFwdIni = 0.03f;   /* INI [Controls] fMenuGunUp / fMenuGunForward */
static float g_mmCurveR = 3.0f;          /* INI [Display] fMainMenuCurveRadius: gentle curved-TV bend (metres) */
static float g_uiArcDeg = 80.0f;         /* INI [Display] fUIArc (degrees the HUD panel wraps around you) */
static float g_menuDist = 0.9f, g_menuArcDeg = 60.0f;   /* INI [Display] fMenuDistance / fMenuArc: pop-up menus (talking, trading...) */
static float g_uiScale = 0.6f;                  /* INI [Display] fUIScale: share of the view the HUD fills */
static float g_menuScale = 0.5f;                /* INI [Display] fMenuScale: share of the view pop-up menus fill */
static float g_dlgScale = 0.5f;
static float g_menuYOff = -0.12f;               /* INI [Display] fMenuHeight: pop-up menus moved up (+) / down (-), share of the view */                 /* INI [Display] fDialogueScale: share of the view talking (dialogue) fills */
static bool g_farActive2;
static bool g_dlgSnapWant = false;
static volatile bool g_uiXfOn = false; static float g_uiXfBase[4], g_uiXfCur[4];   /* interface camera: the game's frustum and the one FNVR set */
static bool  g_adsEnabled = false;              /* INI [Controls] bAimDownSights: left trigger aims down sights */
static bool  g_pipOpen = false;      /* the Pip-Boy is open (shown on your wrist, not as a flat screen) */
static int   g_pipPulse = 0;          /* (unused) */
static double g_pipPressUntil = 0;    /* wrist gesture: hold a button until this time */
static double g_pipExpectUntil = 0;   /* the Pip-Boy was just asked to open: don't flash the flat screen */
static WORD  g_pipPressMask = 0;
static float g_pipScale = 1.0f;       /* INI [Controls] fPipBoyScale: how much bigger the screen picture is while open */
static bool  g_wristPip = true;       /* INI [Controls] bWristPipBoy */
static int   g_pipModeRaw = 0;
static bool g_pipRealTime;
/* ---- Live menus: talking, trading, containers and pop-up messages with the world still running ----
   These menus normally freeze the game and FNVR showed them on a screen in a separate room. With live menus on,
   the world keeps running and you stay in it (the menu shows on the curved panel in front of you, as the HUD
   does); look well away from it and it closes. [Display] bLiveMenus, fLookAwayClose (degrees, 0 = off). */
static bool  g_liveMenus = true; static float g_lookAwayDeg = 75.0f;
static void Hud_Load(void);
static volatile bool g_liveMenuNow = false;
static const int g_liveMenuIds[] = { /* (1001 message boxes: not live - their OK buttons need the game's normal menu mode) */ 1008 /* container */, 1009 /* dialogue */, 1016 /* how many */, 1035 /* repair */,
                                     1053 /* barter */, 1058 /* repair services */, 1061 /* weapon mods */, 1075 /* companion wheel */, 1077 /* crafting */ };
static bool g_liveBlocked = false;   /* (another, non-live menu is up) */
static bool LiveMenu_Check(void)
{
    g_liveBlocked = false;
    if (!g_liveMenus) return false;
    const volatile bool *v = (const volatile bool *)0x011F308F;
    bool any = false;
    for (int id = 1001; id <= 1090; id++) {
        if (!v[id] || id == 1004 /* HUD */) continue;
        bool live = false;
        for (unsigned k = 0; k < sizeof g_liveMenuIds / sizeof g_liveMenuIds[0]; k++) if (g_liveMenuIds[k] == id) live = true;
        if (!live) { g_liveBlocked = true; return false; }   /* another menu is up too (pause menu, loading, lockpicking...): the game's normal way */
        any = true;
    }
    return any;
}
static bool g_pipInstant;
static double g_headSnapAt = 0; static bool g_headSnapPending = false;
static double g_reequipAt = 0, g_hiddenLogAt = 0;
/* any of the Pip-Boy's own screens showing (Items, Stats, Data/Map) */
static bool PipMenusVisible(void) { const bool *v = (const bool *)0x011F308F; return v[0x3EA] || v[0x3EB] || v[0x3FF]; }
static int PipBoyMode(void) { char *im = *(char **)0x011D8A80; return im ? *(int *)(im + 0x4BC) : 0; }
static double QpcMs(void);
static bool  g_handWalls = true;                /* INI [Grab] bHandWallCollision */
static bool  g_holding[2] = { false, false };  /* hand currently holds an object */
/* Two-handed guns: the off hand grips the gun; the gun then points from the aiming hand
   through the off hand (steadier, like a real rifle). */
static bool  g_adsActive = false;
static bool  g_scopeAtEye = false;
static double g_weaponSettleUntil = 0;
static bool  g_wristUp = false;
static float g_fingerDirW[2][3]; static bool g_fingerDirOk[2];   /* wrist-to-knuckles direction in the world (from the palm data) */
static float g_handRefIni[2][9]; static bool g_handRefIniOk[2] = { false, false };   /* INI [Arms] sHandRefL/R: how the hand turns with the controller (learned from the weapon pose) */
static float g_armFollowHand = 0.8f, g_forearmRollLimit = 90.0f;   /* INI [Controls] fArmFollowHand, fForearmRollLimit */
static bool  g_pipLock = false;           /* INI [Controls] bPipBoyLockPose: while open, the game holds the Pip-Boy in front of your face */
static bool  g_pipLockNow = false;          /* left wrist raised toward your face (Pip-Boy pose) */
static double g_recStart; static int g_recOpen, g_recFrame;
static double g_grecStart; static int g_grecN, g_grecFrame, g_grecHand;
static double g_gpStart; static int g_gpN, g_gpFrame;   /* grapple photo burst */
static bool g_newGameWait = false; static double g_newGameReadyAt = 0;   /* new game: VR features wait for the world */
static volatile float g_skipProg = 0.0f; static volatile double g_skipDoneAt = 0;   /* intro skip: hold progress 0-1, when it fired */
static bool g_savePhotos = false;   /* [Debug] bSavePhotos: test photo bursts (Pip-Boy, pick-ups, grapple) */   /* after a weapon change: no scope work until the new gun is out */       /* scoped gun raised to the eye (aims automatically) */
static bool  g_autoScope = false;
static float g_scopeMaxZoom = 4.0f;
static int   g_capPick = -1;          /* INI iScopeCaptureIndex: which grab to use (-1 = the last one) */
static float g_scopeBright = 1.0f;    /* INI fScopeBrightness */
static bool  g_scopesEnabled = true;     /* magnified view inside the held gun's scope */
static bool  g_twoHandCand = false;      /* off hand is on the gun (in reach to grip it) */
static bool  g_twoHandWant = false;      /* off-hand grip held for two-handed aiming */
static bool  g_twoHand = false;          /* two-handed aiming active this frame */
static float g_twoHandLocal[3];          /* off-hand direction in aiming-controller coordinates at grip time */
static float g_twoHandC[3][3];           /* VR-space rotation from one-hand to two-hand aim */
static float g_grabNearest[2] = { 1e9f, 1e9f };/* distance to the nearest loose object (diagnostics) */
static bool  g_handCollision = true;     /* hands and weapons knock loose objects around */
static float g_collisionStrength = 1.0f;
static bool  g_physicalMove = true;      /* real crouching / leaning moves the view and arms */
static float g_maxLean = 0.35f;          /* metres the head can lean before the body follows */
static float g_standY = -1.0f;           /* standing head height (tracking space, metres) */
static float g_leanAnchor[2] = { 0, 0 }; /* head x/z the body is centred under */
static bool  g_haveLeanAnchor = false;
/* body calibration & profiles (see the settings panel) */
static float Body_StandRef(void);
static int   g_profActive = 1;
static char  g_profName[5][32];
static float g_bodyEye = 0;                    /* standing eye height (m); 0 = not calibrated */
static bool  g_seated = false;                 /* profile: seated play - your sitting head height is the character's full height */
static float g_seatEye = 0;                    /* profile: sitting eye height (m); 0 = not measured */
static float g_seatCrouchDepth = 0.85f;        /* INI [Seated] fCrouchDepth: how far the right stick (down) crouches you, metres */
static volatile bool g_seatCrouchWant = false; static float g_seatCrouchK = 0.0f;
static float g_lhGunFwd = 0.0f, g_lhGunUp = 0.0f, g_lhGunSide = 0.0f;   /* INI [LeftHanded] fGunForward/fGunUp/fGunSide (cm): fine-tuning the gun in the left hand */
static bool  g_startSetup = true;              /* INI [Startup] bShowSetup: the setup screen before the main menu */
static float g_bodyReach[2] = { 0, 0 };        /* game shoulder -> hand with the arm straight (m), left / right */
static bool  g_bodyScaleWorld = true;          /* match the world's size to your height */
static bool  g_bodyArmMatch = true;            /* in-game arms sized to yours */
static float g_worldScaleManual = 70.0f;       /* world size when not matched to your height (units per metre) */
static float g_gameEyeUnits = 0;               /* the character's eye height (game units, measured while playing) */
static int   g_armCalMask = 0; static bool g_armCalDone = false;
static bool  g_setOpen = false;                /* the FNVR settings panel is up */
static bool  g_aimCalReq = false;              /* quick click of both sticks: calibrate the aim */
static bool  g_setSwallow = false;             /* panel just closed: ignore the controls until everything is let go */
static bool  g_kidMess = true, g_kidDismember = true, g_kidBlood = true;   /* INI [KidFriendly] bBloodyMessExplodes / bDismemberment / bBlood (ON = the normal game) */
static void Kids_Apply(void);
static bool  g_throwPunch = false;             /* INI [Grab] bThrowAttacks: the throwing motion also counts as a punch */
static float g_throwDmg = 1.0f;                /* INI [Grab] fThrowDamage: thrown-object damage multiplier (0 = none) */
static bool  g_throwHurtFriends = false;       /* INI [Grab] bThrowHurtsCompanions */
static DWORD g_lastThrowTick = 0;
static bool  g_pipWorldDraw = true;            /* INI [Controls] bPipBoyLiveWorld: keep drawing the world while the Pip-Boy is open */
static bool  g_imperial = false;               /* INI [Display] bUSUnits: show feet and inches in the settings panel */
static float Body_ArmScale(int side, float gameLen);
static void  Profile_Save(void);
static void  Profiles_Init(void);
static bool  Settings_Update(const VRControllerState_t *mv, const VRControllerState_t *am, bool okMv, bool okAm);
static void IntroSkip_Frame(void);
static void StartSetup_Tick(void);
static void  Settings_Frame(void);
static void  Settings_Toggle(void);

/* xNVSE script interface: lets FNVR run small scripts (used to drive JIP LN's extra-camera functions). */
typedef struct {
    void *CallFunction, *GetFunctionParams, *ExtractArgsEx, *ExtractFormatStringArgs;
    bool (*CallFunctionAlt)(void *script, void *callingObj, uint8_t numArgs, ...);
    void *(*CompileScript)(const char *text);
    void *(*CompileExpression)(const char *text);
} NVSEScriptIntf;
static NVSEScriptIntf *g_scriptIntf = NULL;

static void LoadSettings(void)
{
    GetModuleFileNameA(g_selfModule, g_pluginDir, MAX_PATH);
    DirOf(g_pluginDir);
    snprintf(g_iniPath, sizeof g_iniPath, "%sFNVR\\FNVR.ini", g_pluginDir);
    g_predictionMs    = (int)GetPrivateProfileIntA("Tracking", "iPredictionMs", 40, g_iniPath);
    g_headsetDisplay  = GetPrivateProfileIntA("Display", "bHeadsetDisplay", 1, g_iniPath) != 0;
    g_pixelDensityPct = (int)GetPrivateProfileIntA("Display", "iPixelDensityPercent", 0, g_iniPath);
    g_matchHeadsetFov = GetPrivateProfileIntA("Display", "bMatchHeadsetFOV", 1, g_iniPath) != 0;
    g_asyncCapture = GetPrivateProfileIntA("Display", "bAsyncCapture", 1, g_iniPath) != 0;
    g_stereo = GetPrivateProfileIntA("Stereo", "bStereo", 0, g_iniPath) != 0;
    g_motionControls = GetPrivateProfileIntA("Controls", "bMotionControls", 1, g_iniPath) != 0;
    g_handAim = GetPrivateProfileIntA("Controls", "bHandAim", 1, g_iniPath) != 0;
    g_leftHanded = GetPrivateProfileIntA("Controls", "bLeftHanded", 0, g_iniPath) != 0;
    g_meleeSwings = GetPrivateProfileIntA("Controls", "bMeleeSwings", 1, g_iniPath) != 0;
    g_meleeTrigger = GetPrivateProfileIntA("Controls", "bMeleeTrigger", 0, g_iniPath) != 0;
    g_fingerTracking = GetPrivateProfileIntA("Controls", "bFingerTracking", 1, g_iniPath) != 0;
    g_fingerAxis = (int)GetPrivateProfileIntA("Controls", "iFingerBendAxis", 2, g_iniPath) % 3;
    g_fingerSign = GetPrivateProfileIntA("Controls", "bFingerBendReverse", 0, g_iniPath) ? -1.0f : 1.0f;
    g_runScancode = (WORD)GetPrivateProfileIntA("Controls", "iRunKeyScancode", 0x2A, g_iniPath);
    g_grabEnabled = GetPrivateProfileIntA("Grab", "bGrabObjects", 1, g_iniPath) != 0;
    g_physicalMove = GetPrivateProfileIntA("Tracking", "bPhysicalCrouchLean", 1, g_iniPath) != 0;
    { char cb[32]; GetPrivateProfileStringA("Seated", "fCrouchDepth", "0.85", cb, sizeof cb, g_iniPath); g_seatCrouchDepth = fminf(fmaxf((float)atof(cb), 0.2f), 1.4f); }
    g_startSetup = GetPrivateProfileIntA("Startup", "bShowSetup", 1, g_iniPath) != 0;
    g_stimOn = GetPrivateProfileIntA("Stimpak", "bChestStimpak", 1, g_iniPath) != 0;
    { char lb[32];
      GetPrivateProfileStringA("LeftHanded", "fGunForward", "0", lb, sizeof lb, g_iniPath); g_lhGunFwd = (float)atof(lb);
      GetPrivateProfileStringA("LeftHanded", "fGunUp", "0", lb, sizeof lb, g_iniPath); g_lhGunUp = (float)atof(lb);
      GetPrivateProfileStringA("LeftHanded", "fGunSide", "0", lb, sizeof lb, g_iniPath); g_lhGunSide = (float)atof(lb); }
    g_handCollision = GetPrivateProfileIntA("Grab", "bHandCollision", 1, g_iniPath) != 0;
    g_climbEnabled = GetPrivateProfileIntA("Grab", "bClimbing", 1, g_iniPath) != 0;
    g_wristPip = GetPrivateProfileIntA("Controls", "bWristPipBoy", 1, g_iniPath) != 0;
    g_flatMenus = GetPrivateProfileIntA("Display", "bFlatMenus", 0, g_iniPath) != 0;
    g_curvedUI = GetPrivateProfileIntA("Display", "bCurvedUI", 1, g_iniPath) != 0;
    { char sb[32]; GetPrivateProfileStringA("Display", "fUIDistance", "0.6", sb, sizeof sb, g_iniPath); float v = (float)atof(sb); if (v >= 0.2f && v <= 5.0f) g_uiDist = v;
      GetPrivateProfileStringA("Display", "fUIArc", "80", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 30.0f && v <= 200.0f) g_uiArcDeg = v;
      GetPrivateProfileStringA("Display", "fMenuDistance", "0.9", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.3f && v <= 5.0f) g_menuDist = v;
      GetPrivateProfileStringA("Display", "fMenuArc", "60", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 20.0f && v <= 200.0f) g_menuArcDeg = v;
      GetPrivateProfileStringA("Display", "fMainMenuDistance", "1.8", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.3f && v <= 10.0f) g_mmDist = v;
      GetPrivateProfileStringA("Display", "fMainMenuWidth", "2.8", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.3f && v <= 10.0f) g_mmWidth = v;
      GetPrivateProfileStringA("Display", "fMainMenuCurveRadius", "3.0", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.5f && v <= 50.0f) g_mmCurveR = v; }
    g_farMenu = GetPrivateProfileIntA("Display", "bMainMenuScreen", 1, g_iniPath) != 0;
    g_farGameMenus = GetPrivateProfileIntA("Display", "bMenusOnScreen", 1, g_iniPath) != 0;
    g_pipNoAnim = GetPrivateProfileIntA("Controls", "bPipBoyNoAnimation", 1, g_iniPath) != 0;
    g_pipLock = GetPrivateProfileIntA("Controls", "bPipBoyLockPose", 0, g_iniPath) != 0;
    for (int s_ = 0; s_ < 2; s_++) {
        char hb[256]; GetPrivateProfileStringA("Arms", s_ ? "sHandRefR" : "sHandRefL", "", hb, sizeof hb, g_iniPath);
        float *m = g_handRefIni[s_];
        g_handRefIniOk[s_] = sscanf(hb, "%f %f %f %f %f %f %f %f %f", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7], &m[8]) == 9;
        if (!g_handRefIniOk[s_]) {   /* built-in start (measured with the game's own skeleton - the same for everyone) */
            static const float def[2][9] = { { 0.0692f, 0.6634f, -0.7451f, 0.3600f, -0.7131f, -0.6015f, -0.9304f, -0.2266f, -0.2882f },
                                             { -0.1232f, -0.6269f, -0.7693f, 0.3401f, -0.7549f, 0.5607f, -0.9323f, -0.1926f, 0.3062f } };
            memcpy(m, def[s_], 36); g_handRefIniOk[s_] = true;
        }
    }
    { char ab[32]; GetPrivateProfileStringA("Controls", "fArmFollowHand", "0.8", ab, sizeof ab, g_iniPath); g_armFollowHand = fminf(fmaxf((float)atof(ab), 0.0f), 1.0f);
      GetPrivateProfileStringA("Controls", "fForearmRollLimit", "90", ab, sizeof ab, g_iniPath); g_forearmRollLimit = fminf(fmaxf((float)atof(ab), 20.0f), 180.0f); }
    g_pipOvEnabledIni = GetPrivateProfileIntA("Controls", "bPipScreenOnWrist", 0, g_iniPath) != 0;
    g_pipOnBoxIni = GetPrivateProfileIntA("Controls", "bPipMenuOnScreenBox", 0, g_iniPath) != 0;
    { char bb[32]; GetPrivateProfileStringA("Controls", "fPipMenuBoxFit", "1.0", bb, sizeof bb, g_iniPath); float v = (float)atof(bb); if (v > 0.2f && v < 4.0f) g_pipBoxFitIni = v;
      GetPrivateProfileStringA("Controls", "fPipMenuBoxLift", "0.3", bb, sizeof bb, g_iniPath); g_pipBoxLiftIni = (float)atof(bb); }
{ char vb[32]; GetPrivateProfileStringA("Controls", "fMenuShotVolume", "0.25", vb, sizeof vb, g_iniPath); g_mmShotVol = (float)atof(vb); if (g_mmShotVol < 0) g_mmShotVol = 0; if (g_mmShotVol > 1) g_mmShotVol = 1; }
        g_seqFlip = GetPrivateProfileIntA("Controls", "bMenuGunTurnAround", 0, g_iniPath) != 0;   /* (the barrel end is measured now - this only overrides it; old bMenuGunFlip ignored) */
    { char rb[32]; GetPrivateProfileStringA("Controls", "fMenuGunRoll", "0", rb, sizeof rb, g_iniPath); g_seqRoll = (float)atof(rb); }
    g_holstersOn = GetPrivateProfileIntA("Controls", "bHolsters", 1, g_iniPath) != 0;
    { char gb2[32]; GetPrivateProfileStringA("Grab", "fHoldPalmAlong", "0.62", gb2, sizeof gb2, g_iniPath); g_holdAlong = (float)atof(gb2);
      GetPrivateProfileStringA("Grab", "fHoldPalmGap", "0.9", gb2, sizeof gb2, g_iniPath); g_holdGap = (float)atof(gb2);
      GetPrivateProfileStringA("Grab", "fHoldFingerWrap", "1.0", gb2, sizeof gb2, g_iniPath); g_holdCurl = (float)atof(gb2); }
    g_chestPause = GetPrivateProfileIntA("Controls", "bChestPause", 1, g_iniPath) != 0;
    g_momentumOnIni = GetPrivateProfileIntA("Controls", "bJumpMomentum", 1, g_iniPath) != 0;
    g_grapOn = GetPrivateProfileIntA("Grapple", "bEnabled", 1, g_iniPath) != 0;
    g_hideBodyGrap = GetPrivateProfileIntA("Grapple", "bHideBody", 1, g_iniPath) != 0;
    Hol_Load();
    g_grapSafe = GetPrivateProfileIntA("Grapple", "bSafeLanding", 1, g_iniPath) != 0;
    { char gb3[32];
      GetPrivateProfileStringA("Grapple", "fRange", "40", gb3, sizeof gb3, g_iniPath); g_grapRange = fminf(fmaxf((float)atof(gb3), 5.0f), 150.0f);
      GetPrivateProfileStringA("Grapple", "fTopSpeed", "28", gb3, sizeof gb3, g_iniPath); g_grapSpeed = fminf(fmaxf((float)atof(gb3), 5.0f), 80.0f);
      GetPrivateProfileStringA("Grapple", "fLaunch", "1.0", gb3, sizeof gb3, g_iniPath); g_grapLaunch = fminf(fmaxf((float)atof(gb3), 0.0f), 3.0f);
      GetPrivateProfileStringA("Grapple", "fTimeToTop", "0.6", gb3, sizeof gb3, g_iniPath); g_grapRamp = fminf(fmaxf((float)atof(gb3), 0.05f), 5.0f);
      GetPrivateProfileStringA("Grapple", "fStartSpeed", "0.35", gb3, sizeof gb3, g_iniPath); g_grapStart = fminf(fmaxf((float)atof(gb3), 0.05f), 1.0f);
      GetPrivateProfileStringA("Grapple", "fAirDrag", "0.35", gb3, sizeof gb3, g_iniPath); g_grapDrag = fminf(fmaxf((float)atof(gb3), 0.0f), 3.0f);
      GetPrivateProfileStringA("Grapple", "fEndHop", "3.5", gb3, sizeof gb3, g_iniPath); g_grapHop = fminf(fmaxf((float)atof(gb3), 0.0f), 10.0f); }
    g_grapHand[0] = GetPrivateProfileIntA("Grapple", "bLeftHand", 1, g_iniPath) != 0;
    g_grapHand[1] = GetPrivateProfileIntA("Grapple", "bRightHand", 0, g_iniPath) != 0;
    g_grapRope = GetPrivateProfileIntA("Grapple", "bShowRope", 1, g_iniPath) != 0;
    g_paNoFall = GetPrivateProfileIntA("Controls", "bPowerArmorNoFallDamage", 1, g_iniPath) != 0;
    g_pipBoxShow = GetPrivateProfileIntA("PipScreen", "bShowScreenBox", 1, g_iniPath) != 0;
    g_pipWorldFirst = GetPrivateProfileIntA("Controls", "bPipBoyWorldFirst", 0, g_iniPath) != 0;
    g_lootCenter = GetPrivateProfileIntA("Display", "bCenterLootMenu", 1, g_iniPath) != 0;
    g_lootControls = GetPrivateProfileIntA("Controls", "bLootMenuControls", 1, g_iniPath) != 0;
    {   /* the quick-loot mod's own controller buttons (Just Mods Assorted) */
        char jp[MAX_PATH]; GetModuleFileNameA(NULL, jp, MAX_PATH); char *sl = strrchr(jp, '\\'); if (sl) sl[1] = 0; else jp[0] = 0;
        strncat(jp, "Data\\config\\JustMods.ini", MAX_PATH - strlen(jp) - 1);
        int b1 = GetPrivateProfileIntA("JLM", "JLMButton1", 4096, jp), b2 = GetPrivateProfileIntA("JLM", "JLMButton2", 16384, jp), ba = GetPrivateProfileIntA("JLM", "JLMButtonAdditional", 2, jp);
        if (b1 > 0 && b1 < 0x10000) g_jlmTake = (WORD)b1;
        if (b2 > 0 && b2 < 0x10000) g_jlmEquip = (WORD)b2;
        if (ba > 0 && ba < 0x10000) g_jlmAll = (WORD)ba;
        g_jlmKey1 = GetPrivateProfileIntA("JLM", "JLMKey1", 18, jp); g_jlmKey2 = GetPrivateProfileIntA("JLM", "JLMKey2", 19, jp); g_jlmKeyAll = GetPrivateProfileIntA("JLM", "JLMKeyAdditional", 42, jp);
    }
    { char lb[32]; GetPrivateProfileStringA("Display", "fLootMenuOffset", "0", lb, sizeof lb, g_iniPath); g_lootOffX = (float)atof(lb); }
    g_pipMenuBg = GetPrivateProfileIntA("PipScreen", "bMenuBackground", 1, g_iniPath) != 0;
    g_armSwing = GetPrivateProfileIntA("Controls", "bArmSwing", 1, g_iniPath) != 0;
    { char sb[32]; GetPrivateProfileStringA("Controls", "fPipBoyStickStrength", "0.35", sb, sizeof sb, g_iniPath); float v = (float)atof(sb); g_pipStickScale = v > 0.05f && v <= 1.0f ? v : 0.35f; }
    g_armSwingSprint = GetPrivateProfileIntA("Controls", "bArmSwingSprint", 1, g_iniPath) != 0;
    { char ab[32]; GetPrivateProfileStringA("Controls", "fArmSwingSensitivity", "1.0", ab, sizeof ab, g_iniPath); g_armSwingSens = fminf(fmaxf((float)atof(ab), 0.3f), 3.0f); }
    g_grapRumble = GetPrivateProfileIntA("Grapple", "bRumble", 1, g_iniPath) != 0;
    g_grapActors = GetPrivateProfileIntA("Grapple", "bHookActors", 1, g_iniPath) != 0;
    g_grapYank = GetPrivateProfileIntA("Grapple", "bPullPeople", 1, g_iniPath) != 0;
    g_ltBullet = GetPrivateProfileIntA("Controls", "bLeftTriggerBulletTime", 1, g_iniPath) != 0;
    g_bulletKey = GetPrivateProfileIntA("Controls", "iBulletTimeKey", 46, g_iniPath);
    { char cb[32]; GetPrivateProfileStringA("Controls", "fChestDown", "0.32", cb, sizeof cb, g_iniPath); g_chestDown = (float)atof(cb);
      GetPrivateProfileStringA("Controls", "fChestForward", "0.10", cb, sizeof cb, g_iniPath); g_chestFwd = (float)atof(cb);
      GetPrivateProfileStringA("Controls", "fChestRadius", "0.14", cb, sizeof cb, g_iniPath); g_chestR = (float)atof(cb); }
    { char hb[32]; struct { const char *k; float *v; const char *d; } hk[6] = {
          { "fHipHolsterDown", &g_hipDown, "0.62" }, { "fHipHolsterSide", &g_hipSide, "0.20" }, { "fShoulderHolsterDown", &g_shDown, "0.12" },
          { "fShoulderHolsterSide", &g_shSide, "0.16" }, { "fShoulderHolsterBack", &g_shBack, "0.12" }, { "fHolsterRadius", &g_holsterR, "0.16" } };
      for (int i = 0; i < 6; i++) { GetPrivateProfileStringA("Controls", hk[i].k, hk[i].d, hb, sizeof hb, g_iniPath); *hk[i].v = (float)atof(hb); }
      GetPrivateProfileStringA("Holsters", "sHip", "0", hb, sizeof hb, g_iniPath); g_holsterId[1] = (uint32_t)strtoul(hb, NULL, 16);
      GetPrivateProfileStringA("Holsters", "sShoulder", "0", hb, sizeof hb, g_iniPath); g_holsterId[2] = (uint32_t)strtoul(hb, NULL, 16); }
    { char sb3[32]; GetPrivateProfileStringA("Controls", "fPipBoyScreenLift", "1.0", sb3, sizeof sb3, g_iniPath); g_pipLift = (float)atof(sb3);
      GetPrivateProfileStringA("Controls", "fForearmTwistShare", "1.0", sb3, sizeof sb3, g_iniPath); float v = (float)atof(sb3); if (v >= 0 && v <= 1) g_twShare = v; }
    { char sb2[32]; const char *k3[3] = { "fPipBoyScreenShiftX", "fPipBoyScreenShiftY", "fPipBoyScreenShiftZ" };
      for (int i = 0; i < 3; i++) { GetPrivateProfileStringA("Controls", k3[i], "0", sb2, sizeof sb2, g_iniPath); g_pipShift[i] = (float)atof(sb2); }
      GetPrivateProfileStringA("Controls", "fPipMenuAboveBox", "0", sb2, sizeof sb2, g_iniPath); g_pipMenuAbove = (float)atof(sb2);
      g_pipHideBoxIni = GetPrivateProfileIntA("Controls", "bPipHideExtraBox", 1, g_iniPath) != 0;
      PP_Load();
      PipScr_Load();
      GetPrivateProfileStringA("Controls", "fPipMenuScale", "1.0", sb2, sizeof sb2, g_iniPath); g_pipMenuScale = (float)atof(sb2); if (!(g_pipMenuScale > 0.2f && g_pipMenuScale < 5.0f)) g_pipMenuScale = 1.0f;
      { const char *k4[3] = { "fPipMenuShiftX", "fPipMenuShiftY", "fPipMenuShiftZ" };
        for (int i = 0; i < 3; i++) { GetPrivateProfileStringA("Controls", k4[i], "0", sb2, sizeof sb2, g_iniPath); g_pipMenuShift[i] = (float)atof(sb2); } } }
    { char pb[32];
      GetPrivateProfileStringA("Controls", "fPipScreenBack", "0.10", pb, sizeof pb, g_iniPath); g_pipOvIni[0] = (float)atof(pb);
      GetPrivateProfileStringA("Controls", "fPipScreenOut", "0.045", pb, sizeof pb, g_iniPath); g_pipOvIni[1] = (float)atof(pb);
      GetPrivateProfileStringA("Controls", "fPipScreenUp", "0.0", pb, sizeof pb, g_iniPath); g_pipOvIni[2] = (float)atof(pb);
      GetPrivateProfileStringA("Controls", "fPipScreenWidth", "0.11", pb, sizeof pb, g_iniPath); g_pipOvIni[3] = (float)atof(pb); }
    { char fb[32]; GetPrivateProfileStringA("Controls", "fPipBoyScreenFit", "1.0", fb, sizeof fb, g_iniPath); float v = (float)atof(fb); if (v >= 0.3f && v <= 3.0f) g_pipFitIni = v; }
    g_mmPointer = GetPrivateProfileIntA("Controls", "bMenuPointer", 1, g_iniPath) != 0;
    g_mmShot = GetPrivateProfileIntA("Controls", "bMenuPointerShot", 1, g_iniPath) != 0;
    { char gb[32]; GetPrivateProfileStringA("Controls", "fMenuGunUp", "0.045", gb, sizeof gb, g_iniPath); g_mmGunUpIni = (float)atof(gb);
      GetPrivateProfileStringA("Controls", "fMenuGunForward", "0.03", gb, sizeof gb, g_iniPath); g_mmGunFwdIni = (float)atof(gb); }
    { char sb[32]; GetPrivateProfileStringA("Display", "fUIScale", "0.6", sb, sizeof sb, g_iniPath); float v = (float)atof(sb); if (v >= 0.15f && v <= 1.0f) g_uiScale = v;
      GetPrivateProfileStringA("Display", "fMenuScale", "0.5", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.15f && v <= 1.0f) g_menuScale = v;
      GetPrivateProfileStringA("Display", "fDialogueScale2", "0.5", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= 0.15f && v <= 1.0f) g_dlgScale = v;
      GetPrivateProfileStringA("Display", "fMenuHeight", "-0.12", sb, sizeof sb, g_iniPath); v = (float)atof(sb); if (v >= -0.5f && v <= 0.5f) g_menuYOff = v; }
    g_adsEnabled = GetPrivateProfileIntA("Controls", "bAimDownSights", 0, g_iniPath) != 0;
    g_pipRealTime = GetPrivateProfileIntA("Controls", "bPipBoyRealTime", 1, g_iniPath) != 0;
    g_liveMenus = GetPrivateProfileIntA("Display", "bLiveMenus", 1, g_iniPath) != 0;
    g_savePhotos = GetPrivateProfileIntA("Debug", "bSavePhotos", 0, g_iniPath) != 0;
    Hud_Load();
    { char lb[32]; GetPrivateProfileStringA("Display", "fLookAwayClose", "75", lb, sizeof lb, g_iniPath); g_lookAwayDeg = (float)atof(lb); }
    g_pipInstant = GetPrivateProfileIntA("Controls", "bInstantPipBoy", 1, g_iniPath) != 0;
    { char sb[32]; GetPrivateProfileStringA("Controls", "fPipBoyScale", "1.0", sb, sizeof sb, g_iniPath); float v = (float)atof(sb); if (v >= 0.5f && v <= 4.0f) g_pipScale = v; }
    g_handWalls = GetPrivateProfileIntA("Grab", "bHandWallCollision", 1, g_iniPath) != 0;
    g_throwPunch = GetPrivateProfileIntA("Grab", "bThrowAttacks", 0, g_iniPath) != 0;
    g_kidMess = GetPrivateProfileIntA("KidFriendly", "bBloodyMessExplodes", 1, g_iniPath) != 0;
    g_kidDismember = GetPrivateProfileIntA("KidFriendly", "bDismemberment", 1, g_iniPath) != 0;
    g_kidBlood = GetPrivateProfileIntA("KidFriendly", "bBlood", 1, g_iniPath) != 0;
    g_imperial = GetPrivateProfileIntA("Display", "bUSUnits", 0, g_iniPath) != 0;
    g_pipWorldDraw = GetPrivateProfileIntA("Controls", "bPipBoyLiveWorld", 1, g_iniPath) != 0;
    g_throwHurtFriends = GetPrivateProfileIntA("Grab", "bThrowHurtsCompanions", 0, g_iniPath) != 0;
    { char tb[32]; GetPrivateProfileStringA("Grab", "fThrowDamage", "1.0", tb, sizeof tb, g_iniPath); float v = (float)atof(tb); if (v >= 0.0f && v <= 5.0f) g_throwDmg = v; }
    g_autoScope = GetPrivateProfileIntA("Controls", "bRaiseToEyeScopes", 0, g_iniPath) != 0;
    g_scopesEnabled = GetPrivateProfileIntA("Controls", "bSeeThroughScopes", 1, g_iniPath) != 0;
    {
        char buf[32];
        GetPrivateProfileStringA("Stereo", "fWorldScale", "70.0", buf, sizeof buf, g_iniPath);
        float v = (float)atof(buf);
        if (v > 5.0f && v < 1000.0f) g_unitsPerMeter = v;
        GetPrivateProfileStringA("Controls", "fShotYawTrim", "-0.75", buf, sizeof buf, g_iniPath);
        g_shotYawTrimDeg = (float)atof(buf);
        GetPrivateProfileStringA("Controls", "fShotPitchTrim", "0.75", buf, sizeof buf, g_iniPath);
        g_shotPitchTrimDeg = (float)atof(buf);
        GetPrivateProfileStringA("Controls", "fSwingSpeed", "2.2", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 0.5f && v <= 10.0f) g_swingSpeed = v;
        GetPrivateProfileStringA("Controls", "fAimPitchOffset", "-60.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= -90.0f && v <= 90.0f) g_aimPitchOffsetDeg = v;
        GetPrivateProfileStringA("Tracking", "fMaxLean", "0.35", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 0.0f && v <= 1.0f) g_maxLean = v;
        GetPrivateProfileStringA("Controls", "fScopeMaxZoom", "4.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 1.0f && v <= 10.0f) g_scopeMaxZoom = v;
        GetPrivateProfileStringA("Controls", "fScopeBrightness", "1.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 0.1f && v <= 4.0f) g_scopeBright = v;
        g_capPick = GetPrivateProfileIntA("Controls", "iScopeCaptureIndex", -1, g_iniPath);
        GetPrivateProfileStringA("Controls", "fSprintSpeed", "1.5", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 1.0f && v <= 4.0f) g_sprintMult = v;
        GetPrivateProfileStringA("Grab", "fGrabReach", "12.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 1.0f && v <= 60.0f) g_grabReach = v;
        GetPrivateProfileStringA("Grab", "fGrabMaxMass", "200.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v > 0.0f) g_grabMaxMass = v;
        GetPrivateProfileStringA("Grab", "fCollisionStrength", "1.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 0.0f && v <= 5.0f) g_collisionStrength = v;
        GetPrivateProfileStringA("Grab", "fThrowStrength", "1.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 0.0f && v <= 5.0f) g_throwStrength = v;
        GetPrivateProfileStringA("Display", "fMenuScreenFOV", "90.0", buf, sizeof buf, g_iniPath);
        v = (float)atof(buf);
        if (v >= 30.0f && v <= 110.0f) g_menuScreenFov = v;
    }
    if (g_predictionMs < 0) g_predictionMs = 0;
    if (g_predictionMs > 150) g_predictionMs = 150;
    if (g_pixelDensityPct != 0 && g_pixelDensityPct < 25) g_pixelDensityPct = 25;
    if (g_pixelDensityPct > 200) g_pixelDensityPct = 200;
    Profiles_Init();
    Log("Settings: %s", g_iniPath);
    Log("  iPredictionMs=%d bHeadsetDisplay=%d iPixelDensityPercent=%d bMatchHeadsetFOV=%d bAsyncCapture=%d bStereo=%d fWorldScale=%.1f",
        g_predictionMs, (int)g_headsetDisplay, g_pixelDensityPct, (int)g_matchHeadsetFov, (int)g_asyncCapture,
        (int)g_stereo, g_unitsPerMeter);
    Log("  bMotionControls=%d bHandAim=%d bLeftHanded=%d fAimPitchOffset=%.1f",
        (int)g_motionControls, (int)g_handAim, (int)g_leftHanded, g_aimPitchOffsetDeg);
    Log("  bGrabObjects=%d fGrabReach=%.1f fGrabMaxMass=%.1f fThrowStrength=%.2f bPhysicalCrouchLean=%d fMaxLean=%.2f",
        (int)g_grabEnabled, g_grabReach, g_grabMaxMass, g_throwStrength, (int)g_physicalMove, g_maxLean);
}

static void SaveInt(const char *section, const char *key, int value)
{
    char buf[16];
    snprintf(buf, sizeof buf, "%d", value);
    WritePrivateProfileStringA(section, key, buf, g_iniPath);
}

static bool GameWindowFocused(void)
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static bool KeyPressed(int vk, bool *wasDown)
{
    bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool pressed = down && !*wasDown;
    *wasDown = down;
    return pressed && GameWindowFocused();
}

static float WrapPi(float a)
{
    while (a >  PI_F) a -= 2.0f * PI_F;
    while (a < -PI_F) a += 2.0f * PI_F;
    return a;
}

/* ======================================================================= */
/* SteamVR connection                                                      */
/* ======================================================================= */

typedef intptr_t (*VR_InitInternal2_t)(EVRInitError *err, EVRApplicationType type, const char *startupInfo);
typedef intptr_t (*VR_InitInternal_t)(EVRInitError *err, EVRApplicationType type);
typedef void     (*VR_ShutdownInternal_t)(void);
typedef intptr_t (*VR_GetGenericInterface_t)(const char *ver, EVRInitError *err);
typedef bool     (*VR_IsRuntimeInstalled_t)(void);
typedef const char *(*VR_GetVRInitErrorAsEnglishDescription_t)(EVRInitError err);

static HMODULE g_ovrDll = NULL;
static VR_ShutdownInternal_t pVR_Shutdown = NULL;
static struct VR_IVRSystem_FnTable     *g_vrSystem = NULL;
static struct VR_IVRCompositor_FnTable *g_vrCompositor = NULL;
static struct VR_IVROverlay_FnTable    *g_vrOverlay = NULL;
static VROverlayHandle_t g_uiOverlay = 0;
static bool  g_curvedActive = false;     /* overlay created and working */
static bool  g_vrFailedPermanently = false;
static float g_ipdMeters = 0.064f;
static DWORD g_lastInitAttempt = 0;

static bool VR_Start(void)
{
    LogMem("before SteamVR");
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%sFNVR\\openvr_api.dll", g_pluginDir);
    if (!g_ovrDll) {
        g_ovrDll = LoadLibraryA(path);
        if (!g_ovrDll) {
            Log("ERROR: could not load %s (Windows error %lu)", path, GetLastError());
            g_vrFailedPermanently = true;
            return false;
        }
        Log("Loaded %s", path);
    }

    VR_InitInternal2_t init2 = (VR_InitInternal2_t)GetProcAddress(g_ovrDll, "VR_InitInternal2");
    VR_InitInternal_t init1 = (VR_InitInternal_t)GetProcAddress(g_ovrDll, "VR_InitInternal");
    VR_GetGenericInterface_t getIntf = (VR_GetGenericInterface_t)GetProcAddress(g_ovrDll, "VR_GetGenericInterface");
    VR_IsRuntimeInstalled_t isInstalled = (VR_IsRuntimeInstalled_t)GetProcAddress(g_ovrDll, "VR_IsRuntimeInstalled");
    VR_GetVRInitErrorAsEnglishDescription_t errStr =
        (VR_GetVRInitErrorAsEnglishDescription_t)GetProcAddress(g_ovrDll, "VR_GetVRInitErrorAsEnglishDescription");
    pVR_Shutdown = (VR_ShutdownInternal_t)GetProcAddress(g_ovrDll, "VR_ShutdownInternal");

    if ((!init2 && !init1) || !getIntf || !pVR_Shutdown) {
        Log("ERROR: openvr_api.dll is missing expected functions");
        g_vrFailedPermanently = true;
        return false;
    }
    if (isInstalled && !isInstalled()) {
        Log("ERROR: SteamVR does not appear to be installed");
        g_vrFailedPermanently = true;
        return false;
    }

    EVRApplicationType type = g_headsetDisplay ? EVRApplicationType_VRApplication_Scene
                                               : EVRApplicationType_VRApplication_Other;
    EVRInitError err = EVRInitError_VRInitError_None;
    if (init2) init2(&err, type, "");
    else       init1(&err, type);
    if (err != EVRInitError_VRInitError_None) {
        Log("SteamVR init failed: error %d (%s). Will retry in 10 seconds.",
            (int)err, errStr ? errStr(err) : "?");
        return false;
    }

    char ver[64];
    snprintf(ver, sizeof ver, "FnTable:%s", IVRSystem_Version);
    g_vrSystem = (struct VR_IVRSystem_FnTable *)getIntf(ver, &err);
    if (!g_vrSystem || err != EVRInitError_VRInitError_None) {
        Log("ERROR: could not get %s (error %d)", ver, (int)err);
        pVR_Shutdown();
        g_vrSystem = NULL;
        return false;
    }
    Log("Connected to SteamVR (%s, app type %s)", IVRSystem_Version, g_headsetDisplay ? "Scene" : "Other");
    LogMem("after SteamVR connect");
    {
        ETrackedPropertyError perr = ETrackedPropertyError_TrackedProp_Success;
        float ipd = g_vrSystem->GetFloatTrackedDeviceProperty(0, ETrackedDeviceProperty_Prop_UserIpdMeters_Float, &perr);
        if (perr == ETrackedPropertyError_TrackedProp_Success && ipd > 0.04f && ipd < 0.09f) g_ipdMeters = ipd;
        Log("Headset IPD: %.1f mm (error %d)", g_ipdMeters * 1000.0f, (int)perr);
    }

    if (g_headsetDisplay) {
        snprintf(ver, sizeof ver, "FnTable:%s", IVRCompositor_Version);
        g_vrCompositor = (struct VR_IVRCompositor_FnTable *)getIntf(ver, &err);
        if (!g_vrCompositor || err != EVRInitError_VRInitError_None) {
            Log("ERROR: could not get %s (error %d) - headset display disabled, tracking only", ver, (int)err);
            g_vrCompositor = NULL;
        } else {
            g_vrCompositor->SetTrackingSpace(ETrackingUniverseOrigin_TrackingUniverseStanding);
            Log("Got compositor (%s)", IVRCompositor_Version);
            if ((g_curvedUI || g_farMenu) && !g_flatMenus) {
                snprintf(ver, sizeof ver, "FnTable:%s", IVROverlay_Version);
                g_vrOverlay = (struct VR_IVROverlay_FnTable *)getIntf(ver, &err);
                if (g_vrOverlay && err == EVRInitError_VRInitError_None && !g_curvedUI) {
                    Log("SteamVR overlays available (curved interface panel off)");
                } else if (g_vrOverlay && err == EVRInitError_VRInitError_None &&
                    g_vrOverlay->CreateOverlay("fnvr.interface", "FNVR interface", &g_uiOverlay) == EVROverlayError_VROverlayError_None) {
                    float arc = g_uiArcDeg / RAD2DEG;
                    g_vrOverlay->SetOverlayWidthInMeters(g_uiOverlay, g_uiDist * arc);
                    g_vrOverlay->SetOverlayCurvature(g_uiOverlay, arc / (2.0f * PI_F));
                    g_vrOverlay->SetOverlayFlag(g_uiOverlay, VROverlayFlags_IsPremultiplied, true);
                    g_curvedActive = true;
                    Log("Curved interface: panel %.2f m away, wrapping %.0f deg", g_uiDist, g_uiArcDeg);
                } else { Log("Curved interface: SteamVR overlay unavailable (error %d) - interface stays in the picture", (int)err); g_vrOverlay = NULL; }
            }
        }
    }
    return true;
}

static void Display_ReleaseAll(void);

static void VR_Stop(const char *why)
{
    if (g_vrSystem && pVR_Shutdown) {
        Log("Disconnecting from SteamVR: %s", why);
        Display_ReleaseAll();
        pVR_Shutdown();
    }
    g_vrSystem = NULL;
    g_vrCompositor = NULL;
}

/* ======================================================================= */
/* Motion controls: Touch controllers -> virtual Xbox controller (XInput)    */
/* ======================================================================= */

typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XPad;
typedef struct { DWORD dwPacketNumber; XPad Gamepad; } XState;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XState *);

#define XB_DPAD_UP 0x0001
#define XB_DPAD_DOWN 0x0002
#define XB_DPAD_LEFT 0x0004
#define XB_DPAD_RIGHT 0x0008
#define XB_START 0x0010
#define XB_BACK 0x0020
#define XB_LTHUMB 0x0040
#define XB_RTHUMB 0x0080
#define XB_LSHOULDER 0x0100
#define XB_RSHOULDER 0x0200
#define XB_A 0x1000
#define XB_B 0x2000
#define XB_X 0x4000
#define XB_Y 0x8000

static XInputGetState_t o_XInputGetState = NULL;
static volatile LONG g_padLock = 0;
static XState g_padState;
static bool  g_padActive = false;        /* controllers connected and feeding the game */
static bool  g_bothSticksClicked = false;
static DWORD g_padPacket = 0;
static unsigned g_xinputCalls = 0;
static TrackedDeviceIndex_t g_handIdx[2] = { 0xFFFFFFFF, 0xFFFFFFFF };   /* 0 = left, 1 = right */
static bool  g_loggedControllers = false;

static void PadLock(void)   { while (InterlockedExchange(&g_padLock, 1)) YieldProcessor(); }
static void PadUnlock(void) { InterlockedExchange(&g_padLock, 0); }

static DWORD WINAPI Hook_XInputGetState(DWORD user, XState *st)
{
    g_xinputCalls++;
    if (g_motionControls && user == 0 && st) {
        /* Always report the virtual controller as plugged in: the game only looks for a controller
           at startup, before the headset connection is up. Until then it reads as idle. */
        PadLock();
        if (g_padActive) *st = g_padState;
        else { memset(st, 0, sizeof *st); st->dwPacketNumber = g_padPacket; }
        PadUnlock();
        return ERROR_SUCCESS;
    }
    return o_XInputGetState ? o_XInputGetState(user, st) : ERROR_DEVICE_NOT_CONNECTED;
}

/* Game code that reads the controller (FalloutNV.exe 1.4.0.525):
   0x9F996E  jmp [0xFDF394]   - thunk to XInputGetState, used by every caller
   0xA23010  OSInputGlobals::PollGamepad, called every frame from the main loop */
#define XINPUT_THUNK_ADDR   0x009F996E
#define XINPUT_IAT_ADDR     0x00FDF394
#define POLLGAMEPAD_ADDR    0x00A23010
#define OSINPUTGLOBALS_PTR  0x011F35CC   /* OSInputGlobals*; its first byte = "controller disabled" */
#define GAMEPAD_CONNECTED   0x011F35C8   /* byte set by PollGamepad */
typedef void (__attribute__((thiscall)) *PollGamepad_t)(void *osInput);

static void DescribeAddress(DWORD addr, char *out, size_t n);

/* Called after all plugins have loaded: report who else touches the controller path, then make the
   game's thunk jump straight to us so nothing in between can swallow the calls. */
static void InstallXInputThunkHook(void)
{
    char d1[128], d2[128];
    BYTE *thunk = (BYTE *)XINPUT_THUNK_ADDR;
    BYTE *poll = (BYTE *)POLLGAMEPAD_ADDR;
    DWORD iatNow = *(DWORD *)XINPUT_IAT_ADDR;
    DescribeAddress(iatNow, d1, sizeof d1);
    Log("Controller path check: IAT XInputGetState -> %s (ours %p)", d1, (void *)Hook_XInputGetState);
    Log("Controller path check: thunk bytes %02X %02X %02X %02X %02X %02X, PollGamepad bytes %02X %02X %02X %02X %02X",
        thunk[0], thunk[1], thunk[2], thunk[3], thunk[4], thunk[5], poll[0], poll[1], poll[2], poll[3], poll[4]);
    if (poll[0] == 0xE9) {
        DWORD tgt = (DWORD)(POLLGAMEPAD_ADDR + 5) + *(int32_t *)(poll + 1);
        DescribeAddress(tgt, d2, sizeof d2);
        Log("Controller path check: PollGamepad is redirected by another mod to %s", d2);
    }
    /* The four places in the game that call XInputGetState. Other mods sometimes redirect these
       (e.g. to skip controller polling for performance) - point them back at FNVR. */
    static const DWORD sites[] = { 0x00709FFB, 0x0070C525, 0x007D4399, 0x00A2304C };
    for (int i = 0; i < 4; i++) {
        BYTE *c = (BYTE *)sites[i];
        if (c[0] != 0xE8) {
            Log("Controller path check: call site %08lX has unexpected byte %02X - left alone", (unsigned long)sites[i], c[0]);
            continue;
        }
        DWORD tgt = (DWORD)(sites[i] + 5) + *(int32_t *)(c + 1);
        if (tgt != XINPUT_THUNK_ADDR) {
            DescribeAddress(tgt, d2, sizeof d2);
            Log("Controller path check: call site %08lX was redirected by another mod to %s - reclaiming it", (unsigned long)sites[i], d2);
        }
        DWORD old;
        VirtualProtect(c, 5, PAGE_EXECUTE_READWRITE, &old);
        *(int32_t *)(c + 1) = (int32_t)((uintptr_t)Hook_XInputGetState - (sites[i] + 5));
        VirtualProtect(c, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), c, 5);
    }
    Log("Controller path: all game controller reads now go to FNVR");

    if (thunk[0] == 0xFF && thunk[1] == 0x25 && *(DWORD *)(thunk + 2) == XINPUT_IAT_ADDR) {
        if (iatNow != (DWORD)(uintptr_t)Hook_XInputGetState && !o_XInputGetState) o_XInputGetState = (XInputGetState_t)(uintptr_t)iatNow;
        DWORD old;
        VirtualProtect(thunk, 6, PAGE_EXECUTE_READWRITE, &old);
        thunk[0] = 0xE9;
        *(int32_t *)(thunk + 1) = (int32_t)((uintptr_t)Hook_XInputGetState - (XINPUT_THUNK_ADDR + 5));
        thunk[5] = 0x90;
        VirtualProtect(thunk, 6, old, &old);
        FlushInstructionCache(GetCurrentProcess(), thunk, 6);
        Log("Controller path: game's XInput thunk now jumps directly to FNVR");
    } else if (thunk[0] == 0xE9) {
        DWORD tgt = (DWORD)(XINPUT_THUNK_ADDR + 5) + *(int32_t *)(thunk + 1);
        DescribeAddress(tgt, d2, sizeof d2);
        Log("Controller path check: XInput thunk already redirected by another mod to %s", d2);
    }
}

/* Point the game's import of XInputGetState (XINPUT1_3 ordinal 2) at our function. */
static void InstallXInputHook(void)
{
    BYTE *base = (BYTE *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) { Log("XInput hook: no import table"); return; }
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; d++) {
        const char *dll = (const char *)(base + d->Name);
        if (_strnicmp(dll, "xinput", 6) != 0) continue;
        IMAGE_THUNK_DATA32 *orig = (IMAGE_THUNK_DATA32 *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        IMAGE_THUNK_DATA32 *iat = (IMAGE_THUNK_DATA32 *)(base + d->FirstThunk);
        for (; orig->u1.AddressOfData; orig++, iat++) {
            bool isGetState = false;
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG32) isGetState = IMAGE_ORDINAL32(orig->u1.Ordinal) == 2;
            else isGetState = strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData))->Name, "XInputGetState") == 0;
            if (!isGetState) continue;
            DWORD old;
            VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old);
            o_XInputGetState = (XInputGetState_t)(uintptr_t)iat->u1.Function;
            iat->u1.Function = (DWORD)(uintptr_t)Hook_XInputGetState;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            Log("XInput hook installed (%s XInputGetState, original %p)", dll, (void *)o_XInputGetState);
            return;
        }
    }
    Log("XInput hook: XInputGetState import not found - motion controller buttons unavailable");
}

static SHORT AxisToShort(float v)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (SHORT)(v * 32767.0f);
}

static bool Pressed(const VRControllerState_t *c, int button) { return (c->ulButtonPressed >> button) & 1ull; }

/* Game actions we need, and the Xbox button the game has each one bound to. */
enum { BIND_JUMP, BIND_RELOAD, BIND_ACTIVATE, BIND_SNEAK, BIND_PIPBOY, BIND_VATS, BIND_COUNT };
static WORD g_bindMask[BIND_COUNT] = { 0x1000 /*A*/, 0x4000 /*X*/, 0x2000 /*B*/, 0x0080 /*RS*/, 0x8000 /*Y*/, 0x0200 /*RB*/ };

/* The game's own numbering of Xbox controls (kXboxCtrl_*) -> XInput button bit. */
static WORD XboxCodeToMask(unsigned code)
{
    switch (code) {
    case 0x01: return XB_DPAD_UP;    case 0x02: return XB_DPAD_DOWN;
    case 0x04: return XB_DPAD_RIGHT; case 0x05: return XB_DPAD_LEFT;
    case 0x06: return XB_START;      case 0x07: return XB_BACK;
    case 0x08: return XB_LTHUMB;     case 0x09: return XB_RTHUMB;
    case 0x0A: return XB_A;          case 0x0B: return XB_B;
    case 0x0C: return XB_X;          case 0x0D: return XB_Y;
    case 0x0E: return XB_RSHOULDER;  case 0x0F: return XB_LSHOULDER;
    default:   return 0;             /* triggers / unbound */
    }
}

static void LoadGameBindings(void)
{
    char path[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, path))) { Log("Bindings: could not find Documents folder"); return; }
    strcat(path, "\\My Games\\FalloutNV\\FalloutPrefs.ini");
    static const char *names[BIND_COUNT] = { "Jump", "Ready Item", "Activate", "Crouch/Sneak", "Menu Mode", "Vats" };
    for (int i = 0; i < BIND_COUNT; i++) {
        char buf[32] = "";
        GetPrivateProfileStringA("Controls", names[i], "", buf, sizeof buf, path);
        if (!buf[0]) continue;
        unsigned long v = strtoul(buf, NULL, 16);
        WORD m = XboxCodeToMask(v & 0xFF);
        if (m) g_bindMask[i] = m;
        Log("Bindings: %-13s = %s -> controller code %02lX -> button mask %04X", names[i], buf, v & 0xFF, g_bindMask[i]);
    }
}

/* Hold/release the game's Run key (keyboard) - sent as a hardware scancode so DirectInput sees it. */
static bool g_runHeld = false;
/* Run: the game's Run key isn't bound on this setup (Run=00 in FalloutPrefs), so instead of faking
   a key press, flip the player's own "always run" switch while the stick is clicked. */
#define PLAYER_ALWAYS_RUN 0x651
static int g_runSaved = -1;   /* the player's always-run setting before we changed it */
static void SetRunKey(bool down)
{
    if (down == g_runHeld) return;
    char *player = (char *)GetPlayer();
    if (!player) { g_runHeld = false; g_runSaved = -1; return; }
    BYTE *ar = (BYTE *)(player + PLAYER_ALWAYS_RUN);
    if (down) {
        g_runSaved = *ar;
        *ar = 1;
        static int logs = 0;
        if (logs++ < 5) Log("Run: stick clicked - always-run was %d, now running", g_runSaved);
    } else if (g_runSaved >= 0) {
        *ar = (BYTE)g_runSaved;
        g_runSaved = -1;
    }
    g_runHeld = down;
}

/* ---- Running by swinging your arms (like Blade & Sorcery) ----------------------------------------------
   Pump both arms like you're running - one forward while the other goes back - and you move forward where
   you're looking. The harder you swing, the faster you go; swinging hard sprints. The stick still works
   and always wins when you push it. */
static bool g_swingSprint = false;
static float ArmSwing_Stick(bool menu)
{
    static float prevRel[2][3], intensity = 0; static double prevT = 0; static bool have = false;
    g_swingSprint = false;
    if (!g_armSwing || menu || !g_inGameplay || g_pipOpen || g_setOpen || g_climb[0] || g_climb[1]) { have = false; intensity = 0; return 0; }
    float rel[2][3];
    for (int h = 0; h < 2; h++) for (int k = 0; k < 3; k++) rel[h][k] = g_vrHandPos[h][k] - g_vrHeadPos[k];
    double now = QpcMs();
    bool changed = !have || memcmp(rel, prevRel, sizeof rel) != 0;
    if (changed) {
        float dt = have ? (float)((now - prevT) / 1000.0) : 0.0f;
        if (have && dt > 0.002f && dt < 0.1f) {
            float v[2][3], sp[2];
            for (int h = 0; h < 2; h++) { for (int k = 0; k < 3; k++) v[h][k] = (rel[h][k] - prevRel[h][k]) / dt; sp[h] = sqrtf(v[h][0] * v[h][0] + v[h][1] * v[h][1] + v[h][2] * v[h][2]); }
            float dot = v[0][0] * v[1][0] + v[0][1] * v[1][1] + v[0][2] * v[1][2];
            /* both hands moving, in opposite directions (a running pump, not a two-handed swing or a wave) */
            float raw = (dot < 0 && sp[0] > 0.25f && sp[1] > 0.25f) ? fminf(fminf(sp[0], sp[1]), 4.0f) : 0.0f;
            if (raw > intensity) intensity += (raw - intensity) * 0.5f;
            else intensity *= expf(-dt / 0.45f);   /* keeps going between swings (hands slow down at each end) */
        } else if (have && dt >= 0.1f) intensity = 0;
        memcpy(prevRel, rel, sizeof rel); prevT = now; have = true;
    } else if (now - prevT > 150.0) intensity = 0;
    float I = intensity * g_armSwingSens;
    if (I < 0.35f) return 0;
    float v = fminf(fmaxf(0.35f + (I - 0.35f) / 0.65f * 0.65f, 0.35f), 1.0f);
    g_swingSprint = g_armSwingSprint && I > 1.8f;
    static bool wasOn = false; static int nl = 0;
    if (!wasOn && nl++ < 6) Log("Arm swing: running (%.2f m/s swing -> stick %.2f%s)", intensity, v, g_swingSprint ? ", sprint" : "");
    wasOn = true;
    return v;
}

/* Sprint: while the stick is clicked, raise the player's walk/run speed. The game keeps these in a
   per-actor cache (BaseProcess::cachedValues); we scale the cached values and let the game recompute
   them on release - nothing is written to the character or the save. */
static bool  g_sprinting = false;
static float g_sprintBase[2] = { 0, 0 }, g_sprintSet[2] = { 0, 0 };
static void Sprint_Update(bool want)
{
    char *player = (char *)GetPlayer();
    char *proc = player ? *(char **)(player + 0x68) : NULL;
    char *cv = proc ? *(char **)(proc + 0x2C) : NULL;
    if (!cv) { g_sprinting = false; return; }
    float *walk = (float *)(cv + 0x38), *run = (float *)(cv + 0x3C);
    uint32_t *flags = (uint32_t *)(cv + 0x44);
    if (want && g_sprintMult > 1.0f) {
        float *sp[2] = { walk, run };
        for (int i = 0; i < 2; i++) {
            /* first frame, or the game recomputed the value since we set it */
            if (!g_sprinting || fabsf(*sp[i] - g_sprintSet[i]) > 0.01f) g_sprintBase[i] = *sp[i];
            g_sprintSet[i] = g_sprintBase[i] * g_sprintMult;
            *sp[i] = g_sprintSet[i];
        }
        if (!g_sprinting) {
            static int logs = 0;
            if (logs++ < 5) Log("Sprint: walk %.0f -> %.0f, run %.0f -> %.0f (cache flags %08X)",
                                g_sprintBase[0], g_sprintSet[0], g_sprintBase[1], g_sprintSet[1], *flags);
        }
        g_sprinting = true;
    } else if (g_sprinting) {
        *walk = g_sprintBase[0]; *run = g_sprintBase[1];   /* back to normal right away */
        *flags &= ~0x3000u;   /* and let the game recompute walk/run speed */
        g_sprinting = false;
    }
}

/* What the player is holding: -1 = nothing (fists), else TESObjectWEAP::eWeaponType
   (0 = hand-to-hand weapon, 1 = one-hand melee, 2 = two-hand melee, 3+ = guns/thrown). */
typedef void *(__attribute__((thiscall)) *GetWeaponInfo_t)(void *process);
static int PlayerWeaponType(void)
{
    char *player = (char *)GetPlayer();
    if (!player) return -2;
    char *process = *(char **)(player + 0x68);
    if (!process) return -2;
    GetWeaponInfo_t f = (GetWeaponInfo_t)(*(void ***)process)[0x148 / 4];
    char *entry = (char *)f(process);
    char *weapon = entry ? *(char **)(entry + 0x08) : NULL;
    if (!weapon) return -1;
    return *(BYTE *)(weapon + 0xF4);
}

static void *PlayerWeaponForm(void);
static bool Scr_Num(void *scr, void *arg, double *out);
static char *Scr_Ref(void *scr, void *arg);
static int Stim_Pick(void);
static bool Stim_Begin(int h, int type);
static volatile int g_animAction = -1;   /* the player's current action (game's GetAnimAction): -1 none, 0 equip, 1 unequip, 2-4 attack, 9 reload */
static double g_animIdleSince = 0;      /* when the action last became "none" */
static bool g_animLastBusy = false;     /* the action before "none" was drawing or holstering */
/* An automatic weapon (the game's own "automatic" flag): for melee that's the chainsaw-type weapons. */
static bool PlayerWeaponAutomatic(void) { char *w = (char *)PlayerWeaponForm(); return w && !IsBadReadPtr(w + 0x100, 1) && (*(BYTE *)(w + 0x100) & 2); }
static void *PlayerWeaponForm(void)
{
    char *player = (char *)GetPlayer();
    if (!player) return NULL;
    char *process = *(char **)(player + 0x68);
    if (!process) return NULL;
    GetWeaponInfo_t f = (GetWeaponInfo_t)(*(void ***)process)[0x148 / 4];
    char *entry = (char *)f(process);
    return entry ? *(void **)(entry + 0x08) : NULL;
}

static bool PlayerWeaponScoped(void)
{
    char *player = (char *)GetPlayer();
    char *process = player ? *(char **)(player + 0x68) : NULL;
    if (!process) return false;
    char *entry = (char *)((GetWeaponInfo_t)(*(void ***)process)[0x148 / 4])(process);
    char *weapon = entry ? *(char **)(entry + 0x08) : NULL;
    return weapon && *(BYTE *)(weapon + 0xF4) >= 3 && (*(BYTE *)(weapon + 0x100) & 4);
}

/* Melee: a fast swing of a hand (punch, slash) presses attack for a moment. */
static int   g_swingFrames = 0;
static DWORD g_lastSwing = 0;

static void Melee_Update(bool menu)
{
    if (!g_meleeSwings || menu || !g_inGameplay) { g_swingFrames = 0; return; }
    if (g_swingFrames > 0) { g_swingFrames--; return; }
    if (g_holding[0] || g_holding[1]) return;   /* moving a held object around isn't a punch */
    if (!g_throwPunch && GetTickCount() - g_lastThrowTick < 900) return;   /* nor is the throw itself */
    int wt = PlayerWeaponType();
    bool melee = wt == -1 || (wt >= 0 && wt <= 2);
    if (!melee) return;
    int aim = g_leftHanded ? 0 : 1, off = 1 - aim;
    float sp = g_vrHandOk[aim] ? g_handSpeed[aim] : 0.0f;
    if (wt == -1 && g_vrHandOk[off]) sp = fmaxf(sp, g_handSpeed[off]);   /* fists: either hand punches */
    DWORD now = GetTickCount();
    if (sp > g_swingSpeed && now - g_lastSwing > 450) {
        g_lastSwing = now;
        g_swingFrames = 6;
        static int logs = 0;
        if (logs++ < 5) Log("Melee swing detected (%.1f m/s, weapon type %d) - attacking", sp, wt);
    }
}

static void *LookupForm(uint32_t id);
static void ExtractYawPitch(const HmdMatrix34_t *mat, float *yaw, float *pitch);
static float V3Len(const float *v);
static void V3Sub(float *o, const float *a, const float *b);
static bool PlayerWeaponOut(void);
/* ---- Holsters: pistols on the right hip, long guns over the right shoulder ------------------
   Reach to a holster with the gun hand and squeeze the grip: a weapon in your hand of the right kind is
   put away there (your hand is free again for grabbing and climbing); with an empty hand the weapon
   kept there is drawn. A light buzz tells you when your hand is at a holster. */
static void *g_holster[3] = { NULL, NULL, NULL };   /* 1 = hip, 2 = shoulder */
static float g_vrHeadYaw = 0.0f;
static WORD  g_holsterPress = 0; static double g_holsterPressUntil = 0, g_holsterCheckAt = 0;
static void *g_scrEquip = NULL, *g_scrUnequip = NULL; static bool g_scrHolsterFailed = false;

static int Holster_SlotOf(int t) { return (t == 0 || t == 1 || t == 3 || t == 4) ? 1 : (t == 2 || (t >= 5 && t <= 9)) ? 2 : 0; }

static int Holster_Zone(void)
{
    if (!g_holstersOn) return 0;
    int h = g_leftHanded ? 0 : 1;
    if (!g_vrHandOk[h]) return 0;
    float side = g_leftHanded ? -1.0f : 1.0f;
    float fw[3] = { -sinf(g_vrHeadYaw), 0, -cosf(g_vrHeadYaw) }, rt[3] = { cosf(g_vrHeadYaw), 0, -sinf(g_vrHeadYaw) };
    float hip[3], sh[3];
    for (int i = 0; i < 3; i++) {
        hip[i] = g_vrHeadPos[i] + rt[i] * g_hipSide * side;
        sh[i]  = g_vrHeadPos[i] + rt[i] * g_shSide * side - fw[i] * g_shBack;
    }
    hip[1] = g_vrHeadPos[1] - g_hipDown; sh[1] = g_vrHeadPos[1] - g_shDown;
    float dh[3], ds[3]; V3Sub(dh, g_vrHandPos[h], hip); V3Sub(ds, g_vrHandPos[h], sh);
    float a = V3Len(dh), b = V3Len(ds);
    if (b < g_holsterR && b <= a) return 2;
    if (a < g_holsterR) return 1;
    return 0;
}

static bool Holster_Scripts(void)
{
    if (g_scrHolsterFailed || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return false;
    if (!g_scrEquip) g_scrEquip = g_scriptIntf->CompileScript("ref w\nBegin Function { w }\nplayer.EquipItem w 0 1\nEnd\n");
    if (!g_scrUnequip) g_scrUnequip = g_scriptIntf->CompileScript("ref w\nBegin Function { w }\nplayer.UnequipItem w 0 1\nEnd\n");
    if (!g_scrEquip || !g_scrUnequip) { g_scrHolsterFailed = true; Log("Holsters: couldn't set up equipping - holsters off"); return false; }
    return true;
}
static void Holster_Save(int slot, void *w)
{
    g_holster[slot] = w;
    uint32_t id = w ? *(uint32_t *)((char *)w + 0x0C) : 0;
    if (id == g_holsterId[slot]) return;
    g_holsterId[slot] = id;
    char b[16]; snprintf(b, sizeof b, "%08X", id);
    WritePrivateProfileStringA("Holsters", slot == 1 ? "sHip" : "sShoulder", b, g_iniPath);
}
static void Holster_Buzz(unsigned short us)
{
    int h = g_leftHanded ? 0 : 1;
    if (g_vrSystem && g_handIdx[h] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, us);
}
/* the gun hand squeezed its grip at a holster */
static void Holster_Use(int zone)
{
    if (!Holster_Scripts()) return;
    void *w = PlayerWeaponForm(); int t = PlayerWeaponType();
    void *player = GetPlayer();
    if (!player) return;
    if (!g_holster[zone] && g_holsterId[zone]) g_holster[zone] = LookupForm(g_holsterId[zone]);
    if (w && t >= 0 && Holster_SlotOf(t) == zone) {          /* put it away */
        Holster_Save(zone, w);
        g_scriptIntf->CallFunctionAlt(g_scrUnequip, player, 1, w);
        Holster_Buzz(3000);
        Log("Holsters: %s put away on your %s", t >= 3 ? "gun" : "weapon", zone == 1 ? "hip" : "shoulder");
    } else if (g_holster[zone] && g_holster[zone] != w) {   /* draw what's kept there */
        if (w && t >= 0 && Holster_SlotOf(t)) Holster_Save(Holster_SlotOf(t), w);
        g_scriptIntf->CallFunctionAlt(g_scrEquip, player, 1, g_holster[zone]);
        g_holsterCheckAt = QpcMs() + 700.0;
        Holster_Buzz(3000);
        Log("Holsters: drew the weapon from your %s", zone == 1 ? "hip" : "shoulder");
    } else {
        Holster_Buzz(800);
        Log("Holsters: nothing kept on your %s yet", zone == 1 ? "hip" : "shoulder");
    }
}
/* every frame (main loop): remember what you equip, make sure a drawn weapon is out */
static void Scope_Forget(void);
static void Holster_Tick(void)
{
    if (!g_inGameplay) return;
    if (!g_holstersOn) {   /* still watch for weapon changes (scope settle time) */
        static void *lw = NULL; void *w0 = PlayerWeaponForm();
        if (w0 != lw) { lw = w0; Scope_Forget(); int t0 = PlayerWeaponType(); g_weaponSettleUntil = (t0 >= 5 && t0 <= 9) ? QpcMs() + 1500.0 : 0.0; g_scopeAtEye = false; }
        return;
    }
    static void *last = NULL;
    void *w = PlayerWeaponForm(); int t = PlayerWeaponType();
    if (w != last) { last = w; Scope_Forget(); g_weaponSettleUntil = (t >= 5 && t <= 9) ? QpcMs() + 1500.0 : 0.0; g_scopeAtEye = false; if (w && t >= 0 && Holster_SlotOf(t)) Holster_Save(Holster_SlotOf(t), w); }   /* long guns only */
    if (g_holsterCheckAt > 0 && QpcMs() > g_holsterCheckAt) {
        g_holsterCheckAt = 0;
        if (w && t >= 0 && !PlayerWeaponOut()) { g_holsterPress = g_bindMask[BIND_RELOAD]; g_holsterPressUntil = QpcMs() + 120.0; Log("Holsters: readying the drawn weapon"); }
    }
}

/* Press a keyboard key the way NVSE scripts do (HoldKey/ReleaseKey) - mods that watch the key see it. */
static void *g_scrHoldKey = NULL, *g_scrReleaseKey = NULL; static bool g_scrKeyFailed = false; static double g_keyReleaseAt = 0;
static void BulletKey_Press(void)
{
    if (g_scrKeyFailed || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return;
    if (!g_scrHoldKey) {
        char a[96], b2[96];
        snprintf(a, sizeof a, "Begin Function { }\nHoldKey %d\nEnd\n", g_bulletKey);
        snprintf(b2, sizeof b2, "Begin Function { }\nReleaseKey %d\nEnd\n", g_bulletKey);
        g_scrHoldKey = g_scriptIntf->CompileScript(a); g_scrReleaseKey = g_scriptIntf->CompileScript(b2);
        if (!g_scrHoldKey || !g_scrReleaseKey) { g_scrKeyFailed = true; Log("Left trigger: couldn't set up key presses"); return; }
    }
    g_scriptIntf->CallFunctionAlt(g_scrHoldKey, GetPlayer(), 0);
    g_keyReleaseAt = QpcMs() + 150.0;
    static int l = 0; if (l++ < 10) Log("Left trigger: bullet-time key (%d) pressed", g_bulletKey);
}
static void BulletKey_Tick(void)
{
    if (g_keyReleaseAt > 0 && QpcMs() > g_keyReleaseAt) { g_keyReleaseAt = 0; if (g_scrReleaseKey) g_scriptIntf->CallFunctionAlt(g_scrReleaseKey, GetPlayer(), 0); }
}

/* Tap a keyboard key (through NVSE) - the quick-loot mod listens to its keyboard keys reliably. */
typedef struct { int key; void *hold, *rel; double relAt; } KeyTap;
static KeyTap g_keyTaps[4]; static int g_keyTapN = 0;
static double g_sendRelAt[2]; static int g_sendKey[2];
static bool Key_Send(int key, bool up)
{
    INPUT in; memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD; in.ki.wScan = (WORD)key; in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0);
    return SendInput(1, &in, sizeof in) == 1;
}
/* A real key press, as if from the keyboard - every mod that reads the keyboard sees it. */
static void Key_Real(int key)
{
    if (key <= 0 || key > 255) return;
    for (int i = 0; i < 2; i++) if (!g_sendKey[i] || g_sendKey[i] == key) {
        HWND fg = GetForegroundWindow(); DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
        bool ours = pid == GetCurrentProcessId();
        bool ok = Key_Send(key, false);
        g_sendKey[i] = key; g_sendRelAt[i] = QpcMs() + 90.0;
        static int l = 0; if (l++ < 20) Log("Key press %d sent as a real keystroke (%s; game window %s)", key, ok ? "ok" : "BLOCKED", ours ? "in front" : "NOT in front - the keystroke goes to another window");
        return;
    }
}
static void Key_RealTick(void)
{
    for (int i = 0; i < 2; i++) if (g_sendKey[i] && QpcMs() > g_sendRelAt[i]) { Key_Send(g_sendKey[i], true); g_sendKey[i] = 0; }
}
/* While the Pip-Boy is open with the world running, the game would still take its buttons as moves (A = jump,
   right stick = turn). The character's movement and looking are switched off for that time (the game's own
   DisablePlayerControls), and switched back on when it closes. */
/* mode: 0 = nothing held, 1 = looking held (Pip-Boy: its A still selects), 2 = movement + looking held (live menus) */
static TrackedDevicePose_t g_waitPoses[16]; static bool g_haveWaitPose;
static void PipControls_Hold(int want)
{
    static int on = 0; static void *dis[3], *en[3]; static bool failed = false;
    if (want == on || failed || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return;
    if (!dis[1]) {
        dis[1] = g_scriptIntf->CompileScript("Begin Function { }\nDisablePlayerControls 0 0 0 0 1 0 0\nEnd\n");
        en[1]  = g_scriptIntf->CompileScript("Begin Function { }\nEnablePlayerControls 0 0 0 0 1 0 0\nEnd\n");
        dis[2] = g_scriptIntf->CompileScript("Begin Function { }\nDisablePlayerControls 1 0 0 0 1 0 0\nEnd\n");
        en[2]  = g_scriptIntf->CompileScript("Begin Function { }\nEnablePlayerControls 1 0 0 0 1 0 0\nEnd\n");
        if (!dis[1] || !en[1] || !dis[2] || !en[2]) { failed = true; Log("Menus: couldn't set up holding your character still"); return; }
    }
    if (on) g_scriptIntf->CallFunctionAlt(en[on], GetPlayer(), 0);
    on = want;
    if (on) g_scriptIntf->CallFunctionAlt(dis[on], GetPlayer(), 0);
    static int l = 0; if (l++ < 8) Log("Menus: character %s", on == 2 ? "movement and looking held" : on == 1 ? "looking held (Pip-Boy)" : "back to normal");
}
static void Key_Tap(int key)
{
    if (key <= 0 || key > 255 || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return;
    KeyTap *k = NULL;
    for (int i = 0; i < g_keyTapN; i++) if (g_keyTaps[i].key == key) k = &g_keyTaps[i];
    if (!k) {
        if (g_keyTapN >= 4) return;
        k = &g_keyTaps[g_keyTapN]; char a[96], b2[96];
        snprintf(a, sizeof a, "Begin Function { }\nHoldKey %d\nEnd\n", key);
        snprintf(b2, sizeof b2, "Begin Function { }\nReleaseKey %d\nEnd\n", key);
        k->hold = g_scriptIntf->CompileScript(a); k->rel = g_scriptIntf->CompileScript(b2);
        if (!k->hold || !k->rel) { Log("Key press %d: couldn't set up", key); return; }
        k->key = key; k->relAt = 0; g_keyTapN++;
    }
    g_scriptIntf->CallFunctionAlt(k->hold, GetPlayer(), 0);
    k->relAt = QpcMs() + 120.0;
}
static void Key_TapTick(void)
{
    for (int i = 0; i < g_keyTapN; i++)
        if (g_keyTaps[i].relAt > 0 && QpcMs() > g_keyTaps[i].relAt) { g_keyTaps[i].relAt = 0; g_scriptIntf->CallFunctionAlt(g_keyTaps[i].rel, GetPlayer(), 0); }
}

/* Read both Touch controllers and build the virtual Xbox controller state. Main thread, every frame. */
/* ---- Pip-Boy part picker: position any Pip-Boy screen part with the controllers ----
   With the Pip-Boy open, click the LEFT stick to start/stop.  The selected part blinks.
     A (right)        = select the next part near the Pip-Boy
     X (left)         = hide / show the selected part
     left stick       = move it left/right and up/down
     right stick      = up/down: toward you / away     left/right: smaller / bigger
     B (right)        = save everything and finish
   Saved per part in [PipParts], always relative to the Pip-Boy, so they come back the same every time. */
typedef struct { char name[64]; char *node; float rel[3]; float scale, base; bool hide, haveR; float Rrel[9]; } PipPart;
static PipPart g_pp[16]; static int g_ppN = 0; static int g_ppAnchorVer = 1;   /* 1 = old "pipboyscreen" frame, 2 = the Pip-Boy 2000 body */
static bool g_adjOn = false, g_pickRebuild = false, g_ppSaveReq = false, g_ppNextReq = false, g_ppHideReq = false;
static float g_adjIn[4]; static float g_adjDt = 0;   /* lx, ly, ry (toward you), rx (size) */
static int   g_adjMode = 0;                  /* Pip-Boy screen adjusting: 0 = move & size, 1 = tilt & stretch */
static bool  g_scrResetReq = false, g_scrTurnReq = false, g_adjStartReq = false;
static void Adj_Buzz(int h, unsigned short us) { if (g_vrSystem && g_handIdx[h] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, us); }
static void PP_Load(void)
{
    g_ppN = 0;
    g_ppAnchorVer = GetPrivateProfileIntA("PipParts", "iAnchor", 1, g_iniPath);
    for (int i = 0; i < 16; i++) {
        char key[16], v[512]; snprintf(key, sizeof key, "Part%d", i);
        GetPrivateProfileStringA("PipParts", key, "", v, sizeof v, g_iniPath);
        if (!v[0]) continue;
        PipPart *p = &g_pp[g_ppN]; memset(p, 0, sizeof *p);
        char *bar = strchr(v, '|'); if (!bar) continue; *bar = 0;
        snprintf(p->name, sizeof p->name, "%s", v);
        float f[14] = { 0 }; int n = sscanf(bar + 1, "%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f", &f[0], &f[1], &f[2], &f[3], &f[4], &f[5], &f[6], &f[7], &f[8], &f[9], &f[10], &f[11], &f[12], &f[13]);
        if (n < 5) continue;
        p->rel[0] = f[0]; p->rel[1] = f[1]; p->rel[2] = f[2]; p->scale = f[3] > 0.1f ? f[3] : 1.0f; p->hide = f[4] != 0;
        if (n >= 14) { memcpy(p->Rrel, &f[5], sizeof p->Rrel); p->haveR = true; }
        g_ppN++;
    }
    { char b[32]; GetPrivateProfileStringA("PipParts", "fTextX", "0", b, sizeof b, g_iniPath); g_txt[0] = (float)atof(b);
      GetPrivateProfileStringA("PipParts", "fTextY", "0", b, sizeof b, g_iniPath); g_txt[1] = (float)atof(b);
      GetPrivateProfileStringA("PipParts", "fTextScale", "1", b, sizeof b, g_iniPath); g_txt[2] = (float)atof(b); if (!(g_txt[2] > 0.1f && g_txt[2] < 10)) g_txt[2] = 1;
      g_txtFollow = false; }
    Log("Pip-Boy parts: %d saved part position(s) loaded; menu text %.2f %.2f size %.2f (follow %d)", g_ppN, g_txt[0], g_txt[1], g_txt[2], (int)g_txtFollow);
}
static void PP_Save(void)
{
    WritePrivateProfileStringA("PipParts", NULL, NULL, g_iniPath);   /* clear the section */
    WritePrivateProfileStringA("PipParts", "iAnchor", g_ppAnchorVer == 2 ? "2" : "1", g_iniPath);
    { char b[32]; snprintf(b, sizeof b, "%.3f", g_txt[0]); WritePrivateProfileStringA("PipParts", "fTextX", b, g_iniPath);
      snprintf(b, sizeof b, "%.3f", g_txt[1]); WritePrivateProfileStringA("PipParts", "fTextY", b, g_iniPath);
      snprintf(b, sizeof b, "%.3f", g_txt[2]); WritePrivateProfileStringA("PipParts", "fTextScale", b, g_iniPath);
      WritePrivateProfileStringA("PipParts", "bTextFollow", g_txtFollow ? "1" : "0", g_iniPath);
      Log("Pip-Boy parts: saved menu text %.2f %.2f size %.2f", g_txt[0], g_txt[1], g_txt[2]); }
    for (int i = 0; i < g_ppN; i++) {
        PipPart *p = &g_pp[i]; char key[16], v[512];
        snprintf(key, sizeof key, "Part%d", i);
        snprintf(v, sizeof v, "%s|%.3f|%.3f|%.3f|%.3f|%d|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f|%.4f", p->name, p->rel[0], p->rel[1], p->rel[2], p->scale, p->hide ? 1 : 0,
                 p->Rrel[0], p->Rrel[1], p->Rrel[2], p->Rrel[3], p->Rrel[4], p->Rrel[5], p->Rrel[6], p->Rrel[7], p->Rrel[8]);
        WritePrivateProfileStringA("PipParts", key, v, g_iniPath);
        Log("Pip-Boy parts: saved '%s' at %.2f %.2f %.2f size %.2f%s", p->name, p->rel[0], p->rel[1], p->rel[2], p->scale, p->hide ? " (hidden)" : "");
    }
}
static bool PipAdjust_Update(const VRControllerState_t *mv, const VRControllerState_t *am, bool lClick, bool rA, bool rB)
{
    static bool lcPrev = false, aPrev = false, bPrev = false, xPrev = false; static double last = 0;
    bool lX = Pressed(mv, EVRButtonId_k_EButton_A);
    double now = QpcMs(); float dt = last > 0 ? (float)((now - last) / 1000.0) : 0; last = now; if (dt > 0.1f) dt = 0.1f;
    bool lcEdge = lClick && !lcPrev, aEdge = rA && !aPrev, bEdge = rB && !bPrev, xEdge = lX && !xPrev;
    lcPrev = lClick; aPrev = rA; bPrev = rB; xPrev = lX;
    int mh = g_leftHanded ? 1 : 0, ah = 1 - mh;
    g_adjIn[0] = g_adjIn[1] = g_adjIn[2] = g_adjIn[3] = 0;
    if (!(g_pipOpen && g_wristPip && g_pipModeRaw == 3)) {
        if (g_adjOn) { g_adjOn = false; Log("Pip-Boy screen: adjusting stopped (Pip-Boy closed)"); }
        return false;
    }
    (void)g_pickRebuild; (void)g_ppNextReq; (void)g_ppHideReq;
    if (lcEdge || g_adjStartReq) {
        g_adjOn = g_adjStartReq ? true : !g_adjOn; g_adjStartReq = false; g_adjMode = 0; g_adjDt = 0; Adj_Buzz(mh, 2000);
        Log("Pip-Boy screen: adjusting %s", g_adjOn ? "ON" : "off");
        if (g_adjOn) UIMessage("Pip-Boy screen: L stick slides it on your arm, R stick up/down size, A tilt, X reset, B save");
        return true;
    }
    if (!g_adjOn) return false;
    if (aEdge) { g_adjMode ^= 1; Adj_Buzz(ah, 1200); UIMessage(g_adjMode ? "Pip-Boy screen: TILT - L stick tilt/turn, R stick turn/taller, X = quarter turn" : "Pip-Boy screen: SLIDE - L stick slides it on your arm, R stick up/down size, X = reset"); }
    if (xEdge) { if (g_adjMode == 0) g_scrResetReq = true; else g_scrTurnReq = true; Adj_Buzz(mh, 1200); }
    if (bEdge) { g_ppSaveReq = true; Adj_Buzz(0, 3999); Adj_Buzz(1, 3999); g_adjOn = false; return true; }
    #define ADZ(v) (fabsf(v) < 0.2f ? 0.0f : (v))
    g_adjIn[0] = ADZ(mv->rAxis[0].x); g_adjIn[1] = ADZ(mv->rAxis[0].y); g_adjIn[2] = ADZ(am->rAxis[0].y); g_adjIn[3] = ADZ(am->rAxis[0].x);
    #undef ADZ
    g_adjDt = fminf(g_adjDt + dt, 0.1f);
    return true;
}

static void Controllers_Poll(bool menu)
{
    if (!g_vrSystem) { g_padActive = false; return; }   /* (with motion controls off, the controllers still run the FNVR settings panel) */
    g_handIdx[0] = g_vrSystem->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_LeftHand);
    g_handIdx[1] = g_vrSystem->GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole_TrackedControllerRole_RightHand);

    VRControllerState_t c[2];
    bool ok[2];
    for (int h = 0; h < 2; h++) {
        memset(&c[h], 0, sizeof c[h]);
        ok[h] = g_handIdx[h] != 0xFFFFFFFF && g_vrSystem->GetControllerState(g_handIdx[h], &c[h], sizeof c[h]);
    }
    static bool lastConn = false;
    if (!g_loggedControllers || (ok[0] || ok[1]) != lastConn) {
        g_loggedControllers = true; lastConn = ok[0] || ok[1];
        Log("Controllers: left %s (device %u), right %s (device %u)",
            ok[0] ? "connected" : "missing", g_handIdx[0], ok[1] ? "connected" : "missing", g_handIdx[1]);
    }
    if (!ok[0] && !ok[1]) { g_padActive = false; return; }

    /* Finger curl from the Touch sensors: trigger = index, grip = other three, thumb resting on a
       button/stick = thumb down. (Physical hands: 0 = left, 1 = right.) */
    for (int h = 0; h < 2; h++) {
        if (!ok[h]) continue;
        const VRControllerState_t *cs = &c[h];
        uint64_t touched = cs->ulButtonTouched | cs->ulButtonPressed;
        bool thumbOn = (touched >> EVRButtonId_k_EButton_A) & 1 || (touched >> EVRButtonId_k_EButton_ApplicationMenu) & 1 ||
                       (touched >> EVRButtonId_k_EButton_SteamVR_Touchpad) & 1;
        bool indexOn = (touched >> EVRButtonId_k_EButton_SteamVR_Trigger) & 1;
        float trig = fminf(fmaxf(cs->rAxis[1].x, 0.0f), 1.0f), grip = fminf(fmaxf(cs->rAxis[2].x, 0.0f), 1.0f);
        g_fingerCurl[h][0] = thumbOn ? 0.8f : 0.0f;
        g_fingerCurl[h][1] = indexOn ? 0.25f + 0.75f * trig : trig;
        g_fingerCurl[h][2] = grip;
    }

    /* "move" hand = left stick, "aim" hand = right stick (swapped for left-handed players) */
    const VRControllerState_t *mv = &c[g_leftHanded ? 1 : 0], *am = &c[g_leftHanded ? 0 : 1];
    bool okMv = ok[g_leftHanded ? 1 : 0], okAm = ok[g_leftHanded ? 0 : 1];
    /* sticks, stick clicks and face buttons stay on the physical controllers for everyone (left stick moves, right stick
       turns, A/B right, X/Y left) - left-handed only swaps the hand things: trigger to fire, grips, the gun */
    const VRControllerState_t *sL = &c[0], *sR = &c[1];
    {   /* both sticks clicked in: held about a second = FNVR settings; a quick click = calibrate the aim */
        static double bothSince = 0; static bool bothUsed = false;
        bool both = okMv && okAm && Pressed(mv, EVRButtonId_k_EButton_SteamVR_Touchpad) && Pressed(am, EVRButtonId_k_EButton_SteamVR_Touchpad);
        double nowB = QpcMs();
        if (both) {
            if (!bothSince) bothSince = nowB;
            if (!bothUsed && nowB - bothSince > 900.0) { bothUsed = true; Settings_Toggle(); }
        } else {
            if (bothSince && !bothUsed && nowB - bothSince < 900.0 && !g_setOpen) g_aimCalReq = true;
            bothSince = 0; bothUsed = false;
        }
    }
    if (g_setSwallow && !g_setOpen) {   /* (so the button that closed the panel doesn't also reload, jump...) */
        bool any = false;
        for (int h = 0; h < 2; h++) if (ok[h] && (c[h].ulButtonPressed || c[h].rAxis[1].x > 0.25f || fabsf(c[h].rAxis[0].x) > 0.5f || fabsf(c[h].rAxis[0].y) > 0.5f)) any = true;
        if (!any) g_setSwallow = false;
    }
    if (g_setOpen || g_setSwallow) {   /* the settings panel has the controllers: the game gets nothing until it closes */
        if (g_setOpen) Settings_Update(mv, am, okMv, okAm);
        XState z; memset(&z, 0, sizeof z);
        PadLock();
        if (memcmp(&z.Gamepad, &g_padState.Gamepad, sizeof(XPad)) != 0) g_padPacket++;
        z.dwPacketNumber = g_padPacket; g_padState = z;
        PadUnlock();
        g_padActive = g_motionControls; g_grabWant[0] = g_grabWant[1] = false; g_bothSticksClicked = false;
        g_gripHeld[0] = g_gripHeld[1] = false; g_gripEdge[0] = g_gripEdge[1] = false; g_twoHandWant = false;
        SetRunKey(false); Sprint_Update(false);
        return;
    }
    if (!g_motionControls) { g_padActive = false; return; }

    XState st;
    memset(&st, 0, sizeof st);
    XPad *p = &st.Gamepad;
    p->sThumbLX = AxisToShort(sL->rAxis[0].x);
    p->sThumbLY = AxisToShort(sL->rAxis[0].y);
    {   float sw = ArmSwing_Stick(menu);   /* arm-swing running: only while the stick is left alone */
        float sx = sL->rAxis[0].x, sy = sL->rAxis[0].y;
        if (sw > 0 && sx * sx + sy * sy < 0.04f) { p->sThumbLX = 0; p->sThumbLY = AxisToShort(sw); } else g_swingSprint = false; }
    p->sThumbRX = AxisToShort(sR->rAxis[0].x);                   /* turning */
    p->sThumbRY = menu ? AxisToShort(sR->rAxis[0].y) : 0;      /* up/down look comes from head/hand */
    p->bLeftTrigger  = (BYTE)(fminf(fmaxf(mv->rAxis[1].x, 0.0f), 1.0f) * 255.0f);   /* aim / block */
    p->bRightTrigger = (BYTE)(fminf(fmaxf(am->rAxis[1].x, 0.0f), 1.0f) * 255.0f);   /* attack */
    Melee_Update(menu);
    if (!menu && g_meleeSwings && !g_meleeTrigger) {   /* melee weapons and fists attack by swinging, not with the trigger -
                                                         except automatic ones (chainsaw, Ripper, Industrial Hand...), which run while it's held */
        int wt = PlayerWeaponType();
        if (wt >= -1 && wt <= 2 && !PlayerWeaponAutomatic()) p->bRightTrigger = 0;
    }
    if (g_swingFrames > 0) p->bRightTrigger = 255;                               /* swing = attack */
    if (!menu && QpcMs() < g_weaponSettleUntil - 600.0) p->bRightTrigger = 0;     /* the new gun is still being drawn */
    {   int ah = g_leftHanded ? 0 : 1;   /* weapon hand making the grapple fist (no gun out): the trigger fires the line, not an attack */
        if (!menu && g_grapOn && g_grapHand[ah] && g_gripHeld[ah] && (!g_weaponOutThisFrame || PlayerWeaponType() <= 2)) p->bRightTrigger = 0; }

    /* Button layout (Touch -> game action). The Xbox button for each action is read from the game's
       own control bindings (FalloutPrefs.ini), so it follows whatever the game is set to.
         right A            jump
         right B            reload
         right grip         activate / talk
         right stick click  sneak
         left Y             Pip-Boy
         left X             pause menu
         left grip          VATS
         left stick click   run (keyboard Run key, held)
       In menus: right A / right grip = select, right B = back. */
    WORD b = 0;
    bool rGrip = Pressed(am, EVRButtonId_k_EButton_Grip) || am->rAxis[2].x > 0.7f;
    bool lGrip = Pressed(mv, EVRButtonId_k_EButton_Grip) || mv->rAxis[2].x > 0.7f;
    bool rGripRaw = rGrip, lGripRaw = lGrip;
    bool rA = Pressed(sR, EVRButtonId_k_EButton_A), rB = Pressed(sR, EVRButtonId_k_EButton_ApplicationMenu);
    bool lX = Pressed(sL, EVRButtonId_k_EButton_A), lY = Pressed(sL, EVRButtonId_k_EButton_ApplicationMenu);
    {   /* a button still held from a menu or the Pip-Boy (A to fast travel, B to close) does nothing in the game until it's let go */
        static bool aLatch = false, bLatch = false;
        if (menu || g_pipOpen) { aLatch = rA; bLatch = rB; }
        else { if (!rA) aLatch = false; if (!rB) bLatch = false; if (aLatch) rA = false; if (bLatch) rB = false; }
    }
    bool rClick = Pressed(sR, EVRButtonId_k_EButton_SteamVR_Touchpad);
    bool lClick = Pressed(sL, EVRButtonId_k_EButton_SteamVR_Touchpad);
    g_bothSticksClicked = rClick && lClick;
    if (g_bothSticksClicked) rClick = lClick = false;   /* calibration gesture - don't also sneak/run */
    /* Grips with no weapon out: grab whatever loose object is in reach of that hand
       (the activate grip still activates/talks when nothing is in reach). */
    {
        static bool gripPrev[2] = { false, false }, grabMode[2] = { false, false }, gripPrevTwo = false;
        int amH = g_leftHanded ? 0 : 1, mvH = 1 - amH;
        bool gripNow[2];
        gripNow[amH] = rGrip; gripNow[mvH] = lGrip;
        bool noWeapon = !PlayerWeaponOut() || PlayerWeaponType() == -1;
        for (int h = 0; h < 2; h++) {
            if (gripNow[h] && !gripPrev[h]) {
                if (h == amH && !menu) g_lootDiag = true;
                grabMode[h] = g_grabEnabled && !menu && g_inGameplay && noWeapon && g_grabCand[h] != NULL;
                static int gripLogs = 0;
                if (!menu && noWeapon && gripLogs < 40) {
                    gripLogs++;
                    Log("Grip %s pressed (no weapon): %s, nearest loose object %.0f units from the hand",
                        h ? "right" : "left", grabMode[h] ? "grabbing" : "nothing in reach", g_grabNearest[h]);
                }
            }
            if (!gripNow[h]) grabMode[h] = false;
            g_grabWant[h] = grabMode[h];
            if (gripNow[h] && !gripPrev[h] && !menu && g_inGameplay && !grabMode[h]) g_gripEdge[h] = true;
            g_gripHeld[h] = gripNow[h] && !menu;
            gripPrev[h] = gripNow[h];
        }
        {   /* weapon out: off-hand grip on the gun = two-handed aim, elsewhere = VATS */
            static bool twoMode = false;
            int wtNow = PlayerWeaponType();
            bool gun = !noWeapon && (wtNow >= 3 || wtNow == 1 || wtNow == 2);   /* guns and melee weapons (not fists) */
            if (gripNow[mvH] && !gripPrevTwo) twoMode = gun && !menu && g_twoHandCand;
            if (!gripNow[mvH] || !gun) twoMode = false;
            gripPrevTwo = gripNow[mvH];
            g_twoHandWant = twoMode;
            if (twoMode) lGrip = false;
        }
        if (!menu && (grabMode[amH] || g_holding[amH] || g_climb[amH])) rGrip = false;      /* grabbing/climbing, not activating */
        {   /* holsters: at the hip / over the shoulder the gun-hand grip holsters or draws instead of activating */
            static bool hPrev = false; static int zPrev = 0;
            int zone = (!menu && g_inGameplay && !g_pipOpen && !grabMode[amH] && !g_holding[amH] && !g_climb[amH]) ? Holster_Zone() : 0;
            if (zone && zone != zPrev) Holster_Buzz(600);
            zPrev = zone;
            if (zone) { if (rGripRaw && !hPrev) Holster_Use(zone); rGrip = false; }
            hPrev = rGripRaw;
        }
        if (g_chestPause && g_vrSystem) {   /* grab at your chest: pause menu (again: close it) */
            TrackedDevicePose_t ps_[16];
            g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, ps_, 16);
            static bool cPrev[2] = { false, false }; static bool inPrev[2] = { false, false };
            if (ps_[0].bPoseIsValid) {
                float yaw_ = 0, pit_ = 0; ExtractYawPitch(&ps_[0].mDeviceToAbsoluteTracking, &yaw_, &pit_);
                float fw_[3] = { -sinf(yaw_), 0, -cosf(yaw_) }, ch[3];
                for (int i = 0; i < 3; i++) ch[i] = ps_[0].mDeviceToAbsoluteTracking.m[i][3] + fw_[i] * g_chestFwd;
                ch[1] -= g_chestDown;
                for (int hh = 0; hh < 2; hh++) {
                    bool grip = hh == amH ? rGripRaw : lGripRaw;
                    bool in = false;
                    if (g_handIdx[hh] < 16 && ps_[g_handIdx[hh]].bPoseIsValid) {
                        float d_[3]; for (int i = 0; i < 3; i++) d_[i] = ps_[g_handIdx[hh]].mDeviceToAbsoluteTracking.m[i][3] - ch[i];
                        in = sqrtf(d_[0] * d_[0] + d_[1] * d_[1] + d_[2] * d_[2]) < g_chestR;
                    }
                    bool busy = !menu && (g_holding[hh] || g_climb[hh] || grabMode[hh] || (hh == mvH && (g_twoHandWant || g_twoHand)) || g_wristUp || g_pipOpen);
                    if (in && !inPrev[hh] && !busy) g_vrSystem->TriggerHapticPulse(g_handIdx[hh], 0, 600);
                    inPrev[hh] = in;
                    if (in && !busy && !g_pipOpen) {
                        if (grip && !cPrev[hh]) {
                            bool gunHand = hh == (g_leftHanded ? 0 : 1) && PlayerWeaponOut() && PlayerWeaponType() != -1;
                            int st = (g_stimOn && !menu && g_inGameplay && !gunHand && g_stimHand < 0 && !g_stimDo && !g_holding[hh]) ? Stim_Pick() : -1;
                            if (st >= 0 && Stim_Begin(hh, st)) g_vrSystem->TriggerHapticPulse(g_handIdx[hh], 0, 2000);
                            else {
                                g_chestPress = XB_START; g_chestPressUntil = QpcMs() + 120.0;
                                g_vrSystem->TriggerHapticPulse(g_handIdx[hh], 0, 3000);
                                Log("Chest grab (%s hand): pause menu %s", hh ? "right" : "left", menu ? "close" : "open");
                            }
                        }
                        if (hh == amH) rGrip = false; else lGrip = false;   /* not activate / VATS */
                    }
                    cPrev[hh] = grip;
                }
            }
        }
        if (!menu && g_climb[mvH]) lGrip = false;
        if (!menu && (noWeapon || !g_adsEnabled)) p->bLeftTrigger = 0;   /* left trigger just curls the fingers (no aim-down-sights) */
        if (!menu && g_autoScope && g_scopeAtEye && !noWeapon && PlayerWeaponScoped())
            p->bLeftTrigger = 255;   /* scope raised to the eye: aim through it (True Scopes draws the view) */
        if (!menu && g_adsActive && p->bLeftTrigger > 40) p->bLeftTrigger = 255;   /* no flicker on a half-pressed trigger */
        {   /* aiming down sights: the game's own sight picture, aimed with your head */
            bool ads = !menu && g_inGameplay && !noWeapon && PlayerWeaponType() >= 3 && p->bLeftTrigger > (g_adsActive ? 40 : 110);
            if (ads != g_adsActive) Log("Aim down sights: %s", ads ? "on (head aims, gun to the eye)" : "off");
            g_adsActive = ads;
        }   /* no weapon: left trigger only curls fingers (no aim/block pose) */
        if (grabMode[mvH] || g_holding[mvH] || noWeapon) lGrip = false;     /* VATS only with a weapon out */
    }
    (void)lY;   /* Pip-Boy button off: the Pip-Boy opens by looking at your wrist */
    if (g_pipPressMask && QpcMs() < g_pipPressUntil) b |= g_pipPressMask; else g_pipPressMask = 0;
    if (g_holsterPress && QpcMs() < g_holsterPressUntil) b |= g_holsterPress; else g_holsterPress = 0;
    if (g_chestPress && QpcMs() < g_chestPressUntil) b |= g_chestPress; else g_chestPress = 0;
    {   /* left trigger: the bullet-time key (C) - held briefly on each pull */
        static bool ltPrev = false;
        float lt = mv->rAxis[1].x;
        g_grapTrigH[g_leftHanded ? 1 : 0] = menu ? 0.0f : lt;
        g_grapTrigH[g_leftHanded ? 0 : 1] = menu ? 0.0f : am->rAxis[1].x;
        bool ltDown = ltPrev ? lt > 0.40f : lt > 0.75f;
        bool fist = Pressed(mv, EVRButtonId_k_EButton_Grip) || mv->rAxis[2].x > 0.5f;   /* a fist (grip + trigger) isn't a bullet-time pull */
        static double fistAt = 0; if (fist) fistAt = QpcMs();   /* (not right after a grapple fist, nor while being pulled) */
        if (g_ltBullet && !g_adsEnabled && !menu && g_inGameplay && !g_pipOpen && !g_wristUp && !fist && ltDown && !ltPrev && QpcMs() - fistAt > 300.0 && !g_grapPulling) BulletKey_Press();
        ltPrev = ltDown;
        BulletKey_Tick();
        Key_TapTick(); Key_RealTick();
    }
    if (lX && !g_pipOpen && !menu && !IsMenuMode() && !g_liveMenuNow) b |= XB_START;   /* (in menus left X is the pad's X: accept a trade, take all) */
    if (g_pipOpen) {   /* Pip-Boy: the full pad - A select, B back, X action, grips = bumpers, triggers, sticks */
        if (rA) b |= XB_A;
        if (rB) b |= XB_B;
        if (lX) b |= XB_X;
        if (lGripRaw) b |= XB_LSHOULDER;
        if (rGripRaw) b |= XB_RSHOULDER;
        if (lClick) b |= XB_LTHUMB;
        if (rClick) b |= XB_RTHUMB;
    } else if (menu) {
        if (rA || rGrip) b |= XB_A;
        if (rB)          b |= XB_B;
        if (lX)          b |= XB_X;   /* trading: accept the deal / containers: take all */
        if (lY)          b |= XB_Y;
        {   static bool xyPrev = false; static int nl = 0;
            if ((lX || lY) && !xyPrev && nl++ < 20) Log("Menus: left %s pressed (the pad's %s)", lX ? "X" : "Y", lX ? "X - accept the trade / take all" : "Y");
            xyPrev = lX || lY;
        }
    } else if (g_lootShown && g_lootControls) {   /* quick-loot window up: its own controls */
        static bool laPrev = false, lbPrev = false;
        if (rA && !laPrev) { Key_Real(g_jlmKey1); static int l = 0; if (l++ < 20) Log("Loot: right A - pressed the loot mod's take key %d", g_jlmKey1); }
        if (rB && !lbPrev) Key_Real(g_jlmKeyAll);
        laPrev = rA; lbPrev = rB;
        if (rGrip)  b |= g_bindMask[BIND_ACTIVATE];      /* right grip: open the full loot menu, as usual */
        if (lY)     b |= g_bindMask[BIND_ACTIVATE];      /* left Y: the same */
        if (rClick) b |= g_bindMask[BIND_SNEAK];
        {   /* right stick up/down scrolls the list: the loot mod scrolls with the mouse wheel, so turn the stick into wheel clicks */
            static double nextWheel = 0; float y = sR->rAxis[0].y;
            if (fabsf(y) > 0.55f) {
                double now = QpcMs();
                if (now >= nextWheel) {
                    INPUT in; memset(&in, 0, sizeof in); in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_WHEEL; in.mi.mouseData = (DWORD)(y > 0 ? WHEEL_DELTA : -WHEEL_DELTA);
                    SendInput(1, &in, sizeof in);
                    nextWheel = now + (nextWheel > 0 && now - nextWheel < 100.0 ? 140.0 : 320.0);   /* first step, then repeat while held */
                }
            } else nextWheel = 0;
            p->sThumbRY = 0;
        }
    } else {
        if (rA)     b |= g_bindMask[BIND_JUMP];
        if (rB)     b |= g_bindMask[BIND_RELOAD];
        if (rGrip)  b |= g_bindMask[BIND_ACTIVATE];
        if (rClick) b |= g_bindMask[BIND_SNEAK];
        /* left grip no longer opens VATS (it grabs objects / steadies the gun) */
    }
    bool pipMap = g_pipOpen && g_wristPip && ((const volatile bool *)0x011F308F)[0x3FF];   /* Pip-Boy on its map page */
    if (menu && !pipMap) {   /* in menus, the aim stick also works as a d-pad for tabs (not in the Pip-Boy: there it is the real right stick) */
        if (sR->rAxis[0].y >  0.7f) b |= XB_DPAD_UP;
        if (sR->rAxis[0].y < -0.7f) b |= XB_DPAD_DOWN;
    }
    {   /* seated play: right stick held down = crouch (reach low things); it doesn't turn you while held */
        float ry = sR->rAxis[0].y, rx = sR->rAxis[0].x;
        bool want = g_seated && g_inGameplay && !menu && !g_pipOpen && !(g_lootShown && g_lootControls) && ry < -0.6f && fabsf(ry) > fabsf(rx);
        if (want != g_seatCrouchWant) { static int cl = 0; if (cl++ < 6) Log("Seated crouch: %s", want ? "down (right stick)" : "back up"); }
        g_seatCrouchWant = want;
        if (want) p->sThumbRX = 0;
    }
    p->wButtons = b;
    if (g_pipOpen && g_wristPip) {   /* Pip-Boy up: the right stick moves the map cursor on the map page; elsewhere it only flips
                                        through lists (as the d-pad) - so it never turns you while the world is running */
        if (pipMap) {   /* map page: the game's right stick moves the map cursor - it also turns the running world, so it's sent
                           at a third of its strength (the cursor still moves, the turning is gentle) */
            /* (just past the game's dead zone, then only a share of the rest - so a small push still moves the cursor) */
            #define PIPSTK(v) (fabsf(v) < 0.12f ? 0.0f : copysignf(0.26f + 0.74f * g_pipStickScale * (fabsf(v) - 0.12f) / 0.88f, (v)))
            p->sThumbRX = AxisToShort(PIPSTK(sR->rAxis[0].x)); p->sThumbRY = AxisToShort(PIPSTK(sR->rAxis[0].y));
            #undef PIPSTK
        }
        else { p->sThumbRX = 0; p->sThumbRY = 0; } }
    if (g_lootShown && g_lootControls && !menu) p->sThumbRX = 0;   /* quick-loot window up: the right stick never turns you */
    g_headingLockWant = false;   /* (holding the facing still made the view snap back each frame - retired) */
    PipControls_Hold(g_liveMenuNow ? 2 : 0);   /* (the Pip-Boy's own hold left activating switched off afterwards - removed) */
    {   /* once: switch every player control back on, in case a save kept them off from the last version */
        static bool done = false;
        if (g_newGameWait) done = true;   /* (a brand-new game: nothing to repair - and the intro scenes set their own controls) */
        if (!done && g_inGameplay && !menu && !g_pipOpen && g_scriptIntf && g_scriptIntf->CompileScript && g_scriptIntf->CallFunctionAlt) {
            done = true;
            void *en = g_scriptIntf->CompileScript("Begin Function { }\nEnablePlayerControls\nEnd\n");
            if (en) { g_scriptIntf->CallFunctionAlt(en, GetPlayer(), 0); Log("Controls: all player controls switched on (repair after 1.7.5-1.8.2)"); }
        }
    }
    {   /* look well away from a live menu and it closes (as if you walked off) */
        static bool was = false; static float yaw0 = 0; static double awaySince = 0, closedAt = 0;
        float yaw = 0, pitch = 0; bool havePose = false;
        if (g_haveWaitPose && g_waitPoses[0].bPoseIsValid) { ExtractYawPitch(&g_waitPoses[0].mDeviceToAbsoluteTracking, &yaw, &pitch); havePose = true; }
        else if (g_vrSystem) { TrackedDevicePose_t hp; g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &hp, 1);
            if (hp.bPoseIsValid) { ExtractYawPitch(&hp.mDeviceToAbsoluteTracking, &yaw, &pitch); havePose = true; } }
        { static double nl = 0; if (g_liveMenuNow && QpcMs() > nl) { nl = QpcMs() + 1000.0;
            float dd = (yaw - yaw0) * RAD2DEG; while (dd > 180) dd -= 360; while (dd < -180) dd += 360;
            Log("Live menus: head turned %.0f deg from where the menu opened (closes past %.0f; head %s)", was ? dd : 0.0f, g_lookAwayDeg, havePose ? "tracked" : "NOT tracked"); } }
        if (g_liveMenuNow && havePose) {
            if (!was) { was = true; yaw0 = yaw; awaySince = 0; }
            float d = (yaw - yaw0) * RAD2DEG; while (d > 180) d -= 360; while (d < -180) d += 360;
            if (g_lookAwayDeg > 1.0f && fabsf(d) > g_lookAwayDeg) {
                if (!awaySince) awaySince = QpcMs();
                if (QpcMs() - awaySince > 350.0 && QpcMs() - closedAt > 600.0) {
                    closedAt = QpcMs();
                    Key_Real(15);   /* Tab: the game's own "leave this menu" key - the pad's B doesn't reach live menus */
                    Log("Live menus: you looked away (%.0f deg) - closing the menu", d);
                }
            } else awaySince = 0;
        } else was = false;
    }
    if (g_pipOpen) {   /* triggers work as the Pip-Boy's own trigger buttons */
        p->bLeftTrigger  = (BYTE)(fminf(fmaxf(mv->rAxis[1].x, 0.0f), 1.0f) * 255.0f);
        p->bRightTrigger = (BYTE)(fminf(fmaxf(am->rAxis[1].x, 0.0f), 1.0f) * 255.0f);
    }
    if (PipAdjust_Update(sL, sR, lClick, rA, rB)) { memset(p, 0, sizeof *p); b = 0; lClick = false; }

    SetRunKey((lClick || g_swingSprint) && !menu);
    Sprint_Update((lClick || g_swingSprint) && !menu && g_inGameplay);

    if (!g_padActive) Log("Controllers now feeding the game as an Xbox controller");
    {
        static int inputLogs = 0;
        static WORD lastB = 0;
        if (inputLogs < 20 && (b != lastB || ((p->bRightTrigger > 128) != (g_padState.Gamepad.bRightTrigger > 128)))) {
            inputLogs++;
            Log("Controller input: buttons %04X, triggers L%u R%u, sticks L(%d,%d) R(%d,%d)",
                b, p->bLeftTrigger, p->bRightTrigger, p->sThumbLX, p->sThumbLY, p->sThumbRX, p->sThumbRY);
        }
        lastB = b;
    }
    PadLock();
    if (memcmp(&st.Gamepad, &g_padState.Gamepad, sizeof(XPad)) != 0) g_padPacket++;
    st.dwPacketNumber = g_padPacket;
    g_padState = st;
    PadUnlock();
    g_padActive = true;

    /* Make sure the game actually listens to the controller. */
    BYTE *osInput = *(BYTE **)OSINPUTGLOBALS_PTR;
    if (osInput && osInput[0]) {
        Log("Game had controller input disabled (OSInputGlobals flag %d) - enabling it", osInput[0]);
        osInput[0] = 0;
    }
    static DWORD lastCalls = 0, quietSince = 0;
    static bool loggedSelfPoll = false;
    if (g_xinputCalls != lastCalls) { lastCalls = g_xinputCalls; quietSince = 0; }
    else if (!quietSince) quietSince = GetTickCount();
    if (quietSince && GetTickCount() - quietSince > 2000 && osInput) {
        /* The game isn't polling the controller this frame - do it for it. */
        if (!loggedSelfPoll) { loggedSelfPoll = true; Log("Game is not polling the controller - FNVR polls it each frame instead"); }
        static int diag = 0;
        unsigned before = g_xinputCalls;
        ((PollGamepad_t)POLLGAMEPAD_ADDR)(osInput);
        if (diag < 3) {
            diag++;
            Log("Self-poll check: controller reads %u -> %u, game says connected=%d, buttons %04X",
                before, g_xinputCalls, *(BYTE *)GAMEPAD_CONNECTED, g_padState.Gamepad.wButtons);
        }
        lastCalls = g_xinputCalls;   /* our own poll doesn't count as the game polling */
        quietSince = GetTickCount() - 2001;
    }
}

/* ======================================================================= */
/* Head tracking                                                           */
/* ======================================================================= */

static bool  g_trackingEnabled = true;
static bool  g_menuCached = true;    /* IsMenuMode() as of this frame's update */
static bool  g_haveLastPose = false;
static float g_lastYaw = 0.0f, g_lastPitch = 0.0f;
static int   g_invalidPoseFrames = 0;
static bool  g_kF8 = false, g_kF6 = false, g_kF7 = false, g_kF10 = false;
static DWORD g_lastStatusLog = 0;
static unsigned g_ticksSinceLog = 0, g_presentsSinceLog = 0, g_submitsSinceLog = 0;
static unsigned g_capWaitsSinceLog;
static unsigned g_stereoFramesSinceLog = 0;
static unsigned g_renderCallsSinceLog = 0, g_renderCallsResize = 0, g_renderCallsMain = 0;
static uintptr_t g_renderCallsOther = 0;

/* Pose handed to us by WaitGetPoses at the end of the previous frame (predicted for this frame). */
static TrackedDevicePose_t g_waitPose;
static TrackedDevicePose_t g_waitPoses[16];   /* all devices (controllers too) */
static bool g_haveWaitPose;

/* What we pointed the camera at this frame - used to tell SteamVR how the frame was rendered. */
static bool  g_frameHasSample = false;
static float g_sampleYaw = 0.0f;         /* VR yaw the camera was pointed at */
static float g_samplePos[3] = {0, 0, 0}; /* head position in tracking space */
static float g_writtenHeading = 0.0f;    /* game heading we wrote for that yaw */

/* Hand aiming: the game aims where the right controller points; the camera is turned to the head
   at draw time. Game heading = g_bodyOffset - VR yaw (offset changes when the game turns you). */
static bool  g_handAimEngaged = false;
static bool  g_handAimValid = false;      /* hand aim known this frame */
static float g_handAimHeading = 0.0f;     /* game heading/pitch the hand points at */
static float g_handAimPitch = 0.0f;
static float g_bodyOffset = 0.0f;
static bool  g_camOverride = false;       /* turn the camera to the head this frame */
static float g_camHead[3][3];             /* HMD rotation (VR space) for this frame */
static bool  g_frameHasHmdPose = false;   /* camera = head exactly: submit the HMD pose as-is */
static HmdMatrix34_t g_frameHmdPose;
static bool  g_kF6aim = false, g_kF7aim = false, g_kF3 = false, g_kF4 = false;

/* Last pose we submitted (reused while menus are open so the image stays put in the world). */
static HmdMatrix34_t g_lastSubmitPose;
static bool g_haveSubmitPose = false;

static bool HeadsetDisplayActive(void);
static void ApplyHeadsetFov(void);

static void ExtractYawPitch(const HmdMatrix34_t *mat, float *yaw, float *pitch)
{
    /* Forward vector is the -Z column (OpenVR: +Y up, -Z forward). */
    float fx = -mat->m[0][2], fy = -mat->m[1][2], fz = -mat->m[2][2];
    *yaw   = atan2f(-fx, -fz);                                     /* + = turned left */
    *pitch = asinf(fy < -1.f ? -1.f : fy > 1.f ? 1.f : fy);        /* + = looking up  */
}

static void VR_PumpEvents(void)
{
    struct VREvent_t ev;
    while (g_vrSystem && g_vrSystem->PollNextEvent(&ev, sizeof ev)) {
        if (ev.eventType == EVREventType_VREvent_Quit) {
            Log("SteamVR is shutting down");
            g_vrSystem->AcknowledgeQuit_Exiting();
            VR_Stop("SteamVR quit");
            UIMessage("FNVR: SteamVR closed");
            return;
        }
    }
}

/* Called once per game frame on the main thread, before the frame is rendered. */

static void *AsNode(char *obj);
static bool player_ok_for_reequip(void) { return GetPlayer() != NULL; }
/* diagnostics: which first-person parts are hidden */
static void Arms_LogHiddenRec(char *n, int depth, char *out, size_t cap)
{
    if (!n || depth > 3) return;
    const char *name = *(const char **)(n + 0x08);
    if ((*(uint32_t *)(n + 0x30) & 1) && name && strlen(out) + strlen(name) + 4 < cap) { strcat(out, " "); strcat(out, name); }
    if (!AsNode(n)) return;
    char **kids = *(char ***)(n + 0xA0);
    unsigned count = *(uint16_t *)(n + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) Arms_LogHiddenRec(kids[i], depth + 1, out, cap);
}
static void Arms_LogHidden(const char *when)
{
    char *player = (char *)GetPlayer();
    char *node = player ? *(char **)(player + 0x694) : NULL;
    char out[1200] = "";
    if (node) Arms_LogHiddenRec(node, 0, out, sizeof out);
    Log("First-person model %s: %s; hidden parts:%s", when, node ? "present" : "MISSING", out[0] ? out : " none");
}

static void VR_Tick(void)
{
    DWORD now = GetTickCount();
    g_ticksSinceLog++;
    g_frameHasSample = false;

    { static bool kF11 = false; if (KeyPressed(VK_F11, &kF11)) Settings_Toggle(); }
    if (KeyPressed(VK_F8, &g_kF8)) {
        g_trackingEnabled = !g_trackingEnabled;
        g_haveLastPose = false;
        Log("Head tracking %s (F8)", g_trackingEnabled ? "ON" : "OFF");
        UIMessage(g_trackingEnabled ? "FNVR: head tracking ON" : "FNVR: head tracking OFF");
    }
    if (KeyPressed(VK_F10, &g_kF10) && HeadsetDisplayActive() && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {   /* Ctrl+F10 (plain F10 is the real-time Pip-Boy) */
        g_stereo = !g_stereo;
        SaveInt("Stereo", "bStereo", g_stereo ? 1 : 0);
        Log("Stereo %s (F10)", g_stereo ? "ON" : "OFF");
        UIMessage(g_stereo ? "FNVR: 3D ON" : "FNVR: 3D OFF (flat)");
    }
    if (HeadsetDisplayActive() && g_handAim && g_motionControls) {
        /* Shot fine-tuning: F3/F4 = left/right, F6/F7 = down/up, half a degree per press */
        float dy = 0, dp = 0;
        if (KeyPressed(VK_F3, &g_kF3)) dy = -0.5f;
        if (KeyPressed(VK_F4, &g_kF4)) dy = +0.5f;
        if (KeyPressed(VK_F6, &g_kF6aim)) dp = +0.5f;
        if (KeyPressed(VK_F7, &g_kF7aim)) dp = -0.5f;
        if (dy != 0 || dp != 0) {
            g_shotYawTrimDeg += dy; g_shotPitchTrimDeg += dp;
            char buf[16];
            snprintf(buf, sizeof buf, "%.2f", g_shotYawTrimDeg);   WritePrivateProfileStringA("Controls", "fShotYawTrim", buf, g_iniPath);
            snprintf(buf, sizeof buf, "%.2f", g_shotPitchTrimDeg); WritePrivateProfileStringA("Controls", "fShotPitchTrim", buf, g_iniPath);
            char msg[80];
            snprintf(msg, sizeof msg, "FNVR: shots %.1f deg %s, %.1f deg %s", fabsf(g_shotYawTrimDeg), g_shotYawTrimDeg >= 0 ? "right" : "left",
                     fabsf(g_shotPitchTrimDeg), g_shotPitchTrimDeg >= 0 ? "down" : "up");
            UIMessage(msg);
            Log("Shot trim yaw %.2f pitch %.2f", g_shotYawTrimDeg, g_shotPitchTrimDeg);
        }
    }
    if (!HeadsetDisplayActive()) {
        int d = 0;
        if (KeyPressed(VK_F6, &g_kF6)) d = -10;
        if (KeyPressed(VK_F7, &g_kF7)) d = +10;
        if (d) {
            g_predictionMs += d;
            if (g_predictionMs < 0) g_predictionMs = 0;
            if (g_predictionMs > 150) g_predictionMs = 150;
            SaveInt("Tracking", "iPredictionMs", g_predictionMs);
            char msg[64]; snprintf(msg, sizeof msg, "FNVR: head prediction %d ms", g_predictionMs);
            UIMessage(msg);
        }
    }

    if (!g_vrSystem) {
        if (g_vrFailedPermanently || now - g_lastInitAttempt < 10000) return;
        g_lastInitAttempt = now;
        if (!VR_Start()) return;
        UIMessage(g_vrCompositor ? "FNVR: headset connected - game shown in headset"
                                 : "FNVR: headset connected - head tracking ON (F8 toggles)");
    }

    VR_PumpEvents();
    if (!g_vrSystem) return;
    ApplyHeadsetFov();
    g_camOverride = false;
    g_frameHasHmdPose = false;
    StartSetup_Tick();
    {   /* what the character is doing (reloading, equipping...) - read 10 times a second */
        static double next = 0; static void *scr = NULL; static bool failed = false;
        if (!g_inGameplay || !PlayerWeaponOut()) { if (g_animAction != -1) { g_animLastBusy = g_animAction == 0 || g_animAction == 1; g_animAction = -1; g_animIdleSince = QpcMs(); } }
        else if (!failed && QpcMs() > next && g_scriptIntf && g_scriptIntf->CompileScript) {
            next = QpcMs() + 200.0;
            if (!scr) { scr = g_scriptIntf->CompileScript("ref rWho\nint iVal\nBegin Function { rWho }\nset iVal to rWho.GetAnimAction\nSetFunctionValue iVal\nEnd\n");
                if (!scr) { failed = true; Log("Arms: couldn't set up reading the reload state"); } }
            double v = -1;
            if (scr && Scr_Num(scr, GetPlayer(), &v)) {
                int a = (int)v;
                if (a != g_animAction) { if (a == -1) { g_animIdleSince = QpcMs(); g_animLastBusy = g_animAction == 0 || g_animAction == 1; } static int al = 0; if (al++ < 30) Log("Action: %d -> %d", g_animAction, a); g_animAction = a; }
            }
        }
    }
    Controllers_Poll(IsMenuMode());

    TrackedDevicePose_t pose;
    if (HeadsetDisplayActive() && g_haveWaitPose) {
        pose = g_waitPose;               /* SteamVR's prediction for when this frame hits the display */
    } else {
        g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding,
                                                    (float)g_predictionMs / 1000.0f, &pose, 1);
    }

    if (!pose.bPoseIsValid) {
        if (g_haveLastPose) Log("Headset pose became invalid (headset asleep or tracking lost)");
        g_haveLastPose = false;
        if (++g_invalidPoseFrames == 600) Log("Still no valid headset pose after ~10s");
        return;
    }
    if (g_invalidPoseFrames) { Log("Headset pose valid again"); g_invalidPoseFrames = 0; }

    float yaw, pitch;
    ExtractYawPitch(&pose.mDeviceToAbsoluteTracking, &yaw, &pitch);

    bool menu = IsMenuMode();
    {   /* Pip-Boy up: the right stick works its menus - it must not turn you */
        static bool held = false; static float heldZ = 0;
        void *pl_ = GetPlayer(); float *rz = pl_ ? PlayerRotZ(pl_) : NULL;
        if (rz && g_pipOpen && g_wristPip) { if (!held) { held = true; heldZ = *rz; } else if (menu) *rz = heldZ; else heldZ = *rz; }
        else held = false;
    }
    if (g_reequipAt > 0 && QpcMs() > g_reequipAt && g_inGameplay && !menu && player_ok_for_reequip()) {
        g_reequipAt = 0;
        Arms_LogHidden("before refresh");
        static void *scr = NULL;
        if (!scr && g_scriptIntf && g_scriptIntf->CompileScript)
            scr = g_scriptIntf->CompileScript("ref w\nBegin Function { }\nlet w := player.GetEquippedObject 5\nif w\nplayer.EquipItem w 0 1\nendif\nEnd\n");
        bool ran = scr && g_scriptIntf->CallFunctionAlt(scr, GetPlayer(), 0);
        Log("First-person arms: refreshed the equipped weapon after loading (%s)", ran ? "done" : "couldn't run");
        g_hiddenLogAt = QpcMs() + 1500.0;
    }
    if (g_hiddenLogAt > 0 && QpcMs() > g_hiddenLogAt) { g_hiddenLogAt = 0; Arms_LogHidden("after refresh"); }
    {
        int pm = PipBoyMode();
        if (pm != g_pipModeRaw) {
            Log("Pip-Boy mode %d -> %d", g_pipModeRaw, pm);
            /* weapon swapped while the Pip-Boy was open (the world kept running, so the swap happened in the
               middle of the Pip-Boy arm animation): refresh it once the Pip-Boy is closed */
            static void *weapAtOpen = NULL;
            if (pm == 3) weapAtOpen = PlayerWeaponForm();
            if (pm == 0 && g_pipModeRaw != 0) {
                void *now = PlayerWeaponForm();
                if (now != weapAtOpen) { g_reequipAt = QpcMs() + 400.0; Log("Pip-Boy: weapon changed while open - refreshing it after closing"); }
            }
            g_pipModeRaw = pm;
        }
        bool open = g_wristPip && menu && (pm == 2 || pm == 3 || pm == 5 || QpcMs() < g_pipExpectUntil || PipMenusVisible());
        { static bool pmv = false; bool v = PipMenusVisible(); if (v != pmv) { pmv = v; Log("Pip-Boy screens %s (mode %d)", v ? "showing" : "gone", pm); } }
        if (open != g_pipOpen) Log("Pip-Boy %s", open ? "open - shown on your wrist" : "closed");
        g_pipOpen = open;
    }
    if (g_pipOpen) menu = false;          /* the world, your head and your arms keep going while the Pip-Boy is up */
    if (g_liveMenuNow) menu = false;      /* ...and while a live menu (talking, trading...) is up */
    g_menuCached = menu;
    void *player = GetPlayer();

    /* Aiming hand pose (right hand, or left for left-handed players) */
    int aimHand = g_leftHanded ? 0 : 1;
    TrackedDevicePose_t handPose;
    bool handOk = false;
    if (g_handAim && g_motionControls && g_padActive && HeadsetDisplayActive() && g_handIdx[aimHand] < 16) {
        if (g_haveWaitPose) handPose = g_waitPoses[g_handIdx[aimHand]];
        else {
            TrackedDevicePose_t all[16];
            g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, all, 16);
            handPose = all[g_handIdx[aimHand]];
        }
        handOk = handPose.bPoseIsValid;
    }

    g_handAimValid = false;
    g_bodyValid = false;
    if (g_trackingEnabled && !menu && player) {
        /* ---- Head drives the game camera (and the character's facing) ---- */
        float *rotX = PlayerRotX(player), *rotZ = PlayerRotZ(player);
        /* Pip-Boy up: the right stick works its menus, so it must not turn your body (undo any stick turn). */
        if (g_haveLastPose && g_pipOpen && g_wristPip) *rotZ = g_writtenHeading;
        if (g_haveLastPose) {
            /* Yaw: apply head turn as a delta so stick/mouse turning still works on top. */
            float newZ = *rotZ - WrapPi(yaw - g_lastYaw);
            while (newZ < 0.0f)         newZ += 2.0f * PI_F;
            while (newZ >= 2.0f * PI_F) newZ -= 2.0f * PI_F;
            *rotZ = newZ;
        }
        float newX;
        if (HeadsetDisplayActive()) {
            newX = -pitch;                                   /* in the headset, pitch follows the head exactly */
        } else {
            newX = *rotX - (g_haveLastPose ? (pitch - g_lastPitch) : 0.0f);
        }
        if (newX >  1.50f) newX =  1.50f;
        if (newX < -1.50f) newX = -1.50f;
        *rotX = newX;

        g_frameHasSample = true;
        g_sampleYaw = yaw;
        g_writtenHeading = *rotZ;
        g_samplePos[0] = pose.mDeviceToAbsoluteTracking.m[0][3];
        g_samplePos[1] = pose.mDeviceToAbsoluteTracking.m[1][3];
        g_samplePos[2] = pose.mDeviceToAbsoluteTracking.m[2][3];

        /* ---- Both hands, for the arms and melee ---- */
        g_bodyValid = true;
        for (int k = 0; k < 3; k++) g_vrHeadPos[k] = pose.mDeviceToAbsoluteTracking.m[k][3];
        { float hp_ = 0; ExtractYawPitch(&pose.mDeviceToAbsoluteTracking, &g_vrHeadYaw, &hp_); }
        g_vrYawOffset = *rotZ + yaw;
        for (int hnd = 0; hnd < 2; hnd++) {
            g_vrHandOk[hnd] = false;
            if (!g_padActive || g_handIdx[hnd] >= 16 || !g_haveWaitPose) continue;
            TrackedDevicePose_t *hp = &g_waitPoses[g_handIdx[hnd]];
            if (!hp->bPoseIsValid) continue;
            const float (*m)[4] = hp->mDeviceToAbsoluteTracking.m;
            float o = g_aimPitchOffsetDeg / RAD2DEG, f[3];
            for (int k = 0; k < 3; k++) { g_vrHandPos[hnd][k] = m[k][3]; f[k] = -m[k][2] * cosf(o) + m[k][1] * sinf(o); }
            /* Full controller orientation (twist included), tilted the same way as the aim direction. */
            for (int k = 0; k < 3; k++) {
                g_vrHandRot[hnd][k][0] = m[k][0];
                g_vrHandRot[hnd][k][1] = m[k][1] * cosf(o) + m[k][2] * sinf(o);
                g_vrHandRot[hnd][k][2] = m[k][2] * cosf(o) - m[k][1] * sinf(o);
            }
            float cy = atan2f(-f[0], -f[2]);
            float cp = asinf(f[1] < -1.f ? -1.f : f[1] > 1.f ? 1.f : f[1]);
            g_handHeading[hnd] = g_vrYawOffset - cy;
            g_handPitch[hnd] = -cp;
            g_handSpeed[hnd] = sqrtf(hp->vVelocity.v[0] * hp->vVelocity.v[0] + hp->vVelocity.v[1] * hp->vVelocity.v[1] +
                                     hp->vVelocity.v[2] * hp->vVelocity.v[2]);
            g_vrHandOk[hnd] = true;
        }

        /* ---- Pip-Boy by looking at your wrist: raise the left forearm in front of your face with the back
           of the wrist toward your eyes and it opens; lower it and it closes. ---- */
        if (g_wristPip && g_vrHandOk[0] && g_inGameplay) {
            const float (*pm)[4] = pose.mDeviceToAbsoluteTracking.m;
            float hd[3] = { pm[0][3] - g_vrHandPos[0][0], pm[1][3] - g_vrHandPos[0][1], pm[2][3] - g_vrHandPos[0][2] };
            float dist = V3Len(hd);
            float back[3] = { -g_vrHandRot[0][0][0], -g_vrHandRot[0][1][0], -g_vrHandRot[0][2][0] };   /* back of the left hand */
            float facing = dist > 0.01f ? V3Dot(back, hd) / dist : 0.0f;
            float hf[3] = { -pm[0][2], -pm[1][2], -pm[2][2] };                                     /* where you look */
            float inView = dist > 0.01f ? -V3Dot(hf, hd) / dist : 0.0f;
            bool lookOpen  = dist < 0.48f && facing > 0.45f && inView > 0.45f;
            bool lookKeep  = dist < 0.58f && facing > 0.35f && inView > 0.35f;   /* turn the wrist away and it closes */
            g_wristUp = dist < 0.62f && facing > 0.25f && inView > 0.25f;
            static bool rearmed = true;
            static int tries = 0;
            if (!lookOpen && !g_pipOpen && g_pipModeRaw == 0) { rearmed = true; tries = 0; }   /* wrist left the pose: may open again */
            /* the game ignored the press (e.g. it came mid-animation): press again, a few times at most */
            if (!rearmed && lookOpen && !g_pipOpen && g_pipModeRaw == 0 && !IsMenuMode() && !PipMenusVisible() &&
                QpcMs() > g_pipExpectUntil + 150.0 && tries < 3) { rearmed = true; tries++; Log("Pip-Boy: didn't open - pressing again (try %d)", tries + 1); }
            static double since = 0, lostSince = 0;
            double now = QpcMs();
            bool otherMenu = IsMenuMode() && !g_pipOpen;
            if (!g_pipOpen) {
                lostSince = 0;
                bool weapBusy = g_animAction == 0 || g_animAction == 1 || (g_animIdleSince && now - g_animIdleSince < 400.0 && g_animAction == -1 && g_animLastBusy);   /* (drawing or holstering a weapon: wait for it) */
                if (lookOpen && rearmed && !otherMenu && !g_twoHand && !g_pipPressMask && !g_setOpen && !weapBusy) {
                    if (!since) since = now;
                    if (now - since > 180.0) {
                        g_pipPressMask = g_bindMask[BIND_PIPBOY]; g_pipPressUntil = now + 250.0; since = 0; g_pipExpectUntil = now + 1000.0;
                        rearmed = false;
                        { char *pr = player ? *(char **)((char *)player + 0x68) : NULL; char *cc = pr ? *(char **)(pr + 0x138) : NULL;
                          Log("Pip-Boy: wrist raised to your face (%.2f m, facing %.2f) - opening (in air %.2fs, weapon out %d, climbing %d)", dist, facing,
                              cc ? *(float *)(cc + 0x548) : -1.0f, (int)PlayerWeaponOut(), (int)(g_climb[0] || g_climb[1])); }
                        if (g_savePhotos && g_recOpen < 3 && !g_recStart) { g_recOpen++; g_recFrame = 0; g_recStart = QpcMs(); Log("REC: recording Pip-Boy open %d", g_recOpen); }
                    }
                } else since = 0;
            } else {
                since = 0;
                static double fullSince = 0;
                if (g_pipModeRaw != 3) fullSince = 0; else if (!fullSince) fullSince = now;
                if (!lookKeep && fullSince && now - fullSince > 300.0) {
                    if (!lostSince) lostSince = now;
                    if (now - lostSince > 250.0 && !g_pipPressMask && g_pipModeRaw == 3) {
                        g_pipPressMask = XB_B; g_pipPressUntil = now + 150.0; lostSince = now + 600.0;   /* back out (retry later if needed) */
                        Log("Pip-Boy: wrist lowered - closing");
                    }
                } else lostSince = 0;
            }
        }

        /* ---- Hand aim: where the weapon points and shots go (relative to the head) ---- */
        if (handOk) {
            const float (*hm)[4] = handPose.mDeviceToAbsoluteTracking.m;
            /* Calibration: click both thumbsticks while pointing the controller at the centre of your view. */
            bool calDown = g_padActive && g_aimCalReq;
            if (calDown) {
                g_aimCalReq = false;
                { float ref = Body_StandRef(); g_standY = ref > 0.3f ? ref : -1.0f; } g_haveLeanAnchor = false;   /* also re-centre standing height and lean */
                const float (*pm)[4] = pose.mDeviceToAbsoluteTracking.m;
                float hf[3] = { -pm[0][2], -pm[1][2], -pm[2][2] };          /* head forward */
                float alongZ = -(hf[0] * hm[0][2] + hf[1] * hm[1][2] + hf[2] * hm[2][2]);
                float alongY =   hf[0] * hm[0][1] + hf[1] * hm[1][1] + hf[2] * hm[2][1];
                g_aimPitchOffsetDeg = atan2f(alongY, alongZ) * RAD2DEG;
                char buf[16]; snprintf(buf, sizeof buf, "%.1f", g_aimPitchOffsetDeg);
                WritePrivateProfileStringA("Controls", "fAimPitchOffset", buf, g_iniPath);
                char msg[64]; snprintf(msg, sizeof msg, "FNVR: aim calibrated (%+.0f deg)", g_aimPitchOffsetDeg);
                UIMessage(msg);
                Log("Aim calibrated: offset %.1f deg", g_aimPitchOffsetDeg);
                Profile_Save();
            }
            float o = g_aimPitchOffsetDeg / RAD2DEG;
            float f[3];   /* aim direction = controller -Z tilted around the controller's X axis */
            for (int k = 0; k < 3; k++) f[k] = -hm[k][2] * cosf(o) + hm[k][1] * sinf(o);
            float cyaw = atan2f(-f[0], -f[2]);
            float cpitch = asinf(f[1] < -1.f ? -1.f : f[1] > 1.f ? 1.f : f[1]);
            float h = *rotZ + WrapPi(yaw - cyaw);            /* game heading of the hand */
            while (h < 0.0f)         h += 2.0f * PI_F;
            while (h >= 2.0f * PI_F) h -= 2.0f * PI_F;
            g_handAimHeading = h;
            g_handAimPitch = -cpitch;                          /* game pitch convention: + = down */
            if (g_handAimPitch >  1.50f) g_handAimPitch =  1.50f;
            if (g_handAimPitch < -1.50f) g_handAimPitch = -1.50f;
            g_handAimValid = true;

            /* ---- Two-handed aiming ---- */
            {
                int aH = g_leftHanded ? 0 : 1, sH = 1 - aH;
                bool both = g_vrHandOk[aH] && g_vrHandOk[sH];
                float d[3] = { 0, 0, 0 }, dl = 0, fw[3];
                M3 Hr; memcpy(Hr, g_vrHandRot[aH], sizeof Hr);
                if (both) { V3Sub(d, g_vrHandPos[sH], g_vrHandPos[aH]); dl = V3Len(d); }
                for (int k = 0; k < 3; k++) fw[k] = -Hr[k][2];
                /* is the off hand on the gun? (in front of the aiming hand, close to the barrel line) */
                g_twoHandCand = false;
                if (both && dl > 0.01f) {
                    float along = V3Dot(d, fw), perp2 = dl * dl - along * along;
                    int wtc = PlayerWeaponType();
                    if (wtc == 1 || wtc == 2) g_twoHandCand = along > -0.60f && along < 0.75f && fabsf(along) > 0.04f && perp2 < 0.18f * 0.18f;   /* melee: anywhere along the handle, either side */
                    else g_twoHandCand = along > 0.05f && along < 0.75f && perp2 < 0.16f * 0.16f;
                }
                bool was = g_twoHand;
                g_twoHand = g_twoHandWant && both && dl > 0.05f;
                if (g_twoHand && !was) {
                    for (int k = 0; k < 3; k++) g_twoHandLocal[k] = (Hr[0][k] * d[0] + Hr[1][k] * d[1] + Hr[2][k] * d[2]) / dl;
                    Log("Two-handed aim: on (off hand %.0f cm in front of the aiming hand)", dl * 100.0f);
                } else if (!g_twoHand && was) Log("Two-handed aim: off");
                if (g_twoHand) {
                    float a[3], dn[3] = { d[0] / dl, d[1] / dl, d[2] / dl };
                    for (int k = 0; k < 3; k++) a[k] = Hr[k][0] * g_twoHandLocal[0] + Hr[k][1] * g_twoHandLocal[1] + Hr[k][2] * g_twoHandLocal[2];
                    M3 C; RotBetween(C, a, dn); memcpy(g_twoHandC, C, sizeof C);
                    float f2[3];
                    for (int k = 0; k < 3; k++) f2[k] = C[k][0] * fw[0] + C[k][1] * fw[1] + C[k][2] * fw[2];
                    float cy2 = atan2f(-f2[0], -f2[2]);
                    float cp2 = asinf(f2[1] < -1.f ? -1.f : f2[1] > 1.f ? 1.f : f2[1]);
                    float h2 = *rotZ + WrapPi(yaw - cy2);
                    while (h2 < 0.0f)         h2 += 2.0f * PI_F;
                    while (h2 >= 2.0f * PI_F) h2 -= 2.0f * PI_F;
                    g_handAimHeading = h2;
                    g_handAimPitch = fminf(fmaxf(-cp2, -1.50f), 1.50f);
                    /* off hand wraps the gun */
                    g_fingerCurl[sH][0] = 0.6f; g_fingerCurl[sH][1] = 0.65f; g_fingerCurl[sH][2] = 0.7f;
                }
            }
            {   /* scope raised to the eye: aiming hand close in front of the eye, gun pointing where you look */
                int aH = g_leftHanded ? 0 : 1;
                bool atEye = false;
                if (g_vrHandOk[aH]) {
                    const float (*pm)[4] = pose.mDeviceToAbsoluteTracking.m;
                    float hf[3] = { -pm[0][2], -pm[1][2], -pm[2][2] };
                    float d[3]; V3Sub(d, g_vrHandPos[aH], g_vrHeadPos);
                    float along = V3Dot(d, hf), dl = V3Len(d);
                    float perp = sqrtf(fmaxf(dl * dl - along * along, 0.0f));
                    float aimF[3] = { -g_vrHandRot[aH][0][2], -g_vrHandRot[aH][1][2], -g_vrHandRot[aH][2][2] };
                    float align = V3Dot(aimF, hf);
                    {
                        static DWORD lastDiag = 0;
                        if (PlayerWeaponScoped() && GetTickCount() - lastDiag > 1500) {
                            lastDiag = GetTickCount();
                            Log("Scope check: hand %.2f m in front of the eye, %.2f m off the line of sight, gun/view alignment %.2f", along, perp, align);
                        }
                    }
                    if (!g_scopeAtEye) atEye = along > -0.02f && along < 0.50f && perp < 0.42f && align > 0.94f;
                    else atEye = along > -0.08f && along < 0.60f && perp < 0.50f && align > 0.85f;   /* a bit looser to stay in */
                }
                if (QpcMs() < g_weaponSettleUntil || !PlayerWeaponOut() || !PlayerWeaponScoped()) atEye = false;   /* weapon still being swapped in / no scope */
                if (atEye != g_scopeAtEye) Log("Scope: gun %s the eye", atEye ? "raised to" : "lowered from");
                g_scopeAtEye = atEye;
            }
            if (!g_handAimEngaged) { g_handAimEngaged = true; Log("Hand aiming active (head looks, hand aims weapon and shots)"); }
        }
    }
    if (menu) g_handAimEngaged = false;
    g_lastYaw = yaw;
    g_lastPitch = pitch;
    g_haveLastPose = true;

    if (now - g_lastStatusLog > 2000) {
        float secs = g_lastStatusLog ? (now - g_lastStatusLog) / 1000.0f : 1.0f;
        float px = 0, pz = 0;
        if (player) { px = *PlayerRotX(player); pz = *PlayerRotZ(player); }
        Log("HMD yaw %+6.1f pitch %+6.1f | player hdg %5.1f pitch %+5.1f | menu=%d trk=%s | game %.0f fps, present %.0f/s, submit %.0f/s, 3D frames %.0f/s, render calls %u (resize %u, main %u, other %08X), copy waits %u, xinput %u, pad %d",
            yaw * RAD2DEG, pitch * RAD2DEG, pz * RAD2DEG, px * RAD2DEG, (int)menu,
            g_trackingEnabled ? "on" : "off",
            g_ticksSinceLog / secs, g_presentsSinceLog / secs, g_submitsSinceLog / secs, g_stereoFramesSinceLog / secs, g_renderCallsSinceLog, g_renderCallsResize, g_renderCallsMain, (unsigned)g_renderCallsOther, g_capWaitsSinceLog, g_xinputCalls, (int)g_padActive * 10 + *(BYTE *)GAMEPAD_CONNECTED);
        {   /* address-space use: the game is 32-bit, so running out of it crashes inside the game */
            MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
            size_t largest = 0;
            MEMORY_BASIC_INFORMATION mbi;
            for (char *addr = (char *)0x10000; addr < (char *)0xFFFF0000; ) {
                if (!VirtualQuery(addr, &mbi, sizeof mbi)) break;
                if (mbi.State == MEM_FREE && mbi.RegionSize > largest) largest = mbi.RegionSize;
                char *next = (char *)mbi.BaseAddress + mbi.RegionSize;
                if (next <= addr) break;
                addr = next;
            }
            if (GlobalMemoryStatusEx(&ms))
                Log("Memory: %u MB of %u MB address space used, largest free block %u MB",
                    (unsigned)((ms.ullTotalVirtual - ms.ullAvailVirtual) >> 20), (unsigned)(ms.ullTotalVirtual >> 20), (unsigned)(largest >> 20));
        }
        if (g_tFrames) {
            Log("Frame time (ms per frame): game drawing %.1f | FNVR: tracking %.2f, grab/collision %.2f (lean %.2f), arms %.2f, held objects %.2f | nearby objects %d",
                g_tGame / g_tFrames, g_tTick / g_tFrames, g_tGrab / g_tFrames, g_tHead / g_tFrames, g_tArms / g_tFrames, g_tVis / g_tFrames, g_nearCount);
        }
        if (g_shMisses || g_shHits) {
            Log("Shader cache: %u reused, %u compiled (%.0f ms compiling) since last report", g_shHits, g_shMisses, g_shMissMs);
            g_shHits = g_shMisses = 0; g_shMissMs = 0;
        }
        g_tGrab = g_tArms = g_tVis = g_tGame = g_tTick = g_tHead = 0; g_tFrames = 0;
        g_xinputCalls = 0;
        g_ticksSinceLog = g_presentsSinceLog = g_submitsSinceLog = g_capWaitsSinceLog = g_stereoFramesSinceLog = 0;
        g_renderCallsSinceLog = g_renderCallsResize = g_renderCallsMain = 0; g_renderCallsOther = 0;
        g_lastStatusLog = now;
    }
}

/* ======================================================================= */
/* Headset display: D3D9 frame -> D3D11 texture -> SteamVR compositor       */
/* ======================================================================= */

/* D3D11 side */
static ID3D11Device           *g_d11 = NULL;
static ID3D11DeviceContext    *g_d11ctx = NULL;
static ID3D11Texture2D        *g_eyeTex[2] = { NULL, NULL };
static ID3D11RenderTargetView *g_eyeRTV[2] = { NULL, NULL };
static UINT  g_eyeW = 0, g_eyeH = 0;
static float g_pxPerTan = 0;                  /* eye texture pixels per unit of view tangent */
static UINT  g_bbW = 0, g_bbH = 0;            /* game backbuffer size */
static float g_uL, g_uR, g_vT, g_vB;          /* tangent range covered by the eye texture (v down) */
static VRTextureBounds_t g_bounds[2];
static bool  g_displayFailed = false;

/* D3D9 side */
static IDirect3DSurface9 *g_resolveSurf = NULL;  /* full-size, non-MSAA copy of the backbuffer */
static UINT g_resolveW = 0, g_resolveH = 0;
static D3DFORMAT g_resolveFmt = D3DFMT_UNKNOWN;

/* Frame copies in flight. The GPU copies a frame into CPU memory in the background and we pick it
   up a frame later, instead of stalling the game until the copy finishes. One slot = one frame
   (one image in mono, two in stereo). */
#define NSLOTS 2   /* images in flight; each costs ~15 MB of the game's 4 GB */
typedef struct {
    IDirect3DSurface9 *rt[2];  /* scaled to eye-texture pixel density */
    IDirect3DSurface9 *sys[2]; /* CPU-readable copy */
    IDirect3DQuery9   *query;  /* signals when the copies are finished */
    UINT w, h;                 /* allocated size */
    RECT src;                  /* part of the game image used */
    UINT dx, dy, dw, dh;       /* where the image goes in the eye texture */
    HmdMatrix34_t pose;        /* head pose the frame was rendered for */
    int  eyes;                 /* images captured in this slot */
    int  targetEye;            /* -1 = mono (shown to both eyes), 0/1 = image for that eye */
    bool filling;              /* being captured this frame */
    bool pending;
    DWORD seq;
} CapSlot;
static CapSlot g_slots[NSLOTS];
static CapSlot *g_fillSlot = NULL;
static DWORD g_capSeq = 0;
static bool  g_haveEyeImage = false;     /* anything uploaded yet */
static bool  g_monoImage = true;         /* latest upload was a mono frame */
static bool  g_haveEye[2] = { false, false };
static HmdMatrix34_t g_eyePose[2];       /* head pose each eye texture was rendered for */
static int   g_aerEye = 0;               /* which eye the next stereo frame renders */

static bool g_loggedFrustum = false;
static bool g_inPresent = false;

static bool HeadsetDisplayActive(void)
{
    return g_vrCompositor != NULL && g_d11 != NULL && !g_displayFailed;
}

static void Release9(IDirect3DSurface9 **s) { if (*s) { IDirect3DSurface9_Release(*s); *s = NULL; } }

static void Scope_Release(void);
static void UI_Release9(void);
static void Far_Release9(void);
static void PipOv_Release9(void);
static void TF_Release9(void);
static void Rec_Release9(void);
static void Far_UiTick(void);
static void Holster_Tick(void);
static void Display_Release9(void)
{
    Scope_Release();
    UI_Release9();
    Far_Release9();
    PipOv_Release9();
    PipGrab_Release();
    PM_Release();
    TF_Release9();
    Rec_Release9();
    Release9(&g_resolveSurf);
    for (int i = 0; i < NSLOTS; i++) {
        for (int e = 0; e < 2; e++) { Release9(&g_slots[i].rt[e]); Release9(&g_slots[i].sys[e]); }
        g_slots[i].filling = false;
        if (g_slots[i].query) { IDirect3DQuery9_Release(g_slots[i].query); g_slots[i].query = NULL; }
        g_slots[i].w = g_slots[i].h = 0;
        g_slots[i].pending = false;
    }
    g_resolveW = g_resolveH = 0;
    g_fillSlot = NULL;
}

static void Display_ReleaseAll(void)
{
    Display_Release9();
    for (int e = 0; e < 2; e++) {
        if (g_eyeRTV[e]) { ID3D11RenderTargetView_Release(g_eyeRTV[e]); g_eyeRTV[e] = NULL; }
        if (g_eyeTex[e]) { ID3D11Texture2D_Release(g_eyeTex[e]); g_eyeTex[e] = NULL; }
    }
    if (g_d11ctx) { ID3D11DeviceContext_Release(g_d11ctx); g_d11ctx = NULL; }
    if (g_d11)    { ID3D11Device_Release(g_d11); g_d11 = NULL; }
    g_haveWaitPose = false;
    g_haveEyeImage = false;
    g_haveEye[0] = g_haveEye[1] = false;
}

static void DisplayFail(const char *what, HRESULT hr)
{
    Log("ERROR: %s failed (hr=0x%08lX) - headset display disabled, tracking still works", what, (unsigned long)hr);
    g_displayFailed = true;
    UIMessage("FNVR: headset display failed - see FNVR.log");
}

typedef HRESULT (WINAPI *CreateDXGIFactory1_t)(REFIID, void **);

static float TargetHTan(float aspectWH);

static bool Display_InitD3D11(void)
{
    int32_t adapterIdx = -1;
    g_vrSystem->GetDXGIOutputInfo(&adapterIdx);
    Log("SteamVR wants DXGI adapter %d", adapterIdx);

    IDXGIAdapter *adapter = NULL;
    IDXGIFactory1 *factory = NULL;
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    CreateDXGIFactory1_t createFactory = dxgi ? (CreateDXGIFactory1_t)GetProcAddress(dxgi, "CreateDXGIFactory1") : NULL;
    if (createFactory && SUCCEEDED(createFactory(&IID_IDXGIFactory1, (void **)&factory))) {
        if (adapterIdx >= 0) IDXGIFactory1_EnumAdapters(factory, (UINT)adapterIdx, &adapter);
        if (adapter) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(IDXGIAdapter_GetDesc(adapter, &desc)))
                Log("Using GPU: %ls (%u MB VRAM)", desc.Description, (unsigned)(desc.DedicatedVideoMemory >> 20));
        }
    }

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, NULL, 0, D3D11_SDK_VERSION,
                                   &g_d11, &fl, &g_d11ctx);
    if (adapter) IDXGIAdapter_Release(adapter);
    if (factory) IDXGIFactory1_Release(factory);
    if (FAILED(hr)) { DisplayFail("D3D11CreateDevice", hr); return false; }
    Log("D3D11 device created (feature level %X)", (unsigned)fl);
    LogMem("after D3D11 device");

    /* Work out the field of view the headset needs, as tangents of the half-angles. */
    float l[2], r[2], t[2], b[2];
    for (int e = 0; e < 2; e++) {
        g_vrSystem->GetProjectionRaw((EVREye)e, &l[e], &r[e], &t[e], &b[e]);
        Log("Eye %d projection (tangents): left %.3f right %.3f top %.3f bottom %.3f", e, l[e], r[e], t[e], b[e]);
    }
    /* OpenVR quirk: in GetProjectionRaw, "bottom" is the UP tangent and "top" the DOWN tangent
       (see ComposeProjection in openvr docs). Convert to a v-down range: up = -bottom, down = -top. */
    float up[2], dn[2];
    for (int e = 0; e < 2; e++) { up[e] = -b[e]; dn[e] = -t[e]; }
    g_uL = fminf(l[0], l[1]); g_uR = fmaxf(r[0], r[1]);
    g_vT = fminf(up[0], up[1]); g_vB = fmaxf(dn[0], dn[1]);
    Log("Headset view range (v down): horizontal %.3f..%.3f, vertical %.3f (up) .. %.3f (down)", g_uL, g_uR, g_vT, g_vB);

    uint32_t recW = 0, recH = 0;
    g_vrSystem->GetRecommendedRenderTargetSize(&recW, &recH);
    if (!recW) recW = 2000;
    if (g_pixelDensityPct > 0) {
        g_pxPerTan = (float)recW / (r[0] - l[0]) * (g_pixelDensityPct / 100.0f);
    } else {
        /* Auto: one game pixel = one eye-texture pixel at the headset-matched FOV (no resampling). */
        float aspect = g_bbH ? (float)g_bbW / (float)g_bbH : 1.0f;
        g_pxPerTan = (float)g_bbW / (2.0f * TargetHTan(aspect));
        Log("Pixel density auto: game %ux%u -> %.0f px per tangent unit (%.0f%% of SteamVR recommended)",
            g_bbW, g_bbH, g_pxPerTan, 100.0f * g_pxPerTan / ((float)recW / (r[0] - l[0])));
    }
    g_eyeW = (UINT)ceilf((g_uR - g_uL) * g_pxPerTan);
    g_eyeH = (UINT)ceilf((g_vB - g_vT) * g_pxPerTan);
    if (g_eyeW > 8192 || g_eyeH > 8192) {
        float s = 8192.0f / (float)(g_eyeW > g_eyeH ? g_eyeW : g_eyeH);
        g_pxPerTan *= s; g_eyeW = (UINT)(g_eyeW * s); g_eyeH = (UINT)(g_eyeH * s);
    }
    Log("SteamVR recommended per-eye size %ux%u; shared eye texture %ux%u (%.0f px per tangent unit)",
        recW, recH, g_eyeW, g_eyeH, g_pxPerTan);

    for (int e = 0; e < 2; e++) {
        g_bounds[e].uMin = (l[e] - g_uL) / (g_uR - g_uL);
        g_bounds[e].uMax = (r[e] - g_uL) / (g_uR - g_uL);
        g_bounds[e].vMin = (up[e] - g_vT) / (g_vB - g_vT);
        g_bounds[e].vMax = (dn[e] - g_vT) / (g_vB - g_vT);
    }

    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = g_eyeW; td.Height = g_eyeH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    for (int e = 0; e < 2; e++) {
        hr = ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_eyeTex[e]);
        if (FAILED(hr)) { DisplayFail("CreateTexture2D (eye)", hr); return false; }
        hr = ID3D11Device_CreateRenderTargetView(g_d11, (ID3D11Resource *)g_eyeTex[e], NULL, &g_eyeRTV[e]);
        if (FAILED(hr)) { DisplayFail("CreateRenderTargetView", hr); return false; }
    }
    return true;
}

/* Tangent range the game camera is rendering this frame (v down). Returns false if unknown. */
static bool GetGameTangents(UINT bbW, UINT bbH, float *gl, float *gr, float *gt, float *gb)
{
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
    float fovDeg = 0.0f;
    float fl = 0, fr = 0, ft = 0, fb = 0;
    bool frustumOk = false;
    if (sg) {
        fovDeg = *(float *)(sg + SCENEGRAPH_FOV);
        char *cam = *(char **)(sg + SCENEGRAPH_CAMERA);
        if (cam) {
            float *f = (float *)(cam + NICAMERA_FRUSTUM);
            fl = f[0]; fr = f[1]; ft = f[2]; fb = f[3];
            frustumOk = fr > 0.05f && fr < 6.0f && fl < -0.05f && fl > -6.0f && ft > 0.03f && ft < 6.0f && fb < -0.03f && fb > -6.0f;
        }
    }
    float aspect = (float)bbH / (float)bbW;
    float fovTan = (fovDeg > 10.0f && fovDeg < 170.0f) ? tanf(fovDeg * 0.5f / RAD2DEG) : 0.0f;

    /* The FOV setting is defined for a 4:3 screen; widescreen widens it horizontally (Hor+). */
    float hTan = fovTan > 0.0f ? fovTan * ((float)bbW / (float)bbH) / (4.0f / 3.0f) : 0.0f;

    static float lastLoggedR = 0.0f;
    static DWORD lastLogTime = 0;
    if (!g_loggedFrustum || (fabsf(fr - lastLoggedR) > 0.02f && GetTickCount() - lastLogTime > 1000)) {
        g_loggedFrustum = true;
        lastLogTime = GetTickCount();
        lastLoggedR = fr;
        Log("Game camera: frustum l %.3f r %.3f t %.3f b %.3f (valid=%d), scene FOV %.1f deg (Hor+ tan %.3f), backbuffer %ux%u",
            fl, fr, ft, fb, (int)frustumOk, fovDeg, hTan, bbW, bbH);
    }

    if (frustumOk) {                      /* exact view the camera is rendering */
        *gl = fl; *gr = fr; *gt = -ft; *gb = -fb;
        return true;
    }
    if (hTan == 0.0f) hTan = tanf(37.5f / RAD2DEG) * ((float)bbW / (float)bbH) / (4.0f / 3.0f);
    *gl = -hTan; *gr = hTan; *gt = -hTan * aspect; *gb = hTan * aspect;
    return true;
}

static bool EnsureSurface9(IDirect3DDevice9 *dev, IDirect3DSurface9 **rt, IDirect3DSurface9 **sys,
                           UINT w, UINT h, D3DFORMAT fmt)
{
    HRESULT hr = IDirect3DDevice9_CreateRenderTarget(dev, w, h, fmt, D3DMULTISAMPLE_NONE, 0, FALSE, rt, NULL);
    if (FAILED(hr)) { DisplayFail("CreateRenderTarget", hr); return false; }
    if (sys) {
        hr = IDirect3DDevice9_CreateOffscreenPlainSurface(dev, w, h, fmt, D3DPOOL_SYSTEMMEM, sys, NULL);
        if (FAILED(hr)) { DisplayFail("CreateOffscreenPlainSurface", hr); return false; }
    }
    return true;
}

/* Half-angle tangent (horizontal) the game must render so its view covers the whole headset view. */
static float TargetHTan(float aspectWH)
{
    float needH = fmaxf(fabsf(g_uL), fabsf(g_uR));
    float needV = fmaxf(fabsf(g_vT), fabsf(g_vB));
    return fmaxf(needH, needV * aspectWH) * 1.01f;   /* 1% margin */
}

/* Keep the game's world FOV matched to the headset during gameplay. Called every game frame.
   In menus (Pip-Boy, dialogue, inventory...) and on the main menu the game's own FOV is restored,
   so 2D screens stay readable in a box in front of you. F9 toggles fill mode. */
static float g_gameWorldFov = 0.0f;     /* FOV the game/mods want (restored in menus) */
static float g_gameFirstFov = 0.0f;
static float g_targetFov = 0.0f;        /* FOV that fills the headset */
static bool  g_fovOverridden = false;
static bool  g_kF9 = false;

static void SetGameFov(char *player, char *sg, float world, float first)
{
    *(float *)(player + PLAYER_WORLD_FOV) = world;
    *(float *)(player + PLAYER_1ST_FOV) = first;
    SetCameraFOV(sg, world, false, NULL, false);
}

static void RestoreGameFov(const char *why)
{
    char *player = (char *)GetPlayer();
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
    if (g_fovOverridden && player && sg && g_gameWorldFov > 0.0f) {
        SetGameFov(player, sg, g_gameWorldFov, g_gameFirstFov);
        Log("Restored game FOV %.1f (%s)", g_gameWorldFov, why);
    }
    g_fovOverridden = false;
}

/* In-game positioning of the Pip-Boy menu and its black box (numpad, NumLock on):
     5 = switch between moving the MENU and the BLACK BOX
     4/6 = left/right   8/2 = up/down   9/3 = toward you / away   (hold to keep moving)
     + / - = bigger/smaller (black box)   . = reset the one you're moving   0 = save */
static void SaveF(const char *key, float v) { char b[32]; snprintf(b, sizeof b, "%.2f", v); WritePrivateProfileStringA("Controls", key, b, g_iniPath); }
static void PipNudge_Tick(void)
{
    if (!GameWindowFocused()) return;
    static bool target = false;   /* false = menu, true = black box */
    static bool k5 = false, k0 = false, kDot = false;
    static double nextRep = 0; static int lastKey = 0;
    char msg[160]; bool show = false;
    if (KeyPressed(VK_NUMPAD5, &k5)) { target = !target; show = true; }
    float *sx, *sy, *sz;
    if (target) { sx = &g_pipShift[0]; sy = &g_pipShift[1]; sz = &g_pipLift; }
    else        { sx = &g_pipMenuShift[0]; sy = &g_pipMenuShift[1]; sz = &g_pipMenuAbove; }
    static const int keys[8] = { VK_NUMPAD4, VK_NUMPAD6, VK_NUMPAD8, VK_NUMPAD2, VK_NUMPAD9, VK_NUMPAD3, VK_ADD, VK_SUBTRACT };
    int held = 0; for (int i = 0; i < 8; i++) if (GetAsyncKeyState(keys[i]) & 0x8000) { held = keys[i]; break; }
    if (held) {
        double now = QpcMs();
        if (held != lastKey || now >= nextRep) {
            nextRep = now + (held != lastKey ? 350.0 : 70.0);
            const float st = 0.1f;
            switch (held) {
            case VK_NUMPAD4: *sx -= st; break;  case VK_NUMPAD6: *sx += st; break;
            case VK_NUMPAD8: *sy += st; break;  case VK_NUMPAD2: *sy -= st; break;
            case VK_NUMPAD9: *sz += st; break;  case VK_NUMPAD3: *sz -= st; break;
            case VK_ADD: if (target) g_pipFitIni = fminf(g_pipFitIni + 0.02f, 3.0f); break;
            case VK_SUBTRACT: if (target) g_pipFitIni = fmaxf(g_pipFitIni - 0.02f, 0.3f); break;
            }
            show = true;
        }
    }
    lastKey = held;
    if (KeyPressed(VK_DECIMAL, &kDot)) { *sx = 0; *sy = 0; *sz = target ? 1.0f : 0.0f; if (target) g_pipFitIni = 1.0f; show = true; }
    if (KeyPressed(VK_NUMPAD0, &k0)) {
        SaveF("fPipMenuShiftX", g_pipMenuShift[0]); SaveF("fPipMenuShiftY", g_pipMenuShift[1]); SaveF("fPipMenuAboveBox", g_pipMenuAbove);
        SaveF("fPipBoyScreenShiftX", g_pipShift[0]); SaveF("fPipBoyScreenShiftY", g_pipShift[1]); SaveF("fPipBoyScreenLift", g_pipLift); SaveF("fPipBoyScreenFit", g_pipFitIni);
        Log("Pip-Boy positions saved: menu %.2f %.2f %.2f, box %.2f %.2f lift %.2f size %.2f", g_pipMenuShift[0], g_pipMenuShift[1], g_pipMenuAbove, g_pipShift[0], g_pipShift[1], g_pipLift, g_pipFitIni);
        UIMessage("FNVR: Pip-Boy menu and black box positions saved"); return;
    }
    static bool pending = false;
    if (show) pending = true;
    if (pending && !held) {   /* one message when you let go (corner messages queue up) */
        pending = false;
        if (target) snprintf(msg, sizeof msg, "Moving BLACK BOX: right %.1f  up %.1f  toward you %.1f  size %.2f  (5 = switch, 0 = save)", g_pipShift[0], g_pipShift[1], g_pipLift, g_pipFitIni);
        else        snprintf(msg, sizeof msg, "Moving MENU: right %.1f  up %.1f  toward you %.1f  (5 = switch, 0 = save)", g_pipMenuShift[0], g_pipMenuShift[1], g_pipMenuAbove);
        UIMessage(msg);
        Log("%s", msg);
    }
}
static void ApplyHeadsetFov(void)
{
    PipNudge_Tick();
    {
        static bool kF10 = false;
        if (KeyPressed(VK_F10, &kF10) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            g_pipRealTime = !g_pipRealTime;
            SaveInt("Controls", "bPipBoyRealTime", g_pipRealTime ? 1 : 0);
            Log("Real-time Pip-Boy %s (F10)", g_pipRealTime ? "ON" : "OFF");
            UIMessage(g_pipRealTime ? "FNVR: world keeps running in the Pip-Boy" : "FNVR: Pip-Boy pauses the world");
        }
    }
    if (KeyPressed(VK_F9, &g_kF9) && HeadsetDisplayActive()) {
        g_matchHeadsetFov = !g_matchHeadsetFov;
        SaveInt("Display", "bMatchHeadsetFOV", g_matchHeadsetFov ? 1 : 0);
        Log("Fill headset view %s (F9)", g_matchHeadsetFov ? "ON" : "OFF");
        UIMessage(g_matchHeadsetFov ? "FNVR: fill view ON" : "FNVR: fill view OFF (window)");
    }

    char *player = (char *)GetPlayer();
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
    if (!player || !sg || !HeadsetDisplayActive() || !g_bbW || !g_bbH) return;

    float aspect = (float)g_bbW / (float)g_bbH;
    /* The game's FOV number is defined for 4:3 and widened/narrowed for other aspect ratios (Hor+). */
    g_targetFov = 2.0f * atanf(TargetHTan(aspect) * (4.0f / 3.0f) / aspect) * RAD2DEG;
    if (g_targetFov > 160.0f) g_targetFov = 160.0f;

    float world = *(float *)(player + PLAYER_WORLD_FOV);
    float first = *(float *)(player + PLAYER_1ST_FOV);
    bool ours = fabsf(world - g_targetFov) < 0.05f;

    /* Remember what the game (or other mods) want whenever it isn't our value. */
    if (!ours && !g_fovOverridden && (fabsf(world - g_gameWorldFov) > 0.05f || fabsf(first - g_gameFirstFov) > 0.05f)) {
        g_gameWorldFov = world; g_gameFirstFov = first;
        Log("Game FOV is world %.1f / weapon %.1f (headset fill needs %.1f)", world, first, g_targetFov);
    }

    int pmF = PipBoyMode();
    bool pipUp = g_wristPip && (g_pipOpen || QpcMs() < g_pipExpectUntil || pmF == 2 || pmF == 3 || pmF == 5);
    bool want = g_matchHeadsetFov && (!g_flatMenus || (g_inGameplay && (!IsMenuMode() || pipUp)));   /* full view everywhere */
    if (!want) {
        if (g_fovOverridden) RestoreGameFov(!g_inGameplay ? "main menu" : IsMenuMode() ? "menu open" : "fill off");
        return;
    }

    float sceneFov = *(float *)(sg + SCENEGRAPH_FOV);
    if (!ours || fabsf(sceneFov - g_targetFov) > 0.5f) {
        /* Includes aiming down sights: zooming the view in VR is uncomfortable, so it stays at headset FOV. */
        SetGameFov(player, sg, g_targetFov, g_targetFov);
        if (!g_fovOverridden) Log("Game FOV set to %.1f to fill the headset view", g_targetFov);
        g_fovOverridden = true;
    }
}

static void BuildSubmitPose(HmdMatrix34_t *out, float yaw, float pitch, const float pos[3])
{
    /* R = Ry(yaw) * Rx(pitch), OpenVR axes (+Y up, -Z forward). */
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    out->m[0][0] = cy;  out->m[0][1] = sy * sp;  out->m[0][2] = sy * cp;  out->m[0][3] = pos[0];
    out->m[1][0] = 0;   out->m[1][1] = cp;       out->m[1][2] = -sp;      out->m[1][3] = pos[1];
    out->m[2][0] = -sy; out->m[2][1] = cy * sp;  out->m[2][2] = cy * cp;  out->m[2][3] = pos[2];
}

/* Diagnostic snapshot: saves both eye images as BMP files once per session, so they can be compared. */
static int  g_snapshotCountdown = 600;   /* stereo frames before taking the snapshot */
static bool g_snapshotDone = false;

static void SaveBMP(const char *name, const BYTE *bits, int pitch, int w, int h)
{
    if (!g_savePhotos) return;   /* test pictures only when [Debug] bSavePhotos=1 */
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    DirOf(path);
    strcat(path, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int rowBytes = (w * 3 + 3) & ~3;
    DWORD imgSize = (DWORD)rowBytes * h;
    BITMAPFILEHEADER fh = { 0x4D42, (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imgSize), 0, 0,
                            sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) };
    BITMAPINFOHEADER ih = { sizeof(BITMAPINFOHEADER), w, h, 1, 24, BI_RGB, imgSize, 2835, 2835, 0, 0 };
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    BYTE *row = (BYTE *)calloc(1, rowBytes);
    for (int y = h - 1; y >= 0; y--) {
        const BYTE *src = bits + (size_t)y * pitch;
        for (int x = 0; x < w; x++) { row[x * 3] = src[x * 4]; row[x * 3 + 1] = src[x * 4 + 1]; row[x * 3 + 2] = src[x * 4 + 2]; }
        fwrite(row, 1, rowBytes, f);
    }
    free(row);
    fclose(f);
    Log("Saved diagnostic snapshot %s (%dx%d)", path, w, h);
}

static void Capture_Upload(CapSlot *slot)
{
    static const float black[4] = { 0, 0, 0, 1 };
    if (slot->eyes < 1 || !slot->sys[0]) return;
    int dst = slot->targetEye < 0 ? 0 : slot->targetEye;
    D3DLOCKED_RECT lr;
    HRESULT hr = IDirect3DSurface9_LockRect(slot->sys[0], &lr, NULL, D3DLOCK_READONLY);
    if (FAILED(hr)) { DisplayFail("LockRect", hr); return; }
    ID3D11DeviceContext_ClearRenderTargetView(g_d11ctx, g_eyeRTV[dst], black);
    D3D11_BOX box = { slot->dx, slot->dy, 0, slot->dx + slot->dw, slot->dy + slot->dh, 1 };
    ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_eyeTex[dst], 0, &box, lr.pBits, (UINT)lr.Pitch, 0);
    if (g_headSnapPending) { g_headSnapPending = false; SaveBMP("FNVR_headset.bmp", (const BYTE *)lr.pBits, lr.Pitch, (int)slot->dw, (int)slot->dh);
        Log("Headset picture: %ux%u taken from the game image at (%ld,%ld)-(%ld,%ld)", slot->dw, slot->dh, slot->src.left, slot->src.top, slot->src.right, slot->src.bottom); }
    if (slot->targetEye >= 0 && !g_snapshotDone && g_snapshotCountdown <= 0) {
        static bool saved[2] = { false, false };
        if (!saved[dst]) {
            SaveBMP(dst == 0 ? "FNVR_left_eye.bmp" : "FNVR_right_eye.bmp", (const BYTE *)lr.pBits, lr.Pitch, (int)slot->dw, (int)slot->dh);
            saved[dst] = true;
        }
        if (saved[0] && saved[1]) g_snapshotDone = true;
    }
    IDirect3DSurface9_UnlockRect(slot->sys[0]);
    ID3D11DeviceContext_Flush(g_d11ctx);

    g_eyePose[dst] = slot->pose;
    if (slot->targetEye < 0) {
        g_monoImage = true;
        g_haveEye[0] = true; g_haveEye[1] = false;
    } else {
        g_monoImage = false;
        g_haveEye[dst] = true;
        if (g_snapshotCountdown > 0) g_snapshotCountdown--;
    }
    g_haveEyeImage = true;
}

/* Upload every finished copy in order (stereo alternates eyes, so none can be skipped).
   If wait is set, block until at least the oldest pending copy is done. */
static void Capture_Collect(bool wait)
{
    bool uploadedAny = false;
    for (;;) {
        CapSlot *oldest = NULL;
        for (int i = 0; i < NSLOTS; i++)
            if (g_slots[i].pending && (!oldest || g_slots[i].seq < oldest->seq)) oldest = &g_slots[i];
        if (!oldest) return;
        bool done = !oldest->query || IDirect3DQuery9_GetData(oldest->query, NULL, 0, D3DGETDATA_FLUSH) == S_OK;
        if (done) {
            Capture_Upload(oldest);
            oldest->pending = false;
            uploadedAny = true;
            continue;
        }
        if (!wait || uploadedAny) return;
        g_capWaitsSinceLog++;
        Sleep(0);
    }
}

static void Display_Submit(void)
{
    if (!g_haveEyeImage) return;
    bool stereo = !g_monoImage && g_haveEye[0] && g_haveEye[1];
    for (int e = 0; e < 2; e++) {
        int src = stereo ? e : 0;
        VRTextureWithPose_t tex;
        tex.handle = g_eyeTex[src];
        tex.eType = ETextureType_TextureType_DirectX;
        tex.eColorSpace = EColorSpace_ColorSpace_Gamma;
        tex.mDeviceToAbsoluteTracking = g_eyePose[src];
        EVRCompositorError ce = g_vrCompositor->Submit((EVREye)e, (struct Texture_t *)&tex, &g_bounds[e],
                                                       EVRSubmitFlags_Submit_TextureWithPose);
        if (ce != EVRCompositorError_VRCompositorError_None) {
            static int errCount = 0;
            if (errCount++ < 10) Log("Submit eye %d returned compositor error %d", e, (int)ce);
            return;
        }
    }
    g_submitsSinceLog++;
}

/* Head pose this frame was rendered for (what SteamVR uses to correct remaining head movement). */
static HmdMatrix34_t CurrentFramePose(void)
{
    if (g_frameHasHmdPose) {
        g_lastSubmitPose = g_frameHmdPose;   /* camera was turned to exactly this head pose */
        g_haveSubmitPose = true;
        return g_frameHmdPose;
    }
    void *player = GetPlayer();
    if (g_frameHasSample && player) {
        /* Camera heading may have moved since we set it (mouse/stick turning) - account for that. */
        float yaw = g_sampleYaw - WrapPi(*PlayerRotZ(player) - g_writtenHeading);
        float pitch = -*PlayerRotX(player);
        BuildSubmitPose(&g_lastSubmitPose, yaw, pitch, g_samplePos);
        g_haveSubmitPose = true;
    }
    if (!g_haveSubmitPose) {
        /* Nothing sampled yet (e.g. main menu): place the image straight ahead at standing height. */
        float pos[3] = { 0.0f, 1.6f, 0.0f };
        BuildSubmitPose(&g_lastSubmitPose, 0.0f, 0.0f, pos);
        g_haveSubmitPose = true;
    }
    return g_lastSubmitPose;
}

/* Flat-screen mode: used whenever the game shows 2D screens (menus, Pip-Boy, dialogue, loading,
   main menu). The screen is placed level, straight ahead of where you were looking when it opened. */
static bool g_inScreenMode = false;
static HmdMatrix34_t g_screenPose;

static bool ScreenModeNow(void)
{
    if (!g_flatMenus) {   /* everything (main menu, loading, menus) in the full VR view */
        if (g_inScreenMode) Log("Flat screen mode OFF - everything in the full VR view");
        g_inScreenMode = false;
        return false;
    }
    int pmNow = PipBoyMode();
    bool pipUp = g_wristPip && (g_pipOpen || QpcMs() < g_pipExpectUntil || pmNow == 2 || pmNow == 3 || pmNow == 5);
    bool screen = !g_inGameplay || (IsMenuMode() && !pipUp && !g_liveMenuNow);
    if (screen && !g_inScreenMode) {
        TrackedDevicePose_t p;
        float pos[3] = { 0.0f, 1.6f, 0.0f }, yaw = 0.0f, pitch = 0.0f;
        if (g_vrSystem) {
            g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &p, 1);
            if (p.bPoseIsValid) {
                ExtractYawPitch(&p.mDeviceToAbsoluteTracking, &yaw, &pitch);
                pos[0] = p.mDeviceToAbsoluteTracking.m[0][3];
                pos[1] = p.mDeviceToAbsoluteTracking.m[1][3];
                pos[2] = p.mDeviceToAbsoluteTracking.m[2][3];
            }
        }
        BuildSubmitPose(&g_screenPose, yaw, 0.0f, pos);
        Log("Flat screen mode ON (%s) - %.0f deg wide screen", !g_inGameplay ? "main menu/loading" : "menu open", g_menuScreenFov);
    } else if (!screen && g_inScreenMode) {
        Log("Flat screen mode OFF - back to full view");
    }
    g_inScreenMode = screen;
    return screen;
}

/* Copy what's currently in the backbuffer as the image for one eye (eye 0 starts a new frame). */
static bool Capture_Eye(IDirect3DDevice9 *dev, int eye)
{
    if (!g_vrCompositor || g_displayFailed) return false;

    IDirect3DSurface9 *bb = NULL;
    HRESULT hr = IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (FAILED(hr) || !bb) return false;
    D3DSURFACE_DESC bd;
    IDirect3DSurface9_GetDesc(bb, &bd);
    g_bbW = bd.Width; g_bbH = bd.Height;
    if (!g_d11 && !Display_InitD3D11()) { IDirect3DSurface9_Release(bb); return false; }

    CapSlot *slot;
    if (eye == 0) {
        /* Where does this frame go in the headset view? */
        float gl, gr, gt, gb;
        bool screen = ScreenModeNow();
        if (screen) {
            /* Menus, dialogue, loading, main menu: show the frame as a flat screen of fixed size. */
            float h = tanf(g_menuScreenFov * 0.5f / RAD2DEG), v = h * (float)bd.Height / (float)bd.Width;
            gl = -h; gr = h; gt = -v; gb = v;
        } else {
            GetGameTangents(bd.Width, bd.Height, &gl, &gr, &gt, &gb);
        }
        float vl = fmaxf(gl, g_uL), vr = fminf(gr, g_uR), vt = fmaxf(gt, g_vT), vb = fminf(gb, g_vB);
        if (vr <= vl || vb <= vt) { IDirect3DSurface9_Release(bb); return false; }
        UINT dx = (UINT)((vl - g_uL) * g_pxPerTan), dy = (UINT)((vt - g_vT) * g_pxPerTan);
        UINT dw = (UINT)((vr - vl) * g_pxPerTan),   dh = (UINT)((vb - vt) * g_pxPerTan);
        if (dx + dw > g_eyeW) dw = g_eyeW - dx;
        if (dy + dh > g_eyeH) dh = g_eyeH - dy;
        if (dw < 8 || dh < 8) { IDirect3DSurface9_Release(bb); return false; }

        /* Pick a free slot (wait for the oldest copy if all are busy). */
        slot = NULL;
        for (int i = 0; i < NSLOTS && !slot; i++) if (!g_slots[i].pending) slot = &g_slots[i];
        if (!slot) {
            Capture_Collect(true);
            for (int i = 0; i < NSLOTS && !slot; i++) if (!g_slots[i].pending) slot = &g_slots[i];
            if (!slot) { IDirect3DSurface9_Release(bb); return false; }
        }
        slot->src.left   = (LONG)((vl - gl) / (gr - gl) * bd.Width);
        slot->src.right  = (LONG)((vr - gl) / (gr - gl) * bd.Width);
        slot->src.top    = (LONG)((vt - gt) / (gb - gt) * bd.Height);
        slot->src.bottom = (LONG)((vb - gt) / (gb - gt) * bd.Height);
        slot->dx = dx; slot->dy = dy; slot->dw = dw; slot->dh = dh;
        slot->pose = screen ? g_screenPose : CurrentFramePose();
        slot->eyes = 0;
        slot->targetEye = -1;
        slot->filling = true;
        g_fillSlot = slot;
    } else {
        slot = g_fillSlot;
        if (!slot || !slot->filling) { IDirect3DSurface9_Release(bb); return false; }
    }

    /* Resolve MSAA if needed (StretchRect can't scale a multisampled surface). */
    IDirect3DSurface9 *source = bb;
    D3DFORMAT fmt = (bd.Format == D3DFMT_A8R8G8B8 || bd.Format == D3DFMT_X8R8G8B8) ? bd.Format : D3DFMT_X8R8G8B8;
    if (bd.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (!g_resolveSurf || g_resolveW != bd.Width || g_resolveH != bd.Height || g_resolveFmt != bd.Format) {
            Release9(&g_resolveSurf);
            if (!EnsureSurface9(dev, &g_resolveSurf, NULL, bd.Width, bd.Height, bd.Format)) { IDirect3DSurface9_Release(bb); return false; }
            g_resolveW = bd.Width; g_resolveH = bd.Height; g_resolveFmt = bd.Format;
            Log("Backbuffer is multisampled (%d) - resolving through %ux%u surface", (int)bd.MultiSampleType, bd.Width, bd.Height);
        }
        hr = IDirect3DDevice9_StretchRect(dev, bb, NULL, g_resolveSurf, NULL, D3DTEXF_NONE);
        if (FAILED(hr)) { IDirect3DSurface9_Release(bb); DisplayFail("StretchRect (MSAA resolve)", hr); return false; }
        source = g_resolveSurf;
    }

    /* Make sure this slot's surfaces are big enough. */
    if (slot->w < slot->dw || slot->h < slot->dh) {
        /* Only reallocate when the frame got bigger - and only at the start of a frame, so an
           eye captured earlier this frame is never thrown away. */
        if (eye != 0) { IDirect3DSurface9_Release(bb); return false; }
        UINT w = slot->dw > slot->w ? slot->dw : slot->w, h = slot->dh > slot->h ? slot->dh : slot->h;
        for (int e = 0; e < 2; e++) { Release9(&slot->rt[e]); Release9(&slot->sys[e]); }
        slot->w = w; slot->h = h;
        if (slot == &g_slots[NSLOTS - 1] && eye == 0) LogMem("capture surfaces resized");
        if (slot == &g_slots[0] && eye == 0)
            Log("Capture surfaces %ux%u (game image %ldx%ld -> %ux%u at eye-texture offset %u,%u), %s copy",
                w, h, slot->src.right - slot->src.left, slot->src.bottom - slot->src.top,
                slot->dw, slot->dh, slot->dx, slot->dy, g_asyncCapture ? "background" : "blocking");
    }
    if (!slot->rt[eye] && !EnsureSurface9(dev, &slot->rt[eye], &slot->sys[eye], slot->w, slot->h, fmt)) {
        IDirect3DSurface9_Release(bb); return false;
    }

    RECT dst = { 0, 0, (LONG)slot->dw, (LONG)slot->dh };
    hr = IDirect3DDevice9_StretchRect(dev, source, &slot->src, slot->rt[eye], &dst, D3DTEXF_LINEAR);
    IDirect3DSurface9_Release(bb);
    if (FAILED(hr)) { DisplayFail("StretchRect (scale)", hr); return false; }
    hr = IDirect3DDevice9_GetRenderTargetData(dev, slot->rt[eye], slot->sys[eye]);
    if (FAILED(hr)) { DisplayFail("GetRenderTargetData", hr); return false; }
    slot->eyes = eye + 1;
    return true;
}

/* All images for this frame have been captured: start the background copy and send the newest
   finished frame to the headset. */
static void Capture_FinishAndSubmit(IDirect3DDevice9 *dev)
{
    CapSlot *slot = g_fillSlot;
    g_fillSlot = NULL;
    if (slot && slot->filling) {
        slot->filling = false;
        if (slot->eyes > 0) {
            if (!slot->query) {
                HRESULT hr = IDirect3DDevice9_CreateQuery(dev, D3DQUERYTYPE_EVENT, &slot->query);
                if (FAILED(hr)) {
                    slot->query = NULL;
                    if (g_asyncCapture) { Log("Event queries unavailable (hr=0x%08lX) - using blocking copy", (unsigned long)hr); g_asyncCapture = false; }
                }
            }
            if (slot->query) IDirect3DQuery9_Issue(slot->query, D3DISSUE_END);
            slot->seq = ++g_capSeq;
            slot->pending = true;
        }
    }
    Capture_Collect(!g_asyncCapture);
    Display_Submit();
}

/* Called from Present. In stereo the eyes were already captured inside the render hook. */
static bool g_stereoCapturedThisFrame = false;

/* ---- Working scopes: the drawing half -----------------------------------------
   When your eye is behind a scoped gun's scope, a magnified picture of what the barrel points at is
   drawn inside the scope, with a crosshair. Worked out while the frame is drawn (Scope_Prepare),
   painted onto the finished frame just before it goes to the headset. */
typedef struct {
    bool  active;
    float cx, cy, r;          /* scope circle on the game image (pixels) */
    float sx, sy;             /* aim point on the game image (pixels) */
    float zoom;
} ScopeView;
static ScopeView g_scope;
static IDirect3DTexture9 *g_scopeTex = NULL;
static IDirect3DStateBlock9 *g_scopeState = NULL;
#define SCOPE_TEX 512

typedef struct { float x, y, z, rhw; float u, v; } VtxT;
typedef struct { float x, y, z, rhw; DWORD c; } VtxC;

static void Cap_Release(void);
static void Scope_Release(void)
{
    Cap_Release();
    if (g_scopeTex) { IDirect3DTexture9_Release(g_scopeTex); g_scopeTex = NULL; }
    if (g_scopeState) { IDirect3DStateBlock9_Release(g_scopeState); g_scopeState = NULL; }
}

static void QuadC(VtxC *v, float x0, float y0, float x1, float y1, DWORD c)
{
    VtxC q[6] = { { x0, y0, 0, 1, c }, { x1, y0, 0, 1, c }, { x0, y1, 0, 1, c },
                  { x1, y0, 0, 1, c }, { x1, y1, 0, 1, c }, { x0, y1, 0, 1, c } };
    memcpy(v, q, sizeof q);
}

/* ---- A clean picture of the world for the scope ------------------------------------------
   The game draws the world first and the first-person gun and arms afterwards, clearing the depth
   buffer in between. At that depth clear (only while you're sighting) FNVR copies a square of the
   picture around the aim point - the world WITHOUT the gun in it. That square is what gets magnified
   inside the lens, so the gun stays in your hands and the scope shows what's really behind it. */
#define CAP_SLOTS 4
#define CAP_SIZE  512
typedef struct { IDirect3DTexture9 *tex; D3DFORMAT fmt; int ox, oy; uintptr_t site; } WorldGrab;
static WorldGrab g_cap[CAP_SLOTS];
static int   g_capCount = 0;          /* world pictures grabbed during this frame's drawing */
static bool  g_capWant = false;       /* set before drawing when you're sighting through the scope */
static float g_capSX = 0, g_capSY = 0;
static bool  g_inRenderFrame = false;
static int   g_capLogClears = 0;      /* log the next N depth clears (diagnostics) */
static bool  g_capSnapWanted = false;
static int   g_capSnaps = 0;
typedef HRESULT (STDMETHODCALLTYPE *DevClear_t)(IDirect3DDevice9 *, DWORD, const D3DRECT *, DWORD, D3DCOLOR, float, DWORD);
static DevClear_t o_DevClear = NULL;
static IDirect3DSurface9 *g_capResolve = NULL;

static void Cap_Release(void)
{
    for (int i = 0; i < CAP_SLOTS; i++) if (g_cap[i].tex) { IDirect3DTexture9_Release(g_cap[i].tex); g_cap[i].tex = NULL; }
    if (g_capResolve) { IDirect3DSurface9_Release(g_capResolve); g_capResolve = NULL; }
    g_capCount = 0;
}

/* Which part of the game's frame drawing we're in: the return address into DoRenderFrame on the stack. */
static uintptr_t Cap_RenderSite(void)
{
    uintptr_t *sp = (uintptr_t *)__builtin_frame_address(0);
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    uintptr_t *top = (uintptr_t *)tib->StackBase;
    for (int i = 0; i < 8192 && sp + i < top; i++) { uintptr_t v = sp[i]; if (v >= 0x008706B0 && v < 0x00870D00) return v; }
    return 0;
}

static void Cap_OnClear(IDirect3DDevice9 *dev, DWORD flags)
{
    IDirect3DSurface9 *rt = NULL, *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetRenderTarget(dev, 0, &rt)) || !rt) return;
    D3DSURFACE_DESC d; IDirect3DSurface9_GetDesc(rt, &d);
    if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) IDirect3DSurface9_Release(bb);
    bool isBB = rt == bb;
    uintptr_t site = Cap_RenderSite();
    bool mainSize = d.Width >= CAP_SIZE && d.Height >= CAP_SIZE && d.Width * 10 >= g_bbW * 9 && d.Height * 10 >= g_bbH * 9;
    int slot = -1; HRESULT hr = S_OK;
    if (!(flags & D3DCLEAR_TARGET) && mainSize && g_capCount < CAP_SLOTS) {
        WorldGrab *cs = &g_cap[g_capCount];
        if (cs->tex && cs->fmt != d.Format) { IDirect3DTexture9_Release(cs->tex); cs->tex = NULL; }
        if (!cs->tex) {
            hr = IDirect3DDevice9_CreateTexture(dev, CAP_SIZE, CAP_SIZE, 1, D3DUSAGE_RENDERTARGET, d.Format, D3DPOOL_DEFAULT, &cs->tex, NULL);
            if (FAILED(hr)) cs->tex = NULL;
            cs->fmt = d.Format;
        }
        if (cs->tex) {
            /* scale the aim point if this picture isn't exactly the back buffer's size */
            float sx = g_capSX * d.Width / (float)g_bbW, sy = g_capSY * d.Height / (float)g_bbH;
            int ox = (int)sx - CAP_SIZE / 2, oy = (int)sy - CAP_SIZE / 2;
            if (ox < 0) ox = 0; if (oy < 0) oy = 0;
            if (ox > (int)d.Width - CAP_SIZE) ox = (int)d.Width - CAP_SIZE;
            if (oy > (int)d.Height - CAP_SIZE) oy = (int)d.Height - CAP_SIZE;
            RECT src = { ox, oy, ox + CAP_SIZE, oy + CAP_SIZE };
            IDirect3DSurface9 *ts = NULL;
            IDirect3DTexture9_GetSurfaceLevel(cs->tex, 0, &ts);
            hr = ts ? IDirect3DDevice9_StretchRect(dev, rt, &src, ts, NULL, D3DTEXF_NONE) : E_FAIL;
            if (FAILED(hr) && ts) {
                /* anti-aliased picture: resolve the whole frame first, then copy the square from that */
                D3DSURFACE_DESC rd;
                if (g_capResolve) { IDirect3DSurface9_GetDesc(g_capResolve, &rd); if (rd.Width != d.Width || rd.Height != d.Height || rd.Format != d.Format) { IDirect3DSurface9_Release(g_capResolve); g_capResolve = NULL; } }
                if (!g_capResolve && FAILED(IDirect3DDevice9_CreateRenderTarget(dev, d.Width, d.Height, d.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &g_capResolve, NULL))) g_capResolve = NULL;
                if (g_capResolve && SUCCEEDED(hr = IDirect3DDevice9_StretchRect(dev, rt, NULL, g_capResolve, NULL, D3DTEXF_NONE)))
                    hr = IDirect3DDevice9_StretchRect(dev, g_capResolve, &src, ts, NULL, D3DTEXF_NONE);
            }
            if (ts) IDirect3DSurface9_Release(ts);
            if (SUCCEEDED(hr)) {
                cs->ox = (int)(ox * (float)g_bbW / d.Width); cs->oy = (int)(oy * (float)g_bbH / d.Height);
                cs->site = site; slot = g_capCount++;
            }
        }
    }
    if (g_capLogClears > 0) {
        g_capLogClears--;
        Log("Scope capture: depth clear (flags %lX) on %ux%u format %u msaa %u%s, from %08X -> %s (0x%08lX)",
            (unsigned long)flags, d.Width, d.Height, (unsigned)d.Format, (unsigned)d.MultiSampleType, isBB ? " [back buffer]" : "",
            (unsigned)site, slot >= 0 ? "grabbed" : "skipped", (unsigned long)hr);
    }
    IDirect3DSurface9_Release(rt);
}

static int g_trOn2(void);
static HRESULT STDMETHODCALLTYPE Hook_DevClear(IDirect3DDevice9 *dev, DWORD n, const D3DRECT *rects, DWORD flags, D3DCOLOR c, float z, DWORD s)
{
    if (g_trOn2()) Log("TRACE   CLEAR flags %lx colour %08lx rects %lu", (unsigned long)flags, (unsigned long)c, (unsigned long)n);
    if (g_capWant && g_inRenderFrame && (flags & D3DCLEAR_ZBUFFER) && g_bbW) Cap_OnClear(dev, flags);
    return o_DevClear(dev, n, rects, flags, c, z, s);
}

static float HalfToF(uint16_t h)
{
    unsigned e = (h >> 10) & 31, m = h & 1023;
    float v = e == 0 ? ldexpf((float)m, -24) : e == 31 ? 65504.0f : ldexpf((float)(m | 1024), (int)e - 25);
    return (h & 0x8000) ? -v : v;
}

/* Diagnostics: save the grabbed world pictures once, so the result can be checked from the files. */
static void Cap_Snapshot(IDirect3DDevice9 *dev)
{
    for (int i = 0; i < g_capCount; i++) {
        WorldGrab *cs = &g_cap[i];
        IDirect3DSurface9 *ts = NULL, *sys = NULL;
        IDirect3DTexture9_GetSurfaceLevel(cs->tex, 0, &ts);
        if (!ts) continue;
        if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, CAP_SIZE, CAP_SIZE, cs->fmt, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
            SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, ts, sys))) {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
                BYTE *out = (BYTE *)malloc(CAP_SIZE * CAP_SIZE * 4);
                bool ok = true;
                for (int y = 0; y < CAP_SIZE && ok; y++) {
                    const BYTE *row = (const BYTE *)lr.pBits + (size_t)y * lr.Pitch;
                    for (int x = 0; x < CAP_SIZE; x++) {
                        BYTE *o = out + (y * CAP_SIZE + x) * 4;
                        if (cs->fmt == D3DFMT_A8R8G8B8 || cs->fmt == D3DFMT_X8R8G8B8) memcpy(o, row + x * 4, 4);
                        else if (cs->fmt == D3DFMT_A16B16G16R16F) {
                            const uint16_t *p = (const uint16_t *)(row + x * 8);
                            for (int k = 0; k < 3; k++) { float v = HalfToF(p[2 - k]); o[k] = (BYTE)(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f); }
                            o[3] = 255;
                        } else if (cs->fmt == D3DFMT_A2R10G10B10) {
                            DWORD p = *(const DWORD *)(row + x * 4);
                            o[0] = (BYTE)((p >> 2) & 255); o[1] = (BYTE)((p >> 12) & 255); o[2] = (BYTE)((p >> 22) & 255); o[3] = 255;
                        } else ok = false;
                    }
                }
                if (ok) { char name[64]; snprintf(name, sizeof name, "FNVR_scopecap_%d.bmp", i); SaveBMP(name, out, CAP_SIZE * 4, CAP_SIZE, CAP_SIZE); }
                else Log("Scope capture: picture format %u can't be saved as a snapshot", (unsigned)cs->fmt);
                free(out);
                IDirect3DSurface9_UnlockRect(sys);
            }
        }
        if (sys) IDirect3DSurface9_Release(sys);
        IDirect3DSurface9_Release(ts);
    }
}

static void Scope_Draw(IDirect3DDevice9 *dev)
{
    if (!g_scope.active || !g_scopesEnabled || !g_bbW) return;
    g_scope.active = false;
    int pick = (g_capPick >= 0 && g_capPick < g_capCount) ? g_capPick : g_capCount - 1;
    static int noCapLogs = 0;
    if (pick < 0) { if (noCapLogs++ < 5) Log("Scope: sighting, but no clean world picture was grabbed this frame - nothing drawn"); return; }
    WorldGrab *cs = &g_cap[pick];
    if (g_capSnapWanted) { g_capSnapWanted = false; Cap_Snapshot(dev); }
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    /* the magnified part: a circle 1/zoom the size of the lens, around the aim point */
    float half = fminf(g_scope.r / g_scope.zoom, CAP_SIZE * 0.5f - 1.0f);
    float uc = (g_scope.sx - cs->ox) / CAP_SIZE, vc = (g_scope.sy - cs->oy) / CAP_SIZE, ur = half / CAP_SIZE;

    /* Save exactly what we change, and put it back afterwards (a state block didn't restore
       everything through ENBoost's wrapper, which smeared the world's textures). */
    static const D3DRENDERSTATETYPE rsList[] = { D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
        D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_CULLMODE, D3DRS_LIGHTING, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_COLORWRITEENABLE };
    enum { NRS = sizeof rsList / sizeof rsList[0] };
    DWORD rsSave[NRS];
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_GetRenderState(dev, rsList[i], &rsSave[i]);
    static const D3DSAMPLERSTATETYPE ssList[] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE };
    enum { NSS = sizeof ssList / sizeof ssList[0] };
    DWORD ssSave[NSS];
    for (int i = 0; i < NSS; i++) IDirect3DDevice9_GetSamplerState(dev, 0, ssList[i], &ssSave[i]);
    DWORD ts0op, ts0a1, ts0a2, ts0aop, ts1op, tfSave;
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLOROP, &ts0op);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG1, &ts0a1);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG2, &ts0a2);
    IDirect3DDevice9_GetRenderState(dev, D3DRS_TEXTUREFACTOR, &tfSave);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAOP, &ts0aop);
    IDirect3DDevice9_GetTextureStageState(dev, 1, D3DTSS_COLOROP, &ts1op);
    IDirect3DSurface9 *rtSave = NULL; IDirect3DDevice9_GetRenderTarget(dev, 0, &rtSave);
    D3DVIEWPORT9 vpSave; IDirect3DDevice9_GetViewport(dev, &vpSave);
    IDirect3DVertexShader9 *vsSave = NULL; IDirect3DDevice9_GetVertexShader(dev, &vsSave);
    IDirect3DPixelShader9 *psSave = NULL; IDirect3DDevice9_GetPixelShader(dev, &psSave);
    IDirect3DVertexDeclaration9 *declSave = NULL; IDirect3DDevice9_GetVertexDeclaration(dev, &declSave);
    DWORD fvfSave = 0; IDirect3DDevice9_GetFVF(dev, &fvfSave);
    IDirect3DBaseTexture9 *texSave = NULL; IDirect3DDevice9_GetTexture(dev, 0, &texSave);
    IDirect3DVertexBuffer9 *vbSave = NULL; UINT vbOff = 0, vbStride = 0; IDirect3DDevice9_GetStreamSource(dev, 0, &vbSave, &vbOff, &vbStride);

    IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
    D3DVIEWPORT9 vp = { 0, 0, g_bbW, g_bbH, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(dev, &vp);
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        enum { SEG = 64 };
        float cx = g_scope.cx, cy = g_scope.cy, r = g_scope.r;
        /* the magnified view */
        VtxT fan[SEG + 2];
        fan[0] = (VtxT){ cx, cy, 0, 1, uc, vc };
        for (int i = 0; i <= SEG; i++) {
            float a = 2.0f * PI_F * i / SEG, ca = cosf(a), sa = sinf(a);
            fan[i + 1] = (VtxT){ cx + ca * r, cy + sa * r, 0, 1, uc + ur * ca, vc + ur * sa };
        }
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)cs->tex);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        {   /* brightness: 1 = as grabbed */
            float b = g_scopeBright; DWORD op = D3DTOP_SELECTARG1;
            if (fabsf(b - 1.0f) > 0.01f) { op = b <= 1.0f ? D3DTOP_MODULATE : b <= 2.0f ? D3DTOP_MODULATE2X : D3DTOP_MODULATE4X; b /= b <= 1.0f ? 1.0f : b <= 2.0f ? 2.0f : 4.0f; }
            DWORD k = (DWORD)(fminf(b, 1.0f) * 255.0f);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, 0xFF000000 | k << 16 | k << 8 | k);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, op);
        }
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, SEG, fan, sizeof(VtxT));

        /* black tube edge and crosshair */
        IDirect3DDevice9_SetTexture(dev, 0, NULL);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        const DWORD black = 0xFF000000;
        VtxC ring[(SEG + 1) * 2];
        float r2 = r * 1.04f;
        for (int i = 0; i <= SEG; i++) {
            float a = 2.0f * PI_F * i / SEG, ca = cosf(a), sa = sinf(a);
            ring[2 * i]     = (VtxC){ cx + ca * r, cy + sa * r, 0, 1, black };
            ring[2 * i + 1] = (VtxC){ cx + ca * r2, cy + sa * r2, 0, 1, black };
        }
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, SEG * 2, ring, sizeof(VtxC));
        float t = fmaxf(r * 0.006f, 1.0f), T = fmaxf(r * 0.025f, 2.0f), gap = r * 0.55f;
        VtxC q[6 * 6];
        QuadC(q +  0, cx - r, cy - t, cx + r, cy + t, black);                 /* thin horizontal */
        QuadC(q +  6, cx - t, cy - r, cx + t, cy + r, black);                 /* thin vertical */
        QuadC(q + 12, cx - r, cy - T, cx - gap, cy + T, black);               /* thick posts */
        QuadC(q + 18, cx + gap, cy - T, cx + r, cy + T, black);
        QuadC(q + 24, cx - T, cy + gap, cx + T, cy + r, black);
        QuadC(q + 30, cx - T, cy - r, cx + T, cy - gap, black);
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, 12, q, sizeof(VtxC));
        IDirect3DDevice9_EndScene(dev);
    }
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_SetRenderState(dev, rsList[i], rsSave[i]);
    for (int i = 0; i < NSS; i++) IDirect3DDevice9_SetSamplerState(dev, 0, ssList[i], ssSave[i]);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, ts0op);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, ts0a1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, ts0a2);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, tfSave);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, ts0aop);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, ts1op);
    if (rtSave) { IDirect3DDevice9_SetRenderTarget(dev, 0, rtSave); IDirect3DSurface9_Release(rtSave); }
    IDirect3DDevice9_SetViewport(dev, &vpSave);
    IDirect3DDevice9_SetVertexShader(dev, vsSave); if (vsSave) IDirect3DVertexShader9_Release(vsSave);
    IDirect3DDevice9_SetPixelShader(dev, psSave); if (psSave) IDirect3DPixelShader9_Release(psSave);
    if (declSave) { IDirect3DDevice9_SetVertexDeclaration(dev, declSave); IDirect3DVertexDeclaration9_Release(declSave); }
    else if (fvfSave) IDirect3DDevice9_SetFVF(dev, fvfSave);
    IDirect3DDevice9_SetTexture(dev, 0, texSave); if (texSave) IDirect3DBaseTexture9_Release(texSave);
    IDirect3DDevice9_SetStreamSource(dev, 0, vbSave, vbOff, vbStride); if (vbSave) IDirect3DVertexBuffer9_Release(vbSave);
    IDirect3DSurface9_Release(bb);
    static int logs = 0;
    if (logs++ < 3) Log("Scope: drawn at (%.0f, %.0f) radius %.0f px, %.1fx zoom of (%.0f, %.0f), world picture %d of %d (from %08X)",
                        g_scope.cx, g_scope.cy, g_scope.r, g_scope.zoom, g_scope.sx, g_scope.sy, pick + 1, g_capCount, (unsigned)cs->site);
}

/* Diagnostics: one snapshot of the game image after aiming for a second (to see the scope). */
static void Scope_Snapshot(IDirect3DDevice9 *dev, bool scopeShown)
{
    static DWORD adsSince = 0; static int taken = 0;
    if (!scopeShown && !g_adsActive) { adsSince = 0; return; }
    if (!adsSince) adsSince = GetTickCount();
    if (taken >= 2 || GetTickCount() - adsSince < 1200) return;
    taken++; adsSince = GetTickCount() + 600000;
    IDirect3DSurface9 *bb = NULL, *sys = NULL;
    D3DSURFACE_DESC bd;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    IDirect3DSurface9_GetDesc(bb, &bd);
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, bd.Width, bd.Height, bd.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, bb, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            char name[64]; snprintf(name, sizeof name, "FNVR_scope_%d.bmp", taken);
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, bd.Width, bd.Height);
            IDirect3DSurface9_UnlockRect(sys);
        }
    } else Log("Scope snapshot failed");
    if (sys) IDirect3DSurface9_Release(sys);
    IDirect3DSurface9_Release(bb);
}

static void Pip_Snapshot(IDirect3DDevice9 *dev)
{
    static double since = 0; static int taken = 0; static int lastMode = -1;
    if (taken >= 2) return;
    int m = g_pipModeRaw;
    if (m != lastMode) { since = QpcMs(); lastMode = m; }
    if (m != 3 || QpcMs() - since < (taken == 0 ? 700.0 : 2000.0)) return;
    IDirect3DSurface9 *bb = NULL, *sys = NULL, *rs = NULL; D3DSURFACE_DESC bd;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    IDirect3DSurface9_GetDesc(bb, &bd);
    IDirect3DSurface9 *src = bb;
    if (bd.MultiSampleType != D3DMULTISAMPLE_NONE &&
        SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, bd.Width, bd.Height, bd.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &rs, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, rs, NULL, D3DTEXF_NONE))) src = rs;
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, bd.Width, bd.Height, bd.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, src, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            char name[64]; snprintf(name, sizeof name, "FNVR_pipboy_%s.bmp", taken == 0 ? "before_realtime" : "with_realtime");
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, bd.Width, bd.Height);
            IDirect3DSurface9_UnlockRect(sys);
        }
    } else Log("Pip-Boy snapshot failed");
    taken++;
    if (sys) IDirect3DSurface9_Release(sys);
    if (rs) IDirect3DSurface9_Release(rs);
    IDirect3DSurface9_Release(bb);
}


/* ---- Curved interface panel ------------------------------------------------------------
   The game draws its interface (menus, HUD, main menu, loading screens) last, through its own camera.
   That camera's viewport is set to a marker size, so the moment the game starts the interface pass FNVR
   swaps the drawing target for a see-through image of its own; everything after that lands there instead
   of on the world picture. Each frame that image is shown on a curved SteamVR panel in front of you:
   fixed in place while a menu is open, travelling with your head for the HUD. */
typedef HRESULT (STDMETHODCALLTYPE *DevSetRT_t)(IDirect3DDevice9 *, DWORD, IDirect3DSurface9 *);
typedef HRESULT (STDMETHODCALLTYPE *DevSetDS_t)(IDirect3DDevice9 *, IDirect3DSurface9 *);
typedef HRESULT (STDMETHODCALLTYPE *DevSetVP_t)(IDirect3DDevice9 *, const D3DVIEWPORT9 *);
typedef HRESULT (STDMETHODCALLTYPE *DevSetRS_t)(IDirect3DDevice9 *, D3DRENDERSTATETYPE, DWORD);
static DevSetRT_t o_SetRT = NULL; static DevSetDS_t o_SetDS = NULL; static DevSetVP_t o_SetVP = NULL; static DevSetRS_t o_SetRS = NULL;
static IDirect3DTexture9 *g_uiTex = NULL; static IDirect3DSurface9 *g_uiSurf = NULL, *g_uiDepth = NULL, *g_uiSmall = NULL, *g_uiSys = NULL;
static IDirect3DSurface9 *g_curRT0 = NULL, *g_bbPtr = NULL, *g_uiSavedDS = NULL;
static bool g_uiPhase = false, g_uiDrewThisFrame = false;
static UINT g_uiW = 0, g_uiH = 0;
#define UI_OUT_W 1920
#define UI_OUT_H 1080
static ID3D11Texture2D *g_uiTex11 = NULL;

static void UI_Release9(void)
{
    if (g_uiSurf) { IDirect3DSurface9_Release(g_uiSurf); g_uiSurf = NULL; }
    if (g_uiTex) { IDirect3DTexture9_Release(g_uiTex); g_uiTex = NULL; }
    if (g_uiDepth) { IDirect3DSurface9_Release(g_uiDepth); g_uiDepth = NULL; }
    if (g_uiSmall) { IDirect3DSurface9_Release(g_uiSmall); g_uiSmall = NULL; }
    if (g_uiSys) { IDirect3DSurface9_Release(g_uiSys); g_uiSys = NULL; }
    g_uiW = g_uiH = 0; g_uiPhase = false; g_bbPtr = NULL; g_curRT0 = NULL;
}

static bool UI_Ensure(IDirect3DDevice9 *dev)
{
    IDirect3DSurface9 *bb = NULL; D3DSURFACE_DESC bd;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    IDirect3DSurface9_GetDesc(bb, &bd); g_bbPtr = bb; IDirect3DSurface9_Release(bb);
    if (g_uiTex && g_uiW == bd.Width && g_uiH == bd.Height) return true;
    UI_Release9(); g_bbPtr = bb;
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, bd.Width, bd.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_uiTex, NULL))) { g_uiTex = NULL; return false; }
    IDirect3DTexture9_GetSurfaceLevel(g_uiTex, 0, &g_uiSurf);
    IDirect3DDevice9_CreateDepthStencilSurface(dev, bd.Width, bd.Height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &g_uiDepth, NULL);
    IDirect3DDevice9_CreateRenderTarget(dev, UI_OUT_W, UI_OUT_H, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_uiSmall, NULL);
    IDirect3DDevice9_CreateOffscreenPlainSurface(dev, UI_OUT_W, UI_OUT_H, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &g_uiSys, NULL);
    g_uiW = bd.Width; g_uiH = bd.Height;
    Log("Curved interface: drawing surfaces %ux%u ready", g_uiW, g_uiH);
    return g_uiSurf && g_uiSmall && g_uiSys;
}

static void UI_BeginPhase(IDirect3DDevice9 *dev)
{
    if (!UI_Ensure(dev)) return;
    g_uiSavedDS = NULL;
    IDirect3DDevice9_GetDepthStencilSurface(dev, &g_uiSavedDS);
    o_SetRT(dev, 0, g_uiSurf);
    o_SetDS(dev, g_uiDepth);
    if (!g_uiDrewThisFrame) IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET | (g_uiDepth ? D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL : 0), 0x00000000, 1.0f, 0);
    /* keep the picture's coverage in the alpha channel */
    o_SetRS(dev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    o_SetRS(dev, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    o_SetRS(dev, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    g_uiPhase = true; g_uiDrewThisFrame = true;
    static int bl = 0; if (bl++ < 2) Log("Curved interface: interface pass started - drawing to the panel");
}

static void UI_EndPhase(IDirect3DDevice9 *dev)
{
    if (!g_uiPhase) return;
    g_uiPhase = false;
    o_SetRS(dev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    if (g_bbPtr) o_SetRT(dev, 0, g_bbPtr);
    o_SetDS(dev, g_uiSavedDS);
    if (g_uiSavedDS) { IDirect3DSurface9_Release(g_uiSavedDS); g_uiSavedDS = NULL; }
}

/* ---- One-frame drawing trace (Pip-Boy) ----------------------------------------------------------
   Once per session for each mode (world paused / running), a moment after the Pip-Boy opens, every step the
   game takes to draw one frame is written to the log: which picture it draws into, which pictures it draws
   with, the viewport, clears, and how many things it draws in between - plus screenshots after the Pip-Boy
   pass and at the end. This shows exactly how the game puts the Pip-Boy menu on screen. */
static int g_trOn = 0, g_trLines = 0;
static int g_trOn2(void); static unsigned g_trDraws = 0; static const char *g_trMode = "";
static int g_trOn2(void) { return g_trOn; }
static void Tr_Flush(void) { if (g_trOn && g_trDraws) { Log("TRACE     ... %u draws", g_trDraws); g_trDraws = 0; } }
static void Tr_Surf(const char *what, IDirect3DSurface9 *sf)
{
    if (!g_trOn || g_trLines > 600) return; Tr_Flush(); g_trLines++;
    if (!sf) { Log("TRACE %s: none", what); return; }
    D3DSURFACE_DESC d; if (FAILED(IDirect3DSurface9_GetDesc(sf, &d))) { Log("TRACE %s: %p ?", what, (void *)sf); return; }
    Log("TRACE %s: %p %ux%u fmt %d usage %lx pool %d ms %d", what, (void *)sf, d.Width, d.Height, (int)d.Format, (unsigned long)d.Usage, (int)d.Pool, (int)d.MultiSampleType);
}
/* Every picture the game draws into while the Pip-Boy is open (the menu's own screen texture is one of them). */
static IDirect3DSurface9 *g_pipRTs[16]; static int g_pipRTn = 0; static bool g_pipRTWatch = false;
/* Pip-Boy menu capture: while the game is drawing its Pip-Boy menu, every picture it draws into is noted,
   so the menu picture can be copied the moment it's finished (the game hands that picture to other jobs,
   like shadows, later in the same frame - which is why it could never be found afterwards). */
static int g_pmDepth = 0;
static unsigned g_frameId = 0;   /* counts presented frames */
static IDirect3DSurface9 *g_pmRec[8]; static int g_pmRecN = 0;
static void PM_CopySurf(IDirect3DSurface9 *sf);
static void PM_Record(IDirect3DSurface9 *sf)
{
    if (!sf || g_pmRecN >= 8) return;
    for (int i = 0; i < g_pmRecN; i++) if (g_pmRec[i] == sf) return;
    IDirect3DSurface9_AddRef(sf); g_pmRec[g_pmRecN++] = sf;
}
static HRESULT STDMETHODCALLTYPE Hook_SetRT(IDirect3DDevice9 *dev, DWORD idx, IDirect3DSurface9 *surf)
{
    if (g_trOn && idx == 0) Tr_Surf("draw INTO", surf);
    if (g_pmDepth > 0 && idx == 0 && g_curRT0 && g_curRT0 != surf) PM_CopySurf(g_curRT0);   /* the game just finished drawing into it */
    if (idx == 0) g_curRT0 = surf;
    if (g_pmDepth > 0 && idx == 0 && surf) PM_Record(surf);
    if (g_pipRTWatch && idx == 0 && surf && surf != g_bbPtr && surf != g_uiSurf) {
        int i = 0; for (; i < g_pipRTn; i++) if (g_pipRTs[i] == surf) break;
        if (i == g_pipRTn && g_pipRTn < 16) { IDirect3DSurface9_AddRef(surf); g_pipRTs[g_pipRTn++] = surf; }
    }
    if (g_uiPhase && idx == 0 && surf && surf == g_bbPtr) surf = g_uiSurf;
    return o_SetRT(dev, idx, surf);
}
/* Pictures the game draws WITH while drawing the Pip-Boy: the menu's picture is one of them (bound as a texture
   when the Pip-Boy screen is drawn), so each one is copied at that moment. */
typedef HRESULT (STDMETHODCALLTYPE *DevSetTex_t)(IDirect3DDevice9 *, DWORD, IDirect3DBaseTexture9 *);
static DevSetTex_t o_SetTex = NULL;
static HRESULT STDMETHODCALLTYPE Hook_SetTex(IDirect3DDevice9 *dev, DWORD st, IDirect3DBaseTexture9 *t)
{
    if (g_trOn && st < 4 && g_trLines <= 600) {
        Tr_Flush(); g_trLines++;
        if (!t) Log("TRACE   tex%lu: none", (unsigned long)st);
        else if (IDirect3DBaseTexture9_GetType(t) == D3DRTYPE_TEXTURE) {
            D3DSURFACE_DESC d; IDirect3DTexture9_GetLevelDesc((IDirect3DTexture9 *)t, 0, &d);
            Log("TRACE   tex%lu: %p %ux%u fmt %d %s", (unsigned long)st, (void *)t, d.Width, d.Height, (int)d.Format, (d.Usage & D3DUSAGE_RENDERTARGET) ? "RENDER-TARGET" : "");
        } else Log("TRACE   tex%lu: %p type %d", (unsigned long)st, (void *)t, (int)IDirect3DBaseTexture9_GetType(t));
    }
    if (g_pmDepth > 0 && t && st < 8 && IDirect3DBaseTexture9_GetType(t) == D3DRTYPE_TEXTURE) {
        IDirect3DTexture9 *tx = (IDirect3DTexture9 *)t; D3DSURFACE_DESC d;
        if (SUCCEEDED(IDirect3DTexture9_GetLevelDesc(tx, 0, &d)) && (d.Usage & D3DUSAGE_RENDERTARGET)) {
            IDirect3DSurface9 *sf = NULL;
            if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(tx, 0, &sf)) && sf) {
                static int nl = 0; if (nl++ < 12) Log("Pip-Boy menu capture: picture %ux%u (format %d) used as a texture (stage %lu) while the Pip-Boy draws", d.Width, d.Height, (int)d.Format, (unsigned long)st);
                PM_CopySurf(sf); IDirect3DSurface9_Release(sf);
            }
        }
    }
    return o_SetTex(dev, st, t);
}
/* The game clips menu lists (dialogue answers, trade lists...) with screen rectangles worked out for a
   full-size interface; with the interface drawn smaller, those rectangles are moved and shrunk to match. */
typedef HRESULT (STDMETHODCALLTYPE *DevSetScissor_t)(IDirect3DDevice9 *, const RECT *);
static DevSetScissor_t o_SetScissor = NULL;
static HRESULT STDMETHODCALLTYPE Hook_SetScissor(IDirect3DDevice9 *dev, const RECT *r)
{
    bool onScreen = false;
    if (r && g_uiXfOn && g_bbW && g_bbH && !g_pipOpen && g_pmDepth == 0) {   /* (never the Pip-Boy's own menu picture) */
        IDirect3DSurface9 *cur = NULL, *bb = NULL;
        IDirect3DDevice9_GetRenderTarget(dev, 0, &cur); IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        onScreen = cur && cur == bb;
        if (cur) IDirect3DSurface9_Release(cur); if (bb) IDirect3DSurface9_Release(bb);
    }
    if (onScreen) {
        const float *B = g_uiXfBase, *C = g_uiXfCur;
        if (B[1] > B[0] && C[1] > C[0] && B[2] != B[3] && C[2] != C[3]) {
            float u[2] = { (float)r->left / g_bbW, (float)r->right / g_bbW }, v[2] = { (float)r->top / g_bbH, (float)r->bottom / g_bbH };
            RECT o;
            for (int i = 0; i < 2; i++) {
                float X = B[0] + u[i] * (B[1] - B[0]); u[i] = (X - C[0]) / (C[1] - C[0]);
                float Y = B[2] - v[i] * (B[2] - B[3]); v[i] = (C[2] - Y) / (C[2] - C[3]);
            }
            o.left = (LONG)floorf(u[0] * g_bbW); o.right = (LONG)ceilf(u[1] * g_bbW); o.top = (LONG)floorf(v[0] * g_bbH); o.bottom = (LONG)ceilf(v[1] * g_bbH);
            static int nl = 0; if (nl++ < 3) Log("Interface: clip area %ld,%ld-%ld,%ld moved to %ld,%ld-%ld,%ld (smaller interface)", r->left, r->top, r->right, r->bottom, o.left, o.top, o.right, o.bottom);
            return o_SetScissor(dev, &o);
        }
    }
    return o_SetScissor(dev, r);
}
typedef HRESULT (STDMETHODCALLTYPE *DevDP_t)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT (STDMETHODCALLTYPE *DevDIP_t)(IDirect3DDevice9 *, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
static DevDP_t o_DP = NULL; static DevDIP_t o_DIP = NULL;
static HRESULT STDMETHODCALLTYPE Hook_DP(IDirect3DDevice9 *dev, D3DPRIMITIVETYPE t, UINT a, UINT b) { if (g_trOn) g_trDraws++; return o_DP(dev, t, a, b); }
static HRESULT STDMETHODCALLTYPE Hook_DIP(IDirect3DDevice9 *dev, D3DPRIMITIVETYPE t, INT a, UINT b, UINT c, UINT d2, UINT e) { if (g_trOn) g_trDraws++; return o_DIP(dev, t, a, b, c, d2, e); }
static HRESULT STDMETHODCALLTYPE Hook_SetDS(IDirect3DDevice9 *dev, IDirect3DSurface9 *surf)
{
    if (g_uiPhase && surf && surf == g_uiSavedDS) surf = g_uiDepth;
    return o_SetDS(dev, surf);
}
static HRESULT STDMETHODCALLTYPE Hook_SetVP(IDirect3DDevice9 *dev, const D3DVIEWPORT9 *vp)
{
    if (g_trOn && vp && g_trLines <= 600) { Tr_Flush(); g_trLines++; Log("TRACE   viewport %lu,%lu %lux%lu", (unsigned long)vp->X, (unsigned long)vp->Y, (unsigned long)vp->Width, (unsigned long)vp->Height); }
    if (g_curvedActive && !g_uiPhase && vp && g_bbW && g_bbH && HeadsetDisplayActive() && (g_inGameplay || !g_farMenu)) {
        /* the interface camera's marker viewport: a hair inside the full frame */
        DWORD mx = (DWORD)(0.0005f * g_bbW), my = (DWORD)(0.0005f * g_bbH);
        bool marker = vp->X >= (mx > 0 ? mx - 1 : 0) && vp->X <= mx + 1 && vp->Y >= (my > 0 ? my - 1 : 0) && vp->Y <= my + 1 &&
                      vp->Width + 2 * vp->X + 3 >= g_bbW && vp->Width + 2 * vp->X <= g_bbW + 3 && vp->X > 0;
        if (marker && (g_curRT0 == NULL || g_curRT0 == g_bbPtr || !g_bbPtr)) UI_BeginPhase(dev);
    }
    return o_SetVP(dev, vp);
}
static HRESULT STDMETHODCALLTYPE Hook_SetRS(IDirect3DDevice9 *dev, D3DRENDERSTATETYPE st, DWORD v)
{
    if (g_uiPhase) {
        if (st == D3DRS_COLORWRITEENABLE && (v & 7)) v |= D3DCOLORWRITEENABLE_ALPHA;   /* coverage must reach the panel */
        if (st == D3DRS_SEPARATEALPHABLENDENABLE || st == D3DRS_SRCBLENDALPHA || st == D3DRS_DESTBLENDALPHA) return S_OK;
    }
    return o_SetRS(dev, st, v);
}

/* Present: finish the interface pass and put its picture on the curved panel. */
static void UI_Present(IDirect3DDevice9 *dev)
{
    if (!g_curvedActive || !g_vrOverlay) return;
    UI_EndPhase(dev);
    bool drew = g_uiDrewThisFrame && !g_farActive && !g_setOpen; g_uiDrewThisFrame = false;
    static bool shown = false;
    if (!drew || !g_uiSurf || !g_d11) { if (shown) { g_vrOverlay->HideOverlay(g_uiOverlay); shown = false; } return; }
    if (FAILED(IDirect3DDevice9_StretchRect(dev, g_uiSurf, NULL, g_uiSmall, NULL, D3DTEXF_LINEAR))) return;
    if (FAILED(IDirect3DDevice9_GetRenderTargetData(dev, g_uiSmall, g_uiSys))) return;
    if (!g_uiTex11) {
        D3D11_TEXTURE2D_DESC td = { UI_OUT_W, UI_OUT_H, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        if (FAILED(ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_uiTex11))) { g_uiTex11 = NULL; return; }
    }
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DSurface9_LockRect(g_uiSys, &lr, NULL, D3DLOCK_READONLY))) return;
    ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_uiTex11, 0, NULL, lr.pBits, (UINT)lr.Pitch, 0);
    IDirect3DSurface9_UnlockRect(g_uiSys);
    ID3D11DeviceContext_Flush(g_d11ctx);
    Texture_t t = { g_uiTex11, ETextureType_TextureType_DirectX, EColorSpace_ColorSpace_Auto };
    g_vrOverlay->SetOverlayTexture(g_uiOverlay, &t);

    /* where: fixed in the room while a menu (or the main menu/loading) is up, with your head for the HUD */
    bool menuUp = !g_inGameplay || (IsMenuMode() && !g_pipOpen);
    static bool wasMenu = false; static HmdMatrix34_t fixedPose;
    /* main menu & loading screens: a big, gently curved screen further away; in play: the wrap-around HUD panel;
       a menu up in play: the same panel further away and smaller, fixed where you were looking */
    int layout = !g_inGameplay ? 2 : menuUp ? 3 : 1;
    static int lastLayout = 0; static float lastSz[4];
    float dist = layout == 2 ? g_mmDist : layout == 3 ? g_menuDist : g_uiDist;
    if (layout != 2 && (lastSz[0] != g_uiDist || lastSz[1] != g_uiArcDeg || lastSz[2] != g_menuDist || lastSz[3] != g_menuArcDeg)) {
        lastSz[0] = g_uiDist; lastSz[1] = g_uiArcDeg; lastSz[2] = g_menuDist; lastSz[3] = g_menuArcDeg; lastLayout = 0;   /* (changed in settings) */
    }
    if (layout != lastLayout) {
        if (layout == 2) {
            g_vrOverlay->SetOverlayWidthInMeters(g_uiOverlay, g_mmWidth);
            g_vrOverlay->SetOverlayCurvature(g_uiOverlay, g_mmWidth / (2.0f * PI_F * g_mmCurveR));
        } else {
            float arcD = layout == 3 ? g_menuArcDeg : g_uiArcDeg, arc = arcD / RAD2DEG;
            g_vrOverlay->SetOverlayWidthInMeters(g_uiOverlay, dist * arc);
            g_vrOverlay->SetOverlayCurvature(g_uiOverlay, arc / (2.0f * PI_F));
        }
        Log("Curved interface: %s layout (%.2f m away)", layout == 2 ? "main menu / loading screen" : layout == 3 ? "menu" : "HUD", dist);
        lastLayout = layout; wasMenu = false;   /* re-place it */
    }
    if (menuUp) {
        if (!wasMenu && g_vrSystem) {
            TrackedDevicePose_t hp; float yaw = 0, pitch = 0, pos[3] = { 0, 1.6f, 0 };
            g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &hp, 1);
            if (hp.bPoseIsValid) { ExtractYawPitch(&hp.mDeviceToAbsoluteTracking, &yaw, &pitch); for (int k = 0; k < 3; k++) pos[k] = hp.mDeviceToAbsoluteTracking.m[k][3]; }
            float c[3] = { pos[0] - sinf(yaw) * dist, pos[1], pos[2] - cosf(yaw) * dist };
            BuildSubmitPose(&fixedPose, yaw, 0.0f, c);
        }
        g_vrOverlay->SetOverlayTransformAbsolute(g_uiOverlay, ETrackingUniverseOrigin_TrackingUniverseStanding, &fixedPose);
    } else {
        HmdMatrix34_t m = { { { 1, 0, 0, 0 }, { 0, 1, 0, -0.02f * g_uiDist / 0.45f }, { 0, 0, 1, -g_uiDist } } };
        g_vrOverlay->SetOverlayTransformTrackedDeviceRelative(g_uiOverlay, 0, &m);
    }
    wasMenu = menuUp;
    if (!shown) { g_vrOverlay->ShowOverlay(g_uiOverlay); shown = true; Log("Curved interface: panel shown"); }
}

/* ---- Main menu, loading screens and menus: a small 3D room ---------------------------------
   While a menu is up, FNVR draws its own little 3D scene for the headset: the game's picture on a
   big, gently curved screen a few feet away, a dark floor, and a long-barrelled revolver in your
   right hand. Point the gun at the screen: the game's cursor follows the laser and highlights what
   it's on; pull the trigger to fire - a Ranger Sequoia shot, recoil and muzzle flash - and that
   clicks whatever you're pointing at (through the menu's own click handling). */
/* (ID3DBlob etc. come from d3dcommon.h via d3d11.h) */
#define MM_W 2560
#define MM_H 1440
static IDirect3DSurface9 *g_mmRT = NULL, *g_mmSys = NULL;
static ID3D11Texture2D *g_mmTex11 = NULL; static ID3D11ShaderResourceView *g_mmSRV = NULL;
static bool g_farFailed = false;
static float g_scrC[3], g_scrFwd[3], g_scrRight[3], g_scrH;   /* screen: middle, facing direction (towards it), right */


typedef struct { float p[3], n[3], c[3], uv[2]; } RVtx;
typedef struct { float mvp[16], model[16], tint[4], mode[4]; } RConst;
typedef struct { ID3D11Buffer *vb; UINT count; } RMesh;
static ID3D11Device *g_rDev = NULL;
static ID3D11VertexShader *g_rVS = NULL; static ID3D11PixelShader *g_rPS = NULL;
static ID3D11InputLayout *g_rIL = NULL; static ID3D11Buffer *g_rCB = NULL;
static ID3D11RasterizerState *g_rRS = NULL; static ID3D11DepthStencilState *g_rDSon = NULL, *g_rDSnoWrite = NULL;
static ID3D11BlendState *g_rBlendAlpha = NULL, *g_rBlendAdd = NULL; static ID3D11SamplerState *g_rSamp = NULL;
static ID3D11Texture2D *g_rMS[2], *g_rOut[2], *g_rDepth[2];
static ID3D11RenderTargetView *g_rRTV[2]; static ID3D11DepthStencilView *g_rDSV[2];
static UINT g_rW = 0, g_rH = 0;
static RMesh g_meshGun, g_meshScreen, g_meshFloor, g_meshBox, g_meshFlash;
static float g_scrBuiltW = 0, g_scrBuiltR = 0, g_scrBuiltH = 0;
static double g_fireT = -1000;                 /* when the gun last fired (ms) */
static volatile LONG g_ptrClickPending = 0;    /* trigger pulled: click in the main loop */
static bool g_ptrOnScreen = false;

/* -- little mesh builder (gun parts are boxes and cylinders: +X forward, +Y up, +Z right) -- */
static RVtx *g_mb = NULL; static UINT g_mbN = 0, g_mbCap = 0;
static void MB_Push(const float p[3], const float n[3], const float c[3], float u, float v)
{
    if (g_mbN >= g_mbCap) { g_mbCap = g_mbCap ? g_mbCap * 2 : 1024; g_mb = (RVtx *)realloc(g_mb, g_mbCap * sizeof(RVtx)); }
    RVtx *o = &g_mb[g_mbN++];
    memcpy(o->p, p, 12); memcpy(o->n, n, 12); memcpy(o->c, c, 12); o->uv[0] = u; o->uv[1] = v;
}
static void MB_Tri(const float a[3], const float b[3], const float c3[3], const float n[3], const float col[3])
{ MB_Push(a, n, col, 0, 0); MB_Push(b, n, col, 0, 0); MB_Push(c3, n, col, 0, 0); }
/* box: centre, half sizes, rotation about Z (degrees, + = front up) */
static void MB_Box(float cx, float cy, float cz, float hx, float hy, float hz, float rotZ, const float col[3])
{
    float a = rotZ / RAD2DEG, ca = cosf(a), sa = sinf(a);
    float v[8][3];
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? hx : -hx, y = (i & 2) ? hy : -hy, z = (i & 4) ? hz : -hz;
        v[i][0] = cx + x * ca - y * sa; v[i][1] = cy + x * sa + y * ca; v[i][2] = cz + z;
    }
    static const int f[6][4] = { { 1, 3, 7, 5 }, { 0, 4, 6, 2 }, { 2, 6, 7, 3 }, { 0, 1, 5, 4 }, { 4, 5, 7, 6 }, { 0, 2, 3, 1 } };
    float nn[6][3] = { { ca, sa, 0 }, { -ca, -sa, 0 }, { -sa, ca, 0 }, { sa, -ca, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int k = 0; k < 6; k++) { MB_Tri(v[f[k][0]], v[f[k][1]], v[f[k][2]], nn[k], col); MB_Tri(v[f[k][0]], v[f[k][2]], v[f[k][3]], nn[k], col); }
}
/* cylinder along X from x0 to x1 */
static void MB_CylX(float x0, float x1, float cy, float cz, float r, int seg, const float col[3])
{
    for (int i = 0; i < seg; i++) {
        float a0 = 2 * PI_F * i / seg, a1 = 2 * PI_F * (i + 1) / seg;
        float y0 = cosf(a0), z0 = sinf(a0), y1 = cosf(a1), z1 = sinf(a1);
        float p00[3] = { x0, cy + r * y0, cz + r * z0 }, p01[3] = { x1, cy + r * y0, cz + r * z0 };
        float p10[3] = { x0, cy + r * y1, cz + r * z1 }, p11[3] = { x1, cy + r * y1, cz + r * z1 };
        float n0[3] = { 0, y0, z0 }, n1[3] = { 0, y1, z1 };
        MB_Push(p00, n0, col, 0, 0); MB_Push(p01, n0, col, 0, 0); MB_Push(p11, n1, col, 0, 0);
        MB_Push(p00, n0, col, 0, 0); MB_Push(p11, n1, col, 0, 0); MB_Push(p10, n1, col, 0, 0);
        float c0[3] = { x0, cy, cz }, c1[3] = { x1, cy, cz }, nb[3] = { -1, 0, 0 }, nf[3] = { 1, 0, 0 };
        MB_Tri(c0, p10, p00, nb, col); MB_Tri(c1, p01, p11, nf, col);
    }
}
static RMesh MB_Finish(void)
{
    RMesh m = { NULL, 0 };
    D3D11_BUFFER_DESC bd = { g_mbN * sizeof(RVtx), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA sd = { g_mb, 0, 0 };
    if (g_mbN && SUCCEEDED(ID3D11Device_CreateBuffer(g_rDev, &bd, &sd, &m.vb))) m.count = g_mbN;
    g_mbN = 0;
    return m;
}
static void RMesh_Free(RMesh *m) { if (m->vb) ID3D11Buffer_Release(m->vb); m->vb = NULL; m->count = 0; }

/* the revolver: a long-barrelled hunting revolver, blued steel, silver trim, walnut grip.
   Origin = where the hand holds it (top of the grip); muzzle at (0.285, 0.030, 0). */
#define GUN_MUZZLE_X 0.285f
#define GUN_MUZZLE_Y 0.030f
static void Room_BuildGun(void)
{
    const float steel[3] = { 0.10f, 0.10f, 0.115f }, steel2[3] = { 0.16f, 0.16f, 0.18f }, silver[3] = { 0.62f, 0.62f, 0.64f };
    const float wood[3] = { 0.42f, 0.25f, 0.13f }, wood2[3] = { 0.33f, 0.19f, 0.10f };
    MB_Box(-0.030f, -0.052f, 0.0f, 0.017f, 0.052f, 0.0135f, -14.0f, wood);           /* grip */
    MB_Box(-0.042f, -0.098f, 0.0f, 0.016f, 0.007f, 0.0140f, -14.0f, steel);          /* butt cap */
    MB_Box(-0.031f, -0.050f, 0.0142f, 0.010f, 0.036f, 0.0008f, -14.0f, wood2);       /* grip panel inlays */
    MB_Box(-0.031f, -0.050f, -0.0142f, 0.010f, 0.036f, 0.0008f, -14.0f, wood2);
    MB_Box(0.002f, 0.012f, 0.0f, 0.038f, 0.020f, 0.011f, 0.0f, steel);               /* frame */
    MB_Box(0.002f, 0.012f, 0.0113f, 0.030f, 0.012f, 0.0006f, 0.0f, silver);          /* engraving panels */
    MB_Box(0.002f, 0.012f, -0.0113f, 0.030f, 0.012f, 0.0006f, 0.0f, silver);
    MB_CylX(-0.018f, 0.030f, 0.019f, 0.0f, 0.0195f, 14, steel2);                    /* cylinder */
    MB_CylX(-0.019f, -0.015f, 0.019f, 0.0f, 0.0200f, 14, silver);                   /* cylinder bands */
    MB_CylX(0.027f, 0.031f, 0.019f, 0.0f, 0.0200f, 14, silver);
    MB_Box(0.003f, 0.038f, 0.0f, 0.034f, 0.004f, 0.007f, 0.0f, steel);               /* top strap */
    MB_CylX(0.030f, GUN_MUZZLE_X, GUN_MUZZLE_Y, 0.0f, 0.0090f, 14, steel);          /* barrel */
    MB_Box(0.150f, GUN_MUZZLE_Y + 0.0095f, 0.0f, 0.125f, 0.0025f, 0.0035f, 0.0f, steel2); /* barrel rib */
    MB_CylX(0.030f, 0.140f, 0.015f, 0.0f, 0.0055f, 10, steel);                      /* ejector housing */
    MB_CylX(GUN_MUZZLE_X - 0.006f, GUN_MUZZLE_X + 0.001f, GUN_MUZZLE_Y, 0.0f, 0.0094f, 14, silver); /* muzzle crown */
    MB_Box(GUN_MUZZLE_X - 0.010f, GUN_MUZZLE_Y + 0.016f, 0.0f, 0.006f, 0.005f, 0.0015f, 0.0f, silver); /* front sight */
    MB_Box(-0.040f, 0.036f, 0.0f, 0.007f, 0.011f, 0.004f, -28.0f, steel2);           /* hammer */
    MB_Box(0.006f, -0.024f, 0.0f, 0.022f, 0.0025f, 0.0045f, 0.0f, steel);            /* trigger guard */
    MB_Box(0.027f, -0.012f, 0.0f, 0.0025f, 0.012f, 0.0045f, 0.0f, steel);
    MB_Box(0.004f, -0.011f, 0.0f, 0.002f, 0.009f, 0.003f, 12.0f, silver);            /* trigger */
    g_meshGun = MB_Finish();
}
static void Room_BuildStatic(void)
{
    const float white[3] = { 1, 1, 1 }, floorc[3] = { 0.045f, 0.045f, 0.05f }, up[3] = { 0, 1, 0 };
    /* floor: 12 m square at standing height 0, with a faint grid */
    float a[3] = { -6, 0, -6 }, b[3] = { 6, 0, -6 }, c[3] = { 6, 0, 6 }, d[3] = { -6, 0, 6 };
    MB_Tri(a, c, b, up, floorc); MB_Tri(a, d, c, up, floorc);
    const float grid[3] = { 0.075f, 0.075f, 0.085f };
    for (int i = -6; i <= 6; i++) {
        float y = 0.002f;
        float p0[3] = { (float)i - 0.006f, y, -6 }, p1[3] = { (float)i + 0.006f, y, -6 }, p2[3] = { (float)i + 0.006f, y, 6 }, p3[3] = { (float)i - 0.006f, y, 6 };
        MB_Tri(p0, p2, p1, up, grid); MB_Tri(p0, p3, p2, up, grid);
        float q0[3] = { -6, y, (float)i - 0.006f }, q1[3] = { 6, y, (float)i - 0.006f }, q2[3] = { 6, y, (float)i + 0.006f }, q3[3] = { -6, y, (float)i + 0.006f };
        MB_Tri(q0, q1, q2, up, grid); MB_Tri(q0, q2, q3, up, grid);
    }
    g_meshFloor = MB_Finish();
    MB_Box(0.5f, 0, 0, 0.5f, 0.5f, 0.5f, 0, white);   /* unit box from x=0..1 (beam) */
    g_meshBox = MB_Finish();
    /* muzzle flash: three crossed flat blades */
    for (int k = 0; k < 3; k++) {
        float ang = k * PI_F / 3, cy = cosf(ang), sz = sinf(ang);
        float p0[3] = { 0, -0.5f * cy, -0.5f * sz }, p1[3] = { 1, -0.15f * cy, -0.15f * sz }, p2[3] = { 1, 0.15f * cy, 0.15f * sz }, p3[3] = { 0, 0.5f * cy, 0.5f * sz };
        float n[3] = { 0, -sz, cy };
        MB_Tri(p0, p1, p2, n, white); MB_Tri(p0, p2, p3, n, white);
    }
    g_meshFlash = MB_Finish();
}
/* the curved screen: part of a vertical cylinder (radius R) around a point behind you */
static void Room_BuildScreen(void)
{
    float W = g_mmWidth, R = g_mmCurveR, H = W * (g_bbW && g_bbH ? (float)g_bbH / g_bbW : 9.0f / 16.0f);
    float arc = W / R; int N = 48;
    const float white[3] = { 1, 1, 1 };
    for (int i = 0; i < N; i++) {
        float t0 = -arc / 2 + arc * i / N, t1 = -arc / 2 + arc * (i + 1) / N;
        /* local: x right, y up, z towards you; screen middle at origin, centre of curve at z = +R */
        float x0 = R * sinf(t0), z0 = R - R * cosf(t0), x1 = R * sinf(t1), z1 = R - R * cosf(t1);
        float a[3] = { x0, H / 2, z0 }, b[3] = { x1, H / 2, z1 }, c[3] = { x1, -H / 2, z1 }, d[3] = { x0, -H / 2, z0 };
        float n0[3] = { -sinf(t0), 0, cosf(t0) }, n1[3] = { -sinf(t1), 0, cosf(t1) };
        float u0 = (float)i / N, u1 = (float)(i + 1) / N;
        MB_Push(a, n0, white, u0, 0); MB_Push(b, n1, white, u1, 0); MB_Push(c, n1, white, u1, 1);
        MB_Push(a, n0, white, u0, 0); MB_Push(c, n1, white, u1, 1); MB_Push(d, n0, white, u0, 1);
    }
    RMesh_Free(&g_meshScreen);
    g_meshScreen = MB_Finish();
    g_scrBuiltW = W; g_scrBuiltR = R; g_scrBuiltH = H; g_scrH = H;
}

static const char g_roomHLSL[] =
    "cbuffer C : register(b0) { row_major float4x4 mvp; row_major float4x4 model; float4 tint; float4 mode; };\n"
    "Texture2D tex : register(t0); SamplerState smp : register(s0);\n"
    "struct VI { float3 p : POSITION; float3 n : NORMAL; float3 c : COLOR; float2 uv : TEXCOORD; };\n"
    "struct VO { float4 p : SV_Position; float3 n : NORMAL; float3 c : COLOR; float2 uv : TEXCOORD; float3 w : TEXCOORD1; };\n"
    "VO vs(VI i) { VO o; o.p = mul(mvp, float4(i.p, 1)); o.n = mul((float3x3)model, i.n); o.c = i.c; o.uv = i.uv;\n"
    "  o.w = mul(model, float4(i.p, 1)).xyz; return o; }\n"
    "float4 ps(VO i) : SV_Target {\n"
    "  if (mode.x > 1.5 && mode.x < 2.5) return tint;\n"
    "  if (mode.x > 0.5 && mode.x < 1.5) return float4(tex.Sample(smp, i.uv).rgb * tint.rgb, 1);\n"
    "  float3 base = mode.x > 2.5 ? tex.Sample(smp, i.uv).rgb : i.c;\n"
    "  float3 n = normalize(i.n);\n"
    "  float key = saturate(dot(n, normalize(float3(0.3, 0.85, 0.45))));\n"
    "  float fill = saturate(dot(n, normalize(float3(-0.4, 0.3, -0.85))));\n"   /* light from the screen side */
    "  float3 v = normalize(mode.yzw - i.w); float3 h = normalize(v + normalize(float3(0.3, 0.85, 0.45)));\n"
    "  float spec = pow(saturate(dot(n, h)), 40) * 0.5;\n"
    "  return float4(base * (0.22 + 0.7 * key + 0.35 * fill) * tint.rgb + spec * tint.a, 1); }\n";

typedef HRESULT (WINAPI *D3DCompile_t)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **);
static bool Room_Init(void)
{
    if (g_rVS) return true;
    if (g_farFailed) return false;
    g_rDev = g_d11;
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
    if (!dc) dc = LoadLibraryA("d3dcompiler_43.dll");
    D3DCompile_t comp = dc ? (D3DCompile_t)(void *)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!comp) { Log("Menu room: no shader compiler (d3dcompiler_47.dll) - menus stay in the normal view"); g_farFailed = true; return false; }
    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    if (FAILED(comp(g_roomHLSL, sizeof g_roomHLSL - 1, "room", NULL, NULL, "vs", "vs_4_0", 0, 0, &vsb, &err)) ||
        FAILED(comp(g_roomHLSL, sizeof g_roomHLSL - 1, "room", NULL, NULL, "ps", "ps_4_0", 0, 0, &psb, &err))) {
        Log("Menu room: shader compile failed: %s", err ? (const char *)ID3D10Blob_GetBufferPointer(err) : "?");
        g_farFailed = true; return false;
    }
    ID3D11Device_CreateVertexShader(g_rDev, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), NULL, &g_rVS);
    ID3D11Device_CreatePixelShader(g_rDev, ID3D10Blob_GetBufferPointer(psb), ID3D10Blob_GetBufferSize(psb), NULL, &g_rPS);
    D3D11_INPUT_ELEMENT_DESC ie[4] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11Device_CreateInputLayout(g_rDev, ie, 4, ID3D10Blob_GetBufferPointer(vsb), ID3D10Blob_GetBufferSize(vsb), &g_rIL);
    ID3D10Blob_Release(vsb); ID3D10Blob_Release(psb);
    D3D11_BUFFER_DESC cb = { sizeof(RConst), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    ID3D11Device_CreateBuffer(g_rDev, &cb, NULL, &g_rCB);
    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd);
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE; rd.MultisampleEnable = TRUE;
    ID3D11Device_CreateRasterizerState(g_rDev, &rd, &g_rRS);
    D3D11_DEPTH_STENCIL_DESC dd; memset(&dd, 0, sizeof dd);
    dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ID3D11Device_CreateDepthStencilState(g_rDev, &dd, &g_rDSon);
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ID3D11Device_CreateDepthStencilState(g_rDev, &dd, &g_rDSnoWrite);
    D3D11_BLEND_DESC bl; memset(&bl, 0, sizeof bl);
    bl.RenderTarget[0].BlendEnable = TRUE; bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD; bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11Device_CreateBlendState(g_rDev, &bl, &g_rBlendAlpha);
    bl.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    ID3D11Device_CreateBlendState(g_rDev, &bl, &g_rBlendAdd);
    D3D11_SAMPLER_DESC sd; memset(&sd, 0, sizeof sd);
    sd.Filter = D3D11_FILTER_ANISOTROPIC; sd.MaxAnisotropy = 8; sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11Device_CreateSamplerState(g_rDev, &sd, &g_rSamp);
    /* eye pictures (4x multisampled, then resolved for SteamVR) */
    uint32_t w = 0, h = 0; g_vrSystem->GetRecommendedRenderTargetSize(&w, &h);
    if (!w || !h) { w = 2016; h = 2208; }
    g_rW = (UINT)(w * 0.85f); g_rH = (UINT)(h * 0.85f);
    for (int e = 0; e < 2; e++) {
        D3D11_TEXTURE2D_DESC td = { g_rW, g_rH, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, { 4, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET, 0, 0 };
        ID3D11Device_CreateTexture2D(g_rDev, &td, NULL, &g_rMS[e]);
        ID3D11Device_CreateRenderTargetView(g_rDev, (ID3D11Resource *)g_rMS[e], NULL, &g_rRTV[e]);
        td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Device_CreateTexture2D(g_rDev, &td, NULL, &g_rOut[e]);
        td.SampleDesc.Count = 4; td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ID3D11Device_CreateTexture2D(g_rDev, &td, NULL, &g_rDepth[e]);
        ID3D11Device_CreateDepthStencilView(g_rDev, (ID3D11Resource *)g_rDepth[e], NULL, &g_rDSV[e]);
    }
    if (!g_rVS || !g_rPS || !g_rIL || !g_rCB || !g_rRTV[0] || !g_rRTV[1] || !g_rOut[1] || !g_rDSV[1]) {
        Log("Menu room: couldn't create the drawing resources - menus stay in the normal view"); g_farFailed = true; return false;
    }
    Room_BuildGun(); Room_BuildStatic();
    Log("Menu room ready (%ux%u per eye, 4x MSAA)", g_rW, g_rH);
    return true;
}

/* -- matrices (row-major, column vectors: v' = M v) -- */
static void M4_Mul(float *o, const float *a, const float *b)
{
    float t[16];
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
    memcpy(o, t, sizeof t);
}
static void M4_From34(float *o, const HmdMatrix34_t *m)
{
    for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) o[r * 4 + c] = m->m[r][c];
    o[12] = o[13] = o[14] = 0; o[15] = 1;
}
static void M4_InvRigid(float *o, const float *m)   /* inverse of rotation + translation */
{
    float t[16];
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) t[r * 4 + c] = m[c * 4 + r];
    for (int r = 0; r < 3; r++) t[r * 4 + 3] = -(t[r * 4] * m[3] + t[r * 4 + 1] * m[7] + t[r * 4 + 2] * m[11]);
    t[12] = t[13] = t[14] = 0; t[15] = 1;
    memcpy(o, t, sizeof t);
}
/* model matrix from axes (columns X, Y, Z), position and uniform-per-axis scale */
static void M4_Basis(float *o, const float X[3], const float Y[3], const float Z[3], const float P[3], float sx, float sy, float sz)
{
    for (int r = 0; r < 3; r++) { o[r * 4] = X[r] * sx; o[r * 4 + 1] = Y[r] * sy; o[r * 4 + 2] = Z[r] * sz; o[r * 4 + 3] = P[r]; }
    o[12] = o[13] = o[14] = 0; o[15] = 1;
}
static void V3_Norm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; } }
static void V3_Cross(float *o, const float *a, const float *b) { float t[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] }; memcpy(o, t, 12); }
static float V3_Dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static void *LookupForm(uint32_t id);
/* -- the real Ranger Sequoia: its model and textures loaded through the game itself -- */
typedef struct { ID3D11Buffer *vb; UINT count; ID3D11ShaderResourceView *srv; char *geom; } RPart;
static RPart g_seq[24]; static int g_seqN = 0; static bool g_seqTried = false; static int g_seqTexTries = 0;
static float g_seqMuzzle[3] = { GUN_MUZZLE_X, GUN_MUZZLE_Y, 0 };
typedef void *(__attribute__((thiscall)) *LoadModel_t)(void *loader, const char *path, uint32_t baseClass, bool f0, int z, bool f5, bool dontInc);
typedef void (__attribute__((thiscall)) *CreateTexImage_t)(void *tes, const char *path, void **out, bool mustFind, bool archiveOnly);
typedef void (__attribute__((thiscall)) *RendCreateSrc_t)(void *renderer, void *tex);

/* A texture the game keeps in video memory only can't be read directly: draw it once onto a plain readable picture
   with the game's own renderer (all of the renderer's settings are put back afterwards), then read that. */
static ID3D11ShaderResourceView *Seq_FromD3D9Draw(IDirect3DTexture9 *t9, UINT w, UINT h)
{
    IDirect3DDevice9 *dev = NULL;
    if (FAILED(IDirect3DTexture9_GetDevice(t9, &dev)) || !dev) return NULL;
    IDirect3DSurface9 *rt = NULL, *sys = NULL, *oldRT = NULL, *oldDS = NULL; IDirect3DStateBlock9 *sb = NULL;
    ID3D11ShaderResourceView *srv = NULL; bool drew = false;
    if (SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, w, h, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &sb))) {
        IDirect3DDevice9_GetRenderTarget(dev, 0, &oldRT);
        IDirect3DDevice9_GetDepthStencilSurface(dev, &oldDS);
        IDirect3DDevice9_SetRenderTarget(dev, 0, rt);
        IDirect3DDevice9_SetDepthStencilSurface(dev, NULL);
        D3DVIEWPORT9 vp = { 0, 0, w, h, 0.0f, 1.0f }; IDirect3DDevice9_SetViewport(dev, &vp);
        IDirect3DDevice9_SetVertexShader(dev, NULL); IDirect3DDevice9_SetPixelShader(dev, NULL);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)t9);
        IDirect3DDevice9_SetTexture(dev, 1, NULL);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXCOORDINDEX, 0);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, 0);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0xF);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_CLIPPING, FALSE);
        struct { float x, y, z, rhw, u, v; } q[4] = {
            { -0.5f, -0.5f, 0, 1, 0, 0 }, { w - 0.5f, -0.5f, 0, 1, 1, 0 }, { -0.5f, h - 0.5f, 0, 1, 0, 1 }, { w - 0.5f, h - 0.5f, 0, 1, 1, 1 } };
        bool began = SUCCEEDED(IDirect3DDevice9_BeginScene(dev));
        drew = SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, q, sizeof q[0]));
        if (began) IDirect3DDevice9_EndScene(dev);
        IDirect3DDevice9_SetRenderTarget(dev, 0, oldRT);
        IDirect3DDevice9_SetDepthStencilSurface(dev, oldDS);
        IDirect3DStateBlock9_Apply(sb);
        if (drew && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, rt, sys))) {
            D3DLOCKED_RECT lr;
            if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
                D3D11_TEXTURE2D_DESC d = { w, h, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
                D3D11_SUBRESOURCE_DATA init = { lr.pBits, (UINT)lr.Pitch, 0 };
                ID3D11Texture2D *t11 = NULL;
                if (SUCCEEDED(ID3D11Device_CreateTexture2D(g_rDev, &d, &init, &t11))) { ID3D11Device_CreateShaderResourceView(g_rDev, (ID3D11Resource *)t11, NULL, &srv); ID3D11Texture2D_Release(t11); }
                IDirect3DSurface9_UnlockRect(sys);
            }
        }
    }
    if (oldRT) IDirect3DSurface9_Release(oldRT);
    if (oldDS) IDirect3DSurface9_Release(oldDS);
    if (sb) IDirect3DStateBlock9_Release(sb);
    if (sys) IDirect3DSurface9_Release(sys);
    if (rt) IDirect3DSurface9_Release(rt);
    IDirect3DDevice9_Release(dev);
    static int l = 0; if (l++ < 4) Log("Menu Sequoia: texture drawn into a readable copy - %s", srv ? "ok" : drew ? "read back failed" : "draw failed");
    return srv;
}
static ID3D11ShaderResourceView *Seq_FromD3D9(IDirect3DTexture9 *t9)
{
    D3DSURFACE_DESC sd;
    if (!t9 || FAILED(IDirect3DTexture9_GetLevelDesc(t9, 0, &sd))) return NULL;
    static int dl = 0; if (dl++ < 4) Log("Menu Sequoia: copying texture %ux%u format %d pool %d levels %lu", sd.Width, sd.Height, (int)sd.Format, (int)sd.Pool, (unsigned long)IDirect3DTexture9_GetLevelCount(t9));
    DXGI_FORMAT f; bool block = true;
    if (sd.Format == D3DFMT_DXT1) f = DXGI_FORMAT_BC1_UNORM;
    else if (sd.Format == D3DFMT_DXT3) f = DXGI_FORMAT_BC2_UNORM;
    else if (sd.Format == D3DFMT_DXT5) f = DXGI_FORMAT_BC3_UNORM;
    else if (sd.Format == D3DFMT_A8R8G8B8 || sd.Format == D3DFMT_X8R8G8B8) { f = DXGI_FORMAT_B8G8R8A8_UNORM; block = false; }
    else { Log("Menu Sequoia: texture format %d not handled", (int)sd.Format); return NULL; }
    UINT levels = IDirect3DTexture9_GetLevelCount(t9); if (levels > 14) levels = 14;
    D3D11_SUBRESOURCE_DATA init[14]; D3DLOCKED_RECT lr[14]; UINT got = 0;
    for (UINT l = 0; l < levels; l++) {
        D3DSURFACE_DESC ld; IDirect3DTexture9_GetLevelDesc(t9, l, &ld);
        if (block && (ld.Width < 4 || ld.Height < 4)) break;
        if (FAILED(IDirect3DTexture9_LockRect(t9, l, &lr[l], NULL, D3DLOCK_READONLY))) break;
        init[l].pSysMem = lr[l].pBits; init[l].SysMemPitch = (UINT)lr[l].Pitch; init[l].SysMemSlicePitch = 0; got++;
    }
    ID3D11ShaderResourceView *srv = NULL;
    if (got) {
        D3D11_TEXTURE2D_DESC d = { sd.Width, sd.Height, got, 1, f, { 1, 0 }, D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        ID3D11Texture2D *t11 = NULL;
        HRESULT hr = ID3D11Device_CreateTexture2D(g_rDev, &d, init, &t11);
        if (SUCCEEDED(hr)) { ID3D11Device_CreateShaderResourceView(g_rDev, (ID3D11Resource *)t11, NULL, &srv); ID3D11Texture2D_Release(t11); }
        else { static int el = 0; if (el++ < 4) Log("Menu Sequoia: headset copy of the texture failed (%08lX, %u levels)", (unsigned long)hr, got); }
    }
    for (UINT l = 0; l < got; l++) IDirect3DTexture9_UnlockRect(t9, l);
    if (!got) srv = Seq_FromD3D9Draw(t9, sd.Width, sd.Height);   /* video memory only: draw it into a readable copy */
    return srv;
}
static IDirect3DTexture9 *Seq_D3DOf(char *tex)
{
    if (!tex || IsBadReadPtr(tex, 0x28)) return NULL;
    char *td = *(char **)(tex + 0x24);
    if (!td || IsBadReadPtr(td, 0x68)) return NULL;
    IDirect3DBaseTexture9 *bt = *(IDirect3DBaseTexture9 **)(td + 0x64);
    if (!bt || IsBadReadPtr(bt, 4) || IDirect3DBaseTexture9_GetType(bt) != D3DRTYPE_TEXTURE) return NULL;
    return (IDirect3DTexture9 *)bt;
}
/* The part's lit-texture material: the cached slot (+A8) is only filled once the game has prepared the model, so
   also look through the part's own property list (+24: first, last, count; nodes are next, prev, data). */
static char *Seq_ShaderProp(char *geom)
{
    char *prop = *(char **)(geom + 0xA8);
    if (prop && !IsBadReadPtr(prop, 4) && *(uintptr_t *)prop == 0x010AE0D0) return prop;
    char **node = *(char ***)(geom + 0x24);
    for (int i = 0; node && i < 16 && !IsBadReadPtr(node, 12); i++, node = (char **)node[0]) {
        char *p = node[2];
        if (p && !IsBadReadPtr(p, 0xC4) && *(uintptr_t *)p == 0x010AE0D0) return p;
    }
    return prop;
}
static ID3D11ShaderResourceView *Seq_CopyTexture(char *geom, bool allowLoad)
{
    char *prop = Seq_ShaderProp(geom);
    { static int ml = 0; if (allowLoad && ml++ < 2) { char **src0 = prop && !IsBadReadPtr(prop, 0xB0) ? *(char ***)(prop + 0xAC) : NULL; Log("Menu Sequoia: part material %p (type %08X), prepared texture %p", (void *)prop, prop && !IsBadReadPtr(prop, 4) ? (unsigned)*(uintptr_t *)prop : 0u, src0 && !IsBadReadPtr(src0, 4) ? (void *)src0[0] : NULL); } }
    if (!prop || IsBadReadPtr(prop, 0xC4) || *(uintptr_t *)prop != 0x010AE0D0) {   /* BSShaderPPLightingProperty */
        static int pl = 0; if (allowLoad && pl++ < 3) Log("Menu Sequoia: part material is type %08X (not the usual lit-texture kind)", prop && !IsBadReadPtr(prop, 4) ? (unsigned)*(uintptr_t *)prop : 0u);
        return NULL;
    }
    /* 1. already prepared by the game? */
    char **src = *(char ***)(prop + 0xAC);
    IDirect3DTexture9 *t9 = (src && !IsBadReadPtr(src, 4)) ? Seq_D3DOf(src[0]) : NULL;
    /* 2. otherwise load the diffuse texture file through the game and prepare it */
    if (!t9 && allowLoad) {
        char *ts = *(char **)(prop + 0xA4);
        const char *path = (ts && !IsBadReadPtr(ts, 0x10)) ? *(const char **)(ts + 0x08) : NULL;
        void *tes = *(void **)0x011DEA10, *rend = *(void **)0x011F4748;
        if (!path || !*path || !tes || !rend) { static int ql = 0; if (ql++ < 3) Log("Menu Sequoia: no texture path (%s) or game not ready (%p %p)", path ? path : "none", tes, rend); return NULL; }
        void *nst = NULL;
        ((CreateTexImage_t)0x004568C0)(tes, path, &nst, true, false);
        if (nst) { void **vt = *(void ***)rend; ((RendCreateSrc_t)vt[0x13C / 4])(rend, nst); t9 = Seq_D3DOf((char *)nst); }
        static int ll = 0; if (ll++ < 4) Log("Menu Sequoia: loading texture %s - %s", path, t9 ? "ok" : "failed");
    }
    return Seq_FromD3D9(t9);
}

/* gather the model's shapes (positions in metres, still in the model's own axes) */
typedef struct { RVtx *v; UINT n; char *geom; } SeqTmp;
static SeqTmp g_seqTmp[24]; static int g_seqTmpN = 0;
static void Seq_Walk(char *n, const float Rp[9], const float Tp[3], float Sp, int depth)
{
    if (!n || depth > 24 || g_seqTmpN >= 24 || IsBadReadPtr(n, 0xC0)) return;
    const float *Lr = (const float *)(n + 0x34), *Lt = (const float *)(n + 0x58);
    float Ls = *(const float *)(n + 0x64); if (!(Ls > 1e-4f && Ls < 1e4f)) Ls = 1.0f;
    float R[9], T[3], S = Sp * Ls;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) R[i * 3 + j] = Rp[i * 3 + 0] * Lr[0 * 3 + j] + Rp[i * 3 + 1] * Lr[1 * 3 + j] + Rp[i * 3 + 2] * Lr[2 * 3 + j];
        T[i] = Tp[i] + (Rp[i * 3 + 0] * Lt[0] + Rp[i * 3 + 1] * Lt[1] + Rp[i * 3 + 2] * Lt[2]) * Sp;
    }
    if (*(uint32_t *)(n + 0x30) & 1) return;   /* hidden */
    uintptr_t vt = *(uintptr_t *)n;
    if (vt == 0x0109D454 || vt == 0x0109CD44) {
        char *gd = *(char **)(n + 0xB8);
        if (!gd) return;
        uint16_t nv = *(uint16_t *)(gd + 0x08);
        const float *V = *(const float **)(gd + 0x20), *N = *(const float **)(gd + 0x24), *UV = *(const float **)(gd + 0x2C);
        if (!nv || !V) return;
        uint16_t *idx = NULL; UINT ni = 0;
        uintptr_t dvt = *(uintptr_t *)gd;
        if (dvt == 0x0109DAC4) {
            uint16_t nt = *(uint16_t *)(gd + 0x40); const uint16_t *tri = *(const uint16_t **)(gd + 0x48);
            if (tri && nt) { idx = (uint16_t *)malloc(nt * 6); memcpy(idx, tri, nt * 6); ni = nt * 3; }
        } else if (dvt == 0x0109D8AC) {
            uint16_t ns = *(uint16_t *)(gd + 0x44); const uint16_t *len = *(const uint16_t **)(gd + 0x48), *st = *(const uint16_t **)(gd + 0x4C);
            if (len && st) {
                UINT tot = 0; for (int k = 0; k < ns; k++) tot += len[k];
                idx = (uint16_t *)malloc(tot * 6 + 6); const uint16_t *p = st;
                for (int k = 0; k < ns; k++) {
                    for (int q = 2; q < len[k]; q++) {
                        uint16_t a = p[q - 2], b = p[q - 1], c = p[q];
                        if (a == b || b == c || a == c) continue;
                        if (q & 1) { idx[ni++] = a; idx[ni++] = c; idx[ni++] = b; } else { idx[ni++] = a; idx[ni++] = b; idx[ni++] = c; }
                    }
                    p += len[k];
                }
            }
        }
        if (!ni) { free(idx); return; }
        RVtx *vb = (RVtx *)malloc(ni * sizeof(RVtx));
        const float k = 1.0f / g_unitsPerMeter;
        for (UINT q = 0; q < ni; q++) {
            uint16_t i0 = idx[q]; if (i0 >= nv) i0 = 0;
            RVtx *o = &vb[q];
            for (int i = 0; i < 3; i++) {
                o->p[i] = (T[i] + (R[i * 3] * V[i0 * 3] + R[i * 3 + 1] * V[i0 * 3 + 1] + R[i * 3 + 2] * V[i0 * 3 + 2]) * S) * k;
                o->n[i] = N ? R[i * 3] * N[i0 * 3] + R[i * 3 + 1] * N[i0 * 3 + 1] + R[i * 3 + 2] * N[i0 * 3 + 2] : 0.0f;
            }
            o->c[0] = o->c[1] = o->c[2] = 1.0f;
            o->uv[0] = UV ? UV[i0 * 2] : 0; o->uv[1] = UV ? UV[i0 * 2 + 1] : 0;
        }
        if (!N) for (UINT q = 0; q + 2 < ni; q += 3) {
            float a[3], b[3], c[3];
            for (int i = 0; i < 3; i++) { a[i] = vb[q + 1].p[i] - vb[q].p[i]; b[i] = vb[q + 2].p[i] - vb[q].p[i]; }
            V3_Cross(c, a, b); V3_Norm(c);
            for (int j = 0; j < 3; j++) memcpy(vb[q + j].n, c, 12);
        }
        g_seqTmp[g_seqTmpN].v = vb; g_seqTmp[g_seqTmpN].n = ni; g_seqTmp[g_seqTmpN].geom = n; g_seqTmpN++;
        free(idx);
        return;
    }
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        for (int c = 0; kids && c < nk; c++) Seq_Walk(kids[c], R, T, S, depth + 1);
    }
}
static double g_seqWantAt = 0;
static void Room_LoadSequoia(void)
{
    if (g_seqTried) return;
    g_seqTried = true;
    char *w = (char *)LookupForm(0x00129A44);   /* WeapNVRangerSequoia */
    const char *path = w ? *(const char **)(w + 0x40) : NULL;
    void *loader = *(void **)0x011C3B3C;
    if (!path || !*path || !loader) { Log("Menu Sequoia: weapon or model path not found - using the stand-in revolver"); return; }
    char *root = (char *)((LoadModel_t)0x00447080)(loader, path, 0, true, 0, false, true);
    if (!root) { Log("Menu Sequoia: couldn't load %s - using the stand-in revolver", path); return; }
    float I[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, Z0[3] = { 0, 0, 0 };
    char **kids = AsNode(root) ? *(char ***)(root + 0xA0) : NULL; uint16_t nk = kids ? *(uint16_t *)(root + 0xA6) : 0;
    g_seqTmpN = 0;
    for (int c = 0; c < nk; c++) Seq_Walk(kids[c], I, Z0, 1.0f, 1);
    if (!g_seqTmpN) { Log("Menu Sequoia: no shapes in %s - using the stand-in revolver", path); return; }
    /* which way is the barrel? the gun's long horizontal axis, toward the far end from the grip (the origin) */
    float mn[3] = { 1e9f, 1e9f, 1e9f }, mx[3] = { -1e9f, -1e9f, -1e9f };
    for (int pi = 0; pi < g_seqTmpN; pi++) for (UINT q = 0; q < g_seqTmp[pi].n; q++) for (int i = 0; i < 3; i++) {
        float v = g_seqTmp[pi].v[q].p[i]; if (v < mn[i]) mn[i] = v; if (v > mx[i]) mx[i] = v; }
    int A = 0; for (int i = 1; i < 3; i++) if (mx[i] - mn[i] > mx[A] - mn[A]) A = i;          /* longest: the barrel */
    int B = -1; for (int i = 0; i < 3; i++) if (i != A && (B < 0 || mx[i] - mn[i] > mx[B] - mn[B])) B = i;   /* next: grip-to-top height */
    /* which end is the barrel: the grip end is tall (the handle hangs down), the barrel end is thin - works for any model,
       wherever its origin sits. Up: the barrel sits above the handle. */
    float sg, su;
    {
        float len = mx[A] - mn[A], lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f }; double sum[2] = { 0, 0 }; int cnt[2] = { 0, 0 };
        for (int pi = 0; pi < g_seqTmpN; pi++) for (UINT q = 0; q < g_seqTmp[pi].n; q++) {
            const float *v = g_seqTmp[pi].v[q].p; float t = (v[A] - mn[A]) / (len > 1e-6f ? len : 1.0f);
            int e = t < 0.3f ? 0 : t > 0.7f ? 1 : -1; if (e < 0) continue;
            if (v[B] < lo[e]) lo[e] = v[B]; if (v[B] > hi[e]) hi[e] = v[B]; sum[e] += v[B]; cnt[e]++;
        }
        bool ok = cnt[0] > 0 && cnt[1] > 0;
        int barrel = ok ? ((hi[1] - lo[1]) < (hi[0] - lo[0]) ? 1 : 0) : (mx[A] >= -mn[A] ? 1 : 0);
        sg = barrel ? 1.0f : -1.0f;
        if (ok) { float bMid = 0.5f * (hi[barrel] + lo[barrel]), gMean = (float)(sum[1 - barrel] / cnt[1 - barrel]); su = bMid >= gMean ? 1.0f : -1.0f; }
        else su = (-mn[B] > mx[B]) ? -1.0f : 1.0f;
        Log("Menu Sequoia: ends - height %.3f vs %.3f m -> barrel at the %s end; %s", hi[0] - lo[0], hi[1] - lo[1], barrel ? "+" : "-", ok ? "measured" : "guessed");
    }
    if (g_seqFlip) sg = -sg;   /* manual override */
    float F[3] = { 0, 0, 0 }, U[3] = { 0, 0, 0 }, Rg[3]; F[A] = sg; U[B] = su;
    if (g_seqRoll != 0.0f) {   /* manual roll about the barrel */
        float W[3]; V3_Cross(W, F, U); float c = cosf(g_seqRoll / RAD2DEG), sn = sinf(g_seqRoll / RAD2DEG);
        for (int i = 0; i < 3; i++) U[i] = U[i] * c + W[i] * sn;
    }
    V3_Cross(Rg, F, U);
    Log("Menu Sequoia: size %.2f x %.2f x %.2f m, barrel along %s%c, up along %s%c", mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], sg > 0 ? "+" : "-", "XYZ"[A], su > 0 ? "+" : "-", "XYZ"[B]);
    float best = -1e9f;
    for (int pi = 0; pi < g_seqTmpN; pi++) {
        RVtx *vb = g_seqTmp[pi].v;
        for (UINT q = 0; q < g_seqTmp[pi].n; q++) {
            float p0[3], n0[3]; memcpy(p0, vb[q].p, 12); memcpy(n0, vb[q].n, 12);
            vb[q].p[0] = V3_Dot(p0, F); vb[q].p[1] = V3_Dot(p0, U); vb[q].p[2] = V3_Dot(p0, Rg);
            vb[q].n[0] = V3_Dot(n0, F); vb[q].n[1] = V3_Dot(n0, U); vb[q].n[2] = V3_Dot(n0, Rg);
            if (vb[q].p[0] > best) { best = vb[q].p[0]; memcpy(g_seqMuzzle, vb[q].p, 12); }
        }
    }
    {   /* the muzzle: centre of the barrel's end */
        float sy = 0, sz = 0; int cnt = 0;
        for (int pi = 0; pi < g_seqTmpN; pi++) for (UINT q = 0; q < g_seqTmp[pi].n; q++) if (g_seqTmp[pi].v[q].p[0] > best - 0.01f) { sy += g_seqTmp[pi].v[q].p[1]; sz += g_seqTmp[pi].v[q].p[2]; cnt++; }
        if (cnt) { g_seqMuzzle[1] = sy / cnt; g_seqMuzzle[2] = sz / cnt; }
    }
    for (int pi = 0; pi < g_seqTmpN && g_seqN < 24; pi++) {
        D3D11_BUFFER_DESC bd = { g_seqTmp[pi].n * sizeof(RVtx), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA sd = { g_seqTmp[pi].v, 0, 0 };
        RPart *pt = &g_seq[g_seqN]; memset(pt, 0, sizeof *pt);
        if (SUCCEEDED(ID3D11Device_CreateBuffer(g_rDev, &bd, &sd, &pt->vb))) { pt->count = g_seqTmp[pi].n; pt->geom = g_seqTmp[pi].geom; pt->srv = Seq_CopyTexture(pt->geom, false); g_seqN++; }
        free(g_seqTmp[pi].v);
    }
    int got = 0; for (int q = 0; q < g_seqN; q++) if (g_seq[q].srv) got++;
    Log("Menu Sequoia: loaded %s - %d parts (%d textured), muzzle at %.3f %.3f %.3f m", path, g_seqN, got, g_seqMuzzle[0], g_seqMuzzle[1], g_seqMuzzle[2]);
}

static void Room_Draw(const RMesh *m, const float *vp, const float *model, float mode, const float tint[4], const float eye[3])
{
    if (!m->vb) return;
    RConst k;
    M4_Mul(k.mvp, vp, model);
    memcpy(k.model, model, 64); memcpy(k.tint, tint, 16);
    k.mode[0] = mode; k.mode[1] = eye[0]; k.mode[2] = eye[1]; k.mode[3] = eye[2];
    ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_rCB, 0, NULL, &k, 0, 0);
    UINT stride = sizeof(RVtx), off = 0;
    ID3D11DeviceContext_IASetVertexBuffers(g_d11ctx, 0, 1, &m->vb, &stride, &off);
    ID3D11DeviceContext_Draw(g_d11ctx, m->count, 0);
}

static void Far_Release9(void) { Release9(&g_mmRT); Release9(&g_mmSys); }

static void SaveBackbuffer(IDirect3DDevice9 *dev, const char *name);
static bool g_farActive2 = false;
/* place the screen straight ahead of where you're looking, level, at head height */
static void Far_PlaceInFront(void)
{
    float yaw = 0, pitch = 0, pos[3] = { 0, 1.6f, 0 };
    TrackedDevicePose_t hp;
    g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &hp, 1);
    if (hp.bPoseIsValid) { ExtractYawPitch(&hp.mDeviceToAbsoluteTracking, &yaw, &pitch); for (int k = 0; k < 3; k++) pos[k] = hp.mDeviceToAbsoluteTracking.m[k][3]; }
    g_scrFwd[0] = -sinf(yaw); g_scrFwd[1] = 0; g_scrFwd[2] = -cosf(yaw);
    g_scrRight[0] = cosf(yaw); g_scrRight[1] = 0; g_scrRight[2] = -sinf(yaw);
    for (int k = 0; k < 3; k++) g_scrC[k] = pos[k] + g_scrFwd[k] * g_mmDist;
    g_scrC[1] = pos[1] - 0.05f;
}

/* Where a ray hits the curved screen: u (0 left..1 right), v (0 top..1 bottom). */
static bool Far_HitScreen(const float o[3], const float d[3], float *u, float *v, float *dist)
{
    float W = g_scrBuiltW, R = g_scrBuiltR, H = g_scrBuiltH;
    if (W <= 0 || R <= 0) return false;
    /* curve centre: R behind the screen's middle (towards you) */
    float cc[3] = { g_scrC[0] - g_scrFwd[0] * R, g_scrC[1], g_scrC[2] - g_scrFwd[2] * R };
    float ox = o[0] - cc[0], oz = o[2] - cc[2];
    float a = d[0] * d[0] + d[2] * d[2], b = 2 * (ox * d[0] + oz * d[2]), c = ox * ox + oz * oz - R * R;
    float disc = b * b - 4 * a * c;
    if (a < 1e-6f || disc < 0) return false;
    float t = (-b + sqrtf(disc)) / (2 * a);   /* from inside: the far side */
    if (t <= 0) return false;
    float p[3] = { o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t };
    float rx = p[0] - cc[0], rz = p[2] - cc[2];
    float along = rx * g_scrFwd[0] + rz * g_scrFwd[2], side = rx * g_scrRight[0] + rz * g_scrRight[2];
    float ang = atan2f(side, along), arc = W / R;
    float uu = ang / arc + 0.5f, vv = 0.5f - (p[1] - g_scrC[1]) / H;
    if (uu < 0 || uu > 1 || vv < 0 || vv > 1) return false;
    *u = uu; *v = vv; *dist = t;
    return true;
}

/* Move the game's menu cursor to a screen position (pixels), the way JIP LN's SetCursorPos does. */
static void Far_SetCursor(float x, float y)
{
    char *im = *(char **)0x011D8A80;
    if (!im) return;
    *(float *)(im + 0x38) = x; *(float *)(im + 0x40) = y;
    char *cur = *(char **)(im + 0x28);
    char *node = cur ? *(char **)(cur + 0x2C) : NULL;
    float conv = *(float *)0x011D8A48;
    if (node && conv > 0.0f && g_bbW && g_bbH) {
        float hw = g_bbW * conv * 0.5f, hh = g_bbH * conv * 0.5f;
        *(float *)(node + 0x58) = x * conv - hw;   /* NiAVObject local position x */
        *(float *)(node + 0x60) = hh - y * conv;   /* ... and z */
    }
}

/* -- clicking and highlighting through the game's own menu code (main loop) -- */
typedef void *(__attribute__((thiscall)) *PickTile_t)(void *im, int arg);
typedef void (__attribute__((thiscall)) *TileSetFloat_t)(void *tile, uint32_t id, float v, bool propagate);
typedef void (__attribute__((thiscall)) *TileSound_t)(void *tile, uint32_t id);
typedef void (__attribute__((thiscall)) *MenuClick_t)(void *menu, int id, void *tile);
typedef void (__attribute__((thiscall)) *InitSoundEDID_t)(void *mgr, void *sound, const char *edid, uint32_t flags);
typedef void (__attribute__((thiscall)) *SoundPlay_t)(void *sound, int arg);
static float Tile_Value(char *tile, uint32_t id, float def)
{
    char **data = *(char ***)(tile + 0x14); uint32_t n = *(uint32_t *)(tile + 0x18);
    if (!data || n > 4096) return def;
    for (uint32_t i = 0; i < n; i++) if (data[i] && *(uint32_t *)data[i] == id) return *(float *)(data[i] + 8);
    return def;
}
static char *Tile_Menu(char *tile)
{
    for (int i = 0; tile && i < 64; i++, tile = *(char **)(tile + 0x28))
        if (*(uintptr_t *)tile == 0x0106ED44) return *(char **)(tile + 0x3C);
    return NULL;
}
static void Far_PlayShot(void)
{
    if (!g_mmShot) return;
    struct { uint32_t key; uint8_t b4, pad[3]; uint32_t u8; } snd = { 0xFFFFFFFF, 0, { 0, 0, 0 }, 0 };
    ((InitSoundEDID_t)0x00AE5680)((void *)0x011F6EF0, &snd, "WPNPistol44MagnumFire2D", 0x121);
    typedef bool (__attribute__((thiscall)) *SoundSetVol_t)(void *sound, float vol);
    if (snd.key != 0xFFFFFFFF) { ((SoundSetVol_t)0x00AD89E0)(&snd, g_mmShotVol); ((SoundPlay_t)0x00AD8830)(&snd, 0); }
    static int logs = 0; if (logs++ < 3) Log("Menu pointer: Sequoia shot %s", snd.key != 0xFFFFFFFF ? "played" : "sound not found");
}
/* Textures the game hadn't prepared: load them through the game, from the main loop (main menu or in the world). */
static void Seq_TexTick(void)
{
    static double next = 0;
    if (!g_seqN || g_seqTexTries >= 3 || !g_rDev || ((const volatile bool *)0x011F308F)[1007]) return;   /* (main menu too - not during loading) */
    double now = QpcMs();
    if (now < next) return;
    next = now + 3000.0;
    bool missing = false; for (int q = 0; q < g_seqN; q++) if (!g_seq[q].srv) missing = true;
    if (!missing) { g_seqTexTries = 99; return; }
    g_seqTexTries++;
    for (int q = 0; q < g_seqN; q++) if (!g_seq[q].srv) g_seq[q].srv = Seq_CopyTexture(g_seq[q].geom, true);
    int got = 0; for (int q = 0; q < g_seqN; q++) if (g_seq[q].srv) got++;
    Log("Menu Sequoia: textures for %d of %d parts (try %d)", got, g_seqN, g_seqTexTries);
}
static void Far_UiTick(void)
{
    Seq_TexTick();
    if (!g_farActive2 || !g_mmPointer) return;
    char *im = *(char **)0x011D8A80;
    if (!im) return;
    static char *hover = NULL;
    char *tile = g_ptrOnScreen ? (char *)((PickTile_t)0x007126C0)(im, 0) : NULL;
    if (tile != hover) {   /* highlight what the laser is on */
        char *cur = *(char **)(im + 0xCC);
        if (cur && cur != tile) ((TileSetFloat_t)0x00A012D0)(cur, 0xFC3 /* mouseover */, 0.0f, false);
        *(char **)(im + 0xCC) = tile;
        if (tile) {
            ((TileSound_t)0x00A0B110)(tile, 0xFE8 /* mouseoversound */);
            ((TileSetFloat_t)0x00A012D0)(tile, 0xFC3, 1.0f, false);
            char *menu = Tile_Menu(tile);
            if (menu) *(char **)(im + 0xD0) = menu;
        }
        hover = tile;
    }
    if (InterlockedExchange(&g_ptrClickPending, 0)) {
        Far_PlayShot();
        if (tile) {
            char *menu = Tile_Menu(tile);
            int id = (int)Tile_Value(tile, 0xFAA /* id */, -1.0f);
            static int logs = 0; if (logs++ < 20) Log("Menu pointer: click on tile id %d (menu %p)", id, (void *)menu);
            ((TileSound_t)0x00A0B110)(tile, 0xFCB /* clicksound */);
            ((TileSetFloat_t)0x00A012D0)(tile, 0xFC7 /* clicked */, 1.0f, false);
            ((TileSetFloat_t)0x00A012D0)(tile, 0xFC7, 0.0f, false);
            if (menu) { void **vt = *(void ***)menu; ((MenuClick_t)vt[3])(menu, id, tile); }
            hover = NULL;   /* the menu may have changed - pick again next frame */
        }
    }
}

/* Returns true when the menu room handled this frame (the normal headset picture is skipped). */
static bool Far_Frame(IDirect3DDevice9 *dev)
{
    int pmNow = PipBoyMode();
    bool pipUp = g_wristPip && (g_pipOpen || QpcMs() < g_pipExpectUntil || pmNow == 2 || pmNow == 3 || pmNow == 5 || PipMenusVisible());
    bool menuScreen = !g_inGameplay || (g_farGameMenus && IsMenuMode() && !pipUp && !g_liveMenuNow);
    bool want = g_farMenu && !g_flatMenus && menuScreen && g_vrSystem && g_vrCompositor && HeadsetDisplayActive();
    if (!want || !Room_Init()) {
        if (g_farActive2) { g_farActive2 = false; g_farActive = false; g_ptrOnScreen = false; Log("Menu screen: off (back in the world)"); }
        return false;
    }
    /* 1. the game's picture -> screen texture */
    if (!g_mmRT) IDirect3DDevice9_CreateRenderTarget(dev, MM_W, MM_H, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_mmRT, NULL);
    if (!g_mmSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, MM_W, MM_H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_mmSys, NULL);
    if (!g_mmTex11) {
        D3D11_TEXTURE2D_DESC td = { MM_W, MM_H, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        if (SUCCEEDED(ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_mmTex11)))
            ID3D11Device_CreateShaderResourceView(g_d11, (ID3D11Resource *)g_mmTex11, NULL, &g_mmSRV);
    }
    IDirect3DSurface9 *bb = NULL;
    if (!g_mmRT || !g_mmSys || !g_mmSRV || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return false;
    bool ok = SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_mmRT, NULL, D3DTEXF_LINEAR)) &&
              SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_mmRT, g_mmSys));
    IDirect3DSurface9_Release(bb);
    if (!ok) { static int e = 0; if (e++ < 3) Log("Menu screen: couldn't copy the game picture"); return false; }
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DSurface9_LockRect(g_mmSys, &lr, NULL, D3DLOCK_READONLY))) {
        ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_mmTex11, 0, NULL, lr.pBits, (UINT)lr.Pitch, 0);
        IDirect3DSurface9_UnlockRect(g_mmSys);
    }
    if (g_scrBuiltW != g_mmWidth || g_scrBuiltR != g_mmCurveR) Room_BuildScreen();
    if (g_mmPointer && !g_seqTried && g_seqWantAt == 0) g_seqWantAt = QpcMs() + 5000.0;   /* loaded later from the main loop, once the menu has settled */
    float mzX = g_seqN ? g_seqMuzzle[0] : GUN_MUZZLE_X, mzY = g_seqN ? g_seqMuzzle[1] : GUN_MUZZLE_Y, mzZ = g_seqN ? g_seqMuzzle[2] : 0.0f;
    static bool lastIngame = false;
    if (!g_farActive2 || lastIngame != g_inGameplay) Far_PlaceInFront();
    lastIngame = g_inGameplay;
    {   /* diagnostic: a picture of the main menu, once per session, 6 s after it shows */
        static double mmShownAt = 0; static bool mmSnapDone = false;
        if (!g_inGameplay && !mmSnapDone) {
            if (mmShownAt == 0) mmShownAt = QpcMs();
            else if (QpcMs() - mmShownAt > 6000) { mmSnapDone = true; SaveBackbuffer(dev, "FNVR_mainmenu.bmp"); Log("Saved diagnostic snapshot FNVR_mainmenu.bmp"); }
        }
    }
    if (!g_farActive2) { g_farActive2 = true; g_farActive = true; Log("Menu screen: on (%s)", g_inGameplay ? "menu or prompt in the game" : "main menu / loading screen");
        char ml[256] = ""; const bool *mv = (const bool *)0x011F308F;
        for (int id = 0x3E9; id <= 0x43C; id++) if (mv[id]) { char t[12]; snprintf(t, sizeof t, " %X", id); strncat(ml, t, sizeof ml - strlen(ml) - 1); }
        Log("Menu screen: menus showing:%s (Pip-Boy mode %d)", ml, PipBoyMode()); }

    /* 2. poses: head and gun hand (predicted for this frame) */
    TrackedDevicePose_t poses[16];
    if (g_haveWaitPose) memcpy(poses, g_waitPoses, sizeof poses);
    else g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.02f, poses, 16);
    if (!poses[0].bPoseIsValid) return true;
    TrackedDeviceIndex_t ri = g_vrSystem->GetTrackedDeviceIndexForControllerRole(
        g_leftHanded ? ETrackedControllerRole_TrackedControllerRole_LeftHand : ETrackedControllerRole_TrackedControllerRole_RightHand);
    bool haveGun = g_mmPointer && ri < 16 && poses[ri].bPoseIsValid;
    float gunM[16], muzzle[3] = { 0 }, dir[3] = { 0 }, hitDist = 3.0f, u = 0, v = 0;
    double now = QpcMs();
    g_ptrOnScreen = false;
    if (haveGun) {
        const float (*m)[4] = poses[ri].mDeviceToAbsoluteTracking.m;
        float o = g_aimPitchOffsetDeg / RAD2DEG, X[3], Y[3], Z[3], P[3];
        for (int k = 0; k < 3; k++) {
            X[k] = -m[k][2] * cosf(o) + m[k][1] * sinf(o);
            Y[k] = m[k][1] * cosf(o) + m[k][2] * sinf(o);
        }
        V3_Norm(X); V3_Cross(Z, X, Y); V3_Norm(Z); V3_Cross(Y, Z, X);
        /* recoil: muzzle flips up and the gun kicks back, settling over ~0.25 s */
        float since = (float)(now - g_fireT) / 1000.0f, kick = since < 0.25f ? expf(-since * 14.0f) : 0.0f;
        float ang = 0.30f * kick, ca = cosf(ang), sa = sinf(ang);
        float Xr[3], Yr[3];
        for (int k = 0; k < 3; k++) { Xr[k] = X[k] * ca + Y[k] * sa; Yr[k] = Y[k] * ca - X[k] * sa; }
        for (int k = 0; k < 3; k++) P[k] = m[k][3] + Y[k] * g_mmGunUpIni + X[k] * (g_mmGunFwdIni - 0.03f * kick);
        M4_Basis(gunM, Xr, Yr, Z, P, 1, 1, 1);
        for (int k = 0; k < 3; k++) { muzzle[k] = P[k] + Xr[k] * mzX + Yr[k] * mzY + Z[k] * mzZ; dir[k] = X[k]; }
        /* aim from the barrel (unrecoiled) */
        float mz[3]; for (int k = 0; k < 3; k++) mz[k] = P[k] + X[k] * mzX + Y[k] * mzY + Z[k] * mzZ;
        if (Far_HitScreen(mz, X, &u, &v, &hitDist)) {
            g_ptrOnScreen = true;
            Far_SetCursor(u * g_bbW, v * g_bbH);
        }
        /* trigger: fire (and click in the main loop) */
        VRControllerState_t cs; memset(&cs, 0, sizeof cs);
        static bool down = false;
        if (!g_setOpen && !g_setSwallow && g_vrSystem->GetControllerState(ri, &cs, sizeof cs)) {   /* (not while the FNVR settings panel has the controls) */
            float tr = cs.rAxis[1].x;
            if (!down && tr > 0.75f) {
                down = true; g_fireT = now;
                InterlockedExchange(&g_ptrClickPending, 1);
                g_vrSystem->TriggerHapticPulse(ri, 0, 3999);
            } else if (down && tr < 0.4f) down = false;
        }
    }

    /* 3. draw both eyes */
    float head[16]; M4_From34(head, &poses[0].mDeviceToAbsoluteTracking);
    float eyePos[3] = { head[3], head[7], head[11] };
    ID3D11DeviceContext *cx = g_d11ctx;
    ID3D11DeviceContext_IASetInputLayout(cx, g_rIL);
    ID3D11DeviceContext_IASetPrimitiveTopology(cx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(cx, g_rVS, NULL, 0); ID3D11DeviceContext_PSSetShader(cx, g_rPS, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(cx, 0, 1, &g_rCB); ID3D11DeviceContext_PSSetConstantBuffers(cx, 0, 1, &g_rCB);
    ID3D11DeviceContext_PSSetSamplers(cx, 0, 1, &g_rSamp);
    ID3D11DeviceContext_RSSetState(cx, g_rRS);
    D3D11_VIEWPORT vpt = { 0, 0, (float)g_rW, (float)g_rH, 0, 1 };
    ID3D11DeviceContext_RSSetViewports(cx, 1, &vpt);
    float scrM[16];
    { float Zs[3] = { -g_scrFwd[0], 0, -g_scrFwd[2] }, Ys[3] = { 0, 1, 0 }; M4_Basis(scrM, g_scrRight, Ys, Zs, g_scrC, 1, 1, 1); }
    float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float beamM[16] = { 0 }, flashM[16] = { 0 };
    bool flash = haveGun && now - g_fireT < 60.0;
    if (haveGun) {
        float Y0[3] = { 0, 1, 0 }, Zb[3], Yb[3];
        V3_Cross(Zb, dir, Y0); if (V3_Dot(Zb, Zb) < 1e-4f) { float X0[3] = { 1, 0, 0 }; V3_Cross(Zb, dir, X0); }
        V3_Norm(Zb); V3_Cross(Yb, Zb, dir);
        M4_Basis(beamM, dir, Yb, Zb, muzzle, hitDist, 0.0018f, 0.0018f);
        float fs = 0.07f + 0.03f * (float)((int)now % 3);
        M4_Basis(flashM, dir, Yb, Zb, muzzle, fs, fs * 0.7f, fs * 0.7f);
    }
    const float tWhite[4] = { 1, 1, 1, 1 }, tGun[4] = { 1, 1, 1, 0.9f }, tBeam[4] = { 1.0f, 0.55f, 0.15f, 0.55f };
    const float tBeamHot[4] = { 1.0f, 0.85f, 0.5f, 0.9f }, tFlash[4] = { 1.0f, 0.75f, 0.35f, 1.0f };
    for (int e = 0; e < 2; e++) {
        HmdMatrix34_t e2h = g_vrSystem->GetEyeToHeadTransform((EVREye)e);
        HmdMatrix44_t pr = g_vrSystem->GetProjectionMatrix((EVREye)e, 0.03f, 100.0f);
        float eh[16], eyeW[16], view[16], proj[16], vp[16];
        M4_From34(eh, &e2h); M4_Mul(eyeW, head, eh); M4_InvRigid(view, eyeW);
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) proj[r * 4 + c] = pr.m[r][c];
        M4_Mul(vp, proj, view);
        float bg[4] = { 0.018f, 0.018f, 0.022f, 1 };
        ID3D11DeviceContext_ClearRenderTargetView(cx, g_rRTV[e], bg);
        ID3D11DeviceContext_ClearDepthStencilView(cx, g_rDSV[e], D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        ID3D11DeviceContext_OMSetRenderTargets(cx, 1, &g_rRTV[e], g_rDSV[e]);
        ID3D11DeviceContext_OMSetDepthStencilState(cx, g_rDSon, 0);
        ID3D11DeviceContext_OMSetBlendState(cx, NULL, NULL, 0xFFFFFFFF);
        ID3D11DeviceContext_PSSetShaderResources(cx, 0, 1, &g_mmSRV);
        Room_Draw(&g_meshFloor, vp, ident, 0, tWhite, eyePos);
        Room_Draw(&g_meshScreen, vp, scrM, 1, tWhite, eyePos);
        if (haveGun) {
            if (g_seqN) {   /* the real Ranger Sequoia */
                for (int q = 0; q < g_seqN; q++) {
                    RMesh pm = { g_seq[q].vb, g_seq[q].count };
                    const float tUntex[4] = { 0.30f, 0.30f, 0.32f, 0.9f };
                    if (g_seq[q].srv) { ID3D11DeviceContext_PSSetShaderResources(cx, 0, 1, &g_seq[q].srv); Room_Draw(&pm, vp, gunM, 3, tGun, eyePos); }
                    else Room_Draw(&pm, vp, gunM, 0, tUntex, eyePos);
                }
                ID3D11DeviceContext_PSSetShaderResources(cx, 0, 1, &g_mmSRV);
            } else Room_Draw(&g_meshGun, vp, gunM, 0, tGun, eyePos);
            ID3D11DeviceContext_OMSetDepthStencilState(cx, g_rDSnoWrite, 0);
            ID3D11DeviceContext_OMSetBlendState(cx, g_rBlendAlpha, NULL, 0xFFFFFFFF);
            Room_Draw(&g_meshBox, vp, beamM, 2, flash ? tBeamHot : tBeam, eyePos);
            if (flash) { ID3D11DeviceContext_OMSetBlendState(cx, g_rBlendAdd, NULL, 0xFFFFFFFF); Room_Draw(&g_meshFlash, vp, flashM, 2, tFlash, eyePos); }
        }
        ID3D11DeviceContext_ResolveSubresource(cx, (ID3D11Resource *)g_rOut[e], 0, (ID3D11Resource *)g_rMS[e], 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
    ID3D11DeviceContext_OMSetRenderTargets(cx, 0, NULL, NULL);
    ID3D11DeviceContext_Flush(cx);
    for (int e = 0; e < 2; e++) {
        VRTextureWithPose_t tex;
        tex.handle = g_rOut[e]; tex.eType = ETextureType_TextureType_DirectX; tex.eColorSpace = EColorSpace_ColorSpace_Gamma;
        tex.mDeviceToAbsoluteTracking = poses[0].mDeviceToAbsoluteTracking;
        EVRCompositorError ce = g_vrCompositor->Submit((EVREye)e, (struct Texture_t *)&tex, NULL, EVRSubmitFlags_Submit_TextureWithPose);
        if (ce != EVRCompositorError_VRCompositorError_None) { static int ec = 0; if (ec++ < 10) Log("Menu room: SteamVR refused eye %d picture (compositor error %d)", e, (int)ce); }
    }
    {   /* is the game the app SteamVR is showing? (if not, the headset shows SteamVR's own scene instead) */
        static double next = 0; static int was = -1;
        if (QpcMs() > next && g_vrCompositor->GetCurrentSceneFocusProcess) {
            next = QpcMs() + 2000.0;
            uint32_t fp = g_vrCompositor->GetCurrentSceneFocusProcess();
            int ours = fp == GetCurrentProcessId() ? 1 : 0;
            if (ours != was) { was = ours; Log("SteamVR is showing %s (focus process %u, game %lu)", ours ? "the game" : "ANOTHER APP - not the game", fp, (unsigned long)GetCurrentProcessId()); }
        }
    }
    g_submitsSinceLog++;
    (void)u; (void)v;
    return true;
}

static char *g_pOn;   /* (defined with the Pip-Boy picture code) */
/* ---- Pip-Boy menu on your real wrist ------------------------------------------------------
   The game draws the Pip-Boy menu into a texture of its own (normally shown on a floating quad that it
   lines up with its own arm pose). FNVR copies that texture onto a SteamVR overlay fixed to your left
   controller, sitting on the back of your wrist - so the menu is always exactly where your wrist is,
   whatever the arm model is doing. */
static VROverlayHandle_t g_pipOv = 0;
static void *g_pipSrcTex = NULL;
static IDirect3DSurface9 *g_pipRT = NULL, *g_pipSys = NULL; static UINT g_pipW = 0, g_pipH = 0;
static ID3D11Texture2D *g_pipTex11 = NULL;
static bool g_pipOvShown = false, g_pipOvFailed = false;
/* "On the screen" mode: the menu picture laid exactly over the Pip-Boy's dark screen box, like the scope lens. */
static bool g_pipOnBox = true; static float g_pipBoxFit = 1.0f, g_pipBoxLift = 0.3f;   /* INI [Controls] bPipMenuOnScreenBox, fPipMenuBoxFit, fPipMenuBoxLift */
static HmdMatrix34_t g_pipBoxPose; static bool g_pipBoxOk = false; static float g_pipBoxWidthM = 0.08f;
#define g_pipOvBack g_pipOvIni[0]
#define g_pipOvOut g_pipOvIni[1]
#define g_pipOvUp g_pipOvIni[2]
#define g_pipOvWidth g_pipOvIni[3]
#define g_pipOvEnabled g_pipOvEnabledIni

static bool Rd(const void *p, size_t n) { return p && !IsBadReadPtr(p, n); }
static IDirect3DTexture9 *Pip_TexOf(char *tex)
{
    if (!Rd(tex, 0x28)) return NULL;
    char *td = *(char **)(tex + 0x24);
    if (!Rd(td, 0x68)) return NULL;
    IDirect3DBaseTexture9 *bt = *(IDirect3DBaseTexture9 **)(td + 0x64);
    if (!Rd(bt, 4) || IDirect3DBaseTexture9_GetType(bt) != D3DRTYPE_TEXTURE) return NULL;
    IDirect3DTexture9 *t = (IDirect3DTexture9 *)bt;
    D3DSURFACE_DESC d; if (FAILED(IDirect3DTexture9_GetLevelDesc(t, 0, &d))) return NULL;
    return (d.Usage & D3DUSAGE_RENDERTARGET) && d.Width >= 128 ? t : NULL;
}
static IDirect3DTexture9 *Pip_TexOfGeom(char *g)
{
    if (!Rd(g, 0xC0)) return NULL;
    IDirect3DTexture9 *t = NULL;
    char *sp = *(char **)(g + 0xA8);
    if (Rd(sp, 0x68)) t = Pip_TexOf(*(char **)(sp + 0x60));
    char *tp = *(char **)(g + 0xB0);
    if (!t && Rd(tp, 0x24)) { char **arr = *(char ***)(tp + 0x20); if (Rd(arr, 4) && Rd(arr[0], 0x0C)) t = Pip_TexOf(*(char **)(arr[0] + 0x08)); }
    return t;
}
static IDirect3DTexture9 *Pip_FindIn(char *n, int depth)
{
    if (!Rd(n, 0xA8) || depth > 10) return NULL;
    uintptr_t vt = *(uintptr_t *)n;
    if (vt == 0x0109D454 || vt == 0x0109CD44) return Pip_TexOfGeom(n);
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        for (int k = 0; Rd(kids, 4) && k < nk; k++) { IDirect3DTexture9 *t = Pip_FindIn(kids[k], depth + 1); if (t) return t; }
    }
    return NULL;
}
/* Recognise D3D textures by their vtable (taken from textures FNVR makes itself), then look for one
   inside the game's menu texture object, a couple of pointers deep. */
static void *g_texVt[2];
static bool IsD3DTex(void *p) { return Rd(p, 8) && g_texVt[0] && (*(void **)p == g_texVt[0] || *(void **)p == g_texVt[1]); }
/* Collect every render-target texture reachable a few pointers deep from the game's Pip-Boy objects. */
/* Candidate pictures for the menu, as surfaces we hold a reference to (so none can vanish under us). */
static IDirect3DSurface9 *g_pcand[24]; static int g_pncand; static int g_pcVisits;
static void Pip_AddCand(IDirect3DSurface9 *sf)
{
    if (!sf || g_pncand >= 24) return;
    for (int i = 0; i < g_pncand; i++) if (g_pcand[i] == sf) return;
    IDirect3DSurface9_AddRef(sf); g_pcand[g_pncand++] = sf;
}
static void Pip_ClearCands(void) { for (int i = 0; i < g_pncand; i++) IDirect3DSurface9_Release(g_pcand[i]); g_pncand = 0; }
static void Pip_Collect(char *p, int depth)
{
    if (!Rd(p, 8) || g_pncand >= 24 || ++g_pcVisits > 20000) return;
    if (IsD3DTex(p)) {
        IDirect3DTexture9 *t = (IDirect3DTexture9 *)p; D3DSURFACE_DESC d;
        if (FAILED(IDirect3DTexture9_GetLevelDesc(t, 0, &d)) || !(d.Usage & D3DUSAGE_RENDERTARGET) || d.Width < 128) return;
        IDirect3DSurface9 *sf = NULL;
        if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(t, 0, &sf)) && sf) { Pip_AddCand(sf); IDirect3DSurface9_Release(sf); }
        return;
    }
    if (depth >= 4) return;
    int span = depth <= 1 ? 0x100 : depth == 2 ? 0x80 : 0x40;
    if (!Rd(p, span)) { span = 0x40; if (!Rd(p, span)) return; }
    for (int o = 0; o < span; o += 4) Pip_Collect(*(char **)(p + o), depth + 1);
}
static bool Pip_PlainSurf(IDirect3DSurface9 *sf, D3DSURFACE_DESC *d)
{
    if (FAILED(IDirect3DSurface9_GetDesc(sf, d))) return false;
    int f = (int)d->Format;
    bool plain = f == D3DFMT_A8R8G8B8 || f == D3DFMT_X8R8G8B8 || f == D3DFMT_R5G6B5 || f == D3DFMT_X1R5G5B5 || f == D3DFMT_A1R5G5B5 || f == D3DFMT_A4R4G4B4 || f == D3DFMT_A8B8G8R8 || f == D3DFMT_X8B8G8R8;
    return plain && d->MultiSampleType == D3DMULTISAMPLE_NONE && (d->Usage & D3DUSAGE_RENDERTARGET) && d->Pool == D3DPOOL_DEFAULT &&
           d->Width >= 128 && d->Height >= 128 && d->Width <= 2048 && d->Height <= 2048;
}
/* How much it looks like the Pip-Boy menu: mostly dark, with bright amber/green text and lines. */
static float Pip_Score(IDirect3DDevice9 *dev, IDirect3DSurface9 *s0, int idx)
{
    D3DSURFACE_DESC d; if (!Pip_PlainSurf(s0, &d)) return -1000;
    IDirect3DSurface9 *rt = NULL, *sys = NULL; float score = -1000;
    if (SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, d.Width, d.Height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, d.Width, d.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_StretchRect(dev, s0, NULL, rt, NULL, D3DTEXF_NONE)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, rt, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            long n = 0, glow = 0; double bright = 0;
            for (UINT y = 0; y < d.Height; y += 2) { const uint32_t *row = (const uint32_t *)((const BYTE *)lr.pBits + y * lr.Pitch);
                for (UINT x = 0; x < d.Width; x += 2) { uint32_t c = row[x]; int R = (c >> 16) & 255, G = (c >> 8) & 255, B = c & 255;
                    int mx = R > G ? (R > B ? R : B) : (G > B ? G : B), mn = R < G ? (R < B ? R : B) : (G < B ? G : B);
                    bright += mx; n++;
                    if (mx > 110 && mx - mn > 60 && B < R && B < G + 10) glow++; } }
            float mean = n ? (float)(bright / n) : 0, frac = n ? (float)glow / n : 0;
            score = frac * 1000.0f - (mean > 90 ? (mean - 90) * 2.0f : 0);
            char nm[48]; snprintf(nm, sizeof nm, "FNVR_pipcand_%d.bmp", idx);
            SaveBMP(nm, (const BYTE *)lr.pBits, lr.Pitch, d.Width, d.Height);
            IDirect3DSurface9_UnlockRect(sys);
            Log("Pip-Boy menu: picture %d (%ux%u, format %d) brightness %.0f, glowing text %.1f%% -> score %.1f", idx, d.Width, d.Height, (int)d.Format, mean, frac * 100, score);
        }
    }
    if (sys) IDirect3DSurface9_Release(sys); if (rt) IDirect3DSurface9_Release(rt);
    return score;
}
static IDirect3DSurface9 *g_pipPicked = NULL; static bool g_pipPickDone = false;
static void Pip_DropPick(void) { if (g_pipPicked) IDirect3DSurface9_Release(g_pipPicked); g_pipPicked = NULL; g_pipPickDone = false; }
static IDirect3DSurface9 *Pip_FindMenuSurf(IDirect3DDevice9 *dev)
{
    if (g_pipPickDone) return g_pipPicked;
    char *im = *(char **)0x011D8A80;
    char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
    if (!Rd(pm, 0x28)) return NULL;
    static double tryAt = 0; if (QpcMs() < tryAt) return NULL;
    tryAt = QpcMs() + 1500.0;
    static int stage = 0; if (stage++ == 0) return NULL;   /* let the menu draw a few frames first */
    static int tries = 0; if (tries >= 6) return NULL; tries++;
    Pip_ClearCands(); g_pcVisits = 0;
    Pip_Collect(*(char **)(pm + 0x10), 0);
    for (int i = 0; i < g_pipRTn; i++) Pip_AddCand(g_pipRTs[i]);
    float best = 5.0f; IDirect3DSurface9 *bs = NULL; int bi = -1;
    for (int i = 0; i < g_pncand; i++) {
        D3DSURFACE_DESC d; bool ok = Pip_PlainSurf(g_pcand[i], &d);
        if (!ok) { IDirect3DSurface9_GetDesc(g_pcand[i], &d); Log("Pip-Boy menu: picture %d (%ux%u, format %d) skipped", i, d.Width, d.Height, (int)d.Format); continue; }
        float sc = Pip_Score(dev, g_pcand[i], i); if (sc > best) { best = sc; bs = g_pcand[i]; bi = i; }
    }
    Log("Pip-Boy menu: try %d - %d pictures, %s", tries, g_pncand, bs ? "found the menu" : "none looks like the menu yet");
    if (bs) { IDirect3DSurface9_AddRef(bs); g_pipPicked = bs; g_pipPickDone = true; Log("Pip-Boy menu: using picture %d (score %.1f)", bi, best); }
    Pip_ClearCands();
    return bs;
}

static void PipOv_Hide(void)
{
    if (g_pipOvShown && g_vrOverlay && g_pipOv) g_vrOverlay->HideOverlay(g_pipOv);
    g_pipOvShown = false;
    if (g_pOn) *(uint32_t *)(g_pOn + 0x30) &= ~1u;   /* the game's own floating picture back */
}
static void PipOv_Release9(void)
{
    Release9(&g_pipRT); Release9(&g_pipSys); g_pipW = g_pipH = 0; g_pipSrcTex = NULL;
    Pip_DropPick(); Pip_ClearCands();
    for (int i = 0; i < g_pipRTn; i++) IDirect3DSurface9_Release(g_pipRTs[i]); g_pipRTn = 0;
}

/* Present: copy the Pip-Boy menu to the wrist screen. */
static void PipOv_Frame(IDirect3DDevice9 *dev)
{
    g_pipOnBox = g_pipOnBoxIni; g_pipBoxFit = g_pipBoxFitIni; g_pipBoxLift = g_pipBoxLiftIni;
    g_pipRTWatch = g_pipOnBoxIni && g_pipOpen && g_pipModeRaw == 3;
    if (!g_pipOpen && g_pipRTn) { for (int i = 0; i < g_pipRTn; i++) IDirect3DSurface9_Release(g_pipRTs[i]); g_pipRTn = 0; }
    bool boxMode = g_pipOnBox && g_pipBoxOk;
    bool want = (g_pipOvEnabled || boxMode) && g_wristPip && g_pipOpen && g_pipModeRaw == 3 && g_vrOverlay && g_vrSystem && g_d11 && !g_pipOvFailed;
    if (!want) { if (g_pipOvShown) { PipOv_Hide(); Log("Pip-Boy wrist screen: hidden"); } return; }
    if (!g_pipOv) {
        if (g_vrOverlay->CreateOverlay("fnvr.pipboy", "FNVR Pip-Boy", &g_pipOv) != EVROverlayError_VROverlayError_None) { g_pipOvFailed = true; Log("Pip-Boy wrist screen: no overlay"); return; }
        g_vrOverlay->SetOverlaySortOrder(g_pipOv, 2);
    }
    if (!g_texVt[0]) {
        IDirect3DTexture9 *a = NULL, *b = NULL;
        if (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &a, NULL))) { g_texVt[0] = *(void **)a; }
        if (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &b, NULL))) { g_texVt[1] = *(void **)b; }
        if (!g_texVt[1]) g_texVt[1] = g_texVt[0];
        if (a) IDirect3DTexture9_Release(a); if (b) IDirect3DTexture9_Release(b);
    }
    IDirect3DSurface9 *src = Pip_FindMenuSurf(dev);
    if (!src) return;
    D3DSURFACE_DESC d; IDirect3DSurface9_GetDesc(src, &d);
    if (src != g_pipSrcTex || d.Width != g_pipW || d.Height != g_pipH) {
        Release9(&g_pipRT); Release9(&g_pipSys);
        if (g_pipTex11) { ID3D11Texture2D_Release(g_pipTex11); g_pipTex11 = NULL; }
        g_pipW = d.Width; g_pipH = d.Height; g_pipSrcTex = src;
        IDirect3DDevice9_CreateRenderTarget(dev, g_pipW, g_pipH, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_pipRT, NULL);
        IDirect3DDevice9_CreateOffscreenPlainSurface(dev, g_pipW, g_pipH, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &g_pipSys, NULL);
        D3D11_TEXTURE2D_DESC td = { g_pipW, g_pipH, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_pipTex11);
        Log("Pip-Boy wrist screen: menu texture %ux%u (format %d) found", g_pipW, g_pipH, (int)d.Format);
        g_vrOverlay->SetOverlayWidthInMeters(g_pipOv, g_pipOvWidth);
    }
    if (!g_pipRT || !g_pipSys || !g_pipTex11) return;
    bool ok = SUCCEEDED(IDirect3DDevice9_StretchRect(dev, src, NULL, g_pipRT, NULL, D3DTEXF_NONE)) &&
              SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_pipRT, g_pipSys));
    if (!ok) { static int l = 0; if (l++ < 3) Log("Pip-Boy wrist screen: couldn't copy the menu"); return; }
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(IDirect3DSurface9_LockRect(g_pipSys, &lr, NULL, 0))) {
        for (UINT y = 0; y < g_pipH; y++) { uint32_t *row = (uint32_t *)((BYTE *)lr.pBits + y * lr.Pitch); for (UINT x = 0; x < g_pipW; x++) row[x] |= 0xFF000000u; }
        ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_pipTex11, 0, NULL, lr.pBits, (UINT)lr.Pitch, 0);
        { static int snaps = 0; static double nextSnap = 0;
          if (snaps < 3 && QpcMs() > nextSnap) { nextSnap = QpcMs() + 2000.0; char nm[48]; snprintf(nm, sizeof nm, "FNVR_pipmenu_%d.bmp", snaps++); SaveBMP(nm, (const BYTE *)lr.pBits, lr.Pitch, g_pipW, g_pipH); Log("Pip-Boy menu: saved %s", nm); } }
        IDirect3DSurface9_UnlockRect(g_pipSys);
    }
    ID3D11DeviceContext_Flush(g_d11ctx);
    Texture_t t = { g_pipTex11, ETextureType_TextureType_DirectX, EColorSpace_ColorSpace_Auto };
    g_vrOverlay->SetOverlayTexture(g_pipOv, &t);
    if (boxMode) {   /* exactly over the Pip-Boy's dark screen box */
        g_vrOverlay->SetOverlayWidthInMeters(g_pipOv, g_pipBoxWidthM);
        g_vrOverlay->SetOverlayTransformAbsolute(g_pipOv, ETrackingUniverseOrigin_TrackingUniverseStanding, &g_pipBoxPose);
        if (!g_pipOvShown) { g_vrOverlay->ShowOverlay(g_pipOv); g_pipOvShown = true; Log("Pip-Boy menu: shown over the Pip-Boy's screen box (%.3f m wide)", g_pipBoxWidthM); }
        return;
    }
    /* on the back of the left wrist: across the forearm, facing out of the back of the hand */
    TrackedDeviceIndex_t li = g_vrSystem->GetTrackedDeviceIndexForControllerRole(
        g_leftHanded ? ETrackedControllerRole_TrackedControllerRole_RightHand : ETrackedControllerRole_TrackedControllerRole_LeftHand);
    if (li >= 16) return;
    float o = g_aimPitchOffsetDeg / RAD2DEG, co = cosf(o), so = sinf(o);
    float side = g_leftHanded ? 1.0f : -1.0f;                     /* back of the hand: -X for the left hand */
    float Xl[3] = { 1, 0, 0 }, Yt[3] = { 0, co, so }, Zt[3] = { 0, -so, co };
    float ovX[3], ovY[3], ovZ[3], P[3];
    for (int i = 0; i < 3; i++) { ovX[i] = -Zt[i]; ovZ[i] = side * Xl[i]; }   /* along the forearm toward the fingers; facing out */
    V3_Cross(ovY, ovZ, ovX);
    for (int i = 0; i < 3; i++) P[i] = Zt[i] * g_pipOvBack + side * Xl[i] * g_pipOvOut + Yt[i] * g_pipOvUp;
    HmdMatrix34_t m;
    for (int i = 0; i < 3; i++) { m.m[i][0] = ovX[i]; m.m[i][1] = ovY[i]; m.m[i][2] = ovZ[i]; m.m[i][3] = P[i]; }
    g_vrOverlay->SetOverlayTransformTrackedDeviceRelative(g_pipOv, li, &m);
    if (!g_pipOvShown) {
        g_vrOverlay->ShowOverlay(g_pipOv); g_pipOvShown = true;
        if (g_pOn) *(uint32_t *)(g_pOn + 0x30) |= 1u;   /* hide the game's floating copy */
        Log("Pip-Boy wrist screen: shown on your left wrist (%.2f m wide)", g_pipOvWidth);
    }
}

static void SaveBackbuffer(IDirect3DDevice9 *dev, const char *name)
{
    IDirect3DSurface9 *bb = NULL, *sys = NULL, *rs = NULL; D3DSURFACE_DESC bd;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return;
    IDirect3DSurface9_GetDesc(bb, &bd);
    IDirect3DSurface9 *src = bb;
    if (bd.MultiSampleType != D3DMULTISAMPLE_NONE &&
        SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, bd.Width, bd.Height, bd.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &rs, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, rs, NULL, D3DTEXF_NONE))) src = rs;
    if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, bd.Width, bd.Height, bd.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)) &&
        SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, src, sys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(sys, &lr, NULL, D3DLOCK_READONLY))) {
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, bd.Width, bd.Height);
            IDirect3DSurface9_UnlockRect(sys);
        }
    }
    if (sys) IDirect3DSurface9_Release(sys);
    if (rs) IDirect3DSurface9_Release(rs);
    IDirect3DSurface9_Release(bb);
}

static void Pip_Record(IDirect3DDevice9 *dev);
static void Grab_Record(IDirect3DDevice9 *dev);
static void Grap_Record(IDirect3DDevice9 *dev);
static void Rec_Release9(void);
/* ---- Find the part that shows the Pip-Boy's glowing menu text ----
   Hide each part near the Pip-Boy for one frame and count the amber menu-coloured pixels on screen;
   the part whose hiding removes the text is the menu. Runs once, a moment after the Pip-Boy opens. */
static char *g_tfCand[64]; static int g_tfN = 0, g_tfStep = -2; static long g_tfBase = 0, g_tfCount[64];
static IDirect3DSurface9 *g_tfRT = NULL, *g_tfSys = NULL;
static void TF_Walk(char *n, const float *A, int depth)
{
    if (!Rd(n, 0xC0) || depth > 40 || g_tfN >= 64) return;
    if (*(uint32_t *)(n + 0x30) & 1) return;
    const float *b = *(const float **)(n + 0x20);
    if (Rd(b, 16)) { float d[3]; V3Sub(d, b, A); if (V3Len(d) > b[3] + 30.0f) return; }
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        if (!Rd(kids, nk * 4)) return;
        for (int k = 0; k < nk; k++) TF_Walk(kids[k], A, depth + 1);
        return;
    }
    if (!Rd(b, 16) || b[3] < 0.5f || b[3] > 40.0f) return;
    g_tfCand[g_tfN++] = n;
}
static long TF_Count(IDirect3DDevice9 *dev)
{
    if (!g_tfRT) IDirect3DDevice9_CreateRenderTarget(dev, 640, 360, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_tfRT, NULL);
    if (!g_tfSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 640, 360, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_tfSys, NULL);
    IDirect3DSurface9 *bb = NULL; long n = -1;
    if (!g_tfRT || !g_tfSys || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return -1;
    if (SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_tfRT, NULL, D3DTEXF_LINEAR)) && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_tfRT, g_tfSys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(g_tfSys, &lr, NULL, D3DLOCK_READONLY))) {
            n = 0;
            for (int y = 0; y < 360; y++) { const uint32_t *row = (const uint32_t *)((const BYTE *)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < 640; x++) { uint32_t c = row[x]; int R = (c >> 16) & 255, G = (c >> 8) & 255, B = c & 255;
                    if (R > 110 && G > 60 && B < 90 && R - B > 70 && R >= G) n++; } }
            IDirect3DSurface9_UnlockRect(g_tfSys);
        }
    }
    IDirect3DSurface9_Release(bb);
    return n;
}
static void TF_Release9(void) { Release9(&g_tfRT); Release9(&g_tfSys); }
/* Narrow it down level by level: hide each child of a branch for one frame; the child whose hiding takes
   the amber menu text away is where the text lives - then look inside that child, and so on. */
/* only real game objects (their vtable lives in the game's own code), so we never call into garbage */
static bool IsNiObj(char *p) { if (!Rd(p, 0xC0)) return false; uintptr_t vt = *(uintptr_t *)p; return vt >= 0x01000000 && vt < 0x01300000 && Rd((void *)vt, 0x10); }
static void TF_AddKids(char *n)
{
    if (!IsNiObj(n) || !AsNode(n)) return;
    char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
    if (!Rd(kids, nk * 4)) return;
    for (int k = 0; k < nk && g_tfN < 64; k++) if (IsNiObj(kids[k]) && !(*(uint32_t *)(kids[k] + 0x30) & 1)) g_tfCand[g_tfN++] = kids[k];
}
static void TF_AddObj(char *n) { if (IsNiObj(n) && g_tfN < 64) { for (int i = 0; i < g_tfN; i++) if (g_tfCand[i] == n) return; g_tfCand[g_tfN++] = n; } }
static long TF_Count2(IDirect3DDevice9 *dev);
static void PipText_Find(IDirect3DDevice9 *dev)
{
    if (dev) return;   /* not needed: the menu is the Pip-Boy screen shape */
    static double openAt = 0; static int runs = 0, level = 0; static char *parent = NULL;
    bool up = g_pipOpen && g_pipModeRaw == 3 && g_wristPip;
    if (!up) {
        if (g_tfStep >= 0 && g_tfStep < g_tfN) *(uint32_t *)(g_tfCand[g_tfStep] + 0x30) &= ~1u;
        if (g_tfStep >= -1) { g_tfStep = -2; Log("Pip-Boy menu search: stopped (Pip-Boy closed)"); }
        openAt = 0; level = 0; parent = NULL; return;
    }
    if (g_menuTextNode || runs >= 2) return;
    if (openAt == 0) openAt = QpcMs();
    if (g_tfStep == -2) {
        if (QpcMs() - openAt < 2000) return;
        g_tfN = 0;
        if (!parent) {   /* top level: the whole world scene, plus the Pip-Boy menu's own pieces */
            char *sg = *(char **)SCENEGRAPH_PTR_ADDR; TF_AddKids(sg);
            char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
            if (Rd(pm, 0x40)) for (int o = 4; o < 0x40; o += 4) { char *q = *(char **)(pm + o); if (IsNiObj(q) && AsNode(q)) TF_AddObj(q); }
            char **arr = Rd((void *)0x011F350C, 4) ? *(char ***)0x011F350C : NULL;
            static const int ids[3] = { 0x3EA, 0x3EB, 0x3FF };
            for (int k = 0; arr && k < 3; k++) { char *tm = Rd(arr + (ids[k] - 0x3E9), 4) ? arr[ids[k] - 0x3E9] : NULL; if (Rd(tm, 0x30)) TF_AddObj(*(char **)(tm + 0x2C)); }
            if (Rd(im, 0x100)) for (int o = 0x40; o < 0x100; o += 4) { char *q = *(char **)(im + o); if (IsNiObj(q) && AsNode(q)) TF_AddObj(q); }
            runs++;
        } else TF_AddKids(parent);
        Log("Pip-Boy menu search (level %d): %d branches to try", level, g_tfN);
        if (!g_tfN) { g_tfStep = -2; openAt = QpcMs() + 1e9; return; }
        g_tfStep = -1; return;
    }
    if (g_tfStep == -1) { static int w = 0; if (++w < 5) return; w = 0; g_tfBase = TF_Count2(dev); Log("Pip-Boy menu search: %ld amber menu spots on screen", g_tfBase);
        if (g_tfBase < 40) { g_tfStep = -2; openAt = QpcMs() + 3000; return; }
        g_tfStep = 0; *(uint32_t *)(g_tfCand[0] + 0x30) |= 1u; return; }
    /* the game draws a frame or two behind, so keep each one hidden for a few frames before looking */
    static int held = 0;
    *(uint32_t *)(g_tfCand[g_tfStep] + 0x30) |= 1u;
    if (++held < 5) return;
    held = 0;
    long c = TF_Count2(dev);
    g_tfCount[g_tfStep] = c;
    *(uint32_t *)(g_tfCand[g_tfStep] + 0x30) &= ~1u;
    g_tfStep++;
    if (g_tfStep < g_tfN) { *(uint32_t *)(g_tfCand[g_tfStep] + 0x30) |= 1u; return; }
    int bi = -1; long bestDrop = 0;
    for (int i = 0; i < g_tfN; i++) {
        long drop = g_tfBase - g_tfCount[i];
        const char *nm = *(const char **)(g_tfCand[i] + 0x08); if (!Rd(nm, 1)) nm = "?";
        Log("   level %d: hiding '%s' (%s) -> %ld of %ld amber spots gone", level, nm, AsNode(g_tfCand[i]) ? "branch" : "shape", drop, g_tfBase);
        if (drop > bestDrop) { bestDrop = drop; bi = i; }
    }
    g_tfStep = -2;
    if (bi < 0 || bestDrop < g_tfBase * 5 / 10) {
        if (parent) { g_menuTextNode = parent; Log("Pip-Boy menu search: FOUND - the menu text is the branch '%s' itself", Rd(*(char **)(parent + 8), 1) ? *(char **)(parent + 8) : "?"); }
        else { Log("Pip-Boy menu search: the menu text isn't in anything I can hide (best %ld of %ld)", bestDrop, g_tfBase); openAt = QpcMs() + 1e9; }
        return;
    }
    char *hit = g_tfCand[bi];
    if (AsNode(hit) && level < 12) { parent = hit; level++; openAt = QpcMs() - 2000 + 100; return; }   /* look inside it next */
    g_menuTextNode = hit;
    const char *nm = *(const char **)(hit + 0x08); if (!Rd(nm, 1)) nm = "?";
    char *par = *(char **)(hit + 0x18); const char *pn = Rd(par, 0x10) ? *(const char **)(par + 0x08) : NULL; if (!Rd(pn, 1)) pn = "?";
    Log("Pip-Boy menu search: FOUND - the menu text is on '%s' (parent '%s')", nm, pn);
}
/* amber Pip-Boy text: strongly orange, low blue (skin and sand have more blue) */
static long TF_Count2(IDirect3DDevice9 *dev)
{
    if (!g_tfRT) IDirect3DDevice9_CreateRenderTarget(dev, 640, 360, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_tfRT, NULL);
    if (!g_tfSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 640, 360, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_tfSys, NULL);
    IDirect3DSurface9 *bb = NULL; long n = -1;
    if (!g_tfRT || !g_tfSys || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) return -1;
    if (SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_tfRT, NULL, D3DTEXF_LINEAR)) && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_tfRT, g_tfSys))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(g_tfSys, &lr, NULL, D3DLOCK_READONLY))) {
            n = 0;
            for (int y = 0; y < 360; y++) { const uint32_t *row = (const uint32_t *)((const BYTE *)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < 640; x++) { uint32_t c = row[x]; int R = (c >> 16) & 255, G = (c >> 8) & 255, B = c & 255;
                    if (R > 120 && G > 60 && B < 55 && R > G && R - G < 140) n++; } }
            IDirect3DSurface9_UnlockRect(g_tfSys);
        }
    }
    IDirect3DSurface9_Release(bb);
    return n;
}

static void Display_Frame(IDirect3DDevice9 *dev)
{
    UI_Present(dev);
    Settings_Frame();
    IntroSkip_Frame();
    PipOv_Frame(dev);
    PipText_Find(dev);
    Grab_Record(dev);
    Grap_Record(dev);
    if (g_headSnapAt > 0 && QpcMs() > g_headSnapAt) { g_headSnapAt = 0; SaveBackbuffer(dev, "FNVR_monitor.bmp"); g_headSnapPending = true; }
    bool scopeShown = g_scope.active;
    Scope_Draw(dev);
    if (g_trOn) { Tr_Flush(); Log("TRACE ===== Present (end of frame) ====="); char nm[64]; snprintf(nm, sizeof nm, "FNVR_trace_%s_final.bmp", g_trMode); SaveBackbuffer(dev, nm); g_trOn = 0; }
    PipScr_Draw(dev);
    if (g_dlgSnapWant) { g_dlgSnapWant = false; SaveBackbuffer(dev, "FNVR_dialogue.bmp"); Log("Dialogue: picture saved (FNVR_dialogue.bmp)"); }
    g_frameId++;
    Rope_Draw(dev);
    Pip_Snapshot(dev);   /* (after the menu is drawn, so the pictures show what you see) */
    Pip_Record(dev);
    Scope_Snapshot(dev, scopeShown);
    g_presentsSinceLog++;
    if (!g_vrCompositor || g_displayFailed) return;
    if (Far_Frame(dev)) { g_stereoCapturedThisFrame = false; return; }
    if (!g_stereoCapturedThisFrame) Capture_Eye(dev, 0);
    g_stereoCapturedThisFrame = false;
    Capture_FinishAndSubmit(dev);
}

/* After presenting: block until SteamVR is ready for the next frame and get its predicted pose. */
static void Display_WaitForNextFrame(void)
{
    if (!HeadsetDisplayActive()) return;
    TrackedDevicePose_t poses[16];
    static double lastEnd = 0;
    double w0 = QpcMs();
    EVRCompositorError ce = g_vrCompositor->WaitGetPoses(poses, 16, NULL, 0);
    double w1 = QpcMs();
    {   /* how long each frame takes vs. how long it waits for the headset */
        static double sumWork = 0, sumWait = 0, maxWork = 0, rep = 0; static int n = 0;
        if (lastEnd > 0) { double work = w0 - lastEnd; sumWork += work; if (work > maxWork) maxWork = work; sumWait += w1 - w0; n++; }
        lastEnd = w1;
        if (w1 - rep > 4000.0 && n > 0) {
            rep = w1;
            float hz = g_vrSystem ? g_vrSystem->GetFloatTrackedDeviceProperty(0, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, NULL) : 0;
            double avg = sumWork / n;
            Log("Frame pacing: game+FNVR work %.1f ms per frame (worst %.1f), waiting for the headset %.1f ms | headset %.0f Hz -> fits %s",
                avg, maxWork, sumWait / n, hz, hz > 1 && avg < 1000.0 / hz * 0.92 ? "every refresh" : "every 2nd refresh");
            sumWork = sumWait = maxWork = 0; n = 0;
        }
    }
    if (ce == EVRCompositorError_VRCompositorError_None) {
        memcpy(g_waitPoses, poses, sizeof poses);
        g_waitPose = poses[0];
        g_haveWaitPose = true;
    } else {
        static int errCount = 0;
        if (errCount++ < 10) Log("WaitGetPoses returned compositor error %d", (int)ce);
        g_haveWaitPose = false;
    }
}

/* ---- Stereo: render each frame twice, once per eye ------------------------- */

/* OSGlobals::DoRenderFrame(renderer, bool, bool) - draws one complete frame (world, weapon, HUD, menus).
   Called once per frame from the main loop at 0x8721A4. We redirect that call so it runs twice. */
#define DORENDERFRAME_ADDR      0x008706B0
#define DORENDERFRAME_CALLSITE  0x008721A4
#define NIAV_WORLD_TRANSLATE    0x8C        /* NiAVObject world position (x, y, z) */
#define NIAV_LOCAL_TRANSLATE    0x58        /* NiAVObject local position (relative to parent) */
#define NIAV_WORLD_ROTATE       0x68        /* NiAVObject world rotation, 3x3 row-major */
#define NIAV_WORLD_SCALE        0x98        /* NiAVObject world scale */
#define NIAV_PARENT             0x18        /* NiNode* parent */

typedef void (__attribute__((thiscall)) *DoRenderFrame_t)(void *osGlobals, void *renderer, uint32_t a, uint32_t b);
static DoRenderFrame_t o_DoRenderFrame = NULL;
static bool g_loggedStereoActive = false;
static bool g_loggedStereoCheck = false;

/* Right before drawing, make sure the camera uses exactly the headset-filling FOV. Other mods ease the
   FOV back toward their own value every frame; fixing it here keeps every drawn frame identical. */
static void LockFovForDraw(void)
{
    if (!g_fovOverridden || g_menuCached || g_targetFov <= 0.0f) return;
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
    char *player = (char *)GetPlayer();
    if (!sg || !player) return;
    if (fabsf(*(float *)(sg + SCENEGRAPH_FOV) - g_targetFov) > 0.01f) {
        *(float *)(player + PLAYER_WORLD_FOV) = g_targetFov;
        *(float *)(player + PLAYER_1ST_FOV) = g_targetFov;
        SetCameraFOV(sg, g_targetFov, false, NULL, false);
    }
}

/* ---- Turning the camera to the head while the game aims elsewhere ------ */

static float PermError(M3 k)
{
    float e = 0;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) { float a = fabsf(k[i][j]); e += fminf(a, fabsf(a - 1.0f)); }
    return e;
}
/* Head-style basis (columns right, up, back) in game world coords for a heading/elevation. */
static void GameBasis(M3 out, float heading, float elev)
{
    float f[3] = { sinf(heading) * cosf(elev), cosf(heading) * cosf(elev), sinf(elev) };
    float r[3] = { cosf(heading), -sinf(heading), 0.0f };
    float u[3] = { r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0] };
    for (int i = 0; i < 3; i++) { out[i][0] = r[i]; out[i][1] = u[i]; out[i][2] = -f[i]; }
}

static int  g_camConvention = 0;   /* 0 = unknown, 1 = world = C*local, 2 = stored transposed, -1 = failed */
static bool g_loggedTurnCheck = false;
static int  g_wtcConvention = 0;   /* world-to-screen matrix layout: 0 unknown, 1 direct, 2 transposed, -1 unknown */
#define NICAMERA_WORLD_TO_CAM 0x9C

static __attribute__((unused)) bool Camera_TurnToHead(char *cam, void *player, float savedR[25])
{
    if (g_camConvention < 0) return false;
    float *Cf = (float *)(cam + NIAV_WORLD_ROTATE);
    memcpy(savedR, Cf, 9 * sizeof(float));
    M3 C; memcpy(C, Cf, sizeof C);

    /* What the game's camera represents: aim heading/pitch, no roll. */
    M3 A; GameBasis(A, *PlayerRotZ(player), -*PlayerRotX(player));
    M3 At; M3T(At, A);
    M3 K1, K2, Ct;
    M3Mul(K1, At, C);
    M3T(Ct, C); M3Mul(K2, At, Ct);

    if (g_camConvention == 0) {
        float e1 = PermError(K1), e2 = PermError(K2);
        if (e1 < 0.6f && e1 <= e2) g_camConvention = 1;
        else if (e2 < 0.6f) g_camConvention = 2;
        Log("Camera convention check: error %.3f (direct) / %.3f (transposed) -> %s", e1, e2,
            g_camConvention == 1 ? "direct" : g_camConvention == 2 ? "transposed" : "undetermined, retrying");
        static int tries = 0;
        if (!g_camConvention && ++tries > 120) { g_camConvention = -1; Log("Camera turning disabled: could not match the camera"); }
        if (!g_camConvention) return false;
    }

    /* Head rotation in game world: map VR axes (x right, y up, z back) into the game at the body offset. */
    float O = g_bodyOffset;
    M3 Mvr = { { cosf(O), 0.0f, -sinf(O) }, { -sinf(O), 0.0f, -cosf(O) }, { 0.0f, 1.0f, 0.0f } };
    M3 H; memcpy(H, g_camHead, sizeof H);
    M3 Hg; M3Mul(Hg, Mvr, H);

    M3 Cn;
    if (g_camConvention == 1) M3Mul(Cn, Hg, K1);
    else { M3 t; M3Mul(t, Hg, K2); M3T(Cn, t); }
    memcpy(Cf, Cn, sizeof Cn);

    /* The camera also keeps a cached world-to-screen matrix, which the game uses to decide what's
       visible. Rotate it the same way, or objects you look at (but don't aim at) pop in and out. */
    float *W = (float *)(cam + NICAMERA_WORLD_TO_CAM);   /* 4x4 */
    memcpy(savedR + 9, W, 16 * sizeof(float));
    const float *T = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    if (g_wtcConvention == 0) {
        /* A point straight ahead of the game camera must land in the middle of the screen. */
        float p[3];
        for (int i = 0; i < 3; i++) p[i] = T[i] - A[i][2] * 1000.0f;   /* A col 2 = back */
        float r1[4], r2[4];
        for (int i = 0; i < 4; i++) {
            r1[i] = W[i * 4 + 0] * p[0] + W[i * 4 + 1] * p[1] + W[i * 4 + 2] * p[2] + W[i * 4 + 3];
            r2[i] = W[0 * 4 + i] * p[0] + W[1 * 4 + i] * p[1] + W[2 * 4 + i] * p[2] + W[3 * 4 + i];
        }
        bool c1 = fabsf(r1[3]) > 1e-3f && fabsf(r1[0] / r1[3]) < 0.05f && fabsf(r1[1] / r1[3]) < 0.05f;
        bool c2 = fabsf(r2[3]) > 1e-3f && fabsf(r2[0] / r2[3]) < 0.05f && fabsf(r2[1] / r2[3]) < 0.05f;
        g_wtcConvention = c1 ? 1 : c2 ? 2 : -1;
        Log("Visibility matrix check: direct (%.3f,%.3f) transposed (%.3f,%.3f) -> %s",
            r1[3] ? r1[0] / r1[3] : 99.0f, r1[3] ? r1[1] / r1[3] : 99.0f, r2[3] ? r2[0] / r2[3] : 99.0f, r2[3] ? r2[1] / r2[3] : 99.0f,
            g_wtcConvention == 1 ? "direct" : g_wtcConvention == 2 ? "transposed" : "not recognised (left alone)");
    }
    if (g_wtcConvention > 0) {
        /* Q maps world points so the old camera sees them where the new camera should. */
        M3 CnT, Q; M3T(CnT, Cn); M3Mul(Q, C, CnT);
        float P[4][4] = { { 0 } };
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) P[i][j] = Q[i][j];
            P[i][3] = T[i] - (Q[i][0] * T[0] + Q[i][1] * T[1] + Q[i][2] * T[2]);
        }
        P[3][3] = 1.0f;
        float Wn[16];
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
            float acc = 0;
            for (int k = 0; k < 4; k++)
                acc += g_wtcConvention == 1 ? savedR[9 + i * 4 + k] * P[k][j]      /* W * P   */
                                            : P[k][i] * savedR[9 + k * 4 + j];     /* P^T * W */
            Wn[i * 4 + j] = acc;
        }
        memcpy(W, Wn, sizeof Wn);
    }
    return true;
}

static __attribute__((unused)) void Camera_Restore(char *cam, const float savedR[25])
{
    memcpy((float *)(cam + NICAMERA_WORLD_TO_CAM), savedR + 9, 16 * sizeof(float));
    float *Cf = (float *)(cam + NIAV_WORLD_ROTATE);
    if (!g_loggedTurnCheck) {
        g_loggedTurnCheck = true;
        Log("Camera turn check: rotation %s while drawing", fabsf(Cf[0] - savedR[0]) + fabsf(Cf[4] - savedR[4]) < 1e-6f &&
            memcmp(Cf, savedR, 9 * sizeof(float)) == 0 ? "was reset by the game (override lost)" : "kept");
    }
    memcpy(Cf, savedR, 9 * sizeof(float));
}

/* ---- Generic function-entry hook with trampoline -------------------------- */

static void *HookEntry(DWORD addr, const BYTE *expected, int len, void *hook, const char *name)
{
    BYTE *fn = (BYTE *)addr;
    if (memcmp(fn, expected, len) != 0) {
        Log("%s: unexpected bytes at %08lX (%02X %02X %02X %02X %02X) - not hooked", name, (unsigned long)addr, fn[0], fn[1], fn[2], fn[3], fn[4]);
        return NULL;
    }
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return NULL;
    memcpy(tramp, fn, len);
    tramp[len] = 0xE9;
    *(int32_t *)(tramp + len + 1) = (int32_t)((addr + len) - ((uintptr_t)tramp + len + 5));
    FlushInstructionCache(GetCurrentProcess(), tramp, 64);
    DWORD old;
    VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int32_t *)(fn + 1) = (int32_t)((uintptr_t)hook - (addr + 5));
    for (int i = 5; i < len; i++) fn[i] = 0x90;
    VirtualProtect(fn, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, len);
    Log("%s: hooked at %08lX", name, (unsigned long)addr);
    return tramp;
}


/* ---- Shader cache ----------------------------------------------------------
   ENBoost rebuilds game shaders through D3DX the first time each one is used (a hit, a muzzle flash,
   the Pip-Boy...), and each build freezes the game for seconds. The results never change, so FNVR
   keeps every compiled shader on disk and hands it straight back the next time. */
typedef struct { const char *Name, *Definition; } D3DXMACRO_;
typedef HRESULT (WINAPI *D3DXCompileShader_t)(const char *src, UINT len, const D3DXMACRO_ *defs, void *inc,
    const char *fn, const char *profile, DWORD flags, void **shader, void **errors, void **ctab);
typedef HRESULT (WINAPI *D3DXAssembleShader_t)(const char *src, UINT len, const D3DXMACRO_ *defs, void *inc,
    DWORD flags, void **shader, void **errors);
typedef HRESULT (WINAPI *D3DXCreateBuffer_t)(DWORD bytes, void **buf);
typedef HRESULT (WINAPI *D3DXGetShaderConstantTable_t)(const DWORD *fn, void **ctab);
static D3DXCompileShader_t  o_D3DXCompileShader = NULL;
static D3DXAssembleShader_t o_D3DXAssembleShader = NULL;
static D3DXCreateBuffer_t   p_D3DXCreateBuffer = NULL;
static D3DXGetShaderConstantTable_t p_D3DXGetShaderConstantTable = NULL;
static char g_shaderDir[MAX_PATH] = "";

static uint64_t Fnv64(uint64_t h, const void *data, size_t n)
{
    const BYTE *b = (const BYTE *)data;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
static uint64_t ShaderKey(char kind, const char *src, UINT len, const D3DXMACRO_ *defs, const char *fn, const char *prof, DWORD flags)
{
    uint64_t h = 14695981039346656037ull;
    h = Fnv64(h, &kind, 1);
    h = Fnv64(h, src, len);
    for (int i = 0; defs && defs[i].Name && i < 256; i++) {
        h = Fnv64(h, defs[i].Name, strlen(defs[i].Name) + 1);
        if (defs[i].Definition) h = Fnv64(h, defs[i].Definition, strlen(defs[i].Definition) + 1);
    }
    if (fn) h = Fnv64(h, fn, strlen(fn) + 1);
    if (prof) h = Fnv64(h, prof, strlen(prof) + 1);
    h = Fnv64(h, &flags, sizeof flags);
    return h;
}
static void ShaderPath(uint64_t key, char *out, size_t n)
{
    snprintf(out, n, "%s%08lX%08lX.bin", g_shaderDir, (unsigned long)(key >> 32), (unsigned long)key);
}
/* Cached bytecode -> new ID3DXBuffer, or false. */
static bool ShaderLoad(uint64_t key, void **out)
{
    char path[MAX_PATH]; ShaderPath(key, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    bool ok = false;
    void *buf = NULL;
    if (n > 0 && n < 4 * 1024 * 1024 && SUCCEEDED(p_D3DXCreateBuffer((DWORD)n, &buf))) {
        void *mem = ((void *(WINAPI *)(void *))(*(void ***)buf)[3])(buf);   /* GetBufferPointer */
        if (fread(mem, 1, (size_t)n, f) == (size_t)n) { *out = buf; ok = true; }
        else ((ULONG (WINAPI *)(void *))(*(void ***)buf)[2])(buf);         /* Release */
    }
    fclose(f);
    return ok;
}
static void ShaderSave(uint64_t key, void *buf)
{
    if (!buf) return;
    void *mem = ((void *(WINAPI *)(void *))(*(void ***)buf)[3])(buf);
    DWORD n = ((DWORD (WINAPI *)(void *))(*(void ***)buf)[4])(buf);          /* GetBufferSize */
    char path[MAX_PATH], tmp[MAX_PATH]; ShaderPath(key, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    bool ok = fwrite(mem, 1, n, f) == n;
    fclose(f);
    if (ok) MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING); else DeleteFileA(tmp);
}

static HRESULT WINAPI Hook_D3DXCompileShader(const char *src, UINT len, const D3DXMACRO_ *defs, void *inc,
    const char *fn, const char *profile, DWORD flags, void **shader, void **errors, void **ctab)
{
    uint64_t key = 0;
    if (src && len && shader && p_D3DXCreateBuffer) {
        key = ShaderKey('C', src, len, defs, fn, profile, flags);
        void *buf = NULL;
        if (ShaderLoad(key, &buf)) {
            const DWORD *code = ((const DWORD *(WINAPI *)(void *))(*(void ***)buf)[3])(buf);
            if (ctab) { *ctab = NULL; if (p_D3DXGetShaderConstantTable) p_D3DXGetShaderConstantTable(code, ctab); }
            if (errors) *errors = NULL;
            *shader = buf;
            g_shHits++;
            return S_OK;
        }
    }
    double t0 = QpcMs();
    HRESULT hr = o_D3DXCompileShader(src, len, defs, inc, fn, profile, flags, shader, errors, ctab);
    double ms = QpcMs() - t0;
    g_shMisses++; g_shMissMs += ms;
    if (SUCCEEDED(hr) && key && *shader) ShaderSave(key, *shader);
    static int logs = 0;
    if (ms > 200.0 && logs++ < 10) Log("Shader cache: compiled a %s shader in %.0f ms (saved for next time)", profile ? profile : "?", ms);
    return hr;
}

static HRESULT WINAPI Hook_D3DXAssembleShader(const char *src, UINT len, const D3DXMACRO_ *defs, void *inc,
    DWORD flags, void **shader, void **errors)
{
    uint64_t key = 0;
    if (src && len && shader && p_D3DXCreateBuffer) {
        key = ShaderKey('A', src, len, defs, NULL, NULL, flags);
        void *buf = NULL;
        if (ShaderLoad(key, &buf)) { if (errors) *errors = NULL; *shader = buf; g_shHits++; return S_OK; }
    }
    double t0 = QpcMs();
    HRESULT hr = o_D3DXAssembleShader(src, len, defs, inc, flags, shader, errors);
    g_shMisses++; g_shMissMs += QpcMs() - t0;
    if (SUCCEEDED(hr) && key && *shader) ShaderSave(key, *shader);
    return hr;
}

/* Length of a recognised function prologue (whole instructions, >= 5 bytes), or 0. */
static int PrologueLen(const BYTE *p)
{
    int n = 0;
    while (n < 5) {
        const BYTE *q = p + n;
        if (q[0] == 0x8B && q[1] == 0xFF) n += 2;                       /* mov edi,edi */
        else if (q[0] == 0x55) n += 1;                                  /* push ebp */
        else if (q[0] == 0x8B && q[1] == 0xEC) n += 2;                  /* mov ebp,esp */
        else if (q[0] == 0x83 && q[1] == 0xEC) n += 3;                  /* sub esp,imm8 */
        else if (q[0] == 0x81 && q[1] == 0xEC) n += 6;                  /* sub esp,imm32 */
        else if (q[0] == 0x6A) n += 2;                                  /* push imm8 */
        else if (q[0] == 0x68) n += 5;                                  /* push imm32 */
        else if (q[0] >= 0x50 && q[0] <= 0x57) n += 1;                  /* push reg */
        else if (q[0] == 0x8B && q[1] == 0x45) n += 3;                  /* mov eax,[ebp+d8] */
        else if (q[0] == 0x83 && q[1] == 0xE4) n += 3;                  /* and esp,imm8 */
        else return 0;
    }
    return n;
}

static void *HookExport(HMODULE m, const char *name, void *hook)
{
    BYTE *fn = (BYTE *)GetProcAddress(m, name);
    if (!fn) { Log("Shader cache: %s not found", name); return NULL; }
    if (fn[0] == 0xE9) Log("Shader cache: %s already starts with a jump (another mod hooks it) - chaining", name);
    int len = fn[0] == 0xE9 ? 5 : PrologueLen(fn);
    if (!len) { Log("Shader cache: %s starts %02X %02X %02X %02X %02X %02X - not hooked", name, fn[0], fn[1], fn[2], fn[3], fn[4], fn[5]); return NULL; }
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return NULL;
    if (fn[0] == 0xE9) {   /* follow the existing jump */
        tramp[0] = 0xE9;
        uintptr_t target = (uintptr_t)fn + 5 + *(int32_t *)(fn + 1);
        *(int32_t *)(tramp + 1) = (int32_t)(target - ((uintptr_t)tramp + 5));
    } else {
        memcpy(tramp, fn, len);
        tramp[len] = 0xE9;
        *(int32_t *)(tramp + len + 1) = (int32_t)(((uintptr_t)fn + len) - ((uintptr_t)tramp + len + 5));
    }
    FlushInstructionCache(GetCurrentProcess(), tramp, 64);
    DWORD old;
    VirtualProtect(fn, len, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int32_t *)(fn + 1) = (int32_t)((uintptr_t)hook - ((uintptr_t)fn + 5));
    for (int i = 5; i < len; i++) fn[i] = 0x90;
    VirtualProtect(fn, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, len);
    return tramp;
}

static void InstallShaderCache(void)
{
    static bool done = false;
    if (done) return;
    done = true;
    if (!GetModuleHandleA("d3d9.dll") || !GetProcAddress(GetModuleHandleA("d3d9.dll"), "Direct3DCreate9")) return;
    HMODULE m = LoadLibraryA("d3dx9_43.dll");
    if (!m) { Log("Shader cache: d3dx9_43.dll not available"); return; }
    snprintf(g_shaderDir, sizeof g_shaderDir, "%sFNVR\\shadercache\\", g_pluginDir);
    CreateDirectoryA(g_shaderDir, NULL);
    p_D3DXCreateBuffer = (D3DXCreateBuffer_t)GetProcAddress(m, "D3DXCreateBuffer");
    p_D3DXGetShaderConstantTable = (D3DXGetShaderConstantTable_t)GetProcAddress(m, "D3DXGetShaderConstantTable");
    if (!p_D3DXCreateBuffer) { Log("Shader cache: D3DXCreateBuffer missing - off"); return; }
    o_D3DXCompileShader = (D3DXCompileShader_t)HookExport(m, "D3DXCompileShader", (void *)Hook_D3DXCompileShader);
    o_D3DXAssembleShader = (D3DXAssembleShader_t)HookExport(m, "D3DXAssembleShader", (void *)Hook_D3DXAssembleShader);
    WIN32_FIND_DATAA fd; char pat[MAX_PATH]; int count = 0;
    snprintf(pat, sizeof pat, "%s*.bin", g_shaderDir);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) { do count++; while (FindNextFileA(h, &fd)); FindClose(h); }
    Log("Shader cache: %s (compile %s, assemble %s), %d shaders cached in %s", o_D3DXCompileShader || o_D3DXAssembleShader ? "on" : "OFF",
        o_D3DXCompileShader ? "hooked" : "no", o_D3DXAssembleShader ? "hooked" : "no", count, g_shaderDir);
}

/* ---- Shots follow the hand ---------------------------------------------- */

static bool Arms_Apply(char *player, char *cam);
static void Arms_Restore(void);

/* TESObjectWEAP::Fire(Actor *shooter) - thiscall, ret 4. The shot direction comes from the
   shooter's facing, so for the player we point the facing at the hand for the duration of the call. */
#define FIREWEAPON_ADDR 0x00523150
typedef void (__attribute__((thiscall)) *FireWeapon_t)(void *weapon, void *actor);
static FireWeapon_t o_FireWeapon = NULL;
static unsigned g_shotsRedirected = 0;

static bool g_scopeSighting;
static void __attribute__((fastcall)) Hook_FireWeapon(void *weapon, void *unusedEdx, void *actor)
{
    (void)unusedEdx;
    void *player = GetPlayer();
    if (actor && actor == player && g_handAimValid && !g_menuCached && !g_adsActive && QpcMs() >= g_weaponSettleUntil) {
        float *rx = PlayerRotX(player), *rz = PlayerRotZ(player);
        float sx = *rx, sz = *rz;
        /* Put the arms (and so the gun's muzzle) where your hands are while the shot is created, so it
           leaves from the barrel in your hand instead of from the gun's normal spot near your eyes. */
        char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
        char *cam = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
        bool arms = cam && Arms_Apply((char *)player, cam);
        *rz = g_handAimHeading + g_shotYawTrimDeg / RAD2DEG;
        *rx = g_handAimPitch + g_shotPitchTrimDeg / RAD2DEG;
        /* perfect accuracy: no weapon spread for your shots (min spread +0x104, spread +0x108) */
        float *spr = (float *)((char *)weapon + 0x104), sp0 = spr[0], sp1 = spr[1];
        static int sl = 0;
        if (sl++ < 3) Log("Accuracy: weapon spread %.3f / min spread %.3f set to 0 for the shot", sp1, sp0);
        spr[0] = 0.0f; spr[1] = 0.0f;
        /* The game bends each bullet toward wherever the camera (your head) is looking. For the moment
           the shot is created, point the camera down the gun's aim line so nothing gets bent. */
        float camSave[9]; bool camTurned = false;
        if (cam) {
            float h = *rz, p = *rx;
            float f[3] = { sinf(h) * cosf(p), cosf(h) * cosf(p), -sinf(p) };
            float u[3] = { 0, 0, 1 }, pd = V3Dot(u, f), r[3];
            for (int i = 0; i < 3; i++) u[i] -= pd * f[i];
            float ul = V3Len(u);
            if (ul > 0.05f) {
                for (int i = 0; i < 3; i++) u[i] /= ul;
                V3Cross(r, f, u);
                float *Cw = (float *)(cam + NIAV_WORLD_ROTATE);
                memcpy(camSave, Cw, sizeof camSave);
                for (int i = 0; i < 3; i++) { Cw[i * 3 + 0] = f[i]; Cw[i * 3 + 1] = u[i]; Cw[i * 3 + 2] = r[i]; }
                camTurned = true;
            }
        }
        /* The game aims the bullet from the barrel at the spot your head is looking at (stored when the
           frame was updated). Find any stored world point that lies exactly on your head's line of sight
           and move it onto the gun's aim line for this shot. */
        typedef struct { float *p; float old[3]; } AimPatch;
        AimPatch ap[16]; int nap = 0;
        if (cam && camTurned && g_scopeSighting) {
            const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
            float H[3] = { camSave[0], camSave[3], camSave[6] };          /* head look direction */
            float f[3];
            { const float *Cw = (const float *)(cam + NIAV_WORLD_ROTATE); f[0] = Cw[0]; f[1] = Cw[3]; f[2] = Cw[6]; }
            char *areas[3] = { (char *)player, *(char **)((char *)player + 0x68), NULL };
            int sizes[3] = { 0x1000, 0x800, 0 };
            static int al = 0;
            char found[400] = "";
            for (int a = 0; a < 2; a++) {
                char *base = areas[a]; if (!base) continue;
                for (int off = 0; off + 12 <= sizes[a] && nap < 16; off += 4) {
                    float *v = (float *)(base + off);
                    if (IsBadReadPtr(v, 12)) break;
                    if (!isfinite(v[0]) || !isfinite(v[1]) || !isfinite(v[2])) continue;
                    float d[3]; V3Sub(d, v, C);
                    float L = V3Len(d);
                    if (L < 40.0f || L > 300000.0f) continue;
                    if (V3Dot(d, H) / L < 0.9995f) continue;              /* exactly where the head looks */
                    if (V3Dot(d, f) / L > 0.9995f) continue;              /* already on the aim line */
                    ap[nap].p = v; memcpy(ap[nap].old, v, 12);
                    for (int k = 0; k < 3; k++) v[k] = C[k] + f[k] * L;
                    if (al < 3) { char t[48]; snprintf(t, sizeof t, " %s+%X(%.0f)", a ? "process" : "player", off, L); strncat(found, t, sizeof found - strlen(found) - 1); }
                    nap++; off += 8;
                }
            }
            if (al++ < 3) Log("Aim point: head-look points moved onto the aim line for the shot:%s", nap ? found : " none found");
        }
        o_FireWeapon(weapon, actor);
        for (int i = 0; i < nap; i++) memcpy(ap[i].p, ap[i].old, 12);
        if (camTurned) memcpy(cam + NIAV_WORLD_ROTATE, camSave, sizeof camSave);
        spr[0] = sp0; spr[1] = sp1;
        *rx = sx; *rz = sz;
        if (arms) Arms_Restore();
        if (g_shotsRedirected++ < 3)
            Log("Shot fired along hand aim (heading %.1f pitch %.1f; head was %.1f / %.1f)",
                g_handAimHeading * RAD2DEG, g_handAimPitch * RAD2DEG, sz * RAD2DEG, sx * RAD2DEG);
        return;
    }
    o_FireWeapon(weapon, actor);
}

/* Projectile::Spawn(projectile, shooter, combatCtrl, weapon, pos x/y/z, rotZ, rotX, ...) - cdecl.
   The game hands it the final launch direction (with its random spread already added). While you're
   sighting through a scope, FNVR sends the bullet exactly down the scope's line instead, so it lands
   on the crosshair. */
#define PROJSPAWN_ADDR 0x009BCA60
typedef void *(__cdecl *ProjSpawn_t)(void *, void *, void *, void *, float, float, float, float, float, void *, void *, uint32_t, uint32_t, float, float, uint32_t);
static ProjSpawn_t o_ProjSpawn = NULL;
static bool g_scopeSighting = false;
static float g_scopeEye[3];
static float g_projAxis[3]; static bool g_projAxisOk = false;
static char *g_weaponNode;
static char *FindBone(char *node, const char *name, int depth);
static void *LookupForm(uint32_t id);
typedef struct { char *ref; uint32_t refID; int frames, n; float h, p, eye[3]; float last[3]; } BulletTrack;
static BulletTrack g_trk; static int g_tracked = 0;
static void Trk_Offset(const float *pos, float *along, float *right, float *up)
{
    float f[3] = { sinf(g_trk.h) * cosf(g_trk.p), cosf(g_trk.h) * cosf(g_trk.p), -sinf(g_trk.p) };
    float r[3] = { cosf(g_trk.h), -sinf(g_trk.h), 0.0f }, u[3]; V3Cross(u, r, f);
    float d[3]; V3Sub(d, pos, g_trk.eye);
    *along = V3Dot(d, f); *right = V3Dot(d, r); *up = V3Dot(d, u);
}
static void Trk_Dump(const char *when)
{
    char line[6000]; int n = 0;
    for (int off = 0; off < 0x400 && !IsBadReadPtr(g_trk.ref + off, 4) && n < (int)sizeof line - 40; off += 4) {
        float v = *(float *)(g_trk.ref + off);
        uint32_t u = *(uint32_t *)(g_trk.ref + off);
        if (u == 0) continue;
        if (isfinite(v) && fabsf(v) > 1e-4f && fabsf(v) < 1e7f) n += snprintf(line + n, sizeof line - n, " %X=%.4g", off, v);
        else n += snprintf(line + n, sizeof line - n, " %X=#%08X", off, u);
    }
    Log("Bullet 1 memory %s:%s", when, line);
}
/* once per frame: where the tracked bullet really is */
static void Trk_Update(void)
{
    if (!g_trk.ref) return;
    g_trk.frames++;
    bool alive = LookupForm(g_trk.refID) == (void *)g_trk.ref && g_trk.frames < 120;
    if (alive) memcpy(g_trk.last, g_trk.ref + 0x30, 12);
    int fr = g_trk.frames;
    if (alive && g_trk.n == 1 && fr == 1) Trk_Dump("after 1 frame");
    if (!alive || fr == 1 || fr == 2 || fr == 4 || fr == 8 || fr == 16 || fr == 32) {
        float al, rt, up; Trk_Offset(g_trk.last, &al, &rt, &up);
        Log("Bullet %d: frame %d%s - %.0f units out, %.1f right, %.1f up of the crosshair line (rot %.2f/%.2f, aimed %.2f/%.2f)",
            g_trk.n, fr, alive ? "" : " (gone - last seen)", al, rt, up,
            alive ? *(float *)(g_trk.ref + 0x2C) * RAD2DEG : 0.0f, alive ? *(float *)(g_trk.ref + 0x24) * RAD2DEG : 0.0f, g_trk.h * RAD2DEG, g_trk.p * RAD2DEG);
    }
    if (!alive) g_trk.ref = NULL;
}
static void *__cdecl Hook_ProjSpawn(void *proj, void *shooter, void *cc, void *weap, float px, float py, float pz, float rotZ, float rotX,
                                    void *node, void *target, uint32_t b1, uint32_t b2, float f1, float f2, uint32_t z)
{
    if (shooter && shooter == GetPlayer() && g_handAimValid && !g_menuCached && !g_adsActive && QpcMs() >= g_weaponSettleUntil) {
        float h = g_handAimHeading + g_shotYawTrimDeg / RAD2DEG, pt = g_handAimPitch + g_shotPitchTrimDeg / RAD2DEG;
        float dz = remainderf(rotZ - h, 2.0f * PI_F), dx = rotX - pt;
        bool same = fabsf(dz) < 0.26f && fabsf(dx) < 0.26f;    /* same angle convention, within 15 degrees */
        static int logs = 0;
        if (logs++ < 6) Log("Bullet launch: game direction is %+.2f deg sideways, %+.2f deg up/down from the hand aim%s",
                            dz * RAD2DEG, dx * RAD2DEG, g_scopeSighting ? (same ? " - scope: sent straight down the crosshair" : " - not corrected (unexpected angles)") : "");
        if (g_scopeSighting && same) { rotZ = h; rotX = pt; }
        /* The game launches the bullet along the gun model's muzzle point (node), not the angles above.
           Learn which way that point "shoots" in its own frame, then turn it onto the aim line while the
           bullet is created. */
        float fA[3] = { sinf(h) * cosf(pt), cosf(h) * cosf(pt), -sinf(pt) };
        char *pnode = (char *)node;
        if (!pnode && g_weaponNode) pnode = FindBone(g_weaponNode, "ProjectileNode", 0);   /* the gun model's muzzle point */
        float *Nw = pnode && !IsBadReadPtr(pnode + NIAV_WORLD_ROTATE, 36) ? (float *)(pnode + NIAV_WORLD_ROTATE) : NULL;
        static int nl = 0;
        if (nl++ < 2) Log("Bullet launch: muzzle point argument %p, model ProjectileNode %p", node, (void *)pnode);
        M3 N0; bool nodeTurned = false;
        if (Nw) memcpy(N0, Nw, sizeof N0);
        if (Nw && g_scopeSighting && same && g_projAxisOk && false) {
            float gp[3]; for (int i = 0; i < 3; i++) gp[i] = N0[i][0] * g_projAxis[0] + N0[i][1] * g_projAxis[1] + N0[i][2] * g_projAxis[2];
            float v[3]; V3Cross(v, gp, fA);
            float sn = V3Len(v), c = V3Dot(gp, fA);
            if (sn > 1e-5f && c > 0.9f) {          /* rotation taking the predicted direction onto the aim (Rodrigues) */
                float K[3][3] = { { 0, -v[2], v[1] }, { v[2], 0, -v[0] }, { -v[1], v[0], 0 } }, Q[3][3];
                float k2 = (1.0f - c) / (sn * sn);
                for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
                    float kk = 0; for (int m = 0; m < 3; m++) kk += K[i][m] * K[m][j];
                    Q[i][j] = (i == j) + K[i][j] + kk * k2;
                }
                M3 N1; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) N1[i][j] = Q[i][0] * N0[0][j] + Q[i][1] * N0[1][j] + Q[i][2] * N0[2][j];
                memcpy(Nw, N1, sizeof N1); nodeTurned = true;
            }
        }
        char *ref = (char *)o_ProjSpawn(proj, shooter, cc, weap, px, py, pz, rotZ, rotX, node, target, b1, b2, f1, f2, z);
        if (nodeTurned) memcpy(Nw, N0, sizeof N0);
        if (Nw && ref && g_scopeSighting && same && !nodeTurned && !g_projAxisOk && false) {   /* learn the muzzle point's shooting axis */
            float r0x = *(float *)(ref + 0x24), r0z = *(float *)(ref + 0x2C);
            float g[3] = { sinf(r0z) * cosf(r0x), cosf(r0z) * cosf(r0x), -sinf(r0x) };
            for (int i = 0; i < 3; i++) g_projAxis[i] = N0[0][i] * g[0] + N0[1][i] * g[1] + N0[2][i] * g[2];
            g_projAxisOk = true;
            const char *nn = *(const char **)(pnode + 0x08);
            Log("Bullet launch: the muzzle point \"%s\" shoots along its local (%.3f, %.3f, %.3f) - corrected from the next shot on",
                nn && !IsBadStringPtrA(nn, 64) ? nn : "?", g_projAxis[0], g_projAxis[1], g_projAxis[2]);
        }
        if (g_scopeSighting && same && ref) {
            /* the game turns the bullet after creating it (spread / aim toward where your head looks):
               put it back on the crosshair line, along with any stored direction vector that matches */
            float r0x = *(float *)(ref + 0x24), r0z = *(float *)(ref + 0x2C);
            float f[3] = { sinf(h) * cosf(pt), cosf(h) * cosf(pt), -sinf(pt) };
            float g[3] = { sinf(r0z) * cosf(r0x), cosf(r0z) * cosf(r0x), -sinf(r0x) };
            char found[256] = ""; int nf = 0;
            for (int off = 0x3C; off <= 0x400; off += 4) {
                float *v = (float *)(ref + off);
                if (IsBadReadPtr(v, 12)) break;
                float L = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (!(L > 0.0001f) || !isfinite(L) || L > 1e6f) continue;
                if (fabsf(v[0]) < 1e-3f || fabsf(v[1]) < 1e-3f) continue;     /* a real 3D direction has all parts */
                float c = (v[0] * g[0] + v[1] * g[1] + v[2] * g[2]) / L, cf = (v[0] * f[0] + v[1] * f[1] + v[2] * f[2]) / L;
                bool lenOk = fabsf(L - 1.0f) < 0.01f || L > 5.0f;   /* a unit direction or a velocity */
                if (lenOk && (c > 0.9998f || cf > 0.9998f)) {      /* points the way the bullet goes */
                    if (g_tracked < 6 && nf < 6) { char t[48]; snprintf(t, sizeof t, " +%X(len %.1f)", off, L); strcat(found, t); }
                    for (int k = 0; k < 3; k++) v[k] = f[k] * L;
                    nf++; off += 8;
                }
            }
            if (g_tracked < 6)
                Log("Bullet %d: right after creation it pointed %+.2f deg sideways, %+.2f deg down vs the crosshair; extra args %.4f %.4f %u %u; direction values fixed:%s",
                    g_tracked + 1, remainderf(r0z - h, 2.0f * PI_F) * RAD2DEG, (r0x - pt) * RAD2DEG, f1, f2, b1 & 0xFF, b2 & 0xFF, nf ? found : " none found");
            *(float *)(ref + 0x24) = pt; *(float *)(ref + 0x2C) = h;
        }
        if (g_scopeSighting && ref && g_tracked < 6) {   /* follow this bullet for a moment (diagnostics) */
            g_tracked++;
            g_trk.ref = ref; g_trk.refID = *(uint32_t *)(ref + 0x0C); g_trk.frames = 0; g_trk.n = g_tracked;
            g_trk.h = rotZ; g_trk.p = rotX; memcpy(g_trk.eye, g_scopeEye, 12);
            float m[3] = { px, py, pz }; float rt, up, al; Trk_Offset(m, &al, &rt, &up);
            memcpy(g_trk.last, m, 12);
            if (g_trk.n == 1) Trk_Dump("at creation");
            Log("Bullet %d: lookup %s, flags %08X", g_trk.n, LookupForm(g_trk.refID) == (void *)ref ? "ok" : "FAILED", *(uint32_t *)(ref + 0x08));
            Log("Bullet %d: leaves the muzzle %.1f units right, %.1f up of the scope's crosshair line (%.1f ahead of the eye)", g_trk.n, rt, up, al);
        }
        return ref;
    }
    return o_ProjSpawn(proj, shooter, cc, weap, px, py, pz, rotZ, rotX, node, target, b1, b2, f1, f2, z);
}

static void InstallFireHook(void)
{
    static const BYTE expect[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };   /* push ebp; mov ebp,esp; push -1 */
    o_FireWeapon = (FireWeapon_t)HookEntry(FIREWEAPON_ADDR, expect, 5, (void *)Hook_FireWeapon, "Fire hook");
    o_ProjSpawn = (ProjSpawn_t)HookEntry(PROJSPAWN_ADDR, expect, 5, (void *)Hook_ProjSpawn, "Bullet launch hook");
}

/* ---- Weapon model follows the hand --------------------------------------- */

#define PLAYER_NODE_1ST_PERSON  0x694       /* NiNode* first-person arms + weapon */
#define NIAV_LOCAL_ROTATE       0x34
#define NIAV_LOCAL_SCALE        0x64
typedef void (__attribute__((thiscall)) *UpdateDownwardPass_t)(void *node, const void *updateData, uint32_t flags);
static const BYTE g_niUpdateData[16] = { 0 };   /* NiUpdateData: no time, no controllers */
static bool g_loggedWeaponTurn = false;

static void NodeUpdate(char *node)
{
    UpdateDownwardPass_t f = (UpdateDownwardPass_t)(*(void ***)node)[0xA4 / 4];
    f(node, g_niUpdateData, 0);
}

/* Rotate the first-person arms/weapon about the camera from the head's direction to the hand's. */
static __attribute__((unused)) bool Weapon_TurnToHand(char *player, char *cam, float saved[12])
{
    char *node = *(char **)(player + PLAYER_NODE_1ST_PERSON);
    if (!node || !cam) return false;
    char *parent = *(char **)(node + NIAV_PARENT);
    float *LR = (float *)(node + NIAV_LOCAL_ROTATE), *LT = (float *)(node + NIAV_LOCAL_TRANSLATE);
    memcpy(saved, LR, 9 * sizeof(float));
    memcpy(saved + 9, LT, 3 * sizeof(float));

    /* D rotates head-aim directions onto hand-aim directions (game world space). */
    M3 Bh, Ba, BhT, D;
    GameBasis(Bh, *PlayerRotZ(player), -*PlayerRotX(player));
    GameBasis(Ba, g_handAimHeading, -g_handAimPitch);
    M3T(BhT, Bh);
    M3Mul(D, Ba, BhT);

    M3 W; memcpy(W, node + NIAV_WORLD_ROTATE, sizeof W);
    const float *WT = (const float *)(node + NIAV_WORLD_TRANSLATE);
    const float *P = (const float *)(cam + NIAV_WORLD_TRANSLATE);   /* pivot: the eye */
    M3 Wn; M3Mul(Wn, D, W);
    float d[3] = { WT[0] - P[0], WT[1] - P[1], WT[2] - P[2] }, Tn[3];
    for (int i = 0; i < 3; i++) Tn[i] = P[i] + D[i][0] * d[0] + D[i][1] * d[1] + D[i][2] * d[2];

    /* Convert the new world transform into the node's local space and refresh its children. */
    M3 Ln; float Ltn[3];
    if (parent) {
        M3 PR, PRt; memcpy(PR, parent + NIAV_WORLD_ROTATE, sizeof PR);
        const float *PT = (const float *)(parent + NIAV_WORLD_TRANSLATE);
        float ps = *(const float *)(parent + NIAV_WORLD_SCALE);
        if (!(ps > 0.0001f)) ps = 1.0f;
        M3T(PRt, PR);
        M3Mul(Ln, PRt, Wn);
        float e[3] = { Tn[0] - PT[0], Tn[1] - PT[1], Tn[2] - PT[2] };
        for (int i = 0; i < 3; i++) Ltn[i] = (PRt[i][0] * e[0] + PRt[i][1] * e[1] + PRt[i][2] * e[2]) / ps;
    } else {
        memcpy(Ln, Wn, sizeof Ln);
        memcpy(Ltn, Tn, sizeof Ltn);
    }
    memcpy(LR, Ln, sizeof Ln);
    memcpy(LT, Ltn, sizeof Ltn);
    NodeUpdate(node);
    if (!g_loggedWeaponTurn) {
        g_loggedWeaponTurn = true;
        const float *nw = (const float *)(node + NIAV_WORLD_TRANSLATE);
        Log("Weapon turn: node %p parent %p, world pos now (%.1f, %.1f, %.1f), target (%.1f, %.1f, %.1f)",
            (void *)node, (void *)parent, nw[0], nw[1], nw[2], Tn[0], Tn[1], Tn[2]);
    }
    return true;
}

static __attribute__((unused)) void Weapon_Restore(char *player, const float saved[12])
{
    char *node = *(char **)(player + PLAYER_NODE_1ST_PERSON);
    if (!node) return;
    memcpy(node + NIAV_LOCAL_ROTATE, saved, 9 * sizeof(float));
    memcpy(node + NIAV_LOCAL_TRANSLATE, saved + 9, 3 * sizeof(float));
    NodeUpdate(node);
}

/* ---- Arms follow the controllers (two-bone reach for each arm) ----------- */

typedef void *(__attribute__((thiscall)) *GetNiNode_t)(void *obj);
static void *AsNode(char *obj)
{
    if (!obj) return NULL;
    GetNiNode_t f = (GetNiNode_t)(*(void ***)obj)[0x0C / 4];
    return f(obj);
}

static char *FindBone(char *node, const char *name, int depth)
{
    if (!node || depth > 40) return NULL;
    const char *n = *(const char **)(node + 0x08);
    if (n && !_stricmp(n, name)) return node;
    if (!AsNode(node)) return NULL;
    char **kids = *(char ***)(node + 0xA0);
    unsigned count = *(uint16_t *)(node + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) {
        char *r = FindBone(kids[i], name, depth + 1);
        if (r) return r;
    }
    return NULL;
}

enum { B_UPPER, B_FORE, B_HAND, B_COUNT };
static const char *g_boneNames[2][B_COUNT] = {
    { "Bip01 L UpperArm", "Bip01 L Forearm", "Bip01 L Hand" },
    { "Bip01 R UpperArm", "Bip01 R Forearm", "Bip01 R Hand" },
};
static char *g_bones[2][B_COUNT];
static char *g_bonesFor = NULL;          /* first-person node the bones were found in */
static uint32_t g_fpGen = 1;
static float g_lhT[3]; static M3 g_lhD, g_lhBa; static bool g_lhOk = false;   /* left-handed: where the left hand is drawn this frame, and its turn */
static float g_lhWeapL[12]; static char *g_lhWeapNode = NULL;          /* the gun's own placement, put back after drawing */              /* bumped when the first-person body is rebuilt in place: every cache below re-reads */
static float g_boneSaved[2][B_COUNT][12];
static bool  g_armsLogged = false;

/* Give a bone a new WORLD rotation by writing the matching local rotation, then refresh its children. */
static void SetBoneWorldRot(char *bone, M3 Wn)
{
    char *parent = *(char **)(bone + NIAV_PARENT);
    M3 L;
    if (parent) { M3 P, Pt; memcpy(P, parent + NIAV_WORLD_ROTATE, sizeof P); M3T(Pt, P); M3Mul(L, Pt, Wn); }
    else memcpy(L, Wn, sizeof L);
    memcpy(bone + NIAV_LOCAL_ROTATE, L, sizeof L);
    NodeUpdate(bone);
}

/* Bend one arm so its hand reaches 'target' (game world), then orient the hand with handDelta. */
typedef bool (__attribute__((thiscall)) *IsWeaponOut_t)(void *process);
static bool PlayerWeaponOut(void)
{
    char *player = (char *)GetPlayer();
    char *process = player ? *(char **)(player + 0x68) : NULL;
    if (!process) return true;
    return ((IsWeaponOut_t)(*(void ***)process)[0x454 / 4])(process);
}
/* Hand orientation relative to the head while the weapon is out - reused when it's put away,
   so holstered hands keep a natural forward pose instead of the animation's "arms down". */
static M3   g_handRef[2];
static bool g_haveHandRef[2] = { false, false };
static bool g_weaponOutThisFrame;

static char *g_twistBone[2]; static float g_twistSaved[2][3]; static bool g_twistScaled[2];   /* forearm twist bones moved with the arm length */
static void Arm_Reach(int side, const float *target, M3 handDelta, M3 Bh)
{
    char *up = g_bones[side][B_UPPER], *fo = g_bones[side][B_FORE], *ha = g_bones[side][B_HAND];
    const float *S = (const float *)(up + NIAV_WORLD_TRANSLATE);
    {   /* Arm length. With your body calibrated, the in-game arm is as long as yours (your arm straight =
           its arm straight, bent = bent). Either way, if your hand is further than the arm reaches, the arm
           stretches a little rather than let the hand stop short, so the visible hand is where your hand is. */
        float e[3], w[3], t[3];
        V3Sub(e, (const float *)(fo + NIAV_WORLD_TRANSLATE), S); V3Sub(w, (const float *)(ha + NIAV_WORLD_TRANSLATE), (const float *)(fo + NIAV_WORLD_TRANSLATE)); V3Sub(t, target, S);
        float len = V3Len(e) + V3Len(w), dt = V3Len(t);
        if (g_armCalMask & (1 << side)) {   /* arm calibration: with the arm held straight, this is your reach */
            if (dt > 5.0f && g_unitsPerMeter > 1.0f) g_bodyReach[side] = dt / g_unitsPerMeter;
            g_armCalMask &= ~(1 << side);
            if (!g_armCalMask) g_armCalDone = true;
            Log("Body: %s arm reach measured %.1f units = %.3f m (the game's arm is %.1f units)", side ? "right" : "left", dt, g_bodyReach[side], len);
        }
        float k = Body_ArmScale(side, len), full = len * k * 0.98f, stretch = 1.0f;
        if (full > 1.0f && dt > full) stretch = fminf(dt / full, k == 1.0f ? 1.4f : 1.25f);
        float kk = k * stretch;
        if (len > 1.0f && fabsf(kk - 1.0f) > 0.002f) {
            float *lf = (float *)(fo + NIAV_LOCAL_TRANSLATE), *lh = (float *)(ha + NIAV_LOCAL_TRANSLATE);
            for (int i = 0; i < 3; i++) { lf[i] *= kk; lh[i] *= kk; }
            {   /* the forearm's twist bone (the Pip-Boy sits on it) keeps its place along the longer/shorter forearm */
                static char *twFor[2] = { NULL, NULL }, *twB[2] = { NULL, NULL }; static uint32_t twGen[2];
                if (twFor[side] != g_bonesFor || twGen[side] != g_fpGen) { twFor[side] = g_bonesFor; twGen[side] = g_fpGen; g_twistScaled[side] = false; twB[side] = g_bonesFor ? FindBone(g_bonesFor, side ? "Bip01 R ForeTwist" : "Bip01 L ForeTwist", 0) : NULL; }
                char *tb = twB[side];
                if (tb && *(char **)(tb + NIAV_PARENT) == fo) {
                    float *lt = (float *)(tb + NIAV_LOCAL_TRANSLATE);
                    if (!g_twistScaled[side]) { memcpy(g_twistSaved[side], lt, 12); g_twistBone[side] = tb; g_twistScaled[side] = true; }
                    for (int i = 0; i < 3; i++) lt[i] = g_twistSaved[side][i] * kk;
                }
            }
            NodeUpdate(up);
        }
        static int al = 0;
        if (k != 1.0f && al < 2 && side == 1) { al++; Log("Arms: sized to yours (x%.2f of the game's length)", k); }
    }
    float E0[3], W0[3];
    memcpy(E0, fo + NIAV_WORLD_TRANSLATE, sizeof E0);
    memcpy(W0, ha + NIAV_WORLD_TRANSLATE, sizeof W0);
    M3 handWorld0; memcpy(handWorld0, ha + NIAV_WORLD_ROTATE, sizeof handWorld0);
    {
        M3 BhT; M3T(BhT, Bh);
        bool pipAnim = g_pipOpen || g_pipModeRaw != 0;   /* the Pip-Boy animation poses the hand: ignore it */
        if (!g_haveHandRef[side] && g_handRefIniOk[side]) { memcpy(g_handRef[side], g_handRefIni[side], 36); g_haveHandRef[side] = true; }   /* (saved from an earlier game) */
        if (g_weaponOutThisFrame && !pipAnim) {
            M3Mul(g_handRef[side], BhT, handWorld0); g_haveHandRef[side] = true;
            static double seenAt[2] = { 0, 0 }; static bool saved[2] = { false, false };
            if (g_animAction != -1) seenAt[side] = 0;   /* (only the plain holding pose - never mid-equip or mid-reload) */
            else if (!seenAt[side]) seenAt[side] = QpcMs();
            if (!saved[side] && seenAt[side] && QpcMs() - seenAt[side] > 1500.0) {   /* remember it for next time (and for a new game, before any weapon is drawn) */
                saved[side] = true;
                const float *m = &g_handRef[side][0][0]; char hb[256];
                snprintf(hb, sizeof hb, "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
                WritePrivateProfileStringA("Arms", side ? "sHandRefR" : "sHandRefL", hb, g_iniPath);
                memcpy(g_handRefIni[side], m, 36); g_handRefIniOk[side] = true;
                Log("Arms: %s hand's turn saved (%s)", side ? "right" : "left", hb);
            }
        }
        else if (g_haveHandRef[side]) M3Mul(handWorld0, Bh, g_handRef[side]);
        static int pl = 0;
        if (pipAnim && side == 0 && pl++ < 3) Log("Pip-Boy arm: weapon-out flag %d, hand reference %s", (int)g_weaponOutThisFrame, g_haveHandRef[side] ? "kept" : "MISSING");
    }

    float u0[3], f0[3], st[3];
    V3Sub(u0, E0, S); V3Sub(f0, W0, E0); V3Sub(st, target, S);
    float L1 = V3Len(u0), L2 = V3Len(f0), d = V3Len(st);
    if (L1 < 0.01f || L2 < 0.01f || d < 0.01f) return;
    if (d > (L1 + L2) * 0.999f) d = (L1 + L2) * 0.999f;
    if (d < fabsf(L1 - L2) * 1.001f + 0.01f) d = fabsf(L1 - L2) * 1.001f + 0.01f;
    float dir[3] = { st[0] / V3Len(st), st[1] / V3Len(st), st[2] / V3Len(st) };

    /* Keep the elbow bending the same way the animation had it. */
    float sw[3]; V3Sub(sw, W0, S);
    float swl = V3Len(sw), pole[3];
    float sdir[3] = { sw[0] / swl, sw[1] / swl, sw[2] / swl };
    float pr = V3Dot(u0, sdir);
    for (int i = 0; i < 3; i++) pole[i] = u0[i] - pr * sdir[i];
    float pd = V3Dot(pole, dir);
    for (int i = 0; i < 3; i++) pole[i] -= pd * dir[i];
    if (V3Len(pole) < 0.01f) { float down[3] = { 0, 0, -1 }; float dd = V3Dot(down, dir); for (int i = 0; i < 3; i++) pole[i] = down[i] - dd * dir[i]; }
    float pl = V3Len(pole);
    for (int i = 0; i < 3; i++) pole[i] /= pl;
    if (g_armFollowHand > 0.0f && g_fingerDirOk[side]) {
        /* a real forearm runs back from the hand along the way the fingers point: put the elbow there
           (as far as the arm's reach allows), so the forearm lines up with the wrist and hand */
        float fd[3]; memcpy(fd, g_fingerDirW[side], 12);
        float fl = V3Len(fd);
        if (fl > 0.1f) {
            float want[3], ph[3];
            for (int i = 0; i < 3; i++) want[i] = target[i] - fd[i] / fl * L2 - S[i];
            float wd = V3Dot(want, dir);
            for (int i = 0; i < 3; i++) ph[i] = want[i] - wd * dir[i];
            float phl = V3Len(ph);
            /* how clearly the hand says where the elbow is: fade the hand's say in smoothly (when the fingers
               point straight away from the shoulder the elbow could be anywhere around that line) */
            float wgt = g_armFollowHand * fminf(fmaxf((phl - 0.12f * L2) / (0.35f * L2), 0.0f), 1.0f);
            if (wgt > 0.0f && phl > 1e-3f) {
                float np[3];
                for (int i = 0; i < 3; i++) np[i] = pole[i] * (1.0f - wgt) + ph[i] / phl * wgt;
                float nl = V3Len(np);
                if (nl > 0.2f) for (int i = 0; i < 3; i++) pole[i] = np[i] / nl;   /* (a blend of opposite directions keeps the old one) */
            }
        }
    }
    {   /* a real elbow hangs below the shoulder-to-hand line - it never swings up toward your face */
        float dn[3] = { 0, 0, -1 }, dd = V3Dot(dn, dir), dp[3];
        for (int i = 0; i < 3; i++) dp[i] = dn[i] - dd * dir[i];
        float dl = V3Len(dp);
        if (dl > 0.3f) {   /* (not when reaching straight up or down - then any side is fine) */
            for (int i = 0; i < 3; i++) dp[i] /= dl;
            float c = V3Dot(pole, dp);
            if (c < 0.0f) {   /* pointing upward: bring it back to sideways at most */
                float sd[3]; V3Cross(sd, dir, dp);
                float sgn = V3Dot(pole, sd) >= 0 ? 1.0f : -1.0f;
                for (int i = 0; i < 3; i++) pole[i] = sd[i] * sgn;
            }
        }
    }
    {   /* the elbow never whips round: it can swing at most ~25 degrees per frame and settles smoothly */
        static float prevPole[2][3]; static bool havePole[2] = { false, false }; static double poleAt[2] = { 0, 0 };
        double nowP = QpcMs();
        if (havePole[side] && nowP - poleAt[side] < 250.0) {
            float pp[3]; memcpy(pp, prevPole[side], 12);
            float pd2 = V3Dot(pp, dir); for (int i = 0; i < 3; i++) pp[i] -= pd2 * dir[i];   /* keep it square to the reach line */
            float ppl = V3Len(pp);
            if (ppl > 0.1f) {
                for (int i = 0; i < 3; i++) pp[i] /= ppl;
                /* both lie square to the reach line: turn the old elbow direction around that line toward the new one */
                float cr[3]; V3Cross(cr, pp, pole);
                float ang = atan2f(V3Dot(cr, dir), V3Dot(pp, pole));
                float step = ang * 0.6f, maxStep = 0.44f;                  /* follow 60% of the way, at most ~25 degrees a frame */
                if (step > maxStep) step = maxStep; else if (step < -maxStep) step = -maxStep;
                float kx[3]; V3Cross(kx, dir, pp);
                float cs = cosf(step), sn = sinf(step);
                for (int i = 0; i < 3; i++) pole[i] = pp[i] * cs + kx[i] * sn;
            }
        }
        memcpy(prevPole[side], pole, 12); havePole[side] = true; poleAt[side] = nowP;
    }

    float a = (L1 * L1 - L2 * L2 + d * d) / (2.0f * d);
    float h = sqrtf(fmaxf(L1 * L1 - a * a, 0.0f));
    float E[3];
    for (int i = 0; i < 3; i++) E[i] = S[i] + a * dir[i] + h * pole[i];

    /* Upper arm: turn so it points at the new elbow. */
    float u1[3]; V3Sub(u1, E, S);
    M3 R1, Wu, Wun; RotBetween(R1, u0, u1);
    memcpy(Wu, up + NIAV_WORLD_ROTATE, sizeof Wu);
    M3Mul(Wun, R1, Wu);
    SetBoneWorldRot(up, Wun);

    /* Forearm: turn so it points at the target. */
    float Ec[3], Wc[3], f1[3], f2[3];
    memcpy(Ec, fo + NIAV_WORLD_TRANSLATE, sizeof Ec);
    memcpy(Wc, ha + NIAV_WORLD_TRANSLATE, sizeof Wc);
    V3Sub(f1, Wc, Ec);
    float tgt[3] = { S[0] + dir[0] * d, S[1] + dir[1] * d, S[2] + dir[2] * d };
    V3Sub(f2, tgt, Ec);
    M3 R2, Wf, Wfn; RotBetween(R2, f1, f2);
    memcpy(Wf, fo + NIAV_WORLD_ROTATE, sizeof Wf);
    M3Mul(Wfn, R2, Wf);
    SetBoneWorldRot(fo, Wfn);

    /* Hand: the animation's hand orientation, turned from the head's direction to the controller's. */
    M3 Wh; M3Mul(Wh, handDelta, handWorld0);
    {   /* Forearm twist follows the wrist (so the Pip-Boy on the left forearm turns with your hand). */
        float lf[3];   /* the forearm's own lengthwise axis */
        for (int i = 0; i < 3; i++) lf[i] = Wfn[0][i] * f2[0] + Wfn[1][i] * f2[1] + Wfn[2][i] * f2[2];
        int ax = fabsf(lf[0]) > fabsf(lf[1]) ? (fabsf(lf[0]) > fabsf(lf[2]) ? 0 : 2) : (fabsf(lf[1]) > fabsf(lf[2]) ? 1 : 2);
        int a1 = (ax + 1) % 3, a2 = (ax + 2) % 3;
        M3 FT, R0, R1n, WfT0;
        M3T(WfT0, Wf); M3Mul(R0, WfT0, handWorld0);    /* animation: hand relative to forearm */
        M3T(FT, Wfn);  M3Mul(R1n, FT, Wh);             /* now: hand relative to the turned forearm */
        /* the Pip-Boy animation changes the arm's own pose; keep using the everyday wrist/forearm relation */
        static M3 R0Base[2]; static bool R0Have[2] = { false, false };
        if (g_pipModeRaw == 0 && !g_pipOpen) { memcpy(R0Base[side], R0, sizeof R0); R0Have[side] = true; }
        else if (R0Have[side]) memcpy(R0, R0Base[side], sizeof R0);
        /* How far the wrist has turned around the forearm since the everyday pose: the twist part of
           (now x everyday^-1), measured with a quaternion so bending the wrist doesn't read as turning. */
        M3 R0T, Dl; M3T(R0T, R0); M3Mul(Dl, R1n, R0T);
        float raw = TwistAbout(Dl, ax);
        /* A real forearm only turns so far each way. Past that point the arm stays at the limit on the side
           it reached - it never jumps over to the other side (the old 180-degree flip). It only follows
           again once your wrist comes back into the range a real arm can reach. */
        static float twOut[2] = { 0, 0 }; static double twAt[2] = { 0, 0 };
        float lim = g_forearmRollLimit / RAD2DEG, dtwFull;
        double twNow = QpcMs(); float twDt = (float)((twNow - twAt[side]) / 1000.0);
        if (twAt[side] == 0 || twDt > 0.3f) dtwFull = fminf(fmaxf(raw, -lim), lim);   /* (first time / after a pause: take it as it is) */
        else {
            float want = (raw >= -lim && raw <= lim) ? raw : (twOut[side] >= 0.0f ? lim : -lim);
            float jump = want - twOut[side];
            if (fabsf(jump) > 1.2f) {   /* no real wrist turns that far between two frames: roll over smoothly instead of flipping */
                float mx = 6.0f * fminf(fmaxf(twDt, 0.005f), 0.05f);
                dtwFull = twOut[side] + (jump > 0 ? mx : -mx);
            } else dtwFull = want;
        }
        twOut[side] = dtwFull; twAt[side] = twNow;
        static int twl = 0;
        if ((g_pipOpen || g_pipModeRaw) && side == 0 && (twl++ % 90) == 0 && twl < 900)
            Log("Pip-Boy arm twist: wrist turned %.0f deg, forearm follows %.0f deg (limit %.0f, axis %d)", raw * RAD2DEG, dtwFull * RAD2DEG, g_forearmRollLimit, ax);
        if (side == 0) { g_twRoll = dtwFull * (1.0f - g_twShare); g_twAx = ax; g_twOk = true; }
        float dtw = side == 0 ? dtwFull * g_twShare : dtwFull;   /* left: the twist bone (wrist end) carries the rest */
        M3 Rt = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
        float c = cosf(dtw), sn = sinf(dtw);
        Rt[a1][a1] = c; Rt[a1][a2] = -sn; Rt[a2][a1] = sn; Rt[a2][a2] = c;
        M3 Wft; M3Mul(Wft, Wfn, Rt);
        SetBoneWorldRot(fo, Wft);
    }
    SetBoneWorldRot(ha, Wh);

    if (!g_armsLogged) {
        const float *Wf2 = (const float *)(ha + NIAV_WORLD_TRANSLATE);
        Log("Arm %s: upper %.1f + fore %.1f units, reach %.1f, hand now (%.1f, %.1f, %.1f) target (%.1f, %.1f, %.1f)",
            side ? "R" : "L", L1, L2, d, Wf2[0], Wf2[1], Wf2[2], target[0], target[1], target[2]);
        if (side == 1) g_armsLogged = true;
    }
}

/* ---- Fingers ------------------------------------------------------------ */
/* Biped finger bones: FingerN (knuckle), FingerN1, FingerN2 for N = 0 (thumb) .. 4 (little). */
static char *g_fingers[2][5][3];
static float g_fingerSaved[2][5][3][9];
static bool  g_fingersApplied[2] = { false, false };
static bool  g_fingersLogged = false;
static char *g_weaponNode = NULL;        /* first-person "Weapon" node (the held weapon hangs off it) */
static bool  g_weaponHidden = false;

static void LogBoneTree(char *node, int depth, char *line, size_t cap)
{
    if (!node || depth > 6) return;
    const char *n = *(const char **)(node + 0x08);
    if (n && strlen(line) + strlen(n) + 3 < cap) { strcat(line, n); strcat(line, ", "); }
    if (!AsNode(node)) return;
    char **kids = *(char ***)(node + 0xA0);
    unsigned count = *(uint16_t *)(node + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) LogBoneTree(kids[i], depth + 1, line, cap);
}

static void FindFingers(char *node)
{
    int found = 0;
    for (int s = 0; s < 2; s++) for (int f = 0; f < 5; f++) for (int j = 0; j < 3; j++) {
        char name[40];
        if (j == 0) snprintf(name, sizeof name, "Bip01 %c Finger%d", s ? 'R' : 'L', f);
        else        snprintf(name, sizeof name, "Bip01 %c Finger%d%d", s ? 'R' : 'L', f, j);
        char *b = FindBone(node, name, 0);
        if (!b && f == 0) {   /* this skeleton names thumbs Thumb1, Thumb11, Thumb12 */
            if (j == 0) snprintf(name, sizeof name, "Bip01 %c Thumb1", s ? 'R' : 'L');
            else        snprintf(name, sizeof name, "Bip01 %c Thumb1%d", s ? 'R' : 'L', j);
            b = FindBone(node, name, 0);
        }
        g_fingers[s][f][j] = b;
        if (b) found++;
    }
    Log("Fingers: found %d of 30 finger bones", found);
    for (int s = 0; s < 2; s++) {
        char line[1600] = "";
        LogBoneTree(g_bones[s][B_HAND], 0, line, sizeof line);
        Log("Bones under %s hand: %s", s ? "right" : "left", line);
    }
    g_weaponNode = FindBone(node, "Weapon", 0);
    Log("Weapon node: %p", (void *)g_weaponNode);
}

/* Rotation about local axis 'ax' by 'ang' radians. */
static void AxisRot(M3 R, int ax, float ang)
{
    float c = cosf(ang), sn = sinf(ang);
    M3 I = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    memcpy(R, I, sizeof I);
    int a = (ax + 1) % 3, b = (ax + 2) % 3;
    R[a][a] = c; R[a][b] = -sn; R[b][a] = sn; R[b][b] = c;
}
/* Angle of R around axis 'ax' (assuming it's mostly a rotation about that axis). */
static float AxisAngle(M3 R, int ax)
{
    int a = (ax + 1) % 3, b = (ax + 2) % 3;
    return atan2f(R[b][a], R[a][a]);
}

/* Pose one hand's fingers from the controller: straighten each joint's bend, then bend by the curl. */
static void Fingers_Apply(int side, int physHand)
{
    static const float maxBend[3] = { 70.0f, 90.0f, 60.0f };   /* knuckle, middle, tip (degrees) */
    for (int f = 0; f < 5; f++) {
        float curl = f == 0 ? g_fingerCurl[physHand][0] : f == 1 ? g_fingerCurl[physHand][1] : g_fingerCurl[physHand][2];
        for (int j = 0; j < 3; j++) {
            char *bone = g_fingers[side][f][j];
            if (!bone) continue;
            if (f == 0 && j == 0) continue;   /* thumb base stays put - turning it swings the whole thumb off the hand */
            float *L = (float *)(bone + NIAV_LOCAL_ROTATE);
            memcpy(g_fingerSaved[side][f][j], L, 9 * sizeof(float));
            M3 R; memcpy(R, L, sizeof R);
            /* thumb bends less and its knuckle keeps the animation's angle */
            float bend = (f == 0 ? 0.6f : 1.0f) * maxBend[j] / RAD2DEG * curl * g_fingerSign;
            M3 straighten, B, t, out;
            if (f == 0 && j == 0) memcpy(t, R, sizeof t);
            else { AxisRot(straighten, g_fingerAxis, -AxisAngle(R, g_fingerAxis)); M3Mul(t, straighten, R); }
            AxisRot(B, g_fingerAxis, bend);
            M3Mul(out, B, t);
            memcpy(L, out, sizeof out);
        }
    }
    char *hand = g_bones[side][B_HAND];
    if (hand) NodeUpdate(hand);
    g_fingersApplied[side] = true;
}

static void Fingers_Restore(void)
{
    for (int s = 0; s < 2; s++) {
        if (!g_fingersApplied[s]) continue;
        for (int f = 0; f < 5; f++) for (int j = 0; j < 3; j++)
            if (g_fingers[s][f][j] && !(f == 0 && j == 0)) memcpy(g_fingers[s][f][j] + NIAV_LOCAL_ROTATE, g_fingerSaved[s][f][j], 9 * sizeof(float));
        g_fingersApplied[s] = false;
    }
}

static char *FindNameCI(char *node, const char *part, int depth)
{
    if (!node || depth > 40) return NULL;
    const char *n = *(const char **)(node + 0x08);
    if (n) {
        char low[64]; int i = 0;
        for (; n[i] && i < 63; i++) low[i] = (char)tolower((unsigned char)n[i]);
        low[i] = 0;
        if (strstr(low, part)) return node;
    }
    if (!AsNode(node)) return NULL;
    char **kids = *(char ***)(node + 0xA0);
    unsigned count = *(uint16_t *)(node + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) { char *r = FindNameCI(kids[i], part, depth + 1); if (r) return r; }
    return NULL;
}
static void Pip_ListNode(char *n, const float *C, const float *F, const float *U, const float *R, const float *fr, int depth, char *out, size_t cap)
{
    if (!n || depth > 14) return;
    const float *b = *(const float **)(n + 0x20);
    const char *name = *(const char **)(n + 0x08);
    if (b && b[3] > 0.5f && name) {
        float q[3]; V3Sub(q, b, C);
        float z = V3Dot(q, F);
        if (z > 0.5f) {
            float x = (V3Dot(q, R) / z - fr[0]) / (fr[1] - fr[0]), y = (fr[2] - V3Dot(q, U) / z) / (fr[2] - fr[3]);
            if (x > -0.1f && x < 1.1f && y > -0.1f && y < 1.1f && strlen(out) + strlen(name) + 48 < cap) {
                char t[160]; snprintf(t, sizeof t, " [%s at %.2f,%.2f r%.1f d%.0f%s]", name, x, y, b[3], z, (*(uint32_t *)(n + 0x30) & 1) ? " hidden" : "");
                strcat(out, t);
            }
        }
    }
    if (!AsNode(n)) return;
    char **kids = *(char ***)(n + 0xA0);
    unsigned count = *(uint16_t *)(n + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) Pip_ListNode(kids[i], C, F, U, R, fr, depth + 1, out, cap);
}
static void Pip_ListOnScreen(char *node, char *cam)
{
    static char out[6000]; out[0] = 0;
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float F[3] = { Cw[0], Cw[3], Cw[6] }, U[3] = { Cw[1], Cw[4], Cw[7] }, R[3] = { Cw[2], Cw[5], Cw[8] };
    const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
    Pip_ListNode(node, C, F, U, R, fr, 0, out, sizeof out);
    Log("Pip-Boy: parts on screen (x,y from top-left 0..1):%s", out);
}
static bool g_handOnWall[2] = { false, false };
static float g_leftBack[3]; static bool g_leftBackOk = false;
static float g_palmBoneN[2][3]; static bool g_palmBoneOk[2] = { false, false };   /* palm direction in the hand bone's own axes */
static char *RayCast(const float *from, const float *to, float *fracOut);
static bool Climbable(char *obj);
/* The Pip-Boy menu picture ("pipboyon") isn't part of the device: the game places it where the screen is in
   its own raised-arm pose. Keep it on the screen of the Pip-Boy on your real arm. */
static char *g_pOn = NULL, *g_pScr = NULL, *g_pFor = NULL;
static bool g_pScrPicked; static char *g_pScrAuto;
static bool  g_pRelOk = false, g_pOnSaved = false;
#define g_pipFit g_pipFitIni   /* INI [Controls] fPipBoyScreenFit: menu picture size relative to the glass */
static float g_pRelR[3][3], g_pRelT[3], g_pRelS, g_pOnSave[13];
/* Visual centre of the biggest shape under a node (from the shape's own data, in world space). */
static bool Geo_Center(char *n, float c[3], float *r, int depth)
{
    if (!n || depth > 16) return false;
    if (*(uint32_t *)(n + 0x30) & 1) return false;   /* hidden */
    uintptr_t vt = *(uintptr_t *)n;
    bool found = false;
    if (vt == 0x0109D454 || vt == 0x0109CD44) {
        char *gd = *(char **)(n + 0xB8);
        if (gd) {
            const float *mb = (const float *)(gd + 0x10);   /* model-space centre + radius */
            const float *R = (const float *)(n + NIAV_WORLD_ROTATE), *T = (const float *)(n + NIAV_WORLD_TRANSLATE);
            float S = *(const float *)(n + NIAV_WORLD_SCALE);
            float rr = mb[3] * S;
            if (rr > *r) {
                for (int i = 0; i < 3; i++) c[i] = T[i] + (R[i * 3] * mb[0] + R[i * 3 + 1] * mb[1] + R[i * 3 + 2] * mb[2]) * S;
                *r = rr; found = true;
            }
        }
        return found;
    }
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        for (int k = 0; kids && k < nk; k++) found |= Geo_Center(kids[k], c, r, depth + 1);
    }
    return found;
}
static void PipOn_Find(char *node)
{
    static uint32_t pGen = 0;
    if (g_pFor == node && pGen == g_fpGen) return;
    g_pFor = node; pGen = g_fpGen; g_pOn = NULL; g_pScr = FindNameCI(node, "pipboyscreen", 0); g_pScrPicked = false; g_pScrAuto = NULL;
    Log("Pip-Boy: device screen %s", g_pScr ? "found" : "MISSING");
}
/* List every visible part of the first-person body/Pip-Boy (once), with where it sits relative to the
   game's "pipboyscreen" part, in the forearm's own directions - and pick the Pip-Boy's real screen glass. */
static char *g_pScrAuto = NULL; static bool g_pScrPicked = false;
static char *g_pMenuGeo = NULL;
static void Pip_DumpWalk(char *n, char *tw, const float *ref, int depth, int *count)
{
    if (!n || depth > 30 || *count > 160 || IsBadReadPtr(n, 0xC0)) return;
    if (*(uint32_t *)(n + 0x30) & 1) return;
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        for (int k = 0; kids && k < nk; k++) Pip_DumpWalk(kids[k], tw, ref, depth + 1, count);
        return;
    }
    const char *nm = *(const char **)(n + 0x08); if (IsBadReadPtr(nm, 1)) nm = "?";
    const float *wb = *(const float **)(n + 0x20);
    if (IsBadReadPtr(wb, 16) || !(wb[3] > 0.01f)) return;
    const char *texp = "";
    char *sp = *(char **)(n + 0xA8);
    if (!IsBadReadPtr(sp, 0xA8) && *(uintptr_t *)sp == 0x010AE0D0) {
        char *ts = *(char **)(sp + 0xA4);
        if (!IsBadReadPtr(ts, 0x10)) { const char *t0 = *(const char **)(ts + 0x08); if (!IsBadReadPtr(t0, 1)) texp = t0; }
    }
    float rel[3] = { 0, 0, 0 };
    if (tw && ref) {
        const float *R = (const float *)(tw + NIAV_WORLD_ROTATE); float S = *(const float *)(tw + NIAV_WORLD_SCALE); if (!(S > 1e-4f)) S = 1;
        float d[3]; V3Sub(d, wb, ref);
        for (int i = 0; i < 3; i++) rel[i] = (R[0 * 3 + i] * d[0] + R[1 * 3 + i] * d[1] + R[2 * 3 + i] * d[2]) / S;
    }
    Log("  part '%s' size %.1f at %.1f %.1f %.1f (from pipboyscreen, forearm axes) texture '%s'", nm, wb[3], rel[0], rel[1], rel[2], texp);
    (*count)++;
    /* the real screen glass: a mid-sized part whose name or texture says screen/glass/display */
    char low[160]; snprintf(low, sizeof low, "%s|%s", nm, texp); for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    bool named = (strstr(low, "screen") || strstr(low, "glass") || strstr(low, "display") || strstr(low, "lcd") || strstr(low, "monitor"));
    bool notVanilla = _stricmp(nm, "pipboyscreen") != 0 && n != g_pOn && n != g_pMenuGeo && _strnicmp(nm, "pipboyscreen", 12) != 0;
    if (named && notVanilla && wb[3] > 1.5f && wb[3] < 20.0f && !g_pScrAuto) g_pScrAuto = n;
}
/* Find the shape that actually shows the Pip-Boy menu: the one whose material points at the menu's
   rendered picture (the Pip-Boy manager's texture). */
static bool Pip_PtrHas(char *p, int bytes, const void *a, const void *b)
{
    if (!p || IsBadReadPtr(p, bytes)) return false;
    for (int o = 0; o < bytes; o += 4) { void *v = *(void **)(p + o); if (v && (v == a || v == b)) return true; }
    return false;
}
static int g_pfCount;
static char *Pip_FindTexWalk(char *n, const void *a, const void *b, int depth, bool deep, char **path)
{
    if (!n || depth > 40 || g_pfCount > 4000 || IsBadReadPtr(n, 0xC0)) return NULL;
    g_pfCount++;
    if (AsNode(n)) {
        char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
        if (IsBadReadPtr(kids, nk * 4)) return NULL;
        for (int k = 0; kids && k < nk; k++) { char *r = Pip_FindTexWalk(kids[k], a, b, depth + 1, deep, path); if (r) return r; }
        return NULL;
    }
    for (int o = 0x18; o < 0xC0; o += 4) {
        char *p = *(char **)(n + o);
        if (!p || IsBadReadPtr(p, 0x100)) continue;
        if (p == a || p == b || Pip_PtrHas(p, 0x100, a, b)) { *path = p; return n; }
        if (deep) for (int o2 = 0; o2 < 0x80; o2 += 4) { char *q = *(char **)(p + o2); if (q && q != n && Pip_PtrHas(q, 0x80, a, b)) { *path = p; return n; } }
    }
    return NULL;
}
static void Pip_FindMenuGeo(char *firstPerson)
{
    static int tries = 0; static char *forNode = NULL;
    if (forNode != firstPerson) { forNode = firstPerson; tries = 0; g_pMenuGeo = NULL; }
    if (g_pMenuGeo || tries >= 3) return;
    tries++;
    char *im = *(char **)0x011D8A80;
    char *pm = (im && !IsBadReadPtr(im + 0x174, 4)) ? *(char **)(im + 0x174) : NULL;
    if (!pm || IsBadReadPtr(pm, 0x28)) return;
    char *tex = *(char **)(pm + 0x10);
    char *td = (tex && !IsBadReadPtr(tex, 0x28)) ? *(char **)(tex + 0x24) : NULL;
    if (!tex) { Log("Pip-Boy: menu texture not made yet"); return; }
    char *path = NULL, *found = NULL;
    for (int pass = 0; pass < 2 && !found; pass++) {
        g_pfCount = 0; found = Pip_FindTexWalk(firstPerson, tex, td, 0, pass == 1, &path);
    }
    if (!found) {
        char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
        g_pfCount = 0; if (sg) found = Pip_FindTexWalk(sg, tex, td, 0, false, &path);
    }
    if (!found) { Log("Pip-Boy: no shape uses the menu picture (try %d)", tries); return; }
    const char *nm = *(const char **)(found + 0x08); if (!nm || IsBadReadPtr(nm, 1)) nm = "?";
    char *par = *(char **)(found + NIAV_PARENT); const char *pn = (par && !IsBadReadPtr(par, 0x10)) ? *(const char **)(par + 0x08) : NULL; if (!pn || IsBadReadPtr(pn, 1)) pn = "?";
    const float *b = *(const float **)(found + 0x20);
    Log("Pip-Boy: the menu is shown on '%s' (parent '%s', size %.1f, flags %X)", nm, pn, b && !IsBadReadPtr(b, 16) ? b[3] : -1.0f, *(uint32_t *)(found + 0x30));
    g_pMenuGeo = found;
}
static void Pip_DumpParts(char *node)
{
    if (g_pScrPicked || !node) return;
    g_pScrPicked = true;
    char *tw = FindBone(node, "Bip01 L ForeTwist", 0);
    const float *ref = NULL;
    if (g_pScr) { const float *b = *(const float **)(g_pScr + 0x20); if (!IsBadReadPtr(b, 16)) ref = b; }
    int count = 0;
    Log("Pip-Boy: parts of the first-person body:");
    Pip_DumpWalk(node, tw, ref, 0, &count);
    if (g_pScrAuto) { Log("Pip-Boy: using '%s' as the Pip-Boy's screen glass", *(const char **)(g_pScrAuto + 0x08)); g_pScr = g_pScrAuto; }
    else Log("Pip-Boy: no separate screen part found - using the game's pipboyscreen (shift it with fPipBoyScreenShiftX/Y/Z)");
}
/* The Pip-Boy menu itself: the Pip-Boy's interface pieces (its tile menu), which the game shows floating
   where its own raised-arm pose expects the screen. (The manager's screen shape is only the blank glass.) */
static char *PipOn_Quad(void)
{
    char *im = *(char **)0x011D8A80;
    char *pm = (im && !IsBadReadPtr(im + 0x174, 4)) ? *(char **)(im + 0x174) : NULL;
    /* The visible Pip-Boy page's tile menu, from the game's menu table (Inventory / Stats / Map). */
    static const int ids[3] = { 0x3EA, 0x3EB, 0x3FF };
    char ***arrp = (char ***)0x011F350C;
    char **arr = IsBadReadPtr(arrp, 4) ? NULL : *arrp;
    for (int k = 0; arr && k < 3; k++) {
        int id = ids[k];
        if (!*(volatile bool *)(0x011F308F + id)) continue;
        if (IsBadReadPtr(arr + (id - 0x3E9), 4)) continue;
        char *tm = arr[id - 0x3E9];
        if (!tm || IsBadReadPtr(tm, 0x30)) continue;
        char *n = *(char **)(tm + 0x2C);
        if (!n || IsBadReadPtr(n, 0xC0) || !AsNode(n) || !*(char **)(n + NIAV_PARENT)) continue;
        static int lastId = 0;
        if (lastId != id) { lastId = id; Log("Pip-Boy: menu picture = page %X's tile menu", id); }
        return n;
    }
    if (!pm || IsBadReadPtr(pm, 0x1C)) return NULL;
    char *tm = *(char **)(pm + 0x18);   /* TileMenu */
    char *n = (tm && !IsBadReadPtr(tm, 0x30)) ? *(char **)(tm + 0x2C) : NULL;
    if (!n || IsBadReadPtr(n, 0xC0) || !AsNode(n) || !*(char **)(n + NIAV_PARENT)) return NULL;
    return n;
}
static void PipOn_Restore(void);
static void PipOn_Capture(void)   /* in the game's own pose: where is the picture relative to the screen? */
{
    {
        char *q = g_pMenuGeo ? g_pMenuGeo : PipOn_Quad();
        if (q && g_pScr) {   /* only if it really sits in the world next to the Pip-Boy (not a flat menu scene) */
            float dd[3]; V3Sub(dd, (const float *)(q + NIAV_WORLD_TRANSLATE), (const float *)(g_pScr + NIAV_WORLD_TRANSLATE));
            if (!(V3Len(dd) < 120.0f)) { static int wl = 0; if (wl++ < 3) Log("Pip-Boy: menu picture is %.0f units from the glass - not in the world, left alone", V3Len(dd)); q = NULL; }
        }
        if (q != g_pOn) {
            if (g_pOn && g_pOnSaved) PipOn_Restore();
            g_pOn = q; g_pOnSaved = false;
            Log("Pip-Boy: menu %s", q ? "found (the Pip-Boy's interface pieces)" : "not found");
        }
    }
    g_pRelOk = false;
    if (!g_pOn || !g_pScr) return;
    const float *Rs = (const float *)(g_pScr + NIAV_WORLD_ROTATE), *ts = (const float *)(g_pScr + NIAV_WORLD_TRANSLATE);
    const float *Ro = (const float *)(g_pOn + NIAV_WORLD_ROTATE), *to = (const float *)(g_pOn + NIAV_WORLD_TRANSLATE);
    float ss = *(const float *)(g_pScr + NIAV_WORLD_SCALE), so = *(const float *)(g_pOn + NIAV_WORLD_SCALE);
    if (!(ss > 1e-4f) || !(so > 1e-4f)) return;
    float d[3]; V3Sub(d, to, ts);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) g_pRelR[i][j] = Rs[0 * 3 + i] * Ro[0 * 3 + j] + Rs[1 * 3 + i] * Ro[1 * 3 + j] + Rs[2 * 3 + i] * Ro[2 * 3 + j];
        g_pRelT[i] = (Rs[0 * 3 + i] * d[0] + Rs[1 * 3 + i] * d[1] + Rs[2 * 3 + i] * d[2]) / ss;
    }
    g_pRelS = so / ss;
    /* In the game's pose the camera looks straight at the screen, and the picture floats a little in front
       of it. Seen from your eye at an angle that gap shows as an offset - so lay the picture onto the glass. */
    {
        char *cam = NULL; char *sg = *(char **)SCENEGRAPH_PTR_ADDR; if (sg) cam = *(char **)(sg + SCENEGRAPH_CAMERA);
        if (cam) {
            const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
            float dc[3]; V3Sub(dc, C, ts);
            float nl[3]; for (int i = 0; i < 3; i++) nl[i] = Rs[0 * 3 + i] * dc[0] + Rs[1 * 3 + i] * dc[1] + Rs[2 * 3 + i] * dc[2];
            float L = V3Len(nl);
            if (L > 0.5f) {
                for (int i = 0; i < 3; i++) nl[i] /= L;
                /* where the glass is along that direction: the screen geometry's centre */
                float surf = 0.0f;
                char *geo = AsNode(g_pScr) ? (*(uint16_t *)(g_pScr + 0xA6) ? (*(char ***)(g_pScr + 0xA0))[0] : NULL) : NULL;
                const float *gb = geo ? *(const float **)(geo + 0x20) : NULL;
                if (gb) { float dg[3]; V3Sub(dg, gb, ts); for (int i = 0; i < 3; i++) surf += nl[i] * (Rs[0 * 3 + i] * dg[0] + Rs[1 * 3 + i] * dg[1] + Rs[2 * 3 + i] * dg[2]); }
                surf = surf / ss + 0.25f / ss;           /* just above the glass */
                float comp = V3Dot(g_pRelT, nl);
                for (int i = 0; i < 3; i++) g_pRelT[i] += nl[i] * (surf - comp);
                static int ll = 0;
                if (ll++ < 2) Log("Pip-Boy: menu picture floated %.1f units in front of the screen - laid onto the glass", (comp - surf) * ss);
            }
        }
    }
    g_pRelOk = true;
}
static void PipOn_Apply(void)     /* after your arm is posed: put the picture back on the screen */
{
    if (!g_pRelOk || !g_pOn || !g_pScr) return;
    char *par = *(char **)(g_pOn + NIAV_PARENT);
    if (!par) return;
    const float *Rs = (const float *)(g_pScr + NIAV_WORLD_ROTATE), *ts = (const float *)(g_pScr + NIAV_WORLD_TRANSLATE);
    float ss = *(const float *)(g_pScr + NIAV_WORLD_SCALE);
    float Rw[3][3], tw[3], sw = ss * g_pRelS;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) Rw[i][j] = Rs[i * 3 + 0] * g_pRelR[0][j] + Rs[i * 3 + 1] * g_pRelR[1][j] + Rs[i * 3 + 2] * g_pRelR[2][j];
        tw[i] = ts[i] + (Rs[i * 3 + 0] * g_pRelT[0] + Rs[i * 3 + 1] * g_pRelT[1] + Rs[i * 3 + 2] * g_pRelT[2]) * ss;
    }
    const float *Pr = (const float *)(par + NIAV_WORLD_ROTATE), *Pt = (const float *)(par + NIAV_WORLD_TRANSLATE);
    float ps = *(const float *)(par + NIAV_WORLD_SCALE); if (!(ps > 1e-4f)) ps = 1.0f;
    if (!g_pOnSaved) {
        memcpy(g_pOnSave, g_pOn + NIAV_LOCAL_ROTATE, 36); memcpy(g_pOnSave + 9, g_pOn + NIAV_LOCAL_TRANSLATE, 12);
        g_pOnSave[12] = *(float *)(g_pOn + NIAV_LOCAL_SCALE); g_pOnSaved = true;
    }
    float *Lr = (float *)(g_pOn + NIAV_LOCAL_ROTATE), *Lt = (float *)(g_pOn + NIAV_LOCAL_TRANSLATE);
    float d[3]; V3Sub(d, tw, Pt);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) Lr[i * 3 + j] = Pr[0 * 3 + i] * Rw[0][j] + Pr[1 * 3 + i] * Rw[1][j] + Pr[2 * 3 + i] * Rw[2][j];
        Lt[i] = (Pr[0 * 3 + i] * d[0] + Pr[1 * 3 + i] * d[1] + Pr[2 * 3 + i] * d[2]) / ps;
    }
    *(float *)(g_pOn + NIAV_LOCAL_SCALE) = sw / ps;
    NodeUpdate(g_pOn);
    /* Then centre it exactly on the glass and size it to the glass (the game's own offset, measured in its
       raised-arm pose, isn't reliable now that the Pip-Boy opens instantly on your real arm). */
    float bsv[4] = { 0 }, bov[4] = { 0 }; const float *bs = bsv, *bo = bov;
    bool haveS = Geo_Center(g_pScr, bsv, &bsv[3], 0), haveO = Geo_Center(g_pOn, bov, &bov[3], 0);   /* the menu's biggest piece = its panel */
    if (haveS && haveO && bs[3] > 0.01f && bo[3] > 0.01f) {
        float fit = g_pipFit * bs[3] / bo[3];
        if (fit > 0.2f && fit < 5.0f) { *(float *)(g_pOn + NIAV_LOCAL_SCALE) *= fit; NodeUpdate(g_pOn); bov[3] = 0; Geo_Center(g_pOn, bov, &bov[3], 0); }
        char *sg = *(char **)SCENEGRAPH_PTR_ADDR, *cam = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
        float tgt[3] = { bs[0] - g_pipMenuDeltaW[0], bs[1] - g_pipMenuDeltaW[1], bs[2] - g_pipMenuDeltaW[2] };   /* the menu's own nudge doesn't drag the box */
        if (cam) {   /* a hair in front of the glass, toward your eye */
            const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
            float dc[3]; V3Sub(dc, C, bs); float L = V3Len(dc);
            if (L > 0.5f) for (int i = 0; i < 3; i++) tgt[i] += dc[i] / L * g_pipLift;
        }
        {   /* manual nudge (forearm-twist bone axes, game units) */
            static char *twb = NULL; static char *twFor2 = NULL;
            char *root = g_pFor;
            if (twFor2 != root) { twFor2 = root; twb = root ? FindBone(root, "Bip01 L ForeTwist", 0) : NULL; }
            if (twb && (g_pipShift[0] != 0 || g_pipShift[1] != 0 || g_pipShift[2] != 0)) {
                const float *R = (const float *)(twb + NIAV_WORLD_ROTATE); float S = *(const float *)(twb + NIAV_WORLD_SCALE); if (!(S > 1e-4f)) S = 1;
                for (int i = 0; i < 3; i++) tgt[i] += (R[i * 3] * g_pipShift[0] + R[i * 3 + 1] * g_pipShift[1] + R[i * 3 + 2] * g_pipShift[2]) * S;
            }
        }
        float dl[3]; V3Sub(dl, tgt, bo);
        for (int i = 0; i < 3; i++) Lt[i] += (Pr[0 * 3 + i] * dl[0] + Pr[1 * 3 + i] * dl[1] + Pr[2 * 3 + i] * dl[2]) / ps;
        NodeUpdate(g_pOn);
        static int cl = 0;
        if (cl++ < 2) Log("Pip-Boy: menu picture centred on the glass (moved %.1f units, sized x%.2f; glass radius %.1f)", V3Len(dl), fit, bs[3]);
    }
}
static void PipOn_Restore(void)
{
    if (!g_pOnSaved || !g_pOn) return;
    memcpy(g_pOn + NIAV_LOCAL_ROTATE, g_pOnSave, 36); memcpy(g_pOn + NIAV_LOCAL_TRANSLATE, g_pOnSave + 9, 12);
    *(float *)(g_pOn + NIAV_LOCAL_SCALE) = g_pOnSave[12];
    g_pOnSaved = false;
}
/* ---- The Pip-Boy menu on the Pip-Boy 2000's screen ----------------------------------------------------
   The game shows its Pip-Boy menu on a 3D screen shape that it keeps re-placing and re-blending on its own
   (that is what made the menu wander and kept its dark box showing). So FNVR draws the menu itself:
   every frame, right after your arm is posed, it works out where the 2000's glass is - held to the
   forearm-twist bone, which carries the Pip-Boy - and once the frame is finished it lays the game's menu
   picture exactly there, glowing text only (the dark background adds nothing, so no box). The game's own
   copy and the old 3000's dark glass are put out of sight while this works. If the menu picture can't be
   found, the game's own copy is held on the same spot instead, as before. */
static char *PP_FindExact(char *n, const char *name, int depth)
{
    if (!Rd(n, 0xC0) || depth > 40) return NULL;
    const char *nm = *(const char **)(n + 0x08);
    if (Rd(nm, 1) && _stricmp(nm, name) == 0) return n;
    if (!AsNode(n)) return NULL;
    char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
    for (int k = 0; Rd(kids, 4) && k < nk; k++) { char *r = PP_FindExact(kids[k], name, depth + 1); if (r) return r; }
    return NULL;
}
static bool  g_pipScrOn = true;              /* INI [PipScreen] bDrawMenu */
static bool  g_pipOnGameScreen = false;       /* INI [PipScreen] bOnGameScreen: put the menu exactly where the game's own screen is */
static float g_pipScrBright = 1.0f;          /* INI [PipScreen] fBrightness */
static float g_pipScrC[3], g_pipScrR[9];     /* the screen in the twist bone's own frame: centre, axes (columns: right, up, out) */
static float g_pipScrW = 5.0f, g_pipScrH = 3.8f;
static bool  g_pipScrHave = false, g_pipScrUser = false;
static float g_pipScrUV[4] = { 0, 0, 1, 1 }; /* the part of the game's menu picture that holds the menu */
static float g_pipScrAspect = 0;             /* the menu's own width : height */
static char *g_pipTw = NULL, *g_pipTwFor = NULL;
static struct { bool ok; float x[4], y[4], rhw[4]; double at; } g_pipQuad, g_pipQuadLast;
static bool  g_pipScrDrawing = false;        /* FNVR drew the menu on the last frame (the game's copy is hidden) */
static double g_pipScrNoTexAt = 0;           /* when the menu picture last couldn't be found */
static bool g_ppApplyOnly = false; static char *g_lastFP = NULL;

static void PipScr_Ortho(float *R)   /* columns right, up, out -> square and unit length */
{
    float x[3] = { R[0], R[3], R[6] }, y[3] = { R[1], R[4], R[7] }, z[3];
    float l = V3Len(x); if (l < 1e-5f) { x[0] = 1; x[1] = x[2] = 0; l = 1; } for (int i = 0; i < 3; i++) x[i] /= l;
    V3Cross(z, x, y); l = V3Len(z);
    if (l < 1e-5f) { float t[3] = { 0, 0, 1 }; if (fabsf(x[2]) > 0.9f) { t[0] = 1; t[2] = 0; } V3Cross(z, x, t); l = V3Len(z); }
    for (int i = 0; i < 3; i++) z[i] /= l;
    V3Cross(y, z, x);
    for (int i = 0; i < 3; i++) { R[i * 3 + 0] = x[i]; R[i * 3 + 1] = y[i]; R[i * 3 + 2] = z[i]; }
}
static void PipScr_Load(void)
{
    (void)PP_Save;   /* (the old per-part positions are no longer used) */
    g_pipScrOn = GetPrivateProfileIntA("PipScreen", "bDrawMenu", 1, g_iniPath) != 0;
    g_pipOnGameScreen = GetPrivateProfileIntA("PipScreen", "bOnGameScreen", 0, g_iniPath) != 0;
    char b[400];
    GetPrivateProfileStringA("PipScreen", "fBrightness", "1.0", b, sizeof b, g_iniPath);
    float v = (float)atof(b); if (v >= 0.2f && v <= 4.0f) g_pipScrBright = v;
    g_pipScrUser = false;
    if (GetPrivateProfileIntA("PipScreen", "bPlaced", 0, g_iniPath)) {
        GetPrivateProfileStringA("PipScreen", "sPlace", "", b, sizeof b, g_iniPath);
        float f[14]; int n = 0; char *s = b, *e;
        while (n < 14) { float x = strtof(s, &e); if (e == s) break; f[n++] = x; s = e; }
        if (n == 14 && f[12] > 0.3f && f[12] < 40.0f && f[13] > 0.3f && f[13] < 40.0f && fabsf(f[0]) < 80 && fabsf(f[1]) < 80 && fabsf(f[2]) < 80) {
            memcpy(g_pipScrC, f, 12); memcpy(g_pipScrR, f + 3, 36); PipScr_Ortho(g_pipScrR);
            g_pipScrW = f[12]; g_pipScrH = f[13]; g_pipScrHave = g_pipScrUser = true;
        }
    }
    Log("Pip-Boy screen: menu drawn by FNVR %s, brightness %.2f, %s", g_pipScrOn ? "on" : "off", g_pipScrBright,
        g_pipScrUser ? "your saved spot" : "spot found automatically on the 2000's glass");
}
static void PipScr_Save(void)
{
    char b[400];
    snprintf(b, sizeof b, "%.3f %.3f %.3f  %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f  %.3f %.3f",
             g_pipScrC[0], g_pipScrC[1], g_pipScrC[2], g_pipScrR[0], g_pipScrR[1], g_pipScrR[2], g_pipScrR[3], g_pipScrR[4],
             g_pipScrR[5], g_pipScrR[6], g_pipScrR[7], g_pipScrR[8], g_pipScrW, g_pipScrH);
    WritePrivateProfileStringA("PipScreen", "sPlace", b, g_iniPath);
    WritePrivateProfileStringA("PipScreen", "bPlaced", "1", g_iniPath);
    g_pipScrUser = true;
    Log("Pip-Boy screen: your spot saved (centre %.2f %.2f %.2f, %.2f x %.2f units)", g_pipScrC[0], g_pipScrC[1], g_pipScrC[2], g_pipScrW, g_pipScrH);
}
static void PipScr_Forget(void)
{
    WritePrivateProfileStringA("PipScreen", "bPlaced", "0", g_iniPath);
    g_pipScrUser = false; g_pipScrHave = false;
    Log("Pip-Boy screen: back to the automatic spot on the 2000's glass");
}

/* A flat shape's own layout from its corners and picture coordinates: point = O + U*u + V*v (best fit). */
static bool Geo_UVFrame(char *geo, float O[3], float U[3], float V[3], float uvr[4])
{
    if (!IsNiObj(geo) || AsNode(geo)) return false;
    char *gd = *(char **)(geo + 0xB8); if (!IsNiObj(gd)) return false;
    unsigned n = *(uint16_t *)(gd + 0x08);
    const float *vp = *(const float **)(gd + 0x20), *uv = *(const float **)(gd + 0x2C);
    if (n < 3 || n > 4096 || !Rd(vp, n * 12) || !Rd(uv, n * 8)) return false;
    double a = 0, b = 0, c = 0, d = 0, e = 0, f = (double)n, ru[3] = { 0, 0, 0 }, rv[3] = { 0, 0, 0 }, r1[3] = { 0, 0, 0 };
    float umin = 1e9f, umax = -1e9f, vmin = 1e9f, vmax = -1e9f;
    for (unsigned i = 0; i < n; i++) {
        double u = uv[2 * i], v = uv[2 * i + 1];
        if (!isfinite(u) || !isfinite(v)) return false;
        a += u * u; b += u * v; c += u; d += v * v; e += v;
        for (int k = 0; k < 3; k++) { double p = vp[3 * i + k]; ru[k] += u * p; rv[k] += v * p; r1[k] += p; }
        if (u < umin) umin = (float)u; if (u > umax) umax = (float)u; if (v < vmin) vmin = (float)v; if (v > vmax) vmax = (float)v;
    }
    /* solve [a b c; b d e; c e f] x = r for each coordinate (symmetric 3x3) */
    double A00 = d * f - e * e, A01 = -(b * f - c * e), A02 = b * e - c * d, A11 = a * f - c * c, A12 = -(a * e - b * c), A22 = a * d - b * b;
    double det = a * A00 + b * A01 + c * A02;
    if (fabs(det) < 1e-12) return false;
    for (int k = 0; k < 3; k++) {
        U[k] = (float)((A00 * ru[k] + A01 * rv[k] + A02 * r1[k]) / det);
        V[k] = (float)((A01 * ru[k] + A11 * rv[k] + A12 * r1[k]) / det);
        O[k] = (float)((A02 * ru[k] + A12 * rv[k] + A22 * r1[k]) / det);
    }
    uvr[0] = umin; uvr[1] = vmin; uvr[2] = umax; uvr[3] = vmax;
    return V3Len(U) > 1e-4f && V3Len(V) > 1e-4f && umax - umin > 0.01f && vmax - vmin > 0.01f;
}

/* Put the screen rectangle (world centre, world axes, world size) into the twist bone's frame. */
static void PipScr_SetFromWorld(char *tw, const float cw[3], const float rw[3], const float uw[3], float w, float h)
{
    const float *TR = (const float *)(tw + NIAV_WORLD_ROTATE), *TT = (const float *)(tw + NIAV_WORLD_TRANSLATE);
    float TS = *(const float *)(tw + NIAV_WORLD_SCALE); if (!(TS > 1e-4f)) TS = 1;
    float d[3]; V3Sub(d, cw, TT);
    float ow[3]; V3Cross(ow, rw, uw);
    for (int i = 0; i < 3; i++) {
        g_pipScrC[i] = (TR[0 * 3 + i] * d[0] + TR[1 * 3 + i] * d[1] + TR[2 * 3 + i] * d[2]) / TS;
        g_pipScrR[i * 3 + 0] = TR[0 * 3 + i] * rw[0] + TR[1 * 3 + i] * rw[1] + TR[2 * 3 + i] * rw[2];
        g_pipScrR[i * 3 + 1] = TR[0 * 3 + i] * uw[0] + TR[1 * 3 + i] * uw[1] + TR[2 * 3 + i] * uw[2];
        g_pipScrR[i * 3 + 2] = TR[0 * 3 + i] * ow[0] + TR[1 * 3 + i] * ow[1] + TR[2 * 3 + i] * ow[2];
    }
    PipScr_Ortho(g_pipScrR);
    g_pipScrW = w / TS; g_pipScrH = h / TS;
    if (g_pipScrAspect > 0.3f && g_pipScrAspect < 4.0f) {   /* keep the menu's own proportions inside the glass */
        if (g_pipScrW / g_pipScrH > g_pipScrAspect) g_pipScrW = g_pipScrH * g_pipScrAspect; else g_pipScrH = g_pipScrW / g_pipScrAspect;
    }
    g_pipScrHave = true;
}
/* Default spot: the 2000's own glass layer (its glare/static shape sits exactly on the 2000's screen).
   The menu reads upright the way you hold the Pip-Boy up to your face (its top away from you). */
static bool PipScr_DefaultFrom(char *geo, char *tw, char *cam)
{
    if (!IsNiObj(geo) || !tw || !cam) return false;
    const float *GR = (const float *)(geo + NIAV_WORLD_ROTATE), *GT = (const float *)(geo + NIAV_WORLD_TRANSLATE);
    float GS = *(const float *)(geo + NIAV_WORLD_SCALE); if (!(GS > 1e-4f)) GS = 1;
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float camUp[3] = { Cw[1], Cw[4], Cw[7] };
    float cw[3], ax[2][3], ext[2];
    float O[3], U[3], V[3], uvr[4];
    bool skinned = *(char **)(geo + 0xBC) != NULL;
    if (!skinned && Geo_UVFrame(geo, O, U, V, uvr)) {
        float uc = (uvr[0] + uvr[2]) * 0.5f, vc = (uvr[1] + uvr[3]) * 0.5f, cl[3];
        for (int i = 0; i < 3; i++) cl[i] = O[i] + U[i] * uc + V[i] * vc;
        for (int i = 0; i < 3; i++) {
            cw[i] = GT[i] + (GR[i * 3] * cl[0] + GR[i * 3 + 1] * cl[1] + GR[i * 3 + 2] * cl[2]) * GS;
            ax[0][i] = (GR[i * 3] * U[0] + GR[i * 3 + 1] * U[1] + GR[i * 3 + 2] * U[2]) * GS;
            ax[1][i] = (GR[i * 3] * V[0] + GR[i * 3 + 1] * V[1] + GR[i * 3 + 2] * V[2]) * GS;
        }
        ext[0] = V3Len(ax[0]) * (uvr[2] - uvr[0]); ext[1] = V3Len(ax[1]) * (uvr[3] - uvr[1]);
    } else {   /* skinned or no picture coordinates: its bounds, square to the forearm */
        const float *bd = *(const float **)(geo + 0x20);
        if (!Rd(bd, 16) || !(bd[3] > 0.5f && bd[3] < 20.0f)) return false;
        memcpy(cw, bd, 12);
        const float *TR = (const float *)(tw + NIAV_WORLD_ROTATE);
        for (int i = 0; i < 3; i++) { ax[0][i] = TR[i * 3 + 0]; ax[1][i] = TR[i * 3 + 1]; }
        ext[0] = bd[3] * 1.6f; ext[1] = bd[3] * 1.2f;
    }
    float l0 = V3Len(ax[0]), l1 = V3Len(ax[1]);
    if (l0 < 1e-5f || l1 < 1e-5f || !(ext[0] > 0.8f && ext[0] < 30.0f && ext[1] > 0.8f && ext[1] < 30.0f)) return false;
    for (int i = 0; i < 3; i++) { ax[0][i] /= l0; ax[1][i] /= l1; }
    float out[3]; V3Cross(out, ax[0], ax[1]); float ol = V3Len(out); if (ol < 1e-4f) return false;
    for (int i = 0; i < 3; i++) out[i] /= ol;
    float toEye[3]; V3Sub(toEye, C, cw);
    if (V3Dot(out, toEye) < 0) for (int i = 0; i < 3; i++) out[i] = -out[i];
    /* the menu's "up": along the glass's SHORT side (the screen is wider than tall), pointing the way that's
       most like up in your view - so how you hold your wrist when it's found can't turn it sideways */
    int bk = ext[0] < ext[1] ? 0 : 1;
    float bs = V3Dot(ax[bk], camUp) >= 0 ? 1.0f : -1.0f;
    float up[3], right[3];
    for (int i = 0; i < 3; i++) up[i] = ax[bk][i] * bs;
    float pd = V3Dot(up, out); for (int i = 0; i < 3; i++) up[i] -= out[i] * pd;
    float ul = V3Len(up); if (ul < 1e-4f) return false; for (int i = 0; i < 3; i++) up[i] /= ul;
    V3Cross(right, up, out);
    float w = bk == 1 ? ext[0] : ext[1], h = bk == 1 ? ext[1] : ext[0];
    for (int i = 0; i < 3; i++) cw[i] += out[i] * 0.12f;   /* a hair above the glass */
    PipScr_SetFromWorld(tw, cw, right, up, w * 0.94f, h * 0.94f);
    const char *nm = *(const char **)(geo + 0x08);
    Log("Pip-Boy screen: found the 2000's glass on '%s'%s - %.1f x %.1f units, centre %.2f %.2f %.2f on the forearm-twist bone",
        Rd(nm, 1) ? nm : "?", skinned ? " (from its bounds)" : "", w, h, g_pipScrC[0], g_pipScrC[1], g_pipScrC[2]);
    return true;
}
static void PipScr_DefaultFallback(char *tw, char *cam)
{
    /* nothing to measure: in front of the forearm, a little toward the wrist, facing your eye */
    const float *TR = (const float *)(tw + NIAV_WORLD_ROTATE), *TT = (const float *)(tw + NIAV_WORLD_TRANSLATE);
    float TS = *(const float *)(tw + NIAV_WORLD_SCALE); if (!(TS > 1e-4f)) TS = 1;
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float along[3] = { TR[0], TR[3], TR[6] }, out[3] = { TR[2], TR[5], TR[8] }, cw[3];
    for (int i = 0; i < 3; i++) cw[i] = TT[i] + (along[i] * 4.0f) * TS;
    float toEye[3]; V3Sub(toEye, C, cw);
    if (V3Dot(out, toEye) < 0) for (int i = 0; i < 3; i++) out[i] = -out[i];
    for (int i = 0; i < 3; i++) cw[i] += out[i] * 3.2f * TS;
    float camUp[3] = { Cw[1], Cw[4], Cw[7] }, up[3], right[3];
    float pd = V3Dot(camUp, out); for (int i = 0; i < 3; i++) up[i] = camUp[i] - out[i] * pd;
    float ul = V3Len(up); if (ul < 1e-4f) { memcpy(up, along, 12); ul = 1; } for (int i = 0; i < 3; i++) up[i] /= ul;
    V3Cross(right, up, out);
    PipScr_SetFromWorld(tw, cw, right, up, 5.2f * TS, 3.9f * TS);
    Log("Pip-Boy screen: no 2000 glass layer found - placed in front of the forearm (adjust it with the left stick click)");
}

/* the screen's own rotations: turn R around its own axis k by angle a */
static void PipScr_Turn(int k, float a)
{
    M3 R, Q = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }, Rn;
    memcpy(R, g_pipScrR, sizeof R);
    int k1 = (k + 1) % 3, k2 = (k + 2) % 3; float c = cosf(a), s = sinf(a);
    Q[k1][k1] = c; Q[k1][k2] = -s; Q[k2][k1] = s; Q[k2][k2] = c;
    M3Mul(Rn, R, Q); memcpy(g_pipScrR, Rn, sizeof Rn); PipScr_Ortho(g_pipScrR);
}
static void PipScr_Adjust(void)
{
    if (g_scrResetReq) { g_scrResetReq = false; g_pipScrHave = false; g_pipScrUser = false; UIMessage("Pip-Boy screen: back to the automatic spot"); return; }
    if (g_scrTurnReq) { g_scrTurnReq = false; PipScr_Turn(2, -PI_F * 0.5f); float t = g_pipScrW; g_pipScrW = g_pipScrH; g_pipScrH = t; }
    float dt = g_adjDt; g_adjDt = 0;
    if (!g_adjOn || dt <= 0) return;
    float lx = g_adjIn[0], ly = g_adjIn[1], ry = g_adjIn[2], rx = g_adjIn[3];
    if (g_adjMode == 0) {   /* move it across the forearm (as you see it), closer/further, bigger/smaller */
        float sp = 2.0f * dt;
        for (int i = 0; i < 3; i++) g_pipScrC[i] += (g_pipScrR[i * 3 + 0] * lx + g_pipScrR[i * 3 + 1] * ly + g_pipScrR[i * 3 + 2] * rx * 0.5f) * sp;
        float k = 1.0f + ry * 0.5f * dt;
        if (g_pipScrW * k > 0.6f && g_pipScrW * k < 40.0f && g_pipScrH * k > 0.6f && g_pipScrH * k < 40.0f) { g_pipScrW *= k; g_pipScrH *= k; }
    } else {                /* tilt it, turn it, stretch it taller/shorter */
        float sp = 0.6f * dt;
        if (lx) PipScr_Turn(2, -lx * sp);
        if (ly) PipScr_Turn(0, ly * sp);
        if (rx) PipScr_Turn(1, rx * sp);
        float k = 1.0f + ry * 0.5f * dt;
        if (g_pipScrH * k > 0.6f && g_pipScrH * k < 40.0f) g_pipScrH *= k;
    }
    for (int i = 0; i < 3; i++) { if (g_pipScrC[i] > 60) g_pipScrC[i] = 60; if (g_pipScrC[i] < -60) g_pipScrC[i] = -60; }
}

/* Once your arm is posed for the frame: where the screen is, and where that lands in the picture. */
static bool g_pipPrepQuiet = false;   /* (a second look in the same frame: no adjusting, no saving) */
static void PipScr_Prepare(char *player, char *cam)
{
    g_pipQuad.ok = false;
    if (!g_pipScrOn || !g_wristPip || !g_pipOpen || g_pipModeRaw != 3 || !cam || !player || !g_bbW) return;
    char *node = *(char **)(player + PLAYER_NODE_1ST_PERSON);
    if (!node) return;
    static uint32_t ptGen = 0;
    if (g_pipTwFor != node || ptGen != g_fpGen) { g_pipTwFor = node; ptGen = g_fpGen; g_pipTw = FindBone(node, "Bip01 L ForeTwist", 0); if (!g_pipScrUser) g_pipScrHave = false; }
    char *tw = g_pipTw; if (!tw) return;
    {   /* the game's own screen shape: which part of the menu picture is the menu, and its proportions */
        static char *aspFor = NULL;
        char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
        char *menu = Rd(pm, 8) ? *(char **)(pm + 4) : NULL;
        if (IsNiObj(menu) && menu != aspFor) {
            aspFor = menu;
            float O[3], U[3], V[3], uvr[4];
            if (Geo_UVFrame(menu, O, U, V, uvr)) {
                memcpy(g_pipScrUV, uvr, sizeof uvr);
                g_pipScrAspect = V3Len(U) * (uvr[2] - uvr[0]) / fmaxf(V3Len(V) * (uvr[3] - uvr[1]), 1e-4f);
                if (!(g_pipScrAspect > 0.3f && g_pipScrAspect < 4.0f)) g_pipScrAspect = 4.0f / 3.0f;
                Log("Pip-Boy screen: the menu uses picture area %.2f,%.2f - %.2f,%.2f, shape %.2f : 1", uvr[0], uvr[1], uvr[2], uvr[3], g_pipScrAspect);
            } else { g_pipScrAspect = 4.0f / 3.0f; Log("Pip-Boy screen: couldn't read the game's screen shape - using the whole menu picture"); }
        }
    }
    if (!g_pipScrHave) {
        static const char *glass[] = { "glare:0", "static:0", "glare", "static" };
        bool ok = false;
        for (int k = 0; k < 4 && !ok; k++) ok = PipScr_DefaultFrom(PP_FindExact(node, glass[k], 0), tw, cam);
        if (!ok) PipScr_DefaultFallback(tw, cam);
    }
    if (!g_pipPrepQuiet) {
        PipScr_Adjust();
        if (g_ppSaveReq) { g_ppSaveReq = false; PipScr_Save(); UIMessage("Pip-Boy screen: saved"); }
    }
    if (g_pipOnGameScreen && !g_adjOn) {   /* the game's own screen shape on your arm: the menu goes exactly there */
        char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
        char *menu = Rd(pm, 8) ? *(char **)(pm + 4) : NULL;
        float O[3], U[3], V[3], uvr[4];
        if (IsNiObj(menu) && Geo_UVFrame(menu, O, U, V, uvr)) {
            const float *R = (const float *)(menu + NIAV_WORLD_ROTATE), *T = (const float *)(menu + NIAV_WORLD_TRANSLATE);
            float S = *(const float *)(menu + NIAV_WORLD_SCALE); if (!(S > 1e-6f)) S = 1;
            const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
            float F[3] = { Cw[0], Cw[3], Cw[6] }, Up[3] = { Cw[1], Cw[4], Cw[7] }, Rt[3] = { Cw[2], Cw[5], Cw[8] };
            const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
            static const int cu[4] = { 0, 2, 2, 0 }, cv[4] = { 1, 1, 3, 3 };   /* top-left, top-right, bottom-right, bottom-left */
            bool ok = fr[1] > fr[0] && fr[2] > fr[3];
            float W0[3] = { 0, 0, 0 }, Wk[4][3];
            for (int k = 0; k < 4 && ok; k++) {
                float L[3], q[3];
                for (int i = 0; i < 3; i++) L[i] = O[i] + U[i] * uvr[cu[k]] + V[i] * uvr[cv[k]];
                for (int i = 0; i < 3; i++) Wk[k][i] = T[i] + (R[i * 3] * L[0] + R[i * 3 + 1] * L[1] + R[i * 3 + 2] * L[2]) * S;
                V3Sub(q, Wk[k], C);
                float z = V3Dot(q, F); if (z < 0.5f) { ok = false; break; }
                g_pipQuad.x[k] = (V3Dot(q, Rt) / z - fr[0]) / (fr[1] - fr[0]) * g_bbW;
                g_pipQuad.y[k] = (fr[2] - V3Dot(q, Up) / z) / (fr[2] - fr[3]) * g_bbH;
                g_pipQuad.rhw[k] = 1.0f / z;
                for (int i = 0; i < 3; i++) W0[i] += Wk[k][i] * 0.25f;
            }
            /* sanity: the shape has to be on your arm (near the forearm), or it's stale - then fall back */
            const float *TT0 = (const float *)(tw + NIAV_WORLD_TRANSLATE); float dd[3]; V3Sub(dd, W0, TT0);
            if (ok && V3Len(dd) < 60.0f) {
                g_pipQuad.ok = true; g_pipQuad.at = QpcMs(); g_pipQuadLast = g_pipQuad;
                static int nl = 0; if (nl++ == 30) Log("Pip-Boy screen: menu placed on the game's own Pip-Boy screen (%.1f units from the forearm)", V3Len(dd));
                return;
            }
            static int nb = 0; if (nb++ < 5) Log("Pip-Boy screen: the game's screen shape isn't on the arm (%.0f units away) - using your saved spot", V3Len(dd));
        }
    }
    if (!g_pipScrHave) return;
    const float *TR = (const float *)(tw + NIAV_WORLD_ROTATE), *TT = (const float *)(tw + NIAV_WORLD_TRANSLATE);
    float TS = *(const float *)(tw + NIAV_WORLD_SCALE); if (!(TS > 1e-4f)) TS = 1;
    float cw[3], ax[3][3];
    for (int i = 0; i < 3; i++) {
        cw[i] = TT[i] + (TR[i * 3] * g_pipScrC[0] + TR[i * 3 + 1] * g_pipScrC[1] + TR[i * 3 + 2] * g_pipScrC[2]) * TS;
        for (int k = 0; k < 3; k++) ax[k][i] = TR[i * 3] * g_pipScrR[0 * 3 + k] + TR[i * 3 + 1] * g_pipScrR[1 * 3 + k] + TR[i * 3 + 2] * g_pipScrR[2 * 3 + k];
    }
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float toEye[3]; V3Sub(toEye, C, cw);
    if (V3Dot(toEye, ax[2]) <= 0) return;   /* turned away from you */
    float F[3] = { Cw[0], Cw[3], Cw[6] }, Up[3] = { Cw[1], Cw[4], Cw[7] }, Rt[3] = { Cw[2], Cw[5], Cw[8] };
    const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
    float l = fr[0], r = fr[1], t = fr[2], b = fr[3];
    if (!(r > l) || !(t > b)) return;
    float hw = g_pipScrW * TS * 0.5f, hh = g_pipScrH * TS * 0.5f;
    static const float cs[4][2] = { { -1, 1 }, { 1, 1 }, { 1, -1 }, { -1, -1 } };   /* top-left, top-right, bottom-right, bottom-left */
    for (int k = 0; k < 4; k++) {
        float P[3], q[3];
        for (int i = 0; i < 3; i++) P[i] = cw[i] + ax[0][i] * hw * cs[k][0] + ax[1][i] * hh * cs[k][1];
        V3Sub(q, P, C);
        float z = V3Dot(q, F);
        if (z < 0.5f) return;
        g_pipQuad.x[k] = (V3Dot(q, Rt) / z - l) / (r - l) * g_bbW;
        g_pipQuad.y[k] = (t - V3Dot(q, Up) / z) / (t - b) * g_bbH;
        g_pipQuad.rhw[k] = 1.0f / z;
    }
    g_pipQuad.ok = true; g_pipQuad.at = QpcMs();
    g_pipQuadLast = g_pipQuad;
}

/* ---- Pip-Boy menu capture --------------------------------------------------------------------------
   The Pip-Boy manager's own "draw the menu" step is wrapped (through its function table): the pictures the
   game draws into during it are copied straight away into FNVR's own textures. The one that looks like the
   menu (dark, with bright crisp text) is then drawn onto your Pip-Boy screen at the end of every frame - on
   top of everything, so neither the live world nor the Pip-Boy glass can hide it. */
void *g_pmOrig[2] __attribute__((used));
void *g_pmRetStk[32] __attribute__((used));
int   g_pmRetTop __attribute__((used));
void *g_pmJmp __attribute__((used));
void PM_Before0(void) __attribute__((used)); void PM_After0(void) __attribute__((used));
void PM_Before1(void) __attribute__((used)); void PM_After1(void) __attribute__((used));
extern void PM_Stub0(void); extern void PM_Stub1(void);
/* A wrapper that works whatever the wrapped function's arguments are: it swaps the return address for its
   own, so it gets control back after the function has cleaned up its own arguments. */
__asm__(
".text\n"
".p2align 4\n"
"_PM_Stub0:\n"
"  pushl %ecx\n  pushl %edx\n  pushl %eax\n"
"  movl _g_pmRetTop, %eax\n  cmpl $30, %eax\n  jae 1f\n"
"  call _PM_Before0\n"
"  movl _g_pmRetTop, %eax\n  movl 12(%esp), %edx\n  movl %edx, _g_pmRetStk(,%eax,4)\n"
"  incl %eax\n  movl %eax, _g_pmRetTop\n  movl $_PM_Post0, 12(%esp)\n"
"1:\n"
"  popl %eax\n  popl %edx\n  popl %ecx\n"
"  jmpl *_g_pmOrig\n"
"_PM_Post0:\n"
"  pushl %eax\n  pushl %edx\n"
"  call _PM_After0\n"
"  movl _g_pmRetTop, %eax\n  decl %eax\n  movl %eax, _g_pmRetTop\n"
"  movl _g_pmRetStk(,%eax,4), %eax\n  movl %eax, _g_pmJmp\n"
"  popl %edx\n  popl %eax\n"
"  jmpl *_g_pmJmp\n"
".p2align 4\n"
"_PM_Stub1:\n"
"  pushl %ecx\n  pushl %edx\n  pushl %eax\n"
"  movl _g_pmRetTop, %eax\n  cmpl $30, %eax\n  jae 1f\n"
"  call _PM_Before1\n"
"  movl _g_pmRetTop, %eax\n  movl 12(%esp), %edx\n  movl %edx, _g_pmRetStk(,%eax,4)\n"
"  incl %eax\n  movl %eax, _g_pmRetTop\n  movl $_PM_Post1, 12(%esp)\n"
"1:\n"
"  popl %eax\n  popl %edx\n  popl %ecx\n"
"  jmpl *_g_pmOrig+4\n"
"_PM_Post1:\n"
"  pushl %eax\n  pushl %edx\n"
"  call _PM_After1\n"
"  movl _g_pmRetTop, %eax\n  decl %eax\n  movl %eax, _g_pmRetTop\n"
"  movl _g_pmRetStk(,%eax,4), %eax\n  movl %eax, _g_pmJmp\n"
"  popl %edx\n  popl %eax\n"
"  jmpl *_g_pmJmp\n"
);
#define PM_SLOTS 6
static IDirect3DTexture9 *g_pmCopyTex[PM_SLOTS]; static IDirect3DSurface9 *g_pmCopySurf[PM_SLOTS];
static UINT g_pmCopyW[PM_SLOTS], g_pmCopyH[PM_SLOTS]; static D3DFORMAT g_pmCopyF[PM_SLOTS]; static int g_pmCopyRank[PM_SLOTS];
static int g_pmCopyN = 0; static double g_pmCopyAt = 0;
static bool g_pmAlpha[PM_SLOTS]; static bool g_pmLockAlpha = false;   /* the menu is in the picture's transparency (white picture, text in alpha) */
static int g_pmLock = -1; static UINT g_pmLockW, g_pmLockH; static int g_pmLockRank; static IDirect3DSurface9 *g_pmLockSurf, *g_pmCopySrc[PM_SLOTS];   /* the menu picture, once recognised */
static unsigned g_pmCalls[3];
static IDirect3DSurface9 *g_pmSmall, *g_pmSys;   /* for looking at a candidate on the CPU */
static void PM_Release(void)
{
    for (int i = 0; i < PM_SLOTS; i++) {
        if (g_pmCopySurf[i]) IDirect3DSurface9_Release(g_pmCopySurf[i]); g_pmCopySurf[i] = NULL;
        if (g_pmCopyTex[i]) IDirect3DTexture9_Release(g_pmCopyTex[i]); g_pmCopyTex[i] = NULL;
        g_pmCopyW[i] = g_pmCopyH[i] = 0;
    }
    if (g_pmSmall) IDirect3DSurface9_Release(g_pmSmall); g_pmSmall = NULL;
    if (g_pmSys) IDirect3DSurface9_Release(g_pmSys); g_pmSys = NULL;
    g_pmCopyN = 0;
}
static bool PM_Wanted(void) { return g_pipOpen && g_pipModeRaw == 3 && g_pipScrOn && g_wristPip && g_hookedDevice; }
static bool PM_Candidate(IDirect3DSurface9 *sf, D3DSURFACE_DESC *d)
{
    if (FAILED(IDirect3DSurface9_GetDesc(sf, d))) return false;
    if (!(d->Usage & D3DUSAGE_RENDERTARGET) || d->Pool != D3DPOOL_DEFAULT) return false;
    if (d->Width < 128 || d->Height < 128 || d->Width > 2048 || d->Height > 2048) return false;
    if (g_bbW && d->Width * 2 > g_bbW && d->Height * 2 > g_bbH) return false;   /* full-screen work pictures */
    for (UINT k = 2; k <= 16 && g_bbW; k *= 2) if (d->Width * k == g_bbW && d->Height * k == g_bbH) return false;   /* glow/blur steps (screen/2, /4...) */
    float asp = (float)d->Width / (float)d->Height; if (asp > 2.5f || asp < 0.4f) return false;
    int f = (int)d->Format;
    return f == D3DFMT_A8R8G8B8 || f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8B8G8R8 || f == D3DFMT_X8B8G8R8 ||
           f == D3DFMT_A2R10G10B10 || f == D3DFMT_A2B10G10R10 || f == D3DFMT_A16B16G16R16F || f == D3DFMT_A16B16G16R16 || f == D3DFMT_R5G6B5;
}
static bool PM_EnsureCopy(IDirect3DDevice9 *dev, int k, const D3DSURFACE_DESC *d)
{
    if (g_pmCopyTex[k] && g_pmCopyW[k] == d->Width && g_pmCopyH[k] == d->Height && g_pmCopyF[k] == d->Format) return true;
    if (g_pmCopySurf[k]) IDirect3DSurface9_Release(g_pmCopySurf[k]); g_pmCopySurf[k] = NULL;
    if (g_pmCopyTex[k]) IDirect3DTexture9_Release(g_pmCopyTex[k]); g_pmCopyTex[k] = NULL;
    if (FAILED(IDirect3DDevice9_CreateTexture(dev, d->Width, d->Height, 1, D3DUSAGE_RENDERTARGET, d->Format, D3DPOOL_DEFAULT, &g_pmCopyTex[k], NULL)) || !g_pmCopyTex[k]) { g_pmCopyTex[k] = NULL; return false; }
    if (FAILED(IDirect3DTexture9_GetSurfaceLevel(g_pmCopyTex[k], 0, &g_pmCopySurf[k]))) { IDirect3DTexture9_Release(g_pmCopyTex[k]); g_pmCopyTex[k] = NULL; return false; }
    g_pmCopyW[k] = d->Width; g_pmCopyH[k] = d->Height; g_pmCopyF[k] = d->Format;
    return true;
}
/* Copy what the game just drew. A picture is copied when the game switches away from drawing into it and
   again when the wrapped step ends (the last copy within the step wins); once the step is over, the same
   picture reused for something else later in the frame (shadows...) is left alone. */
static unsigned g_pmCapFrame = 0xFFFFFFFFu; static int g_pmFrameN = 0, g_pmSeenN = 0, g_pmWindow = 0;
static struct { IDirect3DSurface9 *sf; UINT w, h; int rank, slot, window; } g_pmSeen[16];
static void PM_CopySurf(IDirect3DSurface9 *sf)
{
    IDirect3DDevice9 *dev = g_hookedDevice;
    if (!dev || !sf || sf == g_uiSurf || !PM_Wanted()) return;
    if (g_pmCapFrame != g_frameId) { g_pmCapFrame = g_frameId; g_pmFrameN = 0; g_pmSeenN = 0; }
    int e = -1; for (int j = 0; j < g_pmSeenN; j++) if (g_pmSeen[j].sf == sf) e = j;
    if (e >= 0 && g_pmSeen[e].window != g_pmWindow) return;   /* reused later in the frame for something else */
    D3DSURFACE_DESC d;
    if (e < 0) {
        IDirect3DSurface9 *bb = NULL; IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        bool isBB = sf == bb; if (bb) IDirect3DSurface9_Release(bb);
        if (isBB || !PM_Candidate(sf, &d) || g_pmSeenN >= 16) return;
        int rank = 0; for (int j = 0; j < g_pmSeenN; j++) if (g_pmSeen[j].w == d.Width && g_pmSeen[j].h == d.Height) rank++;
        int slot = -1;
        if (g_pmLock >= 0) { slot = -1; }
        else if (g_pmFrameN < PM_SLOTS - 1) slot = 1 + g_pmFrameN++;
        e = g_pmSeenN++;
        g_pmSeen[e].sf = sf; g_pmSeen[e].w = d.Width; g_pmSeen[e].h = d.Height; g_pmSeen[e].rank = rank; g_pmSeen[e].slot = slot; g_pmSeen[e].window = g_pmWindow;
    } else if (FAILED(IDirect3DSurface9_GetDesc(sf, &d))) return;
    int slot = g_pmSeen[e].slot;
    if (slot < 0) return;
    if (PM_EnsureCopy(dev, slot, &d) && SUCCEEDED(IDirect3DDevice9_StretchRect(dev, sf, NULL, g_pmCopySurf[slot], NULL, D3DTEXF_NONE))) {
        g_pmCopyRank[slot] = g_pmSeen[e].rank; g_pmCopyAt = QpcMs(); g_pmCopySrc[slot] = sf;
        if (g_pmLock >= 0) g_pmCopyN = 1; else if (slot + 1 > g_pmCopyN || g_pmCopyN > g_pmFrameN) g_pmCopyN = g_pmFrameN;
    }
}
static void PM_Capture(void)
{
    if (g_hookedDevice && PM_Wanted()) {
        IDirect3DSurface9 *cur = NULL;   /* (what's being drawn into right now counts too) */
        if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(g_hookedDevice, 0, &cur)) && cur) { PM_Record(cur); IDirect3DSurface9_Release(cur); }
        for (int i = 0; i < g_pmRecN; i++) PM_CopySurf(g_pmRec[i]);
    }
    for (int i = 0; i < g_pmRecN; i++) IDirect3DSurface9_Release(g_pmRec[i]);
    g_pmRecN = 0;
}
static void PM_Begin(int k)
{
    if (!PM_Wanted()) return;
    g_pmCalls[k]++;
    if (g_pmDepth == 0) g_pmWindow++;
    g_pmDepth++;
    IDirect3DSurface9 *cur = NULL;   /* the picture already set when the step starts counts too */
    if (g_hookedDevice && SUCCEEDED(IDirect3DDevice9_GetRenderTarget(g_hookedDevice, 0, &cur)) && cur) { PM_Record(cur); IDirect3DSurface9_Release(cur); }
}
static void PM_End(void)
{
    if (g_pmDepth <= 0) return;
    PM_Capture();
    g_pmDepth--;
}
static int g_pmActive[3];   /* (which wrapped steps are inside a PM_Begin) */
void PM_Before0(void) { if (g_trOn) { Tr_Flush(); Log("TRACE   [Pip-Boy draw step begins]"); } g_pmActive[0] = PM_Wanted(); if (g_pmActive[0]) PM_Begin(0); }
/* The finished Pip-Boy screen picture: the game draws the menu into a full-size picture during its Pip-Boy
   draw step, then runs its screen effect into a second one - that second one is what is being drawn into
   when the step ends, in paused and running mode alike. It is copied right there, every frame. */
static float PM_Score(IDirect3DDevice9 *dev, int k, bool save);
static void PM_TakeFinal(void)
{
    IDirect3DDevice9 *dev = g_hookedDevice; if (!dev) return;
    IDirect3DSurface9 *cur = NULL;
    if (FAILED(IDirect3DDevice9_GetRenderTarget(dev, 0, &cur)) || !cur) return;
    D3DSURFACE_DESC d; bool ok = SUCCEEDED(IDirect3DSurface9_GetDesc(cur, &d)) && d.Width >= 256 && d.Height >= 256 && d.MultiSampleType == D3DMULTISAMPLE_NONE;
    IDirect3DSurface9 *bb = NULL; IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
    if (cur == bb) ok = false; if (bb) IDirect3DSurface9_Release(bb);
    static int why = 0;
    if (!ok) { if (why++ < 5) Log("Pip-Boy menu: the draw step ended on an unexpected picture (%ux%u) - skipped", d.Width, d.Height); IDirect3DSurface9_Release(cur); return; }
    D3DSURFACE_DESC want = d; want.Width = d.Width > 2048 ? d.Width / 2 : d.Width; want.Height = d.Width > 2048 ? d.Height / 2 : d.Height;
    if (PM_EnsureCopy(dev, 0, &want) && SUCCEEDED(IDirect3DDevice9_StretchRect(dev, cur, NULL, g_pmCopySurf[0], NULL, D3DTEXF_LINEAR))) {
        g_pmLock = 0; g_pmLockSurf = NULL; g_pmLockAlpha = false; g_pmCopyN = 1; g_pmCopyAt = QpcMs();
        g_pmLockW = want.Width; g_pmLockH = want.Height;
        static int n = 0; if (++n == 1) Log("Pip-Boy menu: taking the finished screen picture from the game's Pip-Boy draw step (%ux%u format %d)", d.Width, d.Height, (int)d.Format);
        if (n == 90) PM_Score(dev, 0, true);   /* (snapshot for checking: FNVR_menucap_0.bmp) */
    } else if (why++ < 5) Log("Pip-Boy menu: copying the screen picture failed");
    IDirect3DSurface9_Release(cur);
}
void PM_After0(void)  { if (g_trOn) { Tr_Flush(); Log("TRACE   [Pip-Boy draw step ends]"); } if (g_pmActive[0]) { g_pmActive[0] = 0; PM_End(); PM_TakeFinal(); } }
void PM_Before1(void) { g_pmActive[1] = PM_Wanted(); if (g_pmActive[1]) PM_Begin(1); }
void PM_After1(void)  { if (g_pmActive[1]) { g_pmActive[1] = 0; PM_End(); } }
static void InstallPipMenuCapture(void)
{
    static bool done = false; if (done) return; done = true;
    void **vt = (void **)0x010780AC;   /* FOPipboyManager's function table */
    if (!Rd(vt, 0x58)) { Log("Pip-Boy menu capture: function table not readable"); return; }
    for (int k = 0; k < 2; k++) {
        int slot = k ? 5 : 3;   /* 3 = draw the menu, 5 = update */
        uintptr_t f = (uintptr_t)vt[slot];
        if (f < 0x00401000 || f > 0x7FFFFFFF || !Rd((void *)f, 4)) { Log("Pip-Boy menu capture: entry %d looks wrong (%08lX) - skipped", slot, (unsigned long)f); continue; }
        g_pmOrig[k] = (void *)f;
        PatchVtable(vt, slot, k ? (void *)PM_Stub1 : (void *)PM_Stub0);
        Log("Pip-Boy menu capture: wrapped the Pip-Boy's %s step (%08lX)", k ? "update" : "draw", (unsigned long)f);
    }
}
/* How much a picture looks like the Pip-Boy menu: mostly dark, some bright crisp coloured text and lines. */
static float PM_Score(IDirect3DDevice9 *dev, int k, bool save)
{
    if (!g_pmCopySurf[k]) return -1000;
    if (!g_pmSmall && FAILED(IDirect3DDevice9_CreateRenderTarget(dev, 256, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_pmSmall, NULL))) { g_pmSmall = NULL; return -1000; }
    if (!g_pmSys && FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 256, 256, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &g_pmSys, NULL))) { g_pmSys = NULL; return -1000; }
    /* only the part of the picture the menu uses */
    RECT src = { 0, 0, (LONG)g_pmCopyW[k], (LONG)g_pmCopyH[k] };
    if (g_pipScrUV[2] > g_pipScrUV[0] + 0.05f && g_pipScrUV[3] > g_pipScrUV[1] + 0.05f) {
        src.left = (LONG)(fmaxf(g_pipScrUV[0], 0.0f) * g_pmCopyW[k]); src.right = (LONG)(fminf(g_pipScrUV[2], 1.0f) * g_pmCopyW[k]);
        src.top = (LONG)(fmaxf(g_pipScrUV[1], 0.0f) * g_pmCopyH[k]); src.bottom = (LONG)(fminf(g_pipScrUV[3], 1.0f) * g_pmCopyH[k]);
        if (src.right - src.left < 16 || src.bottom - src.top < 16) { src.left = src.top = 0; src.right = g_pmCopyW[k]; src.bottom = g_pmCopyH[k]; }
    }
    if (FAILED(IDirect3DDevice9_StretchRect(dev, g_pmCopySurf[k], &src, g_pmSmall, NULL, D3DTEXF_POINT)) ||
        FAILED(IDirect3DDevice9_GetRenderTargetData(dev, g_pmSmall, g_pmSys))) return -1000;
    D3DLOCKED_RECT lr; if (FAILED(IDirect3DSurface9_LockRect(g_pmSys, &lr, NULL, D3DLOCK_READONLY))) return -1000;
    long n = 0, dark = 0, bright = 0, edges = 0, colored = 0, aLow = 0, aHigh = 0, aEdges = 0;
    for (int y = 0; y < 256; y++) {
        const uint32_t *row = (const uint32_t *)((const BYTE *)lr.pBits + y * lr.Pitch);
        float prev = -1, prevA = -1;
        for (int x = 0; x < 256; x++) {
            uint32_t c = row[x]; int R = (c >> 16) & 255, G = (c >> 8) & 255, B = c & 255; float A = ((c >> 24) & 255) / 255.0f;
            float lum = (0.30f * R + 0.59f * G + 0.11f * B) / 255.0f;
            int mx = R > G ? (R > B ? R : B) : (G > B ? G : B), mn = R < G ? (R < B ? R : B) : (G < B ? G : B);
            n++;
            if (lum < 0.14f) dark++;
            if (lum > 0.35f) { bright++; if (mx > 0 && (mx - mn) * 100 > mx * 30) colored++; }
            if (prev >= 0 && fabsf(lum - prev) > 0.22f) edges++;
            if (A < 0.10f) aLow++;
            if (A > 0.40f) aHigh++;
            if (prevA >= 0 && fabsf(A - prevA) > 0.25f) aEdges++;
            prev = lum; prevA = A;
        }
    }
    if (save) {   /* colour picture, and its transparency as a grey picture */
        char nm[48]; snprintf(nm, sizeof nm, "FNVR_menucap_%d.bmp", k); SaveBMP(nm, (const BYTE *)lr.pBits, lr.Pitch, 256, 256);
        static uint32_t abuf[256 * 256];
        for (int y = 0; y < 256; y++) { const uint32_t *row = (const uint32_t *)((const BYTE *)lr.pBits + y * lr.Pitch);
            for (int x = 0; x < 256; x++) { uint32_t A = row[x] >> 24; abuf[y * 256 + x] = 0xFF000000u | A << 16 | A << 8 | A; } }
        snprintf(nm, sizeof nm, "FNVR_menucap_%d_alpha.bmp", k); SaveBMP(nm, (const BYTE *)abuf, 256 * 4, 256, 256);
    }
    IDirect3DSurface9_UnlockRect(g_pmSys);
    float fd = (float)dark / n, fb = (float)bright / n, fe = (float)edges / n, fc = bright ? (float)colored / bright : 0;
    float fad = (float)aLow / n, fab = (float)aHigh / n, fae = (float)aEdges / n;
    float sc = 0;
    sc += fd > 0.45f ? 40.0f : fd * 60.0f;
    sc += (fb > 0.003f && fb < 0.45f) ? 30.0f : -60.0f;
    sc += fminf(fe * 400.0f, 30.0f);
    sc += fc > 0.5f ? 10.0f : 0.0f;
    if (fe < 0.006f) sc -= 40.0f;   /* no crisp edges: a soft glow, not text */
    /* the same test on the transparency: a white picture whose text is in its alpha */
    float sa = 0;
    sa += fad > 0.45f ? 40.0f : fad * 60.0f;
    sa += (fab > 0.003f && fab < 0.45f) ? 30.0f : -60.0f;
    sa += fminf(fae * 400.0f, 30.0f) + 10.0f;
    if (fae < 0.006f) sa -= 40.0f;
    bool useA = sa > sc && fe < 0.002f;   /* (only when the colours themselves carry nothing) */
    g_pmAlpha[k] = useA;
    if (useA) sc = sa;
    {   UINT w = g_pmCopyW[k], h = g_pmCopyH[k];   /* the game makes its menu picture a power-of-two size */
        if (w && h && !(w & (w - 1)) && !(h & (h - 1))) sc += 15.0f; }
    Log("Pip-Boy menu capture: picture %d (%ux%u format %d, rank %d) colour: dark %.0f%% bright %.1f%% edges %.1f%% | alpha: clear %.0f%% solid %.1f%% edges %.1f%% -> score %.0f%s",
        k, g_pmCopyW[k], g_pmCopyH[k], (int)g_pmCopyF[k], g_pmCopyRank[k], fd * 100, fb * 100, fe * 100, fad * 100, fab * 100, fae * 100, sc, useA ? " (menu is in the alpha)" : "");
    return sc;
}
static double g_pmOpenAt = 0;
static IDirect3DTexture9 *PM_MenuTex(IDirect3DDevice9 *dev)
{
    static bool wasOpen = false;
    bool open = PM_Wanted();
    if (open && !wasOpen) g_pmOpenAt = QpcMs();
    wasOpen = open;
    if (!open) return NULL;
    double now = QpcMs();
    bool fresh = g_pmCopyN > 0 && now - g_pmCopyAt < 300.0;
    if (g_pmLock >= 0) {
        if (fresh && g_pmCopyTex[0]) return g_pmCopyTex[0];
        static double staleSince = 0;
        if (!staleSince) staleSince = now;
        if (now - staleSince > 2500.0) { staleSince = 0; g_pmLock = -1; Log("Pip-Boy menu capture: the menu picture stopped coming - looking again"); }
        return NULL;
    }
    return NULL;   /* (the picture now comes straight from the end of the game's Pip-Boy draw step - no more guessing) */
    /* not recognised yet: look at what was copied every few tenths of a second */
    static double nextLook = 0; static int looks = 0; static bool saved[PM_SLOTS];
    if (now - g_pmOpenAt > 4000.0) {
        static double nl = 0; if (now > nl) { nl = now + 10000.0;
            Log("Pip-Boy menu capture: menu not recognised yet (draw step called %u times, update %u, menu frame %u; %d pictures copied, %d looks)",
                g_pmCalls[0], g_pmCalls[1], g_pmCalls[2], g_pmCopyN, looks); }
    }
    if (!fresh || now < nextLook || looks >= 40) return NULL;   /* (gives up after 20 looks: each look briefly stalls the graphics card) */
    nextLook = now + (looks < 12 ? 300.0 : 2000.0); looks++;
    float best = 69.0f; int bi = -1;
    for (int k = 0; k < g_pmCopyN; k++) {
        bool save = !saved[k]; saved[k] = true;
        float sc = PM_Score(dev, k, save);
        if (sc > best) { best = sc; bi = k; }
    }
    if (bi >= 0) {
        g_pmLock = bi; g_pmLockW = g_pmCopyW[bi]; g_pmLockH = g_pmCopyH[bi]; g_pmLockRank = g_pmCopyRank[bi]; g_pmLockAlpha = g_pmAlpha[bi]; g_pmLockSurf = g_pmCopySrc[bi];
        if (bi != 0) {   /* the chosen one lives in slot 0 from now on */
            IDirect3DTexture9 *t = g_pmCopyTex[0]; IDirect3DSurface9 *sf = g_pmCopySurf[0]; UINT w = g_pmCopyW[0], hh = g_pmCopyH[0]; D3DFORMAT f = g_pmCopyF[0]; int rk = g_pmCopyRank[0];
            g_pmCopyTex[0] = g_pmCopyTex[bi]; g_pmCopySurf[0] = g_pmCopySurf[bi]; g_pmCopyW[0] = g_pmCopyW[bi]; g_pmCopyH[0] = g_pmCopyH[bi]; g_pmCopyF[0] = g_pmCopyF[bi]; g_pmCopyRank[0] = g_pmCopyRank[bi];
            g_pmCopyTex[bi] = t; g_pmCopySurf[bi] = sf; g_pmCopyW[bi] = w; g_pmCopyH[bi] = hh; g_pmCopyF[bi] = f; g_pmCopyRank[bi] = rk;
        }
        Log("Pip-Boy menu capture: FOUND the menu picture (%ux%u, score %.0f%s) - FNVR now draws the menu on your Pip-Boy", g_pmLockW, g_pmLockH, best, g_pmLockAlpha ? ", text in its alpha" : "");
        return g_pmCopyTex[0];
    }
    return NULL;
}

/* The game's menu picture (a D3D texture), from its screen shape. Only a real D3D texture is accepted. */
static bool PipScr_IsD3DTex(void *p)
{
    if (!Rd(p, 8)) return false;
    void *vt = *(void **)p;
    if (vt && (vt == g_texVt[0] || vt == g_texVt[1])) return true;
    static void *okVt[4]; static int nOk = 0;
    for (int i = 0; i < nOk; i++) if (okVt[i] == vt) return true;
    HMODULE mod = NULL; char path[MAX_PATH];
    if (!Rd(vt, 16) || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)vt, &mod) || !mod) return false;
    if (!GetModuleFileNameA(mod, path, sizeof path)) return false;
    const char *base = strrchr(path, '\\'); base = base ? base + 1 : path;
    if (_stricmp(base, "d3d9.dll") != 0) return false;
    if (nOk < 4) okVt[nOk++] = vt;
    return true;
}
/* A render-target D3D texture somewhere in a texture object's renderer data (the field moves between
   texture kinds, so look through it rather than trust one offset). Returns the offset used, or -1. */
static int PipScr_RTIn(char *tex, IDirect3DTexture9 **out, int prefer)
{
    if (!IsNiObj(tex)) return -1;
    char *td = *(char **)(tex + 0x24);
    if (!Rd(td, 0x100) || *(char **)(td + 0x04) != tex) return -1;   /* (texture data always points back at its texture) */
    for (int pass = 0; pass < 2; pass++)
        for (int o = 0x08; o < 0x100; o += 4) {
            if ((pass == 0) != (o == prefer)) continue;
            void *bt = *(void **)(td + o);
            if (!PipScr_IsD3DTex(bt)) continue;
            if (IDirect3DBaseTexture9_GetType((IDirect3DBaseTexture9 *)bt) != D3DRTYPE_TEXTURE) continue;
            D3DSURFACE_DESC d;
            if (FAILED(IDirect3DTexture9_GetLevelDesc((IDirect3DTexture9 *)bt, 0, &d)) || d.Width < 128 || d.Height < 128) continue;
            if (!(d.Usage & D3DUSAGE_RENDERTARGET)) continue;
            *out = (IDirect3DTexture9 *)bt; return o;
        }
    return -1;
}
/* The game's menu picture: the render-target texture its Pip-Boy screen shows. Found by following the
   menu manager's and the screen shape's pointers a few steps deep; only a real drawn-into texture counts. */
static int g_rtVisits;
static char *g_rtSeen[512]; static int g_rtSeenN;
static char *PipScr_RTWalk(char *p, int depth, IDirect3DTexture9 **out, int *offOut, char *path, size_t cap)
{
    if (!Rd(p, 0x40) || ++g_rtVisits > 4000) return NULL;
    for (int i = 0; i < g_rtSeenN; i++) if (g_rtSeen[i] == p) return NULL;
    if (g_rtSeenN < 512) g_rtSeen[g_rtSeenN++] = p;
    if (IsNiObj(p)) { int off = PipScr_RTIn(p, out, 0x64); if (off >= 0) { *offOut = off; return p; } }
    if (depth >= 3) return NULL;
    int span = Rd(p, 0x100) ? 0x100 : 0x40;
    for (int o = 0; o < span; o += 4) {
        char *q = *(char **)(p + o);
        if (!q || q == p || (uintptr_t)q < 0x10000) continue;
        char *r = PipScr_RTWalk(q, depth + 1, out, offOut, path, cap);
        if (r) { size_t l = strlen(path); if (l + 8 < cap) snprintf(path + l, cap - l, "<%X", o); return r; }
    }
    return NULL;
}
static IDirect3DTexture9 *PipScr_MenuTex(int *where)
{
    char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
    if (!Rd(pm, 0xE0)) return NULL;
    static char *lastTex = NULL; static int lastOff = 0x64; static double nextTry = 0;
    IDirect3DTexture9 *t = NULL;
    if (lastTex && PipScr_RTIn(lastTex, &t, lastOff) == lastOff) { *where = 3; return t; }   /* (same as last frame) */
    if (QpcMs() < nextTry) return NULL;
    nextTry = QpcMs() + 2000.0;
    char *menu = *(char **)(pm + 4);
    char *roots[3] = { IsNiObj(menu) ? *(char **)(menu + 0xA8) : NULL, IsNiObj(menu) ? *(char **)(menu + 0xB0) : NULL, pm };   /* its shader, its textures, the manager */
    for (int k = 0; k < 3; k++) {
        if (!Rd(roots[k], 0x40)) continue;
        g_rtVisits = 0; g_rtSeenN = 0; char path[96] = ""; int off = -1;
        char *tex = PipScr_RTWalk(roots[k], 0, &t, &off, path, sizeof path);
        if (tex) {
            D3DSURFACE_DESC d; IDirect3DTexture9_GetLevelDesc(t, 0, &d);
            Log("Pip-Boy screen: menu picture found from the %s (%s, data +%X) - %ux%u format %d", k == 2 ? "menu manager" : k ? "screen textures" : "screen shader", path, off, d.Width, d.Height, (int)d.Format);
            lastTex = tex; lastOff = off; *where = k == 2 ? 3 : k ? 2 : 1; return t;
        }
    }
    static int nl = 0; if (nl++ < 3) Log("Pip-Boy screen: no drawn menu picture found near the menu (looked at %d places)", g_rtVisits);
    return NULL;
}
static void LineQuadC(VtxC *v, float x0, float y0, float x1, float y1, float th, DWORD c)
{
    float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy); if (L < 1e-3f) L = 1e-3f;
    float nx = -dy / L * th, ny = dx / L * th;
    VtxC q[6] = { { x0 - nx, y0 - ny, 0, 1, c }, { x1 - nx, y1 - ny, 0, 1, c }, { x1 + nx, y1 + ny, 0, 1, c },
                  { x0 - nx, y0 - ny, 0, 1, c }, { x1 + nx, y1 + ny, 0, 1, c }, { x0 + nx, y0 + ny, 0, 1, c } };
    memcpy(v, q, sizeof q);
}
/* Live world behind the Pip-Boy: the game draws its menu onto the Pip-Boy only in its "menu" frame (which
   doesn't draw the world), and the live-world frame drawn after it covers that up. So right after the menu
   frame, the part of the picture where the Pip-Boy's glass is gets copied, and at Present it is put back on
   the same spot over the live frame - the menu exactly as the game drew it, with the world still moving. */
static IDirect3DDevice9 *g_hookedDevice;
static IDirect3DTexture9 *g_pgTex; static IDirect3DSurface9 *g_pgSurf; static D3DFORMAT g_pgFmt;
static bool g_pgOk; static float g_pgBox[4]; static double g_pgAt; static float g_pgSrcX[4], g_pgSrcY[4]; static char g_pgQuad[sizeof g_pipQuadLast];
static void PipGrab_Release(void)
{
    if (g_pgSurf) IDirect3DSurface9_Release(g_pgSurf); g_pgSurf = NULL;
    if (g_pgTex) IDirect3DTexture9_Release(g_pgTex); g_pgTex = NULL;
    g_pgOk = false;
}
static float Pip_Score(IDirect3DDevice9 *dev, IDirect3DSurface9 *s0, int idx);
static void PipGrab_Take(void)
{
    g_pgOk = false;
    IDirect3DDevice9 *dev = g_hookedDevice;
    static int why[4];
    if (!dev || !g_bbW || !g_bbH) { if (why[0]++ < 3) Log("Pip-Boy screen copy: no device yet"); return; }
    /* where the game just drew its menu in this frozen-mode picture: the menu shape's four picture corners,
       seen from the camera as it is now */
    float sx[4], sy[4]; bool have = false;
    {
        char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
        char *menu = Rd(pm, 8) ? *(char **)(pm + 4) : NULL;
        char *sg = *(char **)SCENEGRAPH_PTR_ADDR, *cam = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
        float O[3], U[3], V[3], uvr[4];
        if (IsNiObj(menu) && cam && Geo_UVFrame(menu, O, U, V, uvr)) {
            const float *R = (const float *)(menu + NIAV_WORLD_ROTATE), *T = (const float *)(menu + NIAV_WORLD_TRANSLATE);
            float S = *(const float *)(menu + NIAV_WORLD_SCALE); if (!(S > 1e-6f)) S = 1;
            const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
            float F[3] = { Cw[0], Cw[3], Cw[6] }, Up[3] = { Cw[1], Cw[4], Cw[7] }, Rt[3] = { Cw[2], Cw[5], Cw[8] };
            const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
            static const int cu[4] = { 0, 2, 2, 0 }, cv[4] = { 1, 1, 3, 3 };   /* top-left, top-right, bottom-right, bottom-left */
            have = fr[1] > fr[0] && fr[2] > fr[3];
            for (int k = 0; k < 4 && have; k++) {
                float L[3], W[3], q[3];
                for (int i = 0; i < 3; i++) L[i] = O[i] + U[i] * uvr[cu[k]] + V[i] * uvr[cv[k]];
                for (int i = 0; i < 3; i++) W[i] = T[i] + (R[i * 3] * L[0] + R[i * 3 + 1] * L[1] + R[i * 3 + 2] * L[2]) * S;
                V3Sub(q, W, C);
                float z = V3Dot(q, F); if (z < 0.5f) { have = false; break; }
                sx[k] = (V3Dot(q, Rt) / z - fr[0]) / (fr[1] - fr[0]) * g_bbW;
                sy[k] = (fr[2] - V3Dot(q, Up) / z) / (fr[2] - fr[3]) * g_bbH;
            }
        }
    }
    if (!have) {
        if (!g_pipQuadLast.ok || QpcMs() - g_pipQuadLast.at > 300.0) { if (why[1]++ < 3) Log("Pip-Boy screen copy: the screen spot isn't known this frame (%.0f ms old)", QpcMs() - g_pipQuadLast.at); return; }
        for (int k = 0; k < 4; k++) { if (!(g_pipQuadLast.rhw[k] > 0)) return; sx[k] = g_pipQuadLast.x[k]; sy[k] = g_pipQuadLast.y[k]; }
    }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int k = 0; k < 4; k++) { x0 = fminf(x0, sx[k]); x1 = fmaxf(x1, sx[k]); y0 = fminf(y0, sy[k]); y1 = fmaxf(y1, sy[k]); }
    x0 = floorf(fmaxf(x0 - 2, 0)); y0 = floorf(fmaxf(y0 - 2, 0)); x1 = ceilf(fminf(x1 + 2, (float)g_bbW)); y1 = ceilf(fminf(y1 + 2, (float)g_bbH));
    if (x1 - x0 < 8 || y1 - y0 < 8) return;
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    D3DSURFACE_DESC bd; IDirect3DSurface9_GetDesc(bb, &bd);
    if (!g_pgTex || g_pgFmt != bd.Format) {
        PipGrab_Release(); g_pgFmt = bd.Format;
        if (FAILED(IDirect3DDevice9_CreateTexture(dev, 1024, 1024, 1, D3DUSAGE_RENDERTARGET, bd.Format, D3DPOOL_DEFAULT, &g_pgTex, NULL)) ||
            FAILED(IDirect3DTexture9_GetSurfaceLevel(g_pgTex, 0, &g_pgSurf))) {
            static int nl = 0; if (nl++ < 2) Log("Pip-Boy screen: couldn't make the menu copy picture (format %d)", (int)bd.Format);
            PipGrab_Release(); IDirect3DSurface9_Release(bb); return;
        }
        Log("Pip-Boy screen: copying the menu from the game's menu frame (live world behind it)");
    }
    RECT r = { (LONG)x0, (LONG)y0, (LONG)x1, (LONG)y1 };
    HRESULT hr = IDirect3DDevice9_StretchRect(dev, bb, &r, g_pgSurf, NULL, D3DTEXF_LINEAR);
    if (SUCCEEDED(hr)) {
        g_pgBox[0] = x0; g_pgBox[1] = y0; g_pgBox[2] = x1; g_pgBox[3] = y1; g_pgOk = true; g_pgAt = QpcMs();
        memcpy(g_pgSrcX, sx, sizeof sx); memcpy(g_pgSrcY, sy, sizeof sy);
        memcpy(g_pgQuad, &g_pipQuadLast, sizeof g_pgQuad);
        static int n = 0; if (++n == 60) { Log("Pip-Boy screen copy: working - menu at (%.0f,%.0f)-(%.0f,%.0f) px in the frozen picture (%s), snapshot saved as picture 99",
            x0, y0, x1, y1, have ? "from the menu's own corners" : "from the screen spot"); Pip_Score(dev, g_pgSurf, 99); }
    } else { static int nl = 0; if (nl++ < 3) Log("Pip-Boy screen: copying the menu failed (%08lX)", (unsigned long)hr); }
    IDirect3DSurface9_Release(bb);
}
/* Present: lay the menu picture on the screen spot worked out for this frame - added on as light, so its
   dark background shows nothing and only the glowing menu appears on the 2000's glass. */
static void PipScr_Draw(IDirect3DDevice9 *dev)
{
    /* The screen spot is worked out again right here when this frame's drawing didn't do it (the game
       doesn't always run the parts of the frame where FNVR normally does it) - so the menu is drawn on
       every single frame instead of only now and then. */
    static unsigned stFrames = 0, stDrawn = 0, stRedo = 0, stNoSpot = 0, stNoTex = 0; static double stAt = 0;
    bool openNow = g_pipOpen && g_pipModeRaw == 3 && g_pipScrOn && g_wristPip;
    if (openNow) {
        stFrames++;
        if (!(g_pipQuad.ok && QpcMs() - g_pipQuad.at < 30.0)) {
            char *pl = (char *)GetPlayer(); char *sg = *(char **)SCENEGRAPH_PTR_ADDR; char *cm = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
            if (pl && cm) { g_pipPrepQuiet = true; PipScr_Prepare(pl, cm); g_pipPrepQuiet = false; stRedo++; }
        }
        if (!g_pipQuad.ok) stNoSpot++;
        if (QpcMs() - stAt > 3000.0) {
            if (stAt > 0 && stFrames > 10) Log("Pip-Boy screen: last 3 s - %u frames, menu drawn on %u (spot redone at the end of the frame %u, no spot %u, no picture %u)", stFrames, stDrawn, stRedo, stNoSpot, stNoTex);
            stAt = QpcMs(); stFrames = stDrawn = stRedo = stNoSpot = stNoTex = 0;
        }
    }
    bool want = g_pipQuad.ok && QpcMs() - g_pipQuad.at < 250.0;
    if (g_pgOk && g_pgTex && QpcMs() - g_pgAt < 200.0) { if (!want) memcpy(&g_pipQuad, g_pgQuad, sizeof g_pgQuad); want = true; }   /* (the copy goes onto this frame's screen spot) */
    g_pipQuad.ok = false;
    if (!want || !g_bbW) { g_pipScrDrawing = false; g_pgOk = false; return; }
    if (!g_texVt[0]) {
        IDirect3DTexture9 *a = NULL, *b2 = NULL;
        if (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &a, NULL))) g_texVt[0] = *(void **)a;
        if (SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &b2, NULL))) g_texVt[1] = *(void **)b2;
        if (!g_texVt[1]) g_texVt[1] = g_texVt[0];
        if (a) IDirect3DTexture9_Release(a); if (b2) IDirect3DTexture9_Release(b2);
    }
    int where = 5;
    IDirect3DTexture9 *tex = PM_MenuTex(dev);   /* (the old search through the game's pointers never found it - retired) */
    bool grab = false;
    if (!tex && g_pgOk && g_pgTex && QpcMs() - g_pgAt < 200.0) { tex = g_pgTex; grab = true; where = 4; }
    g_pgOk = false;
    if (!tex) stNoTex++; else stDrawn++;
    if (!tex) {
        if (g_pipScrDrawing || QpcMs() - g_pipScrNoTexAt > 10000.0) Log("Pip-Boy screen: the game's menu picture can't be found - showing the game's own copy instead");
        g_pipScrNoTexAt = QpcMs(); g_pipScrDrawing = false; return;
    }
    {   static IDirect3DTexture9 *lastTex = NULL;
        if (tex != lastTex) { lastTex = tex; D3DSURFACE_DESC d; IDirect3DTexture9_GetLevelDesc(tex, 0, &d);
            Log("Pip-Boy screen: menu picture %ux%u (format %d, found via %s)", d.Width, d.Height, (int)d.Format, where == 5 ? "the Pip-Boy's own draw step" : where == 4 ? "the menu frame copy" : where == 1 ? "screen shader" : where == 2 ? "screen texture" : "menu manager"); } }
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { g_pipScrDrawing = false; return; }

    static const D3DRENDERSTATETYPE rsList[] = { D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
        D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_CULLMODE, D3DRS_LIGHTING, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_COLORWRITEENABLE,
        D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_BLENDOP, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_TEXTUREFACTOR };
    enum { NRS = sizeof rsList / sizeof rsList[0] };
    DWORD rsSave[NRS];
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_GetRenderState(dev, rsList[i], &rsSave[i]);
    static const D3DSAMPLERSTATETYPE ssList[] = { D3DSAMP_MINFILTER, D3DSAMP_MAGFILTER, D3DSAMP_MIPFILTER, D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV, D3DSAMP_SRGBTEXTURE };
    enum { NSS = sizeof ssList / sizeof ssList[0] };
    DWORD ssSave[NSS];
    for (int i = 0; i < NSS; i++) IDirect3DDevice9_GetSamplerState(dev, 0, ssList[i], &ssSave[i]);
    DWORD ts0op, ts0a1, ts0a2, ts0aop, ts1op, ts0aa1;
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, &ts0aa1);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLOROP, &ts0op);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG1, &ts0a1);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG2, &ts0a2);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAOP, &ts0aop);
    IDirect3DDevice9_GetTextureStageState(dev, 1, D3DTSS_COLOROP, &ts1op);
    IDirect3DSurface9 *rtSave = NULL; IDirect3DDevice9_GetRenderTarget(dev, 0, &rtSave);
    D3DVIEWPORT9 vpSave; IDirect3DDevice9_GetViewport(dev, &vpSave);
    IDirect3DVertexShader9 *vsSave = NULL; IDirect3DDevice9_GetVertexShader(dev, &vsSave);
    IDirect3DPixelShader9 *psSave = NULL; IDirect3DDevice9_GetPixelShader(dev, &psSave);
    IDirect3DVertexDeclaration9 *declSave = NULL; IDirect3DDevice9_GetVertexDeclaration(dev, &declSave);
    DWORD fvfSave = 0; IDirect3DDevice9_GetFVF(dev, &fvfSave);
    IDirect3DBaseTexture9 *texSave = NULL; IDirect3DDevice9_GetTexture(dev, 0, &texSave);
    IDirect3DVertexBuffer9 *vbSave = NULL; UINT vbOff = 0, vbStride = 0; IDirect3DDevice9_GetStreamSource(dev, 0, &vbSave, &vbOff, &vbStride);

    IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
    D3DVIEWPORT9 vp = { 0, 0, g_bbW, g_bbH, 0.0f, 1.0f };
    IDirect3DDevice9_SetViewport(dev, &vp);
    IDirect3DDevice9_SetVertexShader(dev, NULL);
    IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_ONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0x7);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_SRGBTEXTURE, FALSE);
    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        float u0 = g_pipScrUV[0], v0 = g_pipScrUV[1], u1 = g_pipScrUV[2], v1 = g_pipScrUV[3];
        float us[4] = { u0, u1, u1, u0 }, vs[4] = { v0, v0, v1, v1 };
        VtxT q[4];
        for (int k = 0; k < 4; k++) q[k] = (VtxT){ g_pipQuad.x[k], g_pipQuad.y[k], 0.0f, g_pipQuad.rhw[k], us[k], vs[k] };
        bool alphaMenu = !grab && where == 5 && g_pmLockAlpha;
        if (alphaMenu) {   /* white picture, text in its alpha: colour it with the Pip-Boy's own colour */
            float col[3] = { 1.0f, 0.72f, 0.26f };
            char *im2 = *(char **)0x011D8A80; char *pm2 = Rd(im2, 0x178) ? *(char **)(im2 + 0x174) : NULL;
            if (Rd(pm2, 0xB8)) { const float *sc = (const float *)(pm2 + 0xA8);
                if (sc[0] >= 0 && sc[0] <= 1.01f && sc[1] >= 0 && sc[1] <= 1.01f && sc[2] >= 0 && sc[2] <= 1.01f && sc[0] + sc[1] + sc[2] > 0.15f) { col[0] = sc[0]; col[1] = sc[1]; col[2] = sc[2]; } }
            float b2 = fminf(fmaxf(g_pipScrBright, 0.0f), 1.0f);
            DWORD cr = (DWORD)(col[0] * b2 * 255), cg = (DWORD)(col[1] * b2 * 255), cb = (DWORD)(col[2] * b2 * 255);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
            IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)tex);
            if (g_pipMenuBg) {   /* the screen's dark background first */
                IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
                IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, 0xFF000000 | (cr / 10) << 16 | (cg / 10) << 8 | (cb / 10));
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
                IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, 2, q, sizeof(VtxT));
                IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            }
            IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, g_pipMenuBg ? D3DBLEND_INVSRCALPHA : D3DBLEND_ONE);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, 0xFF000000 | cr << 16 | cg << 8 | cb);
            IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, 2, q, sizeof(VtxT));
        }
        if (!alphaMenu) {
        if (!grab && g_pipMenuBg) IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);   /* menu background on: the menu as a solid screen */
        if (grab) {   /* the copied glass: the same pixels, put back exactly where they came from */
            float bw = g_pgBox[2] - g_pgBox[0], bh = g_pgBox[3] - g_pgBox[1];
            for (int k = 0; k < 4; k++) { q[k].rhw = 1.0f; q[k].u = (g_pgSrcX[k] - g_pgBox[0]) / bw; q[k].v = (g_pgSrcY[k] - g_pgBox[1]) / bh; }
            IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
        }
        IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)tex);
        float br = grab ? 1.0f : g_pipScrBright; DWORD op = D3DTOP_MODULATE;
        if (br > 1.0f) { op = br <= 2.0f ? D3DTOP_MODULATE2X : D3DTOP_MODULATE4X; br /= br <= 2.0f ? 2.0f : 4.0f; }
        DWORD k8 = (DWORD)(fminf(fmaxf(br, 0.0f), 1.0f) * 255.0f);
        IDirect3DDevice9_SetRenderState(dev, D3DRS_TEXTUREFACTOR, 0xFF000000 | k8 << 16 | k8 << 8 | k8);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, op);
        IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_TEX1);
        IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, 2, q, sizeof(VtxT));
        }
        if (g_adjOn) {   /* adjusting: a frame around it (green = move & size, blue = tilt & stretch) */
            IDirect3DDevice9_SetTexture(dev, 0, NULL);
            IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            DWORD col = fmod(QpcMs(), 900.0) < 600.0 ? (g_adjMode ? 0xFF40C8FF : 0xFF50FF50) : 0xFF206020;
            float th = fmaxf(g_bbH / 540.0f, 1.5f);
            VtxC e[24];
            for (int k = 0; k < 4; k++) LineQuadC(e + k * 6, g_pipQuad.x[k], g_pipQuad.y[k], g_pipQuad.x[(k + 1) % 4], g_pipQuad.y[(k + 1) % 4], th, col);
            IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, 8, e, sizeof(VtxC));
        }
        IDirect3DDevice9_EndScene(dev);
    }
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_SetRenderState(dev, rsList[i], rsSave[i]);
    for (int i = 0; i < NSS; i++) IDirect3DDevice9_SetSamplerState(dev, 0, ssList[i], ssSave[i]);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, ts0op);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, ts0a1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, ts0a2);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, ts0aop);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, ts0aa1);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, ts1op);
    if (rtSave) { IDirect3DDevice9_SetRenderTarget(dev, 0, rtSave); IDirect3DSurface9_Release(rtSave); }
    IDirect3DDevice9_SetViewport(dev, &vpSave);
    IDirect3DDevice9_SetVertexShader(dev, vsSave); if (vsSave) IDirect3DVertexShader9_Release(vsSave);
    IDirect3DDevice9_SetPixelShader(dev, psSave); if (psSave) IDirect3DPixelShader9_Release(psSave);
    if (declSave) { IDirect3DDevice9_SetVertexDeclaration(dev, declSave); IDirect3DVertexDeclaration9_Release(declSave); }
    else if (fvfSave) IDirect3DDevice9_SetFVF(dev, fvfSave);
    IDirect3DDevice9_SetTexture(dev, 0, texSave); if (texSave) IDirect3DBaseTexture9_Release(texSave);
    IDirect3DDevice9_SetStreamSource(dev, 0, vbSave, vbOff, vbStride); if (vbSave) IDirect3DVertexBuffer9_Release(vbSave);
    IDirect3DSurface9_Release(bb);
    if (grab) { g_pipScrDrawing = false; return; }   /* (the game must keep drawing its own copy for the next frame's copy) */
    if (!g_pipScrDrawing) Log("Pip-Boy screen: menu drawn on the Pip-Boy's glass (%.0f,%.0f)-(%.0f,%.0f) px", g_pipQuad.x[0], g_pipQuad.y[0], g_pipQuad.x[2], g_pipQuad.y[2]);
    g_pipScrDrawing = true;
}

/* When FNVR can't draw the menu itself: hold the game's own screen shape on the same spot, with its dark
   background blended away (the way 1.3 did it). */
static void PipScr_PinGameMenu(char *menu, bool keepLook)
{
    if (!g_pipTw || !g_pipScrHave || !IsNiObj(menu)) return;
    char *par = *(char **)(menu + NIAV_PARENT); if (!IsNiObj(par)) return;
    static char *frFor = NULL; static float O[3], U[3], V[3], uvr[4]; static bool frOk = false;
    if (frFor != menu) { frFor = menu; frOk = Geo_UVFrame(menu, O, U, V, uvr); }
    if (!frOk) return;
    char *tw = g_pipTw;
    const float *TR = (const float *)(tw + NIAV_WORLD_ROTATE), *TT = (const float *)(tw + NIAV_WORLD_TRANSLATE);
    float TS = *(const float *)(tw + NIAV_WORLD_SCALE); if (!(TS > 1e-4f)) TS = 1;
    float cw[3], aw[3][3];
    for (int i = 0; i < 3; i++) {
        cw[i] = TT[i] + (TR[i * 3] * g_pipScrC[0] + TR[i * 3 + 1] * g_pipScrC[1] + TR[i * 3 + 2] * g_pipScrC[2]) * TS;
        for (int k = 0; k < 3; k++) aw[k][i] = TR[i * 3] * g_pipScrR[0 * 3 + k] + TR[i * 3 + 1] * g_pipScrR[1 * 3 + k] + TR[i * 3 + 2] * g_pipScrR[2 * 3 + k];
    }
    /* the shape's own right / up / out (in its local space) */
    float lu = V3Len(U), lv = V3Len(V); if (lu < 1e-5f || lv < 1e-5f) return;
    float bx[3], by[3], bz[3];
    for (int i = 0; i < 3; i++) { bx[i] = U[i] / lu; by[i] = -V[i] / lv; }
    float pd = V3Dot(by, bx); for (int i = 0; i < 3; i++) by[i] -= bx[i] * pd;
    float lb = V3Len(by); if (lb < 1e-5f) return; for (int i = 0; i < 3; i++) by[i] /= lb;
    V3Cross(bz, bx, by);
    float Rw[3][3];   /* world rotation that maps the shape's right/up/out onto the screen's */
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) Rw[i][j] = aw[0][i] * bx[j] + aw[1][i] * by[j] + aw[2][i] * bz[j];
    float s = g_pipScrW * TS / (lu * (uvr[2] - uvr[0]));
    float uc = (uvr[0] + uvr[2]) * 0.5f, vc = (uvr[1] + uvr[3]) * 0.5f, cl[3], Tw[3];
    for (int i = 0; i < 3; i++) cl[i] = O[i] + U[i] * uc + V[i] * vc;
    for (int i = 0; i < 3; i++) Tw[i] = cw[i] - (Rw[i][0] * cl[0] + Rw[i][1] * cl[1] + Rw[i][2] * cl[2]) * s;
    const float *Pr = (const float *)(par + NIAV_WORLD_ROTATE), *Pt = (const float *)(par + NIAV_WORLD_TRANSLATE);
    float ps = *(const float *)(par + NIAV_WORLD_SCALE); if (!(ps > 1e-4f)) ps = 1;
    float *Lr = (float *)(menu + NIAV_LOCAL_ROTATE), *Lt = (float *)(menu + NIAV_LOCAL_TRANSLATE);
    float d[3]; V3Sub(d, Tw, Pt);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) Lr[i * 3 + j] = Pr[0 * 3 + i] * Rw[0][j] + Pr[1 * 3 + i] * Rw[1][j] + Pr[2 * 3 + i] * Rw[2][j];
        Lt[i] = (Pr[0 * 3 + i] * d[0] + Pr[1 * 3 + i] * d[1] + Pr[2 * 3 + i] * d[2]) / ps;
    }
    *(float *)(menu + NIAV_LOCAL_SCALE) = s / ps;
    char *ap = *(char **)(menu + 0x9C);
    if (IsNiObj(ap)) {   /* added on as light (no dark box) - except over the live world, where that doesn't show */
        static char *apFor = NULL; static uint16_t apOrig = 0;
        uint16_t *fl = (uint16_t *)(ap + 0x18);
        if (apFor != ap) { apFor = ap; apOrig = *fl; }
        *fl = keepLook ? apOrig : (uint16_t)((apOrig & ~0x1FE) | 1);
    }
    {   /* drawn on top: no depth test, so the Pip-Boy 2000's glass can't hide it (what made it show in 1.3.5) */
        char *sp = *(char **)(menu + 0xA8);
        if (IsNiObj(sp)) { uint32_t *f1 = (uint32_t *)(sp + 0x20);
            if (*f1 & 0x80000000u) { *f1 &= ~0x80000000u; static int lz = 0; if (lz++ < 3) Log("Pip-Boy menu: drawn on top of the Pip-Boy glass (as in 1.3.5)"); } }
    }
    NodeUpdate(menu);
}

/* Each frame (after your arm is posed, and again right before drawing): the game's own copy of the menu
   and the old 3000's dark glass ("the black box") are kept out of sight while the Pip-Boy is open. */
static void PipScreen_Offset(char *firstPerson)
{
    if (!g_ppApplyOnly) g_lastFP = firstPerson;
    static char *forNode = NULL, *box = NULL, *shrunk = NULL; static float shrunkScale = 1.0f; static bool boxHidden = false;
    if (forNode != firstPerson) {
        forNode = firstPerson; shrunk = NULL; boxHidden = false;
        box = firstPerson ? PP_FindExact(firstPerson, "ScreenLit:8", 0) : NULL;
        if (!box && firstPerson) box = FindNameCI(firstPerson, "screenlit", 0);
    }
    char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
    char *menu = Rd(pm, 8) ? *(char **)(pm + 4) : NULL; if (!IsNiObj(menu)) menu = NULL;
    bool open = g_pipOpen && g_pipModeRaw == 3 && g_wristPip;
    bool live = g_pipWorldDraw && g_pipRealTime;   /* the live world drawn behind: the menu only shows with its own dark screen */
    bool hideGame = open && g_pipScrOn && g_pipScrDrawing;
    {   /* the game's own size for its menu shape, taken before FNVR ever changes it */
        static char *origFor = NULL;
        if (menu && menu != origFor) { origFor = menu; shrunkScale = *(float *)(menu + NIAV_LOCAL_SCALE); if (!(shrunkScale > 1e-3f && shrunkScale < 100.0f)) shrunkScale = 1.0f; }
    }
    if (shrunk && (shrunk != menu || !hideGame)) {
        if (IsNiObj(shrunk) && shrunk == menu) { *(float *)(shrunk + NIAV_LOCAL_SCALE) = shrunkScale; NodeUpdate(shrunk); }
        shrunk = NULL;
    }
    if (hideGame && menu) {
        if (shrunk != menu) shrunk = menu;
        *(float *)(menu + NIAV_LOCAL_SCALE) = shrunkScale * 0.01f; NodeUpdate(menu);   /* (tiny, not gone: the game keeps drawing the menu picture) */
    } else if (open && menu && g_pipScrOn) PipScr_PinGameMenu(menu, g_pipMenuBg);
    else if (menu && !g_pipScrOn) {   /* not placed by FNVR: still honour the background switch */
        char *ap = *(char **)(menu + 0x9C);
        static char *apFor = NULL; static uint16_t apOrig = 0;
        if (IsNiObj(ap)) { uint16_t *fl = (uint16_t *)(ap + 0x18); if (apFor != ap) { apFor = ap; apOrig = *fl; }
            *fl = (open && !g_pipMenuBg) ? (uint16_t)((apOrig & ~0x1FE) | 1) : apOrig; }
    }
    (void)live;
    {   static int lb = -1, lm = -1; if (open && (lb != g_pipBoxShow || lm != g_pipMenuBg)) { lb = g_pipBoxShow; lm = g_pipMenuBg;
            Log("Pip-Boy screen: screen box %s, menu background %s, menu placed by FNVR %s, live world %s", g_pipBoxShow ? "shown" : "hidden",
                g_pipMenuBg ? "shown" : "hidden (glow only)", g_pipScrOn ? "on" : "off", g_pipWorldDraw ? "on" : "off"); } }
    if (false && box && IsNiObj(box) && open && g_pipBoxShow && g_pipTw && g_pipScrHave) {   /* (retired: the box stays where the Pip-Boy has it) */
        /* the Pip-Boy's dark screen box, moved to sit right behind the menu */
        static char *bxFor = NULL; static float bxSave[3]; static bool bxSaved = false;
        if (bxFor != box) { bxFor = box; bxSaved = false; }
        char *par = *(char **)(box + NIAV_PARENT);
        const float *bd = *(const float **)(box + 0x20);
        if (IsNiObj(par) && Rd((void *)bd, 16)) {
            const float *TR = (const float *)(g_pipTw + NIAV_WORLD_ROTATE), *TT = (const float *)(g_pipTw + NIAV_WORLD_TRANSLATE);
            float TS = *(const float *)(g_pipTw + NIAV_WORLD_SCALE); if (!(TS > 1e-4f)) TS = 1;
            float tgt[3];
            for (int i = 0; i < 3; i++) {
                float out = TR[i * 3] * g_pipScrR[2] + TR[i * 3 + 1] * g_pipScrR[5] + TR[i * 3 + 2] * g_pipScrR[8];
                tgt[i] = TT[i] + (TR[i * 3] * g_pipScrC[0] + TR[i * 3 + 1] * g_pipScrC[1] + TR[i * 3 + 2] * g_pipScrC[2]) * TS - out * 0.15f * TS;
            }
            float d[3]; V3Sub(d, tgt, bd);
            if (V3Len(d) > 0.02f && V3Len(d) < 40.0f) {
                const float *Pr = (const float *)(par + NIAV_WORLD_ROTATE); float ps = *(const float *)(par + NIAV_WORLD_SCALE); if (!(ps > 1e-4f)) ps = 1;
                float *Lt = (float *)(box + NIAV_LOCAL_TRANSLATE);
                if (!bxSaved) { memcpy(bxSave, Lt, 12); bxSaved = true; }
                for (int i = 0; i < 3; i++) Lt[i] += (Pr[0 * 3 + i] * d[0] + Pr[1 * 3 + i] * d[1] + Pr[2 * 3 + i] * d[2]) / ps;
                NodeUpdate(box);
                static int bl = 0; if (bl++ < 2) Log("Pip-Boy screen: dark screen box moved %.1f units to sit behind the menu", V3Len(d));
            }
        }
    }
    if (box && IsNiObj(box)) {
        uint32_t *f = (uint32_t *)(box + 0x30);
        if (open && !g_pipBoxShow) { *f |= 1u; boxHidden = true; }
        else if (boxHidden) { *f &= ~1u; boxHidden = false; }
    }
}
/* Work out where the Pip-Boy's dark screen box is in the headset's space, for the menu picture. */
static void PipBox_Place(char *firstPerson, char *cam)
{
    g_pipBoxOk = false;
    if (!g_pipOnBoxIni || !firstPerson || !cam) return;
    static char *forNode = NULL, *box = NULL, *twb = NULL;
    if (forNode != firstPerson) {
        forNode = firstPerson;
        char *scr = FindNameCI(firstPerson, "pipboyscreen", 0);
        box = scr ? FindNameCI(scr, "pipboyscreen:", 0) : NULL; if (!box) box = scr;
        twb = FindBone(firstPerson, "Bip01 L ForeTwist", 0);
        Log("Pip-Boy menu: screen box %s", box ? "found" : "not found");
    }
    if (!box || !twb) return;
    /* the box's own shape: centre and size (ignoring whether it's hidden) */
    char *g = box;
    if (AsNode(g)) { char **kids = *(char ***)(g + 0xA0); uint16_t nk = *(uint16_t *)(g + 0xA6); g = (kids && nk) ? kids[0] : NULL; }
    if (!g || IsBadReadPtr(g, 0xC0)) return;
    char *gd = *(char **)(g + 0xB8); if (!gd || IsBadReadPtr(gd, 0x20)) return;
    const float *mb = (const float *)(gd + 0x10);
    const float *GR = (const float *)(g + NIAV_WORLD_ROTATE), *GT = (const float *)(g + NIAV_WORLD_TRANSLATE); float GS = *(const float *)(g + NIAV_WORLD_SCALE);
    float P[3]; for (int i = 0; i < 3; i++) P[i] = GT[i] + (GR[i * 3] * mb[0] + GR[i * 3 + 1] * mb[1] + GR[i * 3 + 2] * mb[2]) * GS;
    float rad = mb[3] * GS; if (!(rad > 0.3f && rad < 30.0f)) return;
    /* the screen's up direction from the forearm; facing your eye */
    const float *TR = (const float *)(twb + NIAV_WORLD_ROTATE);
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    float up[3] = { TR[1], TR[4], TR[7] }; float ul = V3Len(up); if (ul < 1e-4f) return; for (int i = 0; i < 3; i++) up[i] /= ul;
    float toEye[3]; V3Sub(toEye, C, P); float el = V3Len(toEye); if (el < 1.0f) return; for (int i = 0; i < 3; i++) toEye[i] /= el;
    /* the screen's real facing (the forearm's third axis), turned toward your eye */
    float n[3] = { TR[2], TR[5], TR[8] }; float nl = V3Len(n); for (int i = 0; i < 3; i++) n[i] /= (nl > 1e-4f ? nl : 1);
    if (V3Dot(n, toEye) < 0) for (int i = 0; i < 3; i++) n[i] = -n[i];
    float d = V3Dot(up, n); for (int i = 0; i < 3; i++) up[i] -= n[i] * d;   /* make up square to the facing */
    ul = V3Len(up); if (ul < 1e-4f) return; for (int i = 0; i < 3; i++) up[i] /= ul;
    float x[3]; V3_Cross(x, up, n);
    for (int i = 0; i < 3; i++) P[i] += n[i] * g_pipBoxLiftIni;   /* a hair above the box */
    /* game world -> headset space */
    float O = g_bodyOffset;
    float M[3][3] = { { cosf(O), 0.0f, -sinf(O) }, { -sinf(O), 0.0f, -cosf(O) }, { 0.0f, 1.0f, 0.0f } };
    float dP[3]; V3Sub(dP, P, C);
    float v[3], vx[3], vy[3], vz[3];
    for (int j = 0; j < 3; j++) {
        v[j]  = g_vrHeadPos[j] + (M[0][j] * dP[0] + M[1][j] * dP[1] + M[2][j] * dP[2]) / g_unitsPerMeter;
        vx[j] = M[0][j] * x[0] + M[1][j] * x[1] + M[2][j] * x[2];
        vy[j] = M[0][j] * up[0] + M[1][j] * up[1] + M[2][j] * up[2];
        vz[j] = M[0][j] * n[0] + M[1][j] * n[1] + M[2][j] * n[2];
    }
    for (int i = 0; i < 3; i++) { g_pipBoxPose.m[i][0] = vx[i]; g_pipBoxPose.m[i][1] = vy[i]; g_pipBoxPose.m[i][2] = vz[i]; g_pipBoxPose.m[i][3] = v[i]; }
    g_pipBoxWidthM = rad * 1.6f * g_pipBoxFitIni / g_unitsPerMeter;   /* a 4:3 screen's width from its half-diagonal */
    g_pipBoxOk = true;
}
/* Each remembered bone must still hang under the first-person body: walk up from it, checking at every step that the
   parent really lists it as a child (a freed, rebuilt body at the same address fails this). */
static bool Bones_StillAttached(char *node)
{
    for (int s = 0; s < 2; s++) for (int b = 0; b < B_COUNT; b++) {
        char *n = g_bones[s][b];
        if (!n) return false;
        for (int depth = 0; n != node; depth++) {
            if (depth > 40 || IsBadReadPtr(n, 0xA8)) return false;
            char *par = *(char **)(n + NIAV_PARENT);
            if (!par || IsBadReadPtr(par, 0xA8)) return false;
            char **kids = *(char ***)(par + 0xA0); uint16_t nk = *(uint16_t *)(par + 0xA6);
            if (!kids || nk > 512 || IsBadReadPtr(kids, nk * 4)) return false;
            bool found = false; for (int k = 0; k < nk; k++) if (kids[k] == n) { found = true; break; }
            if (!found) return false;
            n = par;
        }
    }
    return true;
}
static void Arms_Heal(const char *why)
{   /* what holstering used to fix by hand: find every part again and refresh the weapon in the hands */
    static double lastAt = 0; double now = QpcMs();
    if (now - lastAt < 5000.0) return;
    lastAt = now;
    g_fpGen++; g_bonesFor = NULL;
    if (!g_reequipAt) g_reequipAt = now + 100.0;
    Log("Arms: repairing the arms by themselves (%s)", why);
}
static bool Arms_Apply(char *player, char *cam)
{
    if (!g_bodyValid || !cam) return false;
    char *node = *(char **)(player + PLAYER_NODE_1ST_PERSON);
    if (!node) return false;
    {   /* rebuilt in place (same address): everything re-reads */
        static int failRun = 0; static bool checkOff = false;
        if (node == g_bonesFor && !checkOff) {
            if (!Bones_StillAttached(node)) {
                if (++failRun >= 3) { checkOff = true; Log("Arms: the rebuild check keeps failing on a fresh body - check switched off for this session"); }
                else {
                    g_fpGen++; g_bonesFor = NULL;
                    static int rl = 0; if (rl++ < 10) Log("Arms: the first-person body was rebuilt - finding the arm, hand and Pip-Boy parts again");
                }
            } else failRun = 0;
        }
    }
    if (node != g_bonesFor) {
        g_bonesFor = node;
        for (int s = 0; s < 2; s++) for (int b = 0; b < B_COUNT; b++) g_bones[s][b] = FindBone(node, g_boneNames[s][b], 0);
        FindFingers(node);
        Log("Arms: bones %s (L %p %p %p, R %p %p %p)",
            g_bones[0][0] && g_bones[0][1] && g_bones[0][2] && g_bones[1][0] && g_bones[1][1] && g_bones[1][2] ? "found" : "MISSING",
            (void *)g_bones[0][0], (void *)g_bones[0][1], (void *)g_bones[0][2],
            (void *)g_bones[1][0], (void *)g_bones[1][1], (void *)g_bones[1][2]);
    }
    for (int s = 0; s < 2; s++) for (int b = 0; b < B_COUNT; b++) if (!g_bones[s][b]) return false;
    PipOn_Find(node);
    if (g_pipOpen && g_pipModeRaw == 3) { Pip_FindMenuGeo(node); Pip_DumpParts(node); }
    if (g_pipOpen) PipOn_Capture(); else g_pRelOk = false;
    {   /* after loading, the game sometimes leaves the whole first-person model switched off until the
           Pip-Boy is used; you're always in first person in VR, so switch it back on */
        uint32_t *fl = (uint32_t *)(node + 0x30);
        if (*fl & 1) { *fl &= ~1u; static int hl = 0; if (hl++ < 3) Log("First-person arms were hidden - shown again"); }
    }

    /* The Pip-Boy animation moves the whole first-person body (shoulders, spine, arms). Turn it off: while
       the Pip-Boy is up, hold every bone in the pose it had just before, then place the arm from your hand. */
    g_pipLockNow = g_pipLock && g_wristPip && g_pipOpen && (g_pipModeRaw == 2 || g_pipModeRaw == 3 || g_pipModeRaw == 5);
    if (g_pipNoAnim && !g_pipLockNow) {
        #define POSE_MAX 512
        static char *pn[POSE_MAX]; static float ps[POSE_MAX][13]; static int pc = 0; static char *pfor = NULL; static bool phave = false;
        static uint32_t pGen = 0;
        if (pfor != node || pGen != g_fpGen) {
            pfor = node; pGen = g_fpGen; pc = 0; phave = false;
            char *spine = FindBone(node, "Bip01 Spine", 0);   /* from the spine down (the root follows your movement) */
            char *stack[POSE_MAX]; int sp = 0; if (spine) stack[sp++] = spine;
            while (sp && pc < POSE_MAX) {
                char *n = stack[--sp];
                if (n == g_pOn) continue;   /* the menu picture is placed separately */
                pn[pc++] = n;
                char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6);
                for (int k = 0; kids && k < nk && sp < POSE_MAX; k++) if (kids[k] && AsNode(kids[k])) stack[sp++] = kids[k];
            }
            Log("Pip-Boy: no-animation pose covers %d body parts", pc);
        }
        bool pipNow = g_pipOpen || g_pipModeRaw != 0 || PipMenusVisible();
        if (!pipNow) {
            for (int i = 0; i < pc; i++) { memcpy(ps[i], pn[i] + NIAV_LOCAL_ROTATE, 36); memcpy(ps[i] + 9, pn[i] + NIAV_LOCAL_TRANSLATE, 12); ps[i][12] = *(float *)(pn[i] + NIAV_LOCAL_SCALE); }
            phave = pc > 0;
        } else if (phave) {
            for (int i = 0; i < pc; i++) { memcpy(pn[i] + NIAV_LOCAL_ROTATE, ps[i], 36); memcpy(pn[i] + NIAV_LOCAL_TRANSLATE, ps[i] + 9, 12); *(float *)(pn[i] + NIAV_LOCAL_SCALE) = ps[i][12]; }
            NodeUpdate(node);
            static int fl = 0; if (fl++ < 2) Log("Pip-Boy: animation held off (everyday pose kept)");
        }
    }

    {   /* Reloads (the Caravan Shotgun's shell-by-shell one above all) can leave the arms frozen in their last reload
           pose - nothing poses them again afterwards. Keep each gun's normal pose while idle; put it back when a reload
           ends; if the arms still look stuck a moment later, refresh them like holstering does. */
        static float snap[2][B_COUNT][12]; static bool snapHave = false, reloaded = false; static void *snapW = NULL; static int lastAct = -1;
        void *w = PlayerWeaponForm();
        bool out = PlayerWeaponOut();
        if (w != snapW || !out) { snapW = w; snapHave = false; reloaded = false; }
        bool pipNow2 = g_pipOpen || g_pipModeRaw != 0;
        /* the normal pose: taken while idle, only before this gun's first reload (so a stuck pose is never taken as normal) */
        if (out && !pipNow2 && !reloaded && g_animAction == -1 && g_animIdleSince && QpcMs() - g_animIdleSince > 1000.0) {
            for (int s2 = 0; s2 < 2; s2++) for (int b = 0; b < B_COUNT; b++) {
                memcpy(snap[s2][b], g_bones[s2][b] + NIAV_LOCAL_ROTATE, 36); memcpy(snap[s2][b] + 9, g_bones[s2][b] + NIAV_LOCAL_TRANSLATE, 12); }
            snapHave = true;
        }
        if (lastAct == 9 && g_animAction != 9) reloaded = true;
        lastAct = g_animAction;
        /* after a reload, while idle: if the arms sit well away from the normal pose, put the normal pose back (every frame) */
        if (reloaded && snapHave && out && !pipNow2 && g_animAction == -1) {
            float worst = 0;
            for (int s2 = 0; s2 < 2; s2++) for (int b = 0; b < B_COUNT; b++) {
                const float *A = (const float *)(g_bones[s2][b] + NIAV_LOCAL_ROTATE), *B = snap[s2][b];
                float tr = 0; for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) tr += A[i * 3 + j] * B[i * 3 + j];
                float c = fminf(fmaxf((tr - 1.0f) * 0.5f, -1.0f), 1.0f), ang = acosf(c) * RAD2DEG;
                if (ang > worst) worst = ang;
            }
            if (worst > 20.0f) {
                for (int s2 = 0; s2 < 2; s2++) for (int b = 0; b < B_COUNT; b++) {
                    memcpy(g_bones[s2][b] + NIAV_LOCAL_ROTATE, snap[s2][b], 36); memcpy(g_bones[s2][b] + NIAV_LOCAL_TRANSLATE, snap[s2][b] + 9, 12); }
                NodeUpdate(node);
                static double ll = 0; if (QpcMs() - ll > 5000.0) { ll = QpcMs(); Log("Arms: holding the normal pose after a reload (the reload left them %.0f deg off)", worst); }
            }
        }
    }
    /* Save the animation's pose so it can be put back after drawing. */
    for (int s = 0; s < 2; s++) for (int b = 0; b < B_COUNT; b++) {
        memcpy(g_boneSaved[s][b], g_bones[s][b] + NIAV_LOCAL_ROTATE, 9 * sizeof(float));
        memcpy(g_boneSaved[s][b] + 9, g_bones[s][b] + NIAV_LOCAL_TRANSLATE, 3 * sizeof(float));
    }

    /* VR -> game world: game = camera + Mvr * (vr - head) * scale */
    float O = g_vrYawOffset;
    M3 Mvr = { { cosf(O), 0.0f, -sinf(O) }, { -sinf(O), 0.0f, -cosf(O) }, { 0.0f, 1.0f, 0.0f } };
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    M3 Bh, BhT;
    GameBasis(Bh, *PlayerRotZ(player), -*PlayerRotX(player));
    M3T(BhT, Bh);

    g_weaponOutThisFrame = PlayerWeaponOut();
    {
        static int shoulderLogs = 0;
        if (shoulderLogs < 2) {
            shoulderLogs++;
            for (int sd = 0; sd < 2; sd++) {
                const float *S = (const float *)(g_bones[sd][B_UPPER] + NIAV_WORLD_TRANSLATE);
                float d[3] = { S[0] - C[0], S[1] - C[1], S[2] - C[2] };
                /* express in head axes: Bh columns = right, up, back */
                Log("Shoulder %s relative to eyes: right %.1f, up %.1f, forward %.1f units (weapon out %d)", sd ? "R" : "L",
                    d[0] * Bh[0][0] + d[1] * Bh[1][0] + d[2] * Bh[2][0],
                    d[0] * Bh[0][1] + d[1] * Bh[1][1] + d[2] * Bh[2][1],
                    -(d[0] * Bh[0][2] + d[1] * Bh[1][2] + d[2] * Bh[2][2]), (int)g_weaponOutThisFrame);
            }
        }
    }
    g_pipLockNow = g_pipLock && g_wristPip && g_pipOpen && (g_pipModeRaw == 2 || g_pipModeRaw == 3 || g_pipModeRaw == 5);
    g_lhOk = false;
    for (int s = 0; s < 2; s++) {
        int hnd = s;   /* side 0 = left arm = left controller */
        if (!g_vrHandOk[hnd]) continue;
        if (s == 0 && g_pipLockNow) continue;   /* Pip-Boy up: the game's own pose holds it square in front of you */
        float rel[3], T[3];
        V3Sub(rel, g_vrHandPos[hnd], g_vrHeadPos);
        for (int i = 0; i < 3; i++)
            T[i] = C[i] + (Mvr[i][0] * rel[0] + Mvr[i][1] * rel[1] + Mvr[i][2] * rel[2]) * g_unitsPerMeter;
        /* Hand orientation in the game world: full controller rotation (point, tilt and twist). */
        M3 Hr, Ba, D;
        memcpy(Hr, g_vrHandRot[hnd], sizeof Hr);
        if (g_twoHand && hnd == (g_leftHanded ? 0 : 1)) { M3 C, CH; memcpy(C, g_twoHandC, sizeof C); M3Mul(CH, C, Hr); memcpy(Hr, CH, sizeof Hr); }
        M3Mul(Ba, Mvr, Hr);
        M3Mul(D, Ba, BhT);
        if (hnd == 0) {   /* back of the left hand (the Pip-Boy screen lines up with it) */
            float pl0 = (g_leftHanded ? 0 : 1) == 0 ? -1.0f : 1.0f;     /* palm sign for this controller */
            for (int i = 0; i < 3; i++) g_leftBack[i] = -Ba[i][0] * pl0;
            g_leftBackOk = true;
        }
        /* Hands stop at walls: if something solid is between your eyes and the hand, the hand is drawn
           against its surface (instead of passing through) and the fingers lie flat on it. */
        g_handOnWall[hnd] = false;
        {
            int wt = PlayerWeaponType();
            bool freeHand = hnd == (g_leftHanded ? 1 : 0) ? !g_twoHand : (!g_weaponOutThisFrame || wt == -1 || wt == 0);
            if (g_handWalls && freeHand && !g_holding[hnd]) {
                float d[3]; V3Sub(d, T, C);
                float L = V3Len(d);
                if (L > 1.0f) {
                    const float r = 2.2f;                       /* palm thickness */
                    float u[3] = { d[0] / L, d[1] / L, d[2] / L }, to[3];
                    for (int i = 0; i < 3; i++) to[i] = T[i] + u[i] * r;
                    float fr; char *o = RayCast(C, to, &fr);
                    if (o && fr < 1.0f && Climbable(o)) {
                        float hitD = fr * (L + r), stop = fmaxf(hitD - r, 1.0f);
                        if (stop < L) for (int i = 0; i < 3; i++) T[i] = C[i] + u[i] * stop;
                        g_handOnWall[hnd] = true;
                        /* the wall's surface direction, from two more rays a little to each side */
                        float e1[3], e2[3], up0[3] = { 0, 0, 1 };
                        V3Cross(e1, u, up0); float l1 = V3Len(e1);
                        if (l1 < 0.1f) { float x0[3] = { 1, 0, 0 }; V3Cross(e1, u, x0); l1 = V3Len(e1); }
                        for (int i = 0; i < 3; i++) e1[i] /= l1;
                        V3Cross(e2, u, e1);
                        float P0[3], P1[3], P2[3], n[3] = { -u[0], -u[1], -u[2] };
                        for (int i = 0; i < 3; i++) P0[i] = C[i] + u[i] * hitD;
                        bool ok = true;
                        for (int k = 0; k < 2 && ok; k++) {
                            const float *e = k ? e2 : e1; float a[3], b[3], f2;
                            for (int i = 0; i < 3; i++) { a[i] = C[i] + e[i] * 1.5f; b[i] = to[i] + e[i] * 1.5f; }
                            char *o2 = RayCast(a, b, &f2);
                            if (!o2 || f2 >= 1.0f) { ok = false; break; }
                            for (int i = 0; i < 3; i++) (k ? P2 : P1)[i] = a[i] + (b[i] - a[i]) * f2;
                        }
                        if (ok) {
                            float d1[3], d2[3], nn[3]; V3Sub(d1, P1, P0); V3Sub(d2, P2, P0); V3Cross(nn, d1, d2);
                            float ln = V3Len(nn);
                            if (ln > 1e-4f) { for (int i = 0; i < 3; i++) nn[i] /= ln; if (V3Dot(nn, u) > 0) for (int i = 0; i < 3; i++) nn[i] = -nn[i]; memcpy(n, nn, 12); }
                        }
                        /* turn the hand so the palm lies flat on the wall (palm = the controller's inner side) */
                        float pl = hnd == (g_leftHanded ? 0 : 1) ? -1.0f : 1.0f;
                        float palm[3] = { Ba[0][0] * pl, Ba[1][0] * pl, Ba[2][0] * pl }, tgt[3] = { -n[0], -n[1], -n[2] };
                        float c = V3Dot(palm, tgt), ax[3]; V3Cross(ax, palm, tgt);
                        float sn = V3Len(ax);
                        if (c > -0.5f && sn > 1e-4f) {
                            float K[3][3] = { { 0, -ax[2], ax[1] }, { ax[2], 0, -ax[0] }, { -ax[1], ax[0], 0 } };
                            float k2 = (1.0f - c) / (sn * sn); M3 Q, QD;
                            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
                                float kk = 0; for (int m = 0; m < 3; m++) kk += K[i][m] * K[m][j];
                                Q[i][j] = (i == j) + K[i][j] + kk * k2;
                            }
                            M3Mul(QD, Q, D); memcpy(D, QD, sizeof QD);
                        }
                        g_fingerCurl[hnd][0] = 0.15f; g_fingerCurl[hnd][1] = 0.05f; g_fingerCurl[hnd][2] = g_climb[hnd] ? 0.25f : 0.05f;   /* flat on the wall */
                        static int wl = 0;
                        if (wl++ < 4) Log("Hand on wall: %s hand stopped %.1f units short of where it is held (fraction %.2f)", hnd ? "right" : "left", L - stop, fr);
                    }
                }
            }
        }
        Arm_Reach(s, T, D, Bh);
        if (s == 0) { memcpy(g_lhT, T, 12); memcpy(g_lhD, D, sizeof D); memcpy(g_lhBa, Ba, sizeof Ba); g_lhOk = true; }
        {   /* the drawn hand must end up where your hand is (when the arm can reach it): if it stays well off, repair */
            static double offSince[2] = { 0, 0 };
            const float *Hw = (const float *)(g_bones[s][B_HAND] + NIAV_WORLD_TRANSLATE), *Sw = (const float *)(g_bones[s][B_UPPER] + NIAV_WORLD_TRANSLATE);
            const float *Ew = (const float *)(g_bones[s][B_FORE] + NIAV_WORLD_TRANSLATE);
            float e[3], st[3], a1[3], a2[3]; V3Sub(e, Hw, T); V3Sub(st, T, Sw); V3Sub(a1, Ew, Sw); V3Sub(a2, Hw, Ew);
            float miss = V3Len(e), need = V3Len(st), len = V3Len(a1) + V3Len(a2);
            bool off = miss > 14.0f && need < len * 0.9f && !g_handOnWall[hnd] && !g_climb[hnd] && !g_holding[hnd];
            if (!off) offSince[s] = 0;
            else if (!offSince[s]) offSince[s] = QpcMs();
            else if (QpcMs() - offSince[s] > 600.0) {
                offSince[s] = 0;
                char why[96]; snprintf(why, sizeof why, "%s hand drawn %.0f units from your real hand", s ? "right" : "left", miss);
                Arms_Heal(why);
            }
        }
    }

    /* Left-handed: the game always holds the gun in its right hand - draw the gun in your LEFT hand instead, turned the
       way the right hand would hold it if it were where your left hand is. Put back after drawing. */
    g_lhWeapNode = NULL;
    if (g_leftHanded && g_lhOk && g_weaponNode && g_weaponOutThisFrame && g_haveHandRef[1] && !g_pipOpen && g_pipModeRaw == 0) {
        char *wn = g_weaponNode, *par = *(char **)(wn + NIAV_PARENT);
        char *lh = g_bones[0][B_HAND];
        if (par && lh && !IsBadReadPtr(par, 0x90)) {
            M3 Hv, Rv; M3Mul(Hv, Bh, g_handRef[1]); M3Mul(Rv, g_lhD, Hv);   /* the right hand's grip, turned by your left controller */
            const float *Pw = (const float *)(par + NIAV_WORLD_TRANSLATE), *Pr = (const float *)(par + NIAV_WORLD_ROTATE);
            float ps = *(const float *)(par + NIAV_WORLD_SCALE); if (!(ps > 1e-4f)) ps = 1.0f;
            float *Lr = (float *)(wn + NIAV_LOCAL_ROTATE), *Lt = (float *)(wn + NIAV_LOCAL_TRANSLATE);
            memcpy(g_lhWeapL, Lr, 36); memcpy(g_lhWeapL + 9, Lt, 12); g_lhWeapNode = wn;
            /* the gun's place relative to the hand that holds it */
            M3 Lrm, Gr; memcpy(Lrm, Lr, 36);
            M3Mul(Gr, Rv, Lrm);
            /* where it sits: the mirror of its spot in the right palm (the two hands' bones are mirror images across their
               own x axis), measured from the left hand - then your fine-tuning along the controller's axes */
            const float *Lw = (const float *)(lh + NIAV_WORLD_TRANSLATE), *Lhr = (const float *)(lh + NIAV_WORLD_ROTATE);
            float tm[3] = { -Lt[0], Lt[1], Lt[2] }, Gp[3];
            for (int i = 0; i < 3; i++) Gp[i] = Lw[i] + (Lhr[i * 3 + 0] * tm[0] + Lhr[i * 3 + 1] * tm[1] + Lhr[i * 3 + 2] * tm[2]) * ps;
            {   float k = g_unitsPerMeter / 100.0f;   /* cm -> game units; controller axes: -Z forward, +Y up, +X right */
                for (int i = 0; i < 3; i++) Gp[i] += (-g_lhBa[i][2] * g_lhGunFwd + g_lhBa[i][1] * g_lhGunUp + g_lhBa[i][0] * g_lhGunSide) * k; }
            /* back into the right hand's frame (the gun stays its child) */
            M3 PrT, Pm, Nl; memcpy(Pm, Pr, 36); M3T(PrT, Pm); M3Mul(Nl, PrT, Gr);
            memcpy(Lr, Nl, 36);
            float d[3]; V3Sub(d, Gp, Pw);
            for (int i = 0; i < 3; i++) Lt[i] = (PrT[i][0] * d[0] + PrT[i][1] * d[1] + PrT[i][2] * d[2]) / ps;
            NodeUpdate(wn);
            static int ll = 0; if (ll++ < 2) Log("Left-handed: the gun is drawn in your left hand");
        }
    }

    /* Pip-Boy bigger (easier to read) while it's open. */
    {
        /* While the Pip-Boy is up: hold the forearm-twist bone (which carries the Pip-Boy) at its everyday
           angle, then roll it around the forearm just enough that the screen faces your eyes. */
        static char *tw = NULL, *twFor = NULL, *scr = NULL; static float twRest[9]; static bool twHave = false;
        static uint32_t twGen = 0;
        if (twFor != node || twGen != g_fpGen) {
            twFor = node; twGen = g_fpGen; tw = FindBone(node, "Bip01 L ForeTwist", 0); twHave = false;
            scr = FindNameCI(node, "pipboyscreen", 0);
            Log("Pip-Boy: twist bone %s, screen %s", tw ? "found" : "MISSING", scr ? *(const char **)(scr + 0x08) : "MISSING");
        }
        if (tw && !g_pipLockNow) {   /* the wrist end of the forearm (which carries the Pip-Boy) turns with your wrist: the forearm
                       bone takes part of the twist near the elbow, this bone the rest - like a real forearm */
            static float twBase[9], twLast[9]; static bool twHaveLast = false;
            float *Lr = (float *)(tw + NIAV_LOCAL_ROTATE);
            if (!(twHaveLast && memcmp(Lr, twLast, sizeof twLast) == 0)) memcpy(twBase, Lr, sizeof twBase);   /* fresh from the animation */
            if (g_pipModeRaw == 0 && !g_pipOpen) { memcpy(twRest, twBase, sizeof twRest); twHave = true; }
            float base[9]; memcpy(base, (g_pipModeRaw || g_pipOpen) && twHave ? twRest : twBase, sizeof base);
            float roll = g_twOk ? g_twRoll : 0.0f;
            int a1 = (g_twAx + 1) % 3, a2 = (g_twAx + 2) % 3;
            float Rt[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }, c = cosf(roll), sn = sinf(roll);
            Rt[a1 * 3 + a1] = c; Rt[a1 * 3 + a2] = -sn; Rt[a2 * 3 + a1] = sn; Rt[a2 * 3 + a2] = c;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) Lr[i * 3 + j] = base[i * 3] * Rt[j] + base[i * 3 + 1] * Rt[3 + j] + base[i * 3 + 2] * Rt[6 + j];
            NodeUpdate(tw);
            memcpy(twLast, Lr, sizeof twLast); twHaveLast = true;
            (void)scr; (void)g_leftBackOk;
        }
        {   /* the Pip-Boy 2000's glass layers (glare, static) cover the menu: hide them while it's open */
            static char *layers[4] = { 0 }; static char *lFor = NULL; static int nl = 0; static bool hidden = false;
            static uint32_t lGen = 0;
            if (lFor != node || lGen != g_fpGen) {
                lFor = node; lGen = g_fpGen; nl = 0;
                const char *want[2] = { "glare:0", "static:0" };
                char *stack[256]; int sp = 0; stack[sp++] = node;
                while (sp && nl < 4) {
                    char *n = stack[--sp]; if (!n || IsBadReadPtr(n, 0xA8)) continue;
                    const char *nm = *(const char **)(n + 0x08);
                    if (nm && !IsBadReadPtr(nm, 1)) for (int w = 0; w < 2; w++) if (_stricmp(nm, want[w]) == 0 && nl < 4) layers[nl++] = n;
                    if (AsNode(n)) { char **kids = *(char ***)(n + 0xA0); uint16_t nk = *(uint16_t *)(n + 0xA6); for (int k = 0; kids && k < nk && sp < 256; k++) stack[sp++] = kids[k]; }
                }
                Log("Pip-Boy: %d glass layer(s) found to clear while it's open", nl);
            }
            bool want = g_pipOpen && g_pipModeRaw == 3 && !g_pipLockNow;
            if (want != hidden) { for (int i = 0; i < nl; i++) { uint32_t *f = (uint32_t *)(layers[i] + 0x30); if (want) *f |= 1u; else *f &= ~1u; } hidden = want; }
        }
        PipScreen_Offset(node);
        PipBox_Place(node, cam);
        if (g_pipOpen) {
            (void)PipOn_Apply;   /* parts are placed by the part picker now */
            static int pl2 = 0;
            if (g_pRelOk && pl2++ < 2) Log("Pip-Boy: menu picture placed on the screen of the Pip-Boy on your arm");
        }
        /* Bigger screen picture, grown around its own centre (not its corner). */
        static char *pipNode = NULL, *pipFor = NULL; static float pipScale0 = 1.0f, pipT0[3], pipCenterL[3]; static bool pipHaveC = false;
        static uint32_t pfGen = 0;
        if (pipFor != node || pfGen != g_fpGen) {
            pipFor = node; pfGen = g_fpGen; pipNode = FindNameCI(node, "pipboyon", 0); pipHaveC = false;
            if (pipNode) { pipScale0 = *(float *)(pipNode + NIAV_LOCAL_SCALE); memcpy(pipT0, pipNode + NIAV_LOCAL_TRANSLATE, 12); }
        }
        if (pipNode && fabsf(g_pipScale - 1.0f) > 0.01f) {   /* (only when a bigger picture is asked for) */
            float *sc = (float *)(pipNode + NIAV_LOCAL_SCALE), *lt = (float *)(pipNode + NIAV_LOCAL_TRANSLATE);
            if (!g_pipOpen && g_pipModeRaw == 0) {
                *sc = pipScale0; memcpy(lt, pipT0, 12);
                const float *bnd = *(const float **)(pipNode + 0x20);
                if (bnd && bnd[3] > 0.01f) {   /* remember where the picture's centre sits, in the node's own frame */
                    const float *Rw = (const float *)(pipNode + NIAV_WORLD_ROTATE), *Tw = (const float *)(pipNode + NIAV_WORLD_TRANSLATE);
                    float ws = *(const float *)(pipNode + NIAV_WORLD_SCALE); if (!(ws > 1e-4f)) ws = 1.0f;
                    float d[3]; V3Sub(d, bnd, Tw);
                    for (int i = 0; i < 3; i++) pipCenterL[i] = (Rw[0 * 3 + i] * d[0] + Rw[1 * 3 + i] * d[1] + Rw[2 * 3 + i] * d[2]) / ws;
                    pipHaveC = true;
                }
            } else if (g_pipOpen && fabsf(g_pipScale - 1.0f) > 0.01f) {
                /* measure the picture's centre at normal size, grow it, then slide it back onto that centre */
                *sc = pipScale0; memcpy(lt, pipT0, 12); NodeUpdate(pipNode);
                const float *bnd = *(const float **)(pipNode + 0x20);
                float c1[3] = { 0, 0, 0 }; bool have = bnd && bnd[3] > 0.01f;
                if (have) memcpy(c1, bnd, 12);
                *sc = pipScale0 * g_pipScale; NodeUpdate(pipNode);
                bnd = *(const float **)(pipNode + 0x20);
                char *par = *(char **)(pipNode + NIAV_PARENT);
                if (have && bnd && par) {
                    float d[3]; V3Sub(d, c1, bnd);
                    const float *Pr = (const float *)(par + NIAV_WORLD_ROTATE);
                    float ps = *(const float *)(par + NIAV_WORLD_SCALE); if (!(ps > 1e-4f)) ps = 1.0f;
                    for (int i = 0; i < 3; i++) lt[i] = pipT0[i] + (Pr[0 * 3 + i] * d[0] + Pr[1 * 3 + i] * d[1] + Pr[2 * 3 + i] * d[2]) / ps;
                    NodeUpdate(pipNode);
                    static int cl = 0;
                    if (cl++ < 2) Log("Pip-Boy: bigger screen picture moved %.1f units back onto the screen centre", V3Len(d));
                }
            }
        }
        /* diagnostics: which parts of the arm model are on screen while the Pip-Boy is open */
        {
            static double openAt = 0; static bool listed = false;
            if (g_pipModeRaw == 3) { if (!openAt) openAt = QpcMs(); } else openAt = 0;
            if (!listed && openAt && QpcMs() - openAt > 800.0) { listed = true; Pip_ListOnScreen(node, cam); }
        }
    }

    /* Weapon put away: hide the gun the first-person hand still carries (the game normally keeps it
       out of sight by lowering the arms, which we no longer do). */
    if (g_weaponNode && !g_weaponOutThisFrame) {
        uint32_t *flags = (uint32_t *)(g_weaponNode + 0x30);
        if (!(*flags & 1)) { *flags |= 1; g_weaponHidden = true; }
    }

    /* Fingers: left hand always; right hand when no weapon is out or when using fists. */
    if (g_fingerTracking) {
        int wt = PlayerWeaponType();
        bool rightFree = !g_weaponOutThisFrame || wt == -1 || wt == 0;
        int leftSide = 0, rightSide = 1;
        int leftPhys = 0, rightPhys = 1;   /* arms follow the physical hands */
        if (g_vrHandOk[leftPhys] && !g_pipLockNow) Fingers_Apply(leftSide, leftPhys);
        if (rightFree && g_vrHandOk[rightPhys]) Fingers_Apply(rightSide, rightPhys);
        if (!g_fingersLogged) {
            g_fingersLogged = true;
            Log("Fingers: curl L thumb %.2f index %.2f grip %.2f | R thumb %.2f index %.2f grip %.2f (right hand %s)",
                g_fingerCurl[0][0], g_fingerCurl[0][1], g_fingerCurl[0][2], g_fingerCurl[1][0], g_fingerCurl[1][1], g_fingerCurl[1][2],
                rightFree ? "free" : "holding weapon");
        }
    }
    return true;
}

static void Arms_Restore(void)
{
    if (g_lhWeapNode) {   /* left-handed: the gun back in the right hand's data */
        memcpy(g_lhWeapNode + NIAV_LOCAL_ROTATE, g_lhWeapL, 36); memcpy(g_lhWeapNode + NIAV_LOCAL_TRANSLATE, g_lhWeapL + 9, 12);
        NodeUpdate(g_lhWeapNode); g_lhWeapNode = NULL;
    }
    PipOn_Restore();
    Fingers_Restore();
    if (g_weaponHidden && g_weaponNode) { *(uint32_t *)(g_weaponNode + 0x30) &= ~1u; g_weaponHidden = false; }
    for (int s = 0; s < 2; s++) for (int b = 0; b < B_COUNT; b++) {
        if (!g_bones[s][b]) continue;
        memcpy(g_bones[s][b] + NIAV_LOCAL_ROTATE, g_boneSaved[s][b], 9 * sizeof(float));
        memcpy(g_bones[s][b] + NIAV_LOCAL_TRANSLATE, g_boneSaved[s][b] + 9, 3 * sizeof(float));
    }
    for (int s = 0; s < 2; s++) if (g_twistScaled[s] && g_twistBone[s]) { memcpy(g_twistBone[s] + NIAV_LOCAL_TRANSLATE, g_twistSaved[s], 12); g_twistScaled[s] = false; }
    for (int s = 0; s < 2; s++) if (g_bones[s][B_UPPER]) NodeUpdate(g_bones[s][B_UPPER]);
}

/* ---- Physical crouch / lean ----------------------------------------------
   Real head movement moves the view (and the arms with it): crouch down to reach the floor, lean
   in over a table or into a shelf. Leaning further than fMaxLean drags the body's centre along. */
static char *g_offCam = NULL, *g_offFirst = NULL;
static float g_offCamT[3], g_offCamL[3], g_offFirstT[3], g_offFirstL[3], g_offBodyT[3], g_offBodyL[3];
static char *g_offBody = NULL;

static void HeadOffset_Reset(void) { g_standY = -1.0f; g_haveLeanAnchor = false; }

static void MoveNode(char *node, const float *w, float *savedT, float *savedL)
{
    float *T = (float *)(node + NIAV_WORLD_TRANSLATE), *L = (float *)(node + NIAV_LOCAL_TRANSLATE);
    memcpy(savedT, T, 12); memcpy(savedL, L, 12);
    float l[3] = { w[0], w[1], w[2] };
    char *parent = *(char **)(node + NIAV_PARENT);
    if (parent) {
        const float *R = (const float *)(parent + NIAV_WORLD_ROTATE);
        float sc = *(const float *)(parent + NIAV_WORLD_SCALE);
        if (!(sc > 0.0001f)) sc = 1.0f;
        for (int k = 0; k < 3; k++) l[k] = (R[0 * 3 + k] * w[0] + R[1 * 3 + k] * w[1] + R[2 * 3 + k] * w[2]) / sc;
    }
    for (int k = 0; k < 3; k++) { T[k] += w[k]; L[k] += l[k]; }
}

/* Moving fast (grapple, launches): the game places the camera from where you were a frame earlier, while
   your body (and a full-body mod's model) is already drawn where you are now - at grapple speed that puts
   your body out in front of you. The view is moved up to where the body really is. */
static void HeadOffset_Lag(char *player, char *cam, float *w)
{
    static float off[3]; static bool haveOff = false; static float prevP[3]; static double prevT = 0;
    const float *P = (const float *)(player + 0x30), *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    double now = QpcMs(); float dt = prevT > 0 ? (float)((now - prevT) / 1000.0) : 0.0f; prevT = now;
    float sp = 0;
    if (dt > 0.001f && dt < 0.2f) { float d[3]; V3Sub(d, P, prevP); sp = V3Len(d) / dt; }
    memcpy(prevP, P, 12);
    float cur[3]; for (int i = 0; i < 3; i++) cur[i] = C[i] - P[i];
    float slow = 1.5f * g_unitsPerMeter, fast = 4.0f * g_unitsPerMeter;
    if (sp < slow || !haveOff) {   /* walking pace: learn where the view normally sits relative to your feet */
        if (!haveOff) { memcpy(off, cur, 12); haveOff = true; }
        else for (int i = 0; i < 3; i++) off[i] += (cur[i] - off[i]) * 0.2f;
        return;
    }
    float k = fminf((sp - slow) / (fast - slow), 1.0f);
    float c[3]; for (int i = 0; i < 3; i++) c[i] = (off[i] - cur[i]) * k;
    float cl = V3Len(c), cap = 1.2f * g_unitsPerMeter;
    if (cl > cap) for (int i = 0; i < 3; i++) c[i] *= cap / cl;
    for (int i = 0; i < 3; i++) w[i] += c[i];
    static int nl = 0; if (nl++ < 3 && cl > 5.0f) Log("Fast movement: view moved %.0f units to stay with your body (%.1f m/s)", cl, sp / g_unitsPerMeter);
}

/* Your body (the full body you see below you) is kept under your head at any speed: the game moves the body
   and the view at slightly different moments in the frame, so at grapple / launch speeds the body ran ahead.
   Where the view sits over the body is learned while walking; moving fast, the body is put back there. */
static void Body_KeepUnder(char *player, char *cam)
{
    g_offBody = NULL;
    char *rs = *(char **)(player + 0x64); char *root = rs && !IsBadReadPtr(rs, 0x18) ? *(char **)(rs + 0x14) : NULL;
    if (!root || IsBadReadPtr(root, 0x90)) return;
    NodeUpdate(root);   /* (its position as of this frame - it may still hold last frame's) */
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *R = (const float *)(root + NIAV_WORLD_TRANSLATE), *P = (const float *)(player + 0x30);
    static float off[2]; static bool have = false; static float prevP[3]; static double prevT = 0;
    double now = QpcMs(); float dt = prevT > 0 ? (float)((now - prevT) / 1000.0) : 0.0f; prevT = now;
    float sp = 0; if (dt > 0.001f && dt < 0.2f) { float d[3]; V3Sub(d, P, prevP); sp = V3Len(d) / dt; }
    memcpy(prevP, P, 12);
    float cur[2] = { C[0] - R[0], C[1] - R[1] };   /* (sideways only - height stays the game's) */
    float still = 0.4f * g_unitsPerMeter, slow = 2.5f * g_unitsPerMeter, full = 5.0f * g_unitsPerMeter;
    if (sp < still || !have) {   /* learned while (nearly) standing still */
        if (!have) { off[0] = cur[0]; off[1] = cur[1]; have = true; }
        else { off[0] += (cur[0] - off[0]) * 0.1f; off[1] += (cur[1] - off[1]) * 0.1f; }
    }
    if (sp < slow) return;   /* walking and running: the game's own body placement */
    float k = fminf((sp - slow) / (full - slow), 1.0f);
    float w[3] = { (cur[0] - off[0]) * k, (cur[1] - off[1]) * k, 0.0f };
    float wl = sqrtf(w[0] * w[0] + w[1] * w[1]); if (wl < 0.5f) return;
    float cap = 3.0f * g_unitsPerMeter; if (wl > cap) { w[0] *= cap / wl; w[1] *= cap / wl; }
    MoveNode(root, w, g_offBodyT, g_offBodyL); NodeUpdate(root); g_offBody = root;
    static int nl = 0; if (nl++ < 3) Log("Fast movement: your body moved %.0f units to stay under your head (%.1f m/s)", wl, sp / g_unitsPerMeter);
}
static void HeadOffset_ApplyInner(char *player, char *cam);
static void HeadOffset_Apply(char *player, char *cam)
{
    HeadOffset_ApplyInner(player, cam);
    /* (Body_KeepUnder retired: moving the body's skeleton while the physics thread reads it is a crash risk, and it made the body run ahead) */
    (void)Body_KeepUnder;
}
static void HeadOffset_ApplyInner(char *player, char *cam)
{
    g_offCam = g_offFirst = NULL;
    if (!g_bodyValid) return;
    float lag[3] = { 0, 0, 0 };
    HeadOffset_Lag(player, cam, lag);
    {   /* seated: the right stick (down) crouches you smoothly - and back up when let go */
        static double at = 0; double now = QpcMs(); float dt = at > 0 ? (float)((now - at) / 1000.0) : 0.0f; at = now;
        if (dt > 0.1f) dt = 0.1f;
        float want = g_seated && g_seatCrouchWant ? 1.0f : 0.0f;
        float spd = 3.0f * dt;
        if (g_seatCrouchK < want) g_seatCrouchK = fminf(g_seatCrouchK + spd, want); else g_seatCrouchK = fmaxf(g_seatCrouchK - spd, want);
    }
    if (!g_physicalMove && !g_seated) {
        if (fabsf(lag[0]) + fabsf(lag[1]) + fabsf(lag[2]) < 0.05f) return;
        MoveNode(cam, lag, g_offCamT, g_offCamL); g_offCam = cam;
        char *first = *(char **)(player + PLAYER_NODE_1ST_PERSON);
        if (first) { MoveNode(first, lag, g_offFirstT, g_offFirstL); NodeUpdate(first); g_offFirst = first; }
        return;
    }
    float hy = g_vrHeadPos[1];
    if (hy > g_standY) g_standY = hy;
    if (!g_haveLeanAnchor) { g_leanAnchor[0] = g_vrHeadPos[0]; g_leanAnchor[1] = g_vrHeadPos[2]; g_haveLeanAnchor = true; }
    float dx = g_vrHeadPos[0] - g_leanAnchor[0], dz = g_vrHeadPos[2] - g_leanAnchor[1];
    float dl = sqrtf(dx * dx + dz * dz);
    if (dl > g_maxLean) {
        float k = (dl - g_maxLean) / dl;
        g_leanAnchor[0] += dx * k; g_leanAnchor[1] += dz * k;
        dx -= dx * k; dz -= dz * k;
    }
    float dy = fminf(hy - g_standY, 0.0f);
    if (g_seated) dy -= g_seatCrouchK * g_seatCrouchDepth;   /* (seated crouch on the right stick) */
    if (dy < -1.6f) dy = -1.6f;
    float O = g_vrYawOffset;
    float w[3] = { ( cosf(O) * dx - sinf(O) * dz) * g_unitsPerMeter,
                   (-sinf(O) * dx - cosf(O) * dz) * g_unitsPerMeter,
                   dy * g_unitsPerMeter };
    for (int i = 0; i < 3; i++) w[i] += lag[i];
    if (fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]) < 0.05f) return;
    MoveNode(cam, w, g_offCamT, g_offCamL); g_offCam = cam;
    char *first = *(char **)(player + PLAYER_NODE_1ST_PERSON);
    if (first) { MoveNode(first, w, g_offFirstT, g_offFirstL); NodeUpdate(first); g_offFirst = first; }
    static int logs = 0;
    if (logs < 3 && (dy < -0.3f || dl > 0.2f)) {
        logs++;
        Log("Physical movement: head %.2f m below standing, leaning %.2f m - view moved (%.0f, %.0f, %.0f) units",
            -dy, sqrtf(dx * dx + dz * dz), w[0], w[1], w[2]);
    }
}

static void HeadOffset_Restore(void)
{
    if (g_offBody) {
        memcpy(g_offBody + NIAV_WORLD_TRANSLATE, g_offBodyT, 12); memcpy(g_offBody + NIAV_LOCAL_TRANSLATE, g_offBodyL, 12);
        NodeUpdate(g_offBody); g_offBody = NULL;
    }
    if (g_offCam) {
        memcpy(g_offCam + NIAV_WORLD_TRANSLATE, g_offCamT, 12); memcpy(g_offCam + NIAV_LOCAL_TRANSLATE, g_offCamL, 12);
        g_offCam = NULL;
    }
    if (g_offFirst) {
        memcpy(g_offFirst + NIAV_WORLD_TRANSLATE, g_offFirstT, 12); memcpy(g_offFirst + NIAV_LOCAL_TRANSLATE, g_offFirstL, 12);
        NodeUpdate(g_offFirst);
        g_offFirst = NULL;
    }
}

/* ---- Hand grabbing --------------------------------------------------------
   With no weapon out, a grip near a loose (physics) object picks it up. The object is driven by
   setting its Havok velocities every frame so it follows the hand's position and full rotation;
   letting go leaves it moving with the hand, so it can be thrown. */
#define HK_TO_GAME          6.9991f      /* game units per Havok unit */
#define TES_PTR_ADDR        0x011DEA10
#define FORMMAP_PTR_ADDR    0x011C54C0   /* NiTMap<refID, TESForm*> of all forms */
#define HK_ACTIVATE_ADDR    0x00C9C1D0   /* hkpRigidBody: wake up / apply new velocities */
#define VTBL_BHKCOLLISIONOBJECT 0x0103046C
#define VTBL_BHKRIGIDBODY       0x010301B4
#define VTBL_BHKRIGIDBODYT      0x010C879C
static void *LookupForm(uint32_t id)
{
    char *map = *(char **)FORMMAP_PTR_ADDR;
    if (!map) return NULL;
    uint32_t n = *(uint32_t *)(map + 4);
    char **buckets = *(char ***)(map + 8);
    if (!n || !buckets) return NULL;
    for (char *e = buckets[id % n]; e; e = *(char **)e)
        if (*(uint32_t *)(e + 4) == id) return *(void **)(e + 8);
    return NULL;
}
typedef void (__attribute__((thiscall)) *HkActivate_t)(void *rigidBody);

typedef struct {
    char    *ref, *rb;
    uint32_t refID;
    float    offLocal[3];     /* object's centre of mass in hand coordinates */
    M3       rotRel;          /* object's rotation relative to the hand */
    float    prevT[3];
    M3       prevR;
    bool     havePrev;
    float    vel[3];          /* smoothed hand-driven velocity (game units/s), used when thrown */
    float    curl;            /* finger wrap for this object's size */
    float    farTime;         /* seconds the object has been stuck far from the hand */
    float    gripR;           /* half the object's thickness where the hand closes on it (game units) */
    float    blend;           /* 0 -> 1 while it slides into the palm after grabbing */
    char    *visNode;         /* root node moved for drawing this frame (restored afterwards) */
    bool     elong;           /* long object (bottle, stick): lies along the curled fingers' axis */
    int      longAx;          /* its long local axis */
    float    visSaved[12];
} Held;
/* Where each hand's palm is, measured from its finger bones, in hand (controller) coordinates
   relative to the hand bone: knuckle point, finger direction, palm normal. */
typedef struct { bool ok; float kn[3], fwd[3], n[3]; bool gripOk; float gc[3], ga[3], seg; bool frameOk; float gcW[3], handAt[3]; } Palm;
static bool g_grabBlock[2] = { false, false };   /* hand lost its object to the other hand: wait for a re-squeeze */
static Palm g_palm[2];
static Held  g_held[2];
static float g_handW[2][3];      /* hand position in the game world this frame */
static M3    g_handWR[2];        /* hand rotation in the game world (columns: right, up, back) */
static bool  g_handWOk[2] = { false, false };
static LARGE_INTEGER g_grabLastQpc;

static void Hands_ComputeWorld(char *player, char *cam)
{
    float O = g_vrYawOffset;
    M3 Mvr = { { cosf(O), 0.0f, -sinf(O) }, { -sinf(O), 0.0f, -cosf(O) }, { 0.0f, 1.0f, 0.0f } };
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    (void)player;
    for (int h = 0; h < 2; h++) {
        g_handWOk[h] = g_bodyValid && g_vrHandOk[h];
        if (!g_handWOk[h]) continue;
        float rel[3];
        V3Sub(rel, g_vrHandPos[h], g_vrHeadPos);
        for (int i = 0; i < 3; i++)
            g_handW[h][i] = C[i] + (Mvr[i][0] * rel[0] + Mvr[i][1] * rel[1] + Mvr[i][2] * rel[2]) * g_unitsPerMeter;
        M3 Hr; memcpy(Hr, g_vrHandRot[h], sizeof Hr);
        M3Mul(g_handWR[h], Mvr, Hr);
    }
}

/* Each Havok body's game-side wrapper (needed to make collision changes take effect). */
static struct { char *rb, *wo; } g_rbWo[16]; static int g_rbWoN;
static void RbWo_Note(char *rb, char *wo)
{
    for (int i = 0; i < 16; i++) if (g_rbWo[i].rb == rb) { g_rbWo[i].wo = wo; return; }
    g_rbWo[g_rbWoN].rb = rb; g_rbWo[g_rbWoN].wo = wo; g_rbWoN = (g_rbWoN + 1) & 15;
}
static char *RbWo_Get(char *rb)
{
    for (int i = 0; i < 16; i++) if (g_rbWo[i].rb == rb && rb) {
        char *wo = g_rbWo[i].wo;
        if (Rd(wo, 0x14) && (*(uint32_t *)wo == VTBL_BHKRIGIDBODY || *(uint32_t *)wo == VTBL_BHKRIGIDBODYT) && *(char **)(wo + 8) == rb) return wo;
    }
    return NULL;
}
/* The movable Havok body of an object (searches its 3D a few levels deep), or NULL. */
static char *FindMobileBody(char *node, int depth)
{
    if (!node || depth > 4) return NULL;
    char *co = *(char **)(node + 0x1C);
    if (co && *(uint32_t *)co == VTBL_BHKCOLLISIONOBJECT) {
        char *wo = *(char **)(co + 0x10);
        if (wo && (*(uint32_t *)wo == VTBL_BHKRIGIDBODY || *(uint32_t *)wo == VTBL_BHKRIGIDBODYT)) {
            char *rb = *(char **)(wo + 0x08);
            if (rb && *(BYTE *)(rb + 0x28) == 1) {
                BYTE mt = *(BYTE *)(rb + 0xE8);
                if (mt == 2 || mt == 3 || mt == 6) { RbWo_Note(rb, wo); return rb; }   /* sphere/box/thin-box inertia = dynamic */
            }
        }
    }
    if (!AsNode(node)) return NULL;
    char **kids = *(char ***)(node + 0xA0);
    unsigned count = *(uint16_t *)(node + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) {
        char *r = FindMobileBody(kids[i], depth + 1);
        if (r) return r;
    }
    return NULL;
}

static char *RefBody(char *ref)
{
    char *rs = *(char **)(ref + 0x64);
    char *root = rs ? *(char **)(rs + 0x14) : NULL;
    return root ? FindMobileBody(root, 0) : NULL;
}

static void BodyCom(char *rb, float *out) { const float *c = (const float *)(rb + 0x140); for (int i = 0; i < 3; i++) out[i] = c[i] * HK_TO_GAME; }
static void BodyRot(char *rb, M3 R) { for (int c = 0; c < 3; c++) for (int r = 0; r < 3; r++) R[r][c] = *(const float *)(rb + 0xF0 + 0x10 * c + 4 * r); }
static float BodyRadius(char *rb) { float r = *(const float *)(rb + 0x190) * HK_TO_GAME; return fminf(fmaxf(r * 0.6f, 1.0f), 25.0f); }

/* Rotation matrix -> rotation vector (axis * angle, radians). */
static void RotVec(M3 E, float *out)
{
    float tr = E[0][0] + E[1][1] + E[2][2];
    float cs = fminf(fmaxf((tr - 1.0f) * 0.5f, -1.0f), 1.0f);
    float ang = acosf(cs);
    float v[3] = { E[2][1] - E[1][2], E[0][2] - E[2][0], E[1][0] - E[0][1] };
    float s = V3Len(v);
    if (ang < 1e-4f) { out[0] = out[1] = out[2] = 0.0f; return; }
    if (s < 1e-5f) {   /* ~180 degrees: axis from the largest diagonal */
        int i = E[0][0] > E[1][1] ? (E[0][0] > E[2][2] ? 0 : 2) : (E[1][1] > E[2][2] ? 1 : 2);
        float a[3] = { E[0][i], E[1][i], E[2][i] }; a[i] += 1.0f;
        float l = V3Len(a); if (l < 1e-6f) { out[0] = out[1] = out[2] = 0.0f; return; }
        for (int k = 0; k < 3; k++) out[k] = a[k] / l * ang;
        return;
    }
    for (int k = 0; k < 3; k++) out[k] = v[k] / s * ang;
}

/* Rotation vector (axis * angle) -> rotation matrix. */
static void VecRot(const float *v, M3 R)
{
    float a = V3Len(v);
    M3 I = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    if (a < 1e-6f) { memcpy(R, I, sizeof I); return; }
    float x = v[0] / a, y = v[1] / a, z = v[2] / a, c = cosf(a), sn = sinf(a), t = 1.0f - c;
    R[0][0] = t * x * x + c;     R[0][1] = t * x * y - sn * z; R[0][2] = t * x * z + sn * y;
    R[1][0] = t * x * y + sn * z; R[1][1] = t * y * y + c;     R[1][2] = t * y * z - sn * x;
    R[2][0] = t * x * z - sn * y; R[2][1] = t * y * z + sn * x; R[2][2] = t * z * z + c;
}

/* ---- Holstered weapons you can see ---------------------------------------------------------------------
   A weapon put away in a holster is shown there on your body: on your hip, or over your shoulder. Its model
   is attached to your body with JIP LN's AttachModel, placed each frame at the holster spot (the same spots
   your hand uses), turned by three angles per holster ([HolsterModel] in FNVR.ini / VR settings), and hidden
   whenever that weapon is in your hand. */
static bool g_holShow = true; static float g_holAng[3][3] = { { 0, 0, 0 }, { 0, -90, 0 }, { 0, 70, 180 } };
static void *g_holAttScr = NULL; static bool g_holAttFailed = false;
typedef struct { void *weap; char path[200]; char name[96]; char *node; double triedAt; } HolModel;
static HolModel g_holM[3];
static void Hol_Load(void)
{
    g_holShow = GetPrivateProfileIntA("HolsterModel", "bShow", 1, g_iniPath) != 0;
    char b[64];
    GetPrivateProfileStringA("HolsterModel", "sHip", "0 -90 0", b, sizeof b, g_iniPath); sscanf(b, "%f %f %f", &g_holAng[1][0], &g_holAng[1][1], &g_holAng[1][2]);
    GetPrivateProfileStringA("HolsterModel", "sShoulder", "0 70 180", b, sizeof b, g_iniPath); sscanf(b, "%f %f %f", &g_holAng[2][0], &g_holAng[2][1], &g_holAng[2][2]);
}
static void Hol_Save(void)
{
    char b[64];
    snprintf(b, sizeof b, "%.0f %.0f %.0f", g_holAng[1][0], g_holAng[1][1], g_holAng[1][2]); WritePrivateProfileStringA("HolsterModel", "sHip", b, g_iniPath);
    snprintf(b, sizeof b, "%.0f %.0f %.0f", g_holAng[2][0], g_holAng[2][1], g_holAng[2][2]); WritePrivateProfileStringA("HolsterModel", "sShoulder", b, g_iniPath);
}
static char *Hol_ChildNamed(char *parent, const char *name)
{
    if (!parent || !AsNode(parent)) return NULL;
    char **kids = *(char ***)(parent + 0xA0); unsigned n = *(uint16_t *)(parent + 0xA6);
    for (unsigned i = 0; kids && i < n; i++) { char *k = kids[i]; const char *kn = k ? *(const char **)(k + 0x08) : NULL; if (kn && !_stricmp(kn, name)) return k; }
    return NULL;
}
static void Holster_Models(char *player)
{
    char *rs = *(char **)(player + 0x64); char *root = rs && !IsBadReadPtr(rs, 0x18) ? *(char **)(rs + 0x14) : NULL;
    if (!root || IsBadReadPtr(root, 0xB0)) return;
    void *inHand = g_weaponOutThisFrame ? PlayerWeaponForm() : NULL;
    for (int z = 1; z <= 2; z++) {
        HolModel *m = &g_holM[z];
        void *w = g_holster[z];
        if (!w && g_holsterId[z]) w = g_holster[z] = LookupForm(g_holsterId[z]);
        bool want = g_holShow && g_holstersOn && w && w != inHand;
        const char *bone = z == 1 ? "Bip01 Pelvis" : "Bip01 Spine2";
        char *parent = FindBone(root, bone, 0);
        if (m->weap != w) {   /* a different weapon in this holster: hide the old one */
            if (m->node && !IsBadReadPtr(m->node, 0x34)) *(uint32_t *)(m->node + 0x30) |= 1;
            memset(m, 0, sizeof *m); m->weap = w;
            const char *mp = w && !IsBadReadPtr((char *)w + 0x40, 4) ? *(const char **)((char *)w + 0x40) : NULL;
            if (mp && !IsBadReadPtr(mp, 1)) snprintf(m->path, sizeof m->path, "%s", mp);
        }
        if (!want || !parent || !m->path[0]) { if (m->node && !IsBadReadPtr(m->node, 0x34)) *(uint32_t *)(m->node + 0x30) |= 1; continue; }
        if (m->node && (IsBadReadPtr(m->node, 0xB0) || *(char **)(m->node + NIAV_PARENT) != parent)) m->node = NULL;
        if (!m->node && m->name[0]) m->node = Hol_ChildNamed(parent, m->name);
        if (!m->node) {   /* attach it (once in a while, until it's there) */
            if (QpcMs() - m->triedAt < 3000.0 || g_holAttFailed || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) continue;
            m->triedAt = QpcMs();
            if (!g_holAttScr) {
                g_holAttScr = g_scriptIntf->CompileScript("string_var sNode\nstring_var sPath\nBegin Function { sNode, sPath }\nplayer.AttachModel $sNode $sPath 0\nEnd\n");
                if (!g_holAttScr) { g_holAttFailed = true; Log("Holsters: JIP LN's AttachModel isn't available - holstered weapons won't show"); continue; }
            }
            unsigned before = AsNode(parent) ? *(uint16_t *)(parent + 0xA6) : 0;
            char **kidsB = AsNode(parent) ? *(char ***)(parent + 0xA0) : NULL; char *old[64]; unsigned nb = before < 64 ? before : 64;
            for (unsigned i = 0; i < nb; i++) old[i] = kidsB ? kidsB[i] : NULL;
            g_scriptIntf->CallFunctionAlt(g_holAttScr, player, 2, bone, m->path);
            char **kids = AsNode(parent) ? *(char ***)(parent + 0xA0) : NULL; unsigned n = AsNode(parent) ? *(uint16_t *)(parent + 0xA6) : 0;
            for (unsigned i = 0; kids && i < n && !m->node; i++) {
                bool seen = false; for (unsigned j = 0; j < nb; j++) if (old[j] == kids[i]) seen = true;
                if (!seen && kids[i]) { m->node = kids[i]; const char *kn = *(const char **)(kids[i] + 0x08); snprintf(m->name, sizeof m->name, "%s", kn ? kn : ""); }
            }
            { static int nl = 0; if (nl++ < 8) Log("Holsters: showing %s on your %s - %s", m->path, z == 1 ? "hip" : "shoulder", m->node ? "attached" : "not attached yet"); }
            if (!m->node) continue;
        }
        /* place it: the holster spot on your body, turned by this holster's angles */
        *(uint32_t *)(m->node + 0x30) &= ~1u;
        float h = *PlayerRotZ(player), U = g_unitsPerMeter, side = g_leftHanded ? -1.0f : 1.0f;
        const float *P = (const float *)(player + 0x30);
        float eye = g_gameEyeUnits > 50.0f ? g_gameEyeUnits : 118.0f;
        float f[3] = { sinf(h), cosf(h), 0 }, r[3] = { cosf(h), -sinf(h), 0 }, up[3] = { 0, 0, 1 };
        float T[3];
        if (z == 1) for (int i = 0; i < 3; i++) T[i] = P[i] + r[i] * g_hipSide * side * U;
        else for (int i = 0; i < 3; i++) T[i] = P[i] + r[i] * g_shSide * side * U - f[i] * g_shBack * U;
        T[2] = P[2] + eye - (z == 1 ? g_hipDown : g_shDown) * U;
        M3 B = { { r[0], f[0], up[0] }, { r[1], f[1], up[1] }, { r[2], f[2], up[2] } }, Ry, Rx, Rz, T1, T2, W;
        float vz[3] = { 0, 0, g_holAng[z][0] / RAD2DEG }, vx[3] = { g_holAng[z][1] / RAD2DEG, 0, 0 }, vy[3] = { 0, g_holAng[z][2] / RAD2DEG, 0 };
        VecRot(vz, Rz); VecRot(vx, Rx); VecRot(vy, Ry);
        M3Mul(T1, Rz, Rx); M3Mul(T2, T1, Ry); M3Mul(W, B, T2);
        M3 PW, PT, L; memcpy(PW, parent + NIAV_WORLD_ROTATE, sizeof PW); M3T(PT, PW);
        float sc = *(const float *)(parent + NIAV_WORLD_SCALE); if (!(sc > 0.0001f)) sc = 1.0f;
        const float *Pt = (const float *)(parent + NIAV_WORLD_TRANSLATE);
        float d[3]; V3Sub(d, T, Pt);
        M3Mul(L, PT, W); memcpy(m->node + NIAV_LOCAL_ROTATE, L, sizeof L);
        float *Lt = (float *)(m->node + NIAV_LOCAL_TRANSLATE);
        for (int i = 0; i < 3; i++) Lt[i] = (PT[i][0] * d[0] + PT[i][1] * d[1] + PT[i][2] * d[2]) / sc;
        *(float *)(m->node + NIAV_LOCAL_SCALE) = 1.0f / sc;
        NodeUpdate(m->node);
    }
}

/* ---- Melee weapon angle in the hand, per weapon ------------------------------------------------------
   Melee weapons sit in the hand at the angle the flat game's animations give them, which can look tilted in
   VR. Three angles (tilt, turn, roll - degrees, saved per weapon model in [MeleeAngle]) turn the weapon in
   the hand; set them in the VR settings while holding the weapon - it changes as you adjust. */
static float g_melA[3] = { 0, 0, 0 }; static char g_melKey[96] = "-";
static char *g_melNode = NULL; static float g_melSaved[9];
static void MeleeAngle_Load(char *w)
{
    const char *nm = (!IsBadReadPtr(w + 0x34, 4) && *(const char **)(w + 0x34) && !IsBadReadPtr(*(const char **)(w + 0x34), 1)) ? *(const char **)(w + 0x34) : "";
    const char *mp = (!IsBadReadPtr(w + 0x40, 4) && *(const char **)(w + 0x40) && !IsBadReadPtr(*(const char **)(w + 0x40), 1)) ? *(const char **)(w + 0x40) : "";
    const char *base = mp; for (const char *c = mp; *c; c++) if (*c == '\\' || *c == '/') base = c + 1;
    snprintf(g_melKey, sizeof g_melKey, "%s", *base ? base : *nm ? nm : "unnamed");
    for (char *c = g_melKey; *c; c++) if (*c == '=' || *c == '[' || *c == ']') *c = ' ';
    char b[96], d[] = "0 0 0";
    GetPrivateProfileStringA("MeleeAngle", g_melKey, d, b, sizeof b, g_iniPath);
    g_melA[0] = g_melA[1] = g_melA[2] = 0; sscanf(b, "%f %f %f", &g_melA[0], &g_melA[1], &g_melA[2]);
    Log("Melee: '%s' (model %s) angle in the hand %.0f / %.0f / %.0f deg", nm, g_melKey, g_melA[0], g_melA[1], g_melA[2]);
}
static void MeleeAngle_Save(void)
{
    char b[96]; snprintf(b, sizeof b, "%.0f %.0f %.0f", g_melA[0], g_melA[1], g_melA[2]);
    WritePrivateProfileStringA("MeleeAngle", g_melKey, b, g_iniPath);
}
static void MeleeAngle_Apply(void)
{
    g_melNode = NULL;
    int wt = PlayerWeaponType();
    if (wt != 1 && wt != 2) return;
    char *w = (char *)PlayerWeaponForm();
    static char *forW = NULL; if (w != forW) { forW = w; if (w) MeleeAngle_Load(w); }
    if (!g_weaponNode || !w || (g_melA[0] == 0 && g_melA[1] == 0 && g_melA[2] == 0)) return;
    float *L = (float *)(g_weaponNode + NIAV_LOCAL_ROTATE);
    memcpy(g_melSaved, L, sizeof g_melSaved);
    M3 Rx, Ry, Rz, T1, T2, Lm, Out;
    float vx[3] = { g_melA[0] / RAD2DEG, 0, 0 }, vy[3] = { 0, g_melA[1] / RAD2DEG, 0 }, vz[3] = { 0, 0, g_melA[2] / RAD2DEG };
    VecRot(vx, Rx); VecRot(vy, Ry); VecRot(vz, Rz);
    M3Mul(T1, Rx, Ry); M3Mul(T2, T1, Rz);
    memcpy(Lm, L, sizeof Lm); M3Mul(Out, Lm, T2);   /* turned in the weapon's own frame */
    memcpy(L, Out, sizeof Out);
    NodeUpdate(g_weaponNode); g_melNode = g_weaponNode;
}
static void MeleeAngle_Restore(void)
{
    if (!g_melNode) return;
    memcpy(g_melNode + NIAV_LOCAL_ROTATE, g_melSaved, sizeof g_melSaved); NodeUpdate(g_melNode); g_melNode = NULL;
}

/* Box dimensions of a body (game units) from its mass and inertia. */
static bool BodyDims(char *rb, float dims[3])
{
    const float *invI = (const float *)(rb + 0x1A0);
    float invM = *(const float *)(rb + 0x1AC);
    if (!(invM > 0.0f)) return false;
    float S[3];
    for (int i = 0; i < 3; i++) { if (!(invI[i] > 0.0f)) return false; S[i] = 12.0f * invM / invI[i]; }
    for (int i = 0; i < 3; i++) {
        float d2 = (S[(i + 1) % 3] + S[(i + 2) % 3] - S[i]) * 0.5f;
        dims[i] = sqrtf(fmaxf(d2, 0.0f)) * HK_TO_GAME;
        if (!(dims[i] < 200.0f)) return false;
    }
    return true;
}

/* Palm geometry from the posed finger bones (called after the arms/fingers are posed). */
static void Palm_Update(void)
{
    for (int s = 0; s < 2; s++) {   /* wrist-to-knuckles direction for the arm (uses last frame's palm data) */
        g_fingerDirOk[s] = false;
        if (g_palm[s].ok && g_handWOk[s]) {
            for (int i = 0; i < 3; i++) g_fingerDirW[s][i] = g_handWR[s][i][0] * g_palm[s].kn[0] + g_handWR[s][i][1] * g_palm[s].kn[1] + g_handWR[s][i][2] * g_palm[s].kn[2];
            g_fingerDirOk[s] = V3Len(g_fingerDirW[s]) > 0.1f;
        }
    }
    for (int s = 0; s < 2; s++) g_palm[s].frameOk = false;
    for (int s = 0; s < 2; s++) {
        char *hand = g_bones[s][B_HAND], *kn = g_fingers[s][2][0], *tip = g_fingers[s][2][2];
        if (!hand || !kn || !tip || !g_handWOk[s]) continue;
        const float *H = (const float *)(hand + NIAV_WORLD_TRANSLATE);
        const float *K = (const float *)(kn + NIAV_WORLD_TRANSLATE);
        const float *Tp = (const float *)(tip + NIAV_WORLD_TRANSLATE);
        float fwd[3], ft[3]; V3Sub(fwd, K, H); V3Sub(ft, Tp, K);
        float fl = V3Len(fwd); if (fl < 0.5f) continue;
        for (int i = 0; i < 3; i++) fwd[i] /= fl;
        float pr = V3Dot(ft, fwd), n[3];
        for (int i = 0; i < 3; i++) n[i] = ft[i] - pr * fwd[i];
        float nl = V3Len(n);
        if (nl < 0.3f) continue;          /* fingers too straight to tell which way the palm faces */
        for (int i = 0; i < 3; i++) n[i] /= nl;
        {   /* remember the palm direction in the drawn hand's own axes (fingers curl toward the palm) */
            const float *Rh = (const float *)(hand + NIAV_WORLD_ROTATE);
            float b[3]; for (int i = 0; i < 3; i++) b[i] = Rh[0 * 3 + i] * n[0] + Rh[1 * 3 + i] * n[1] + Rh[2 * 3 + i] * n[2];
            if (nl > 0.8f || !g_palmBoneOk[s]) { memcpy(g_palmBoneN[s], b, 12); if (!g_palmBoneOk[s]) Log("Palm %s: palm direction in the hand's own axes (%.2f, %.2f, %.2f)", s ? "R" : "L", b[0], b[1], b[2]); g_palmBoneOk[s] = true; }
        }
        M3 BT; M3T(BT, g_handWR[s]);
        float k[3]; V3Sub(k, K, H);
        Palm *pm = &g_palm[s];
        for (int i = 0; i < 3; i++) {
            pm->kn[i]  = BT[i][0] * k[0] + BT[i][1] * k[1] + BT[i][2] * k[2];
            pm->fwd[i] = BT[i][0] * fwd[0] + BT[i][1] * fwd[1] + BT[i][2] * fwd[2];
            pm->n[i]   = BT[i][0] * n[0] + BT[i][1] * n[1] + BT[i][2] * n[2];
        }
        /* Grip circle: the curled index/middle/ring fingers each trace an arc; the centre of the
           circle through a finger's three joints is where a held cylinder's axis runs. */
        {
            float cs[3][3]; int nc = 0; float segSum = 0; int segN = 0;
            for (int f = 1; f <= 3; f++) {
                char *b0 = g_fingers[s][f][0], *b1 = g_fingers[s][f][1], *b2 = g_fingers[s][f][2];
                if (!b0 || !b1 || !b2) continue;
                const float *A = (const float *)(b0 + NIAV_WORLD_TRANSLATE), *B = (const float *)(b1 + NIAV_WORLD_TRANSLATE), *Cc = (const float *)(b2 + NIAV_WORLD_TRANSLATE);
                float ab[3], ac[3], nn[3]; V3Sub(ab, B, A); V3Sub(ac, Cc, A); V3Cross(nn, ab, ac);
                segSum += V3Len(ab); segN++;
                float n2 = V3Dot(nn, nn);
                if (n2 < 1e-3f) continue;
                float t1[3], t2[3]; V3Cross(t1, nn, ab); V3Cross(t2, ac, nn);
                float la = V3Dot(ac, ac), lb = V3Dot(ab, ab), ctr[3];
                for (int i = 0; i < 3; i++) ctr[i] = A[i] + (la * t1[i] + lb * t2[i]) / (2.0f * n2);
                float dk[3]; V3Sub(dk, ctr, A);
                if (V3Len(dk) > 12.0f) continue;   /* fingers nearly straight - circle too big */
                memcpy(cs[nc++], ctr, 12);
            }
            if (segN) pm->seg = segSum / segN;
            pm->gripOk = false;
            if (nc >= 2) {
                float c[3] = { 0, 0, 0 }, ax[3];
                for (int k = 0; k < nc; k++) for (int i = 0; i < 3; i++) c[i] += cs[k][i] / nc;
                V3Sub(ax, cs[nc - 1], cs[0]);
                float al = V3Len(ax);
                if (al > 0.3f) {
                    float rel2[3]; V3Sub(rel2, c, H);
                    for (int i = 0; i < 3; i++) {
                        pm->gc[i] = BT[i][0] * rel2[0] + BT[i][1] * rel2[1] + BT[i][2] * rel2[2];
                        pm->ga[i] = (BT[i][0] * ax[0] + BT[i][1] * ax[1] + BT[i][2] * ax[2]) / al;
                    }
                    pm->gripOk = true;
                    memcpy(pm->gcW, c, 12); memcpy(pm->handAt, g_handW[s], 12); pm->frameOk = true;
                }
            }
        }
        if (!pm->ok) Log("Palm %s: knuckles (%.1f, %.1f, %.1f), fingers (%.2f, %.2f, %.2f), palm faces (%.2f, %.2f, %.2f) in hand coordinates",
                         s ? "R" : "L", pm->kn[0], pm->kn[1], pm->kn[2], pm->fwd[0], pm->fwd[1], pm->fwd[2], pm->n[0], pm->n[1], pm->n[2]);
        pm->ok = true;
    }
}

/* Where an object of grip radius r sits in hand h (hand coordinates, relative to the hand). */
static void PalmHoldPoint(int h, float r, float *out)
{
    Palm *pm = &g_palm[h];
    if (!pm->ok) {   /* no bone data yet: a bit in front of the hand */
        out[0] = 0.0f; out[1] = 0.0f; out[2] = -(4.0f + r);
        return;
    }
    /* like a real grip: centred in the middle of the palm (part way from the wrist to the knuckles),
       resting against the palm - its surface touches the palm, the fingers wrap the rest */
    for (int i = 0; i < 3; i++) out[i] = pm->kn[i] * g_holdAlong + pm->n[i] * (r + g_holdGap);
}

/* ---- Thrown objects hit hard ----------------------------------------------------------------
   An object you throw is followed for a few seconds; if it flies into someone it deals damage that grows
   with how fast and heavy it is and with your Melee Weapons and Unarmed skills (both maxed, a good throw
   kills a deathclaw outright). Companions aren't hurt unless that's turned on. */
static struct { char *ref, *rb; uint32_t refID; double until; float mass; } g_thrown;
/* While held, an object doesn't bump into your own body (it used to block walking forward, so you could
   only walk sideways): its collision layer is set to "no collision" and put back when you let go. */
static uint32_t g_heldFilter[2]; static char *g_heldFilterRb[2];
/* Changing the layer number alone isn't enough: Havok keeps the old contacts (that's what lifted you into
   the air when the held object got under you). The game's own "update collision filter" call rebuilds them. */
typedef void (__attribute__((thiscall)) *WoUpdFilter_t)(void *wo);
static bool Held_SetFilter(char *rb, uint32_t fi)
{
    char *wo = RbWo_Get(rb);
    if (!wo || !Rd(rb, 0x30)) return false;
    if (*(uint32_t *)(rb + 0x2C) == fi) return true;
    *(uint32_t *)(rb + 0x2C) = fi;
    void **vt = *(void ***)wo;
    if (Rd(vt, 0xC8) && vt[0xC4 / 4]) ((WoUpdFilter_t)vt[0xC4 / 4])(wo);
    return true;
}
static void Held_NoCollide(int h, char *rb)
{
    if (!Rd(rb, 0x30) || !RbWo_Get(rb)) { static int nl = 0; if (nl++ < 3) Log("Grab: held object keeps its collision (its Havok wrapper wasn't found)"); return; }
    uint32_t fi = *(uint32_t *)(rb + 0x2C);
    if (g_heldFilterRb[h] != rb) { g_heldFilter[h] = fi; g_heldFilterRb[h] = rb; }
    Held_SetFilter(rb, (g_heldFilter[h] & ~0x7Fu) | 15u);   /* layer 15 = non-collidable */
}
static void Held_Collide(int h)
{
    char *rb = g_heldFilterRb[h];
    if (rb) Held_SetFilter(rb, g_heldFilter[h]);
    g_heldFilterRb[h] = NULL;
}
static void *g_scrThrowHit = NULL; static bool g_scrThrowFailed = false;
static void Thrown_Start(Held *g, float speed)
{
    g_lastThrowTick = GetTickCount();
    if (g_throwDmg <= 0.0f || speed < 250.0f || !g->ref || !g->rb) return;
    g_thrown.ref = g->ref; g_thrown.rb = g->rb; g_thrown.refID = g->refID; g_thrown.until = QpcMs() + 3000.0;
    float im = *(const float *)(g->rb + 0x1AC); g_thrown.mass = im > 1e-4f ? 1.0f / im : 5.0f;
}
static void Thrown_Update(char *player)
{
    if (!g_thrown.ref) return;
    if (QpcMs() > g_thrown.until || LookupForm(g_thrown.refID) != (void *)g_thrown.ref || RefBody(g_thrown.ref) != g_thrown.rb) { g_thrown.ref = NULL; return; }
    const float *lv = (const float *)(g_thrown.rb + 0x1B0);
    float vel[3] = { lv[0] * HK_TO_GAME, lv[1] * HK_TO_GAME, lv[2] * HK_TO_GAME }, speed = V3Len(vel);
    if (speed < 150.0f) { g_thrown.ref = NULL; return; }   /* landed / stopped */
    float P[3]; BodyCom(g_thrown.rb, P);
    char *cells[128]; int nCells = 0;
    char *pc = *(char **)(player + 0x40);
    if (pc) cells[nCells++] = pc;
    if (pc && !(*(BYTE *)(pc + 0x24) & 1)) {
        char *tes = *(char **)TES_PTR_ADDR; char *gca = tes ? *(char **)(tes + 0x08) : NULL;
        if (gca) { uint32_t n = *(uint32_t *)(gca + 0x0C); char **gc = *(char ***)(gca + 0x10);
            if (gc && n > 0 && n <= 11) for (uint32_t i = 0; i < n * n && nCells < 128; i++) if (gc[i] && gc[i] != pc) cells[nCells++] = gc[i]; }
    }
    for (int c = 0; c < nCells; c++) {
        for (char **node = (char **)(cells[c] + 0xAC); node; node = (char **)node[1]) {
            char *ref = node[0];
            if (!ref || ref == player) continue;
            BYTE ft = *(BYTE *)(ref + 0x04);
            if (ft != 0x3B && ft != 0x3C) continue;                      /* people and creatures */
            if (*(uint32_t *)(ref + 0x08) & (0x20 | 0x800)) continue;
            const float *ap = (const float *)(ref + 0x30);
            float dx = P[0] - ap[0], dy = P[1] - ap[1];
            if (dx * dx + dy * dy > 300.0f * 300.0f) continue;
            /* its body: the bounds of its 3D model (or a person-sized column) */
            float bc[3] = { ap[0], ap[1], ap[2] + 60.0f }, br = 45.0f;
            char *rs = *(char **)(ref + 0x64), *root = Rd(rs, 0x18) ? *(char **)(rs + 0x14) : NULL;
            if (IsNiObj(root)) { const float *b = *(const float **)(root + 0x20); if (Rd(b, 16) && b[3] > 5.0f && b[3] < 600.0f) { memcpy(bc, b, 12); br = b[3]; } }
            float d[3]; V3Sub(d, P, bc);
            if (V3Len(d) > br * 0.7f + 4.0f) continue;
            /* a hit */
            float speedF = fminf(fmaxf((speed - 150.0f) / 600.0f, 0.2f), 1.5f), massF = fminf(fmaxf(g_thrown.mass / 5.0f, 0.3f), 2.0f);
            int k = (int)(speedF * massF * g_throwDmg * 100.0f + 0.5f);
            g_thrown.ref = NULL;
            if (g_scrThrowFailed || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return;
            if (!g_scrThrowHit) {
                g_scrThrowHit = g_scriptIntf->CompileScript(
                    "ref t\nint k\nint f\nfloat s\nfloat d\nBegin Function { t, k, f }\n"
                    "if t.GetDead\nreturn\nendif\n"
                    "if f == 0\nif t.GetPlayerTeammate\nreturn\nendif\nendif\n"
                    "let s := (player.GetAV MeleeWeapons + player.GetAV Unarmed) / 200\n"
                    "if s > 1\nlet s := 1\nendif\n"
                    "let d := (8 + 1000 * s * s * s) * k / 100\n"
                    "if player.IsSneaking\nif t.GetDetected player == 0\nlet d := d * 2\nendif\nendif\n"
                    "t.DamageAV Health d\n"
                    "if t.GetDead == 0\nt.StartCombat player\nendif\n"
                    "End\n");
                if (!g_scrThrowHit) { g_scrThrowFailed = true; Log("Thrown objects: damage script didn't compile"); return; }
            }
            g_scriptIntf->CallFunctionAlt(g_scrThrowHit, player, 3, ref, k, g_throwHurtFriends ? 1 : 0);
            static int hl = 0;
            if (hl++ < 20) Log("Thrown objects: hit %08X at %.0f units/s (mass %.1f) - damage factor %d%%", *(uint32_t *)(ref + 0x0C), speed, g_thrown.mass, k);
            return;
        }
    }
}

static void Grab_Drop(int h, bool throwIt)
{
    Held *g = &g_held[h];
    if (g->rb && g_heldFilterRb[h] == g->rb) Held_Collide(h);
    if (g->ref && throwIt && g->rb) Thrown_Start(g, V3Len(g->vel) * g_throwStrength);
    if (g->ref && throwIt && g->rb) {
        float *lv = (float *)(g->rb + 0x1B0);
        for (int i = 0; i < 3; i++) lv[i] = g->vel[i] * g_throwStrength / HK_TO_GAME;
        lv[3] = 0.0f;
        ((HkActivate_t)HK_ACTIVATE_ADDR)(g->rb);
        Log("Grab: %s hand let go of %08X (speed %.0f units/s)", h ? "right" : "left", g->refID, V3Len(g->vel));
    }
    memset(g, 0, sizeof *g);
    g_holding[h] = false;
}

static int g_nearCount;
static void Grab_ClearAll(void)
{
    g_nearCount = 0;
    for (int h = 0; h < 2; h++) { if (g_held[h].ref && LookupForm(g_held[h].refID) == (void *)g_held[h].ref) Held_Collide(h); else g_heldFilterRb[h] = NULL;
        memset(&g_held[h], 0, sizeof g_held[h]); g_holding[h] = false; g_grabCand[h] = NULL; }
}

/* Find the nearest loose object to each free hand. */
/* Loose physics objects near the hands (refreshed every few frames; checked each frame). */
#define NEAR_MAX 96
typedef struct { char *ref, *rb; uint32_t refID; } NearObj;
static NearObj g_near[NEAR_MAX];
static int     g_nearCount = 0;

static void Near_Scan(char *player, float range)
{
    g_nearCount = 0;
    if (!g_handWOk[0] && !g_handWOk[1]) return;
    char *cells[128]; int nCells = 0;
    char *pc = *(char **)(player + 0x40);
    if (pc) cells[nCells++] = pc;
    if (pc && !(*(BYTE *)(pc + 0x24) & 1)) {   /* exterior: the loaded grid around the player */
        char *tes = *(char **)TES_PTR_ADDR;
        char *gca = tes ? *(char **)(tes + 0x08) : NULL;
        if (gca) {
            uint32_t n = *(uint32_t *)(gca + 0x0C);
            char **gc = *(char ***)(gca + 0x10);
            if (gc && n > 0 && n <= 11)
                for (uint32_t i = 0; i < n * n && nCells < 128; i++) if (gc[i] && gc[i] != pc) cells[nCells++] = gc[i];
        }
    }
    char *gameGrabbed = *(char **)(player + 0x638);
    for (int c = 0; c < nCells && g_nearCount < NEAR_MAX; c++) {
        for (char **node = (char **)(cells[c] + 0xAC); node && g_nearCount < NEAR_MAX; node = (char **)node[1]) {
            char *ref = node[0];
            if (!ref || ref == player || ref == gameGrabbed) continue;
            if (*(BYTE *)(ref + 0x04) != 0x3A) continue;                  /* plain object reference (not actors) */
            if (*(uint32_t *)(ref + 0x08) & (0x20 | 0x800)) continue;     /* deleted / disabled */
            const float *rp = (const float *)(ref + 0x30);
            bool close = false;
            for (int h = 0; h < 2; h++) if (g_handWOk[h]) {
                float d[3]; V3Sub(d, rp, g_handW[h]);
                if (V3Dot(d, d) < range * range) close = true;
            }
            if (!close) continue;
            char *rb = RefBody(ref);
            if (!rb) continue;
            g_near[g_nearCount].ref = ref; g_near[g_nearCount].rb = rb; g_near[g_nearCount].refID = *(uint32_t *)(ref + 0x0C);
            g_nearCount++;
        }
    }
}

static bool Near_Valid(NearObj *o)
{
    return LookupForm(o->refID) == (void *)o->ref && !(*(uint32_t *)(o->ref + 0x08) & (0x20 | 0x800)) && RefBody(o->ref) == o->rb;
}

static bool Near_ValidRef(char *ref)
{
    for (int i = 0; i < g_nearCount; i++) if (g_near[i].ref == ref) return Near_Valid(&g_near[i]);
    return false;
}

static void Grab_FindCandidates(void)
{
    float best[2] = { 1e9f, 1e9f }, nearest[2] = { 1e9f, 1e9f };
    char *cand[2] = { NULL, NULL };
    for (int i = 0; i < g_nearCount; i++) {
        char *rb = g_near[i].rb;
        float inv = *(const float *)(rb + 0x1AC);
        if (!(inv > 0.0f) || 1.0f / inv > g_grabMaxMass) continue;
        float com[3]; BodyCom(rb, com);
        float rad = BodyRadius(rb);
        for (int h = 0; h < 2; h++) if (g_handWOk[h] && !g_holding[h]) {
            float d[3]; V3Sub(d, com, g_handW[h]);
            float score = V3Len(d) - rad;
            if (score < nearest[h]) nearest[h] = score;
            if (score < g_grabReach && score < best[h]) { best[h] = score; cand[h] = g_near[i].ref; }
        }
    }
    g_grabCand[0] = cand[0]; g_grabCand[1] = cand[1];
    g_grabNearest[0] = nearest[0]; g_grabNearest[1] = nearest[1];
}

/* ---- Hand / weapon collision ---------------------------------------------
   Each hand (and the weapon it holds) is a few small spheres. When one moves into a loose object,
   the object gets the push a solid hand would give it, at the contact point, so it slides, tips
   over or spins. Objects are treated as boxes sized from their physics shape. */
#define PROBES_MAX 14
typedef struct { float p[3], r; } Probe;
static Probe g_probePrev[2][PROBES_MAX];
static int   g_probePrevN[2] = { 0, 0 };
static bool  g_loggedWeaponLen = false;

static int Collide_BuildProbes(int h, bool weaponHand, Probe *out)
{
    int n = 0;
    const float *T = g_handW[h];
    float fwd[3] = { -g_handWR[h][0][2], -g_handWR[h][1][2], -g_handWR[h][2][2] };
    /* wrist and palm/fingers */
    out[n].r = 3.0f; memcpy(out[n].p, T, 12); n++;
    float palm[3];
    if (g_palm[h].ok) {
        for (int i = 0; i < 3; i++) palm[i] = T[i] + g_handWR[h][i][0] * g_palm[h].kn[0] + g_handWR[h][i][1] * g_palm[h].kn[1] + g_handWR[h][i][2] * g_palm[h].kn[2];
    } else for (int i = 0; i < 3; i++) palm[i] = T[i] + fwd[i] * 5.0f;
    out[n].r = 3.0f; memcpy(out[n].p, palm, 12); n++;
    for (int i = 0; i < 3; i++) out[n].p[i] = palm[i] + (palm[i] - T[i]) * 0.6f;
    out[n].r = 2.5f; n++;
    if (weaponHand && g_weaponNode) {
        const float *bound = *(const float **)(g_weaponNode + 0x20);
        float rad = bound ? bound[3] : 20.0f;
        float len = fminf(fmaxf(rad * 1.6f, 12.0f), 80.0f);
        if (!g_loggedWeaponLen) { g_loggedWeaponLen = true; Log("Collision: weapon reaches %.0f units past the hand (bound radius %.1f)", len, rad); }
        int segs = (int)(len / 6.0f); if (segs > PROBES_MAX - n) segs = PROBES_MAX - n;
        for (int k = 1; k <= segs; k++) {
            float t = len * (float)k / (float)segs;
            for (int i = 0; i < 3; i++) out[n].p[i] = T[i] + fwd[i] * t;
            out[n].r = 2.5f; n++;
        }
    }
    return n;
}

static void Collide_Update(float dt, bool weaponOut)
{
    if (!g_handCollision) { g_probePrevN[0] = g_probePrevN[1] = 0; return; }
    int aimH = g_leftHanded ? 0 : 1;
    int wt = PlayerWeaponType();
    for (int h = 0; h < 2; h++) {
        if (!g_handWOk[h] || g_holding[h]) { g_probePrevN[h] = 0; continue; }
        Probe pr[PROBES_MAX];
        int n = Collide_BuildProbes(h, weaponOut && wt >= 0 && h == aimH, pr);
        float vel[PROBES_MAX][3];
        bool haveVel = g_probePrevN[h] == n;
        for (int k = 0; k < n; k++) {
            for (int i = 0; i < 3; i++) vel[k][i] = haveVel ? (pr[k].p[i] - g_probePrev[h][k].p[i]) / dt : 0.0f;
            if (V3Len(vel[k]) > 1500.0f) { vel[k][0] = vel[k][1] = vel[k][2] = 0.0f; }   /* teleport / load */
        }
        memcpy(g_probePrev[h], pr, sizeof(Probe) * n); g_probePrevN[h] = n;

        for (int o = 0; o < g_nearCount; o++) {
            NearObj *no = &g_near[o];
            if (no->ref == g_held[0].ref || no->ref == g_held[1].ref) continue;
            char *rb = no->rb;
            float dims[3];
            if (!BodyDims(rb, dims)) { float r = BodyRadius(rb); dims[0] = dims[1] = dims[2] = r * 1.4f; }
            float half[3] = { dims[0] * 0.5f, dims[1] * 0.5f, dims[2] * 0.5f };
            float C[3]; M3 R; BodyCom(rb, C); BodyRot(rb, R);
            /* quick reject */
            float d0[3]; V3Sub(d0, C, pr[0].p);
            float reach = V3Len(half) + 90.0f;
            if (V3Dot(d0, d0) > reach * reach) continue;
            bool validated = false;
            for (int k = 0; k < n; k++) {
                float rel[3]; V3Sub(rel, pr[k].p, C);
                float loc[3], cl[3];
                for (int i = 0; i < 3; i++) loc[i] = R[0][i] * rel[0] + R[1][i] * rel[1] + R[2][i] * rel[2];
                for (int i = 0; i < 3; i++) cl[i] = fminf(fmaxf(loc[i], -half[i]), half[i]);
                float dl[3] = { loc[0] - cl[0], loc[1] - cl[1], loc[2] - cl[2] };
                float dist = V3Len(dl);
                if (dist >= pr[k].r) continue;
                /* contact: outward normal (object -> probe) in world */
                float nl[3];
                if (dist > 1e-4f) { for (int i = 0; i < 3; i++) nl[i] = dl[i] / dist; }
                else {   /* centre inside the box: push out along the nearest face */
                    int ax = 0; float bestGap = 1e9f;
                    for (int i = 0; i < 3; i++) { float gap = half[i] - fabsf(loc[i]); if (gap < bestGap) { bestGap = gap; ax = i; } }
                    nl[0] = nl[1] = nl[2] = 0.0f; nl[ax] = loc[ax] >= 0 ? 1.0f : -1.0f;
                    dist = -bestGap;
                }
                float nw[3], cw[3];
                for (int i = 0; i < 3; i++) {
                    nw[i] = R[i][0] * nl[0] + R[i][1] * nl[1] + R[i][2] * nl[2];
                    cw[i] = C[i] + R[i][0] * cl[0] + R[i][1] * cl[1] + R[i][2] * cl[2];
                }
                if (!validated) { if (!Near_Valid(no)) break; validated = true; }
                float depth = pr[k].r - dist;
                /* velocity of the object at the contact point (game units/s) */
                float *lv = (float *)(rb + 0x1B0), *av = (float *)(rb + 0x1C0);
                float r[3]; V3Sub(r, cw, C);
                float wxr[3]; V3Cross(wxr, av, r);
                float vo[3]; for (int i = 0; i < 3; i++) vo[i] = lv[i] * HK_TO_GAME + wxr[i] * 1.0f;
                float vrel = V3Dot(vel[k], nw) - V3Dot(vo, nw);   /* < 0: probe moving into the object */
                float push = 0.0f;
                if (vrel < 0) push = -vrel * 1.2f;                 /* hit (a little bounce) */
                push += depth * 8.0f;                             /* resting contact: ease it out of the hand */
                push *= g_collisionStrength;
                if (push < 1.0f) continue;
                if (push > 1000.0f) push = 1000.0f;
                /* impulse J (along -n, into the object) applied at the contact point */
                float invM = *(const float *)(rb + 0x1AC);
                const float *invI = (const float *)(rb + 0x1A0);
                float dv = push / HK_TO_GAME;                       /* Havok units/s */
                float J[3] = { -nw[0] * dv, -nw[1] * dv, -nw[2] * dv };   /* per unit mass: J/m */
                float rh[3] = { r[0] / HK_TO_GAME, r[1] / HK_TO_GAME, r[2] / HK_TO_GAME };
                float tq[3]; V3Cross(tq, rh, J);                    /* (r x J) / m */
                float tl[3], wl[3], dw[3];
                for (int i = 0; i < 3; i++) tl[i] = R[0][i] * tq[0] + R[1][i] * tq[1] + R[2][i] * tq[2];
                for (int i = 0; i < 3; i++) wl[i] = (invM > 0.0f ? invI[i] / invM : 0.0f) * tl[i];   /* I^-1 (r x J), J scaled by m */
                for (int i = 0; i < 3; i++) dw[i] = R[i][0] * wl[0] + R[i][1] * wl[1] + R[i][2] * wl[2];
                for (int i = 0; i < 3; i++) { lv[i] += J[i]; av[i] += dw[i]; }
                float wlen = V3Len(av); if (wlen > 40.0f) for (int i = 0; i < 3; i++) av[i] *= 40.0f / wlen;
                lv[3] = av[3] = 0.0f;
                ((HkActivate_t)HK_ACTIVATE_ADDR)(rb);
                static int hitLogs = 0;
                if (hitLogs < 15 && vrel < -30.0f) {
                    hitLogs++;
                    Log("Collision: %s %s hit %08X at %.0f units/s", h ? "right" : "left", k >= 3 ? "weapon" : "hand", no->refID, -vrel);
                }
                break;   /* one contact per object per hand per frame */
            }
        }
    }
}

static void Grab_Start(int h, char *ref)
{
    char *rb = RefBody(ref);
    if (!rb) return;
    int other = 1 - h;
    if (g_held[other].ref == ref) { Grab_Drop(other, false); g_grabBlock[other] = true; }   /* passing it from one hand to the other */
    Held *g = &g_held[h];
    memset(g, 0, sizeof *g);
    g->ref = ref; g->rb = rb; g->refID = *(uint32_t *)(ref + 0x0C);

    /* Size where the hand closes on it: the object's thinnest dimension. */
    float dims[3]; bool haveDims = BodyDims(rb, dims);
    int lo = 0, hi = 0;
    if (haveDims) for (int i = 1; i < 3; i++) { if (dims[i] < dims[lo]) lo = i; if (dims[i] > dims[hi]) hi = i; }
    g->gripR = haveDims ? fminf(fmaxf(dims[lo] * 0.5f, 0.8f), 10.0f) : fminf(BodyRadius(rb) * 0.5f, 8.0f);
    PalmHoldPoint(h, g->gripR, g->offLocal);

    /* Keep the orientation it had relative to the hand, but straighten long things:
       guns point where the hand points, bottles/sticks lie across the fingers. */
    M3 Ro, HT; BodyRot(rb, Ro); M3T(HT, g_handWR[h]);
    const char *how = "as grabbed";
    char *base = *(char **)(ref + 0x20);
    bool isWeapon = base && *(BYTE *)(base + 0x04) == 0x28;
    if (haveDims && hi != lo) {
        int mid = 3 - hi - lo;
        float a[3] = { Ro[0][hi], Ro[1][hi], Ro[2][hi] }, d[3] = { 0, 0, 0 };
        bool align = false;
        if (isWeapon) {
            for (int i = 0; i < 3; i++) d[i] = -g_handWR[h][i][2];   /* hand's pointing direction */
            align = true; how = "pointing forward";
        } else if (dims[hi] > dims[mid] * 1.6f) {
            g->elong = true; g->longAx = hi;
        }
        if (!isWeapon && g->elong && g_palm[h].ok) {
            float gl[3]; V3Cross(gl, g_palm[h].fwd, g_palm[h].n);  /* across the fingers */
            for (int i = 0; i < 3; i++) d[i] = g_handWR[h][i][0] * gl[0] + g_handWR[h][i][1] * gl[1] + g_handWR[h][i][2] * gl[2];
            align = true; how = "across the palm";
        }
        if (align) {
            if (V3Dot(a, d) < 0) for (int i = 0; i < 3; i++) d[i] = -d[i];
            M3 C, Rn; RotBetween(C, a, d); M3Mul(Rn, C, Ro); memcpy(Ro, Rn, sizeof Ro);
        }
    }
    M3Mul(g->rotRel, HT, Ro);

    /* Fingers close until they meet the object. */
    {   /* bend each finger joint just enough for the finger to lie around a cylinder of radius gripR */
        /* a real finger bone is about 2.5 units and the finger itself has thickness: wrap around the object's
           surface, not into a closed fist */
        float L = fminf(g_palm[h].seg > 0.5f ? g_palm[h].seg : 2.5f, 2.6f);
        float th = 2.0f * atanf(L / (2.0f * (g->gripR + 0.75f))) * RAD2DEG;
        g->curl = fminf(fmaxf(th / 66.0f * g_holdCurl, 0.25f), 0.9f);
    }
    g_holding[h] = true;
    Held_NoCollide(h, rb);
    if (g_savePhotos && g_grecN < 3 && !g_grecStart) { g_grecN++; g_grecFrame = 0; g_grecHand = h; g_grecStart = QpcMs(); }
    Log("Grab: %s hand picked up %08X%s (mass %.1f, size %.1f x %.1f x %.1f units, grip radius %.1f, fingers %.0f%%, held %s)",
        h ? "right" : "left", g->refID, isWeapon ? " [weapon]" : "", 1.0f / *(const float *)(rb + 0x1AC),
        haveDims ? dims[0] : -1.0f, haveDims ? dims[1] : -1.0f, haveDims ? dims[2] : -1.0f, g->gripR, g->curl * 100.0f, how);
}

/* Where the held object should be turned to: its grab-relative rotation, with long objects
   lined up with the curled fingers' axis (so a bottle sits inside the fist). */
static void HeldTargetRot(int h, M3 Rt)
{
    Held *g = &g_held[h];
    M3Mul(Rt, g_handWR[h], g->rotRel);
    if (!g->elong || !g_palm[h].ok) return;
    float a[3] = { Rt[0][g->longAx], Rt[1][g->longAx], Rt[2][g->longAx] }, d[3], ga[3];
    V3Cross(ga, g_palm[h].fwd, g_palm[h].n);   /* across the palm (thumb side to little finger) */
    for (int i = 0; i < 3; i++) d[i] = g_handWR[h][i][0] * ga[0] + g_handWR[h][i][1] * ga[1] + g_handWR[h][i][2] * ga[2];
    if (V3Dot(a, d) < 0) for (int i = 0; i < 3; i++) d[i] = -d[i];
    M3 C, Rn; RotBetween(C, a, d); M3Mul(Rn, C, Rt); memcpy(Rt, Rn, sizeof Rn);
}

static void Grab_Hold(int h, float dt)
{
    Held *g = &g_held[h];
    /* Still the same loaded object? */
    void *f = LookupForm(g->refID);
    if (f != (void *)g->ref || (*(uint32_t *)(g->ref + 0x08) & (0x20 | 0x800)) || RefBody(g->ref) != g->rb) {
        Log("Grab: held object %08X is gone - releasing", g->refID);
        Grab_Drop(h, false);
        return;
    }
    const float *H = g_handW[h];
    float T[3]; M3 Rt;
    PalmHoldPoint(h, g->gripR, g->offLocal);   /* follows the palm as the fingers move */
    {
        static int fitLogs = 0;
        if (fitLogs < 12 && g->blend > 0.5f && g->blend < 0.7f) {
            fitLogs++;
            Palm *pm = &g_palm[h];
            Log("Grip fit %s: fingers %s, centre (%.1f, %.1f, %.1f), axis (%.2f, %.2f, %.2f), finger segment %.1f, curl %.2f",
                h ? "R" : "L", pm->gripOk ? "wrapped" : "NOT wrapped", pm->gc[0], pm->gc[1], pm->gc[2], pm->ga[0], pm->ga[1], pm->ga[2], pm->seg, g->curl);
        }
    }
    for (int i = 0; i < 3; i++) T[i] = H[i] + g_handWR[h][i][0] * g->offLocal[0] + g_handWR[h][i][1] * g->offLocal[1] + g_handWR[h][i][2] * g->offLocal[2];
    HeldTargetRot(h, Rt);

    float vff[3] = { 0, 0, 0 }, wff[3] = { 0, 0, 0 };
    if (g->havePrev) {
        M3 PT, dR; M3T(PT, g->prevR); M3Mul(dR, Rt, PT);
        RotVec(dR, wff);
        for (int i = 0; i < 3; i++) { vff[i] = (T[i] - g->prevT[i]) / dt; wff[i] /= dt; }
    }
    memcpy(g->prevT, T, sizeof T); memcpy(g->prevR, Rt, sizeof Rt); g->havePrev = true;
    for (int i = 0; i < 3; i++) g->vel[i] = g->vel[i] * 0.5f + vff[i] * 0.5f;

    float P[3], err[3]; M3 Rc, RcT, E;
    BodyCom(g->rb, P); BodyRot(g->rb, Rc);
    {   /* aim the object's middle at the palm: shift the target by middle - weight point */
        char *rs = *(char **)(g->ref + 0x64), *root = rs ? *(char **)(rs + 0x14) : NULL;
        const float *wb = root ? *(const float **)(root + 0x20) : NULL;
        if (wb && !IsBadReadPtr(wb, 16) && wb[3] > 0.1f && wb[3] < 200.0f) {
            float dc[3]; V3Sub(dc, wb, P);
            if (V3Len(dc) < 40.0f) for (int i = 0; i < 3; i++) T[i] -= dc[i];
        }
    }
    V3Sub(err, T, P);
    M3T(RcT, Rc); M3Mul(E, Rt, RcT);
    float rerr[3]; RotVec(E, rerr);

    float dist = V3Len(err);
    g->farTime = dist > 60.0f ? g->farTime + dt : 0.0f;
    if (g->farTime > 0.75f) { Log("Grab: held object stuck %.0f units from the hand - releasing", dist); Grab_Drop(h, false); return; }

    const float kp = 25.0f, ka = 22.0f;
    float v[3], w[3];
    for (int i = 0; i < 3; i++) { v[i] = (vff[i] + err[i] * kp) / HK_TO_GAME; w[i] = wff[i] + rerr[i] * ka; }
    float vl = V3Len(v), wl = V3Len(w);
    if (vl > 120.0f) for (int i = 0; i < 3; i++) v[i] *= 120.0f / vl;
    if (wl > 50.0f)  for (int i = 0; i < 3; i++) w[i] *= 50.0f / wl;
    float *lv = (float *)(g->rb + 0x1B0), *av = (float *)(g->rb + 0x1C0);
    for (int i = 0; i < 3; i++) { lv[i] = v[i]; av[i] = w[i]; }
    lv[3] = av[3] = 0.0f;
    ((HkActivate_t)HK_ACTIVATE_ADDR)(g->rb);

    /* fingers close around it */
    g->blend = fminf(g->blend + dt / 0.12f, 1.0f);
    g_fingerCurl[h][0] = g->curl * 0.85f;
    g_fingerCurl[h][1] = g->curl;
    g_fingerCurl[h][2] = g->curl;
}

/* Draw held objects exactly in the hand (the physics follows a frame or two behind). */
static void Grab_Visual(void)
{
    for (int h = 0; h < 2; h++) {
        Held *g = &g_held[h];
        g->visNode = NULL;
        if (!g_holding[h] || !g->ref || !g_handWOk[h]) continue;
        char *rs = *(char **)(g->ref + 0x64);
        char *root = rs ? *(char **)(rs + 0x14) : NULL;
        if (!root) continue;
        float off[3], T[3]; M3 Rt;
        PalmHoldPoint(h, g->gripR, off);   /* this frame's palm */
        for (int i = 0; i < 3; i++) T[i] = g_handW[h][i] + g_handWR[h][i][0] * off[0] + g_handWR[h][i][1] * off[1] + g_handWR[h][i][2] * off[2];
        if (g_bones[h][B_HAND]) {   /* relative to the drawn hand, not the controller */
            const float *Hb = (const float *)(g_bones[h][B_HAND] + NIAV_WORLD_TRANSLATE);
            for (int i = 0; i < 3; i++) T[i] += Hb[i] - g_handW[h][i];
        }
        HeldTargetRot(h, Rt);
        float Pc[3]; M3 Rc, RcT, E, D;
        BodyCom(g->rb, Pc); BodyRot(g->rb, Rc);
        {   /* hold its middle: the centre of what you see, not its weight point (a bottle's is near the base) */
            const float *wb = *(const float **)(root + 0x20);
            if (wb && !IsBadReadPtr(wb, 16) && wb[3] > 0.1f && wb[3] < 200.0f) {
                float dc[3]; V3Sub(dc, wb, Pc);
                if (V3Len(dc) < 40.0f) for (int i = 0; i < 3; i++) Pc[i] = wb[i];
            }
        }
        M3T(RcT, Rc); M3Mul(E, Rt, RcT);
        float rv[3]; RotVec(E, rv);
        for (int i = 0; i < 3; i++) { rv[i] *= g->blend; T[i] = Pc[i] + (T[i] - Pc[i]) * g->blend; }
        VecRot(rv, D);   /* world-space change that takes the body from where it is to the hand */

        float *Lr = (float *)(root + NIAV_LOCAL_ROTATE), *Lt = (float *)(root + NIAV_LOCAL_TRANSLATE);
        memcpy(g->visSaved, Lr, 36); memcpy(g->visSaved + 9, Lt, 12);
        M3 Wr, Wn; memcpy(Wr, root + NIAV_WORLD_ROTATE, sizeof Wr);
        const float *Wt = (const float *)(root + NIAV_WORLD_TRANSLATE);
        float rel[3], Wtn[3]; V3Sub(rel, Wt, Pc);
        M3Mul(Wn, D, Wr);
        for (int i = 0; i < 3; i++) Wtn[i] = T[i] + D[i][0] * rel[0] + D[i][1] * rel[1] + D[i][2] * rel[2];
        char *parent = *(char **)(root + NIAV_PARENT);
        if (parent) {
            M3 P, PT, L; memcpy(P, parent + NIAV_WORLD_ROTATE, sizeof P); M3T(PT, P);
            float sc = *(const float *)(parent + NIAV_WORLD_SCALE); if (!(sc > 0.0001f)) sc = 1.0f;
            const float *Pt = (const float *)(parent + NIAV_WORLD_TRANSLATE);
            float d[3]; V3Sub(d, Wtn, Pt);
            M3Mul(L, PT, Wn); memcpy(Lr, L, 36);
            for (int i = 0; i < 3; i++) Lt[i] = (PT[i][0] * d[0] + PT[i][1] * d[1] + PT[i][2] * d[2]) / sc;
        } else { memcpy(Lr, Wn, 36); memcpy(Lt, Wtn, 12); }
        NodeUpdate(root);
        g->visNode = root;
    }
}

static void Grab_VisualRestore(void)
{
    for (int h = 0; h < 2; h++) {
        Held *g = &g_held[h];
        if (!g->visNode) continue;
        memcpy(g->visNode + NIAV_LOCAL_ROTATE, g->visSaved, 36);
        memcpy(g->visNode + NIAV_LOCAL_TRANSLATE, g->visSaved + 9, 12);
        NodeUpdate(g->visNode);
        g->visNode = NULL;
    }
}

/* ---- Working scopes: where the scope is and what it looks at (during drawing) ---- */
static char *FindNodeContaining(char *node, const char *part, int depth)
{
    if (!node || depth > 12) return NULL;
    const char *n = *(const char **)(node + 0x08);
    if (n) {
        char low[128]; size_t i = 0;
        for (; n[i] && i < sizeof low - 1; i++) low[i] = (char)tolower((unsigned char)n[i]);
        low[i] = 0;
        if (strstr(low, part) && !strstr(low, "##")) {
            const float *b = *(const float **)(node + 0x20);
            if (b && b[3] > 1.0f) return node;
        }
    }
    if (!AsNode(node)) return NULL;
    char **kids = *(char ***)(node + 0xA0);
    unsigned count = *(uint16_t *)(node + 0xA6);
    for (unsigned i = 0; kids && i < count; i++) { char *r = FindNodeContaining(kids[i], part, depth + 1); if (r) return r; }
    return NULL;
}
static char *g_sightNode = NULL, *g_sightFor = NULL;
static void Scope_Forget(void) { g_sightNode = NULL; g_sightFor = NULL; }   /* the gun model was swapped: its scope part is gone */
/* ---- Scope camera: a real second view rendered into the gun's own scope lens ----
   Uses JIP LN's extra-camera functions (the same ones New Vegas True Scopes uses): a camera is
   attached to the first-person weapon, FNVR points it down the barrel from the lens every frame,
   and the view is rendered straight into the ##ScopeLens texture - so the gun stays in your hands
   and the picture is truly magnified, not an enlarged screenshot. */
static void *g_scrAttach = NULL;
static char g_lensGeomName[64] = "##ScopeLens:0";
static char *g_lensCam = NULL, *g_lensCamFor = NULL;
static bool  g_lensCamFailed = false;
typedef struct { int key; void *script; } ProjScript;
static ProjScript g_projScripts[64]; static int g_nProj = 0;

static void *ProjectScript(float fov, int px)
{
    int key = (int)(fov * 2.0f + 0.5f) * 4096 + px;
    for (int i = 0; i < g_nProj; i++) if (g_projScripts[i].key == key) return g_projScripts[i].script;
    char src[256];
    /* the lens picture lives on the geometry inside the ##ScopeLens node ("##ScopeLens:0") */
    snprintf(src, sizeof src, "Begin Function { }\nplayer.ProjectExtraCamera \"FNVRScope\" \"%s\" %.1f %d 0\nEnd\n", g_lensGeomName, (int)(fov * 2.0f + 0.5f) / 2.0f, px);
    void *scr = g_scriptIntf->CompileScript(src);
    if (!scr) { Log("Scope camera: could not compile the projection script"); return NULL; }
    if (g_nProj < 64) { g_projScripts[g_nProj].key = key; g_projScripts[g_nProj].script = scr; g_nProj++; }
    return scr;
}

static void Scope_LensRender(char *player, const float *E, const float *f, const float *upHint, float fov, int px)
{
    if (!g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt || g_lensCamFailed) return;
    if (g_lensCamFor != g_weaponNode || !g_lensCam) {
        if (!g_scrAttach) {
            g_scrAttach = g_scriptIntf->CompileScript("Begin Function { }\nplayer.AttachExtraCamera \"FNVRScope\" 1 \"Weapon\" 2\nEnd\n");
            if (!g_scrAttach) { Log("Scope camera: JIP LN's AttachExtraCamera isn't available - scope camera off"); g_lensCamFailed = true; return; }
        }
        g_scriptIntf->CallFunctionAlt(g_scrAttach, player, 0);
        g_lensCam = FindBone(g_weaponNode, "FNVRScope", 0);
        g_lensCamFor = g_weaponNode;
        Log("Scope camera: %s", g_lensCam ? "attached to the weapon" : "could not attach");
        if (!g_lensCam) return;
    }
    /* camera axes: look down the barrel (col 0), up (col 1), right (col 2) */
    float u[3], r[3], pd = V3Dot(upHint, f);
    for (int i = 0; i < 3; i++) u[i] = upHint[i] - pd * f[i];
    float ul = V3Len(u); if (ul < 1e-3f) return;
    for (int i = 0; i < 3; i++) u[i] /= ul;
    V3Cross(r, f, u);
    M3 W = { { f[0], u[0], r[0] }, { f[1], u[1], r[1] }, { f[2], u[2], r[2] } };
    float pos[3]; for (int i = 0; i < 3; i++) pos[i] = E[i] + f[i] * 1.5f;   /* just in front of the lens */
    char *parent = *(char **)(g_lensCam + NIAV_PARENT);
    if (!parent) return;
    M3 P, PT, L; memcpy(P, parent + NIAV_WORLD_ROTATE, sizeof P); M3T(PT, P);
    float sc = *(const float *)(parent + NIAV_WORLD_SCALE); if (!(sc > 0.0001f)) sc = 1.0f;
    const float *Pt = (const float *)(parent + NIAV_WORLD_TRANSLATE);
    float d[3]; V3Sub(d, pos, Pt);
    M3Mul(L, PT, W);
    memcpy(g_lensCam + NIAV_LOCAL_ROTATE, L, sizeof L);
    float *Lt = (float *)(g_lensCam + NIAV_LOCAL_TRANSLATE);
    for (int i = 0; i < 3; i++) Lt[i] = (PT[i][0] * d[0] + PT[i][1] * d[1] + PT[i][2] * d[2]) / sc;
    NodeUpdate(g_lensCam);
    void *scr = ProjectScript(fov, px);
    bool ok = scr && g_scriptIntf->CallFunctionAlt(scr, player, 0);
    static int logs = 0;
    if (logs++ < 4) Log("Scope camera: %dpx view at %.1f deg into \"%s\" (%s)", px, fov, g_lensGeomName, ok ? "ran" : "FAILED");
}

/* The highest drawn part of the gun model (seen along the gun's "up"): on a scoped rifle that's the scope.
   Gives the rear end of it along the barrel - where the eyepiece is. */
static void TopPart_Walk(char *node, const float *up, const float *W, int depth, float *bestH, const float **best)
{
    if (!node || depth > 10 || IsBadReadPtr(node, 0xB0)) return;
    const float *b = *(const float **)(node + 0x20);
    char **kids = AsNode(node) ? *(char ***)(node + 0xA0) : NULL;
    unsigned count = kids ? *(uint16_t *)(node + 0xA6) : 0;
    if (!kids || !count) {   /* a drawn piece */
        if (b && !IsBadReadPtr(b, 16) && b[3] > 0.3f && b[3] < 25.0f) {
            float d[3] = { b[0] - W[0], b[1] - W[1], b[2] - W[2] };
            float h = d[0] * up[0] + d[1] * up[1] + d[2] * up[2];
            if (h > *bestH) { *bestH = h; *best = b; }
        }
        return;
    }
    for (unsigned i = 0; i < count && i < 64; i++) TopPart_Walk(kids[i], up, W, depth + 1, bestH, best);
}
static bool ScopeTopPart(char *weapNode, const float *up, const float *f, float *E, float *tubeR)
{
    if (!weapNode) return false;
    const float *W = (const float *)(weapNode + NIAV_WORLD_TRANSLATE);
    float bestH = 1.0f; const float *best = NULL;
    TopPart_Walk(weapNode, up, W, 0, &bestH, &best);
    if (!best) return false;
    float R = best[3];
    for (int i = 0; i < 3; i++) E[i] = best[i] - f[i] * R * 0.80f;
    *tubeR = fminf(fmaxf(R * 0.16f, 1.0f), 2.6f);
    static int nl = 0; if (nl++ < 6) Log("Scope: no lens part in the model - using its highest part (%.1f units above the grip, %.1f across)", bestH, R);
    return true;
}
/* Scope picture placement, per gun (saved by the gun's name): height in cm along the gun's "up" ([ScopeHeightCm],
   + = higher) and size ([ScopeSize], 1 = the lens as the model has it). */
static float g_scopeOffY = 0.0f, g_scopeSize = 1.0f; static char g_scopeKey[96] = "-";
static void ScopeHeight_Load(char *weap)
{
    const char *nm = (!IsBadReadPtr(weap + 0x34, 4) && *(const char **)(weap + 0x34) && !IsBadReadPtr(*(const char **)(weap + 0x34), 1)) ? *(const char **)(weap + 0x34) : "";
    /* keyed by the gun's model file, so guns that are the same model (Gobi Campaign Scout Rifle = Sniper Rifle) share it */
    const char *mp = (!IsBadReadPtr(weap + 0x40, 4) && *(const char **)(weap + 0x40) && !IsBadReadPtr(*(const char **)(weap + 0x40), 1)) ? *(const char **)(weap + 0x40) : "";
    const char *base = mp; for (const char *c = mp; *c; c++) if (*c == '\\' || *c == '/') base = c + 1;
    if (*base && strlen(base) > 4) snprintf(g_scopeKey, sizeof g_scopeKey, "%s", base);
    else snprintf(g_scopeKey, sizeof g_scopeKey, "%s", *nm ? nm : "unnamed");
    for (char *c = g_scopeKey; *c; c++) if (*c == '=' || *c == '[' || *c == ']') *c = ' ';
    float def = 0.0f;
    char low[96]; snprintf(low, sizeof low, "%s", g_scopeKey); for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    char lowN[96]; snprintf(lowN, sizeof lowN, "%s", nm); for (char *c = lowN; *c; c++) *c = (char)tolower((unsigned char)*c);
    if (strstr(lowN, "marksman") || strstr(lowN, "bozar") || strstr(lowN, "anti-materi") || strstr(lowN, "anti materi") || strstr(low, "amr") || strstr(low, "marksman") || strstr(low, "bozar")) def = 11.0f;
    char d[32], b[48]; snprintf(d, sizeof d, "%.2f", def);
    GetPrivateProfileStringA("ScopeHeightCm", g_scopeKey, d, b, sizeof b, g_iniPath); g_scopeOffY = (float)atof(b);
    GetPrivateProfileStringA("ScopeSize", g_scopeKey, "1.0", b, sizeof b, g_iniPath); g_scopeSize = (float)atof(b);
    if (!(g_scopeSize >= 0.2f && g_scopeSize <= 5.0f)) g_scopeSize = 1.0f;
    Log("Scope: '%s' (model %s) picture %+.1f cm high, size %.2f", nm, g_scopeKey, g_scopeOffY, g_scopeSize);
}
static void Scope_Prepare(char *player, char *cam)
{
    g_scope.active = false; g_capWant = false; g_scopeSighting = false;
    { static bool lastOut = false; if (g_weaponOutThisFrame != lastOut) { lastOut = g_weaponOutThisFrame; Scope_Forget(); } }
    if (!g_scopesEnabled || !g_weaponOutThisFrame || !g_weaponNode || !g_bbW || g_adsActive) return;
    char *process = *(char **)(player + 0x68);
    if (!process) return;
    char *entry = (char *)((GetWeaponInfo_t)(*(void ***)process)[0x148 / 4])(process);
    char *weap = entry ? *(char **)(entry + 0x08) : NULL;
    if (!weap || *(BYTE *)(weap + 0xF4) < 3 || !(*(BYTE *)(weap + 0x100) & 4)) return;   /* gun with a scope */

    /* the scope's eyepiece: the model's sighting node if it has one */
    static unsigned searchTick = 0;
    if (g_sightFor != g_weaponNode || (!g_sightNode && (searchTick++ % 60) == 0)) {
        g_sightFor = g_weaponNode;
        g_sightNode = FindBone(g_weaponNode, "##ScopeLens", 0);
        if (!g_sightNode) g_sightNode = FindNodeContaining(g_weaponNode, "scope", 0);
        if (g_sightNode && AsNode(g_sightNode)) {   /* the drawable lens is the node's first child */
            char **kids = *(char ***)(g_sightNode + 0xA0);
            unsigned count = *(uint16_t *)(g_sightNode + 0xA6);
            for (unsigned i = 0; kids && i < count; i++) {
                const char *kn = kids[i] ? *(const char **)(kids[i] + 0x08) : NULL;
                if (kn && *kn && strlen(kn) < sizeof g_lensGeomName) {
                    if (strcmp(g_lensGeomName, kn)) { strcpy(g_lensGeomName, kn); g_nProj = 0; }
                    break;
                }
            }
        }
        static int partLogs = 0;
        if (partLogs++ < 4) {
            char line[1600] = ""; LogBoneTree(g_weaponNode, 0, line, sizeof line);
            Log("Scope: weapon model parts: %s", line);
            Log("Scope: scope part %s", g_sightNode ? *(const char **)(g_sightNode + 0x08) : "not found (using an estimate)");
        }
    }
    int aim = g_leftHanded ? 0 : 1;
    if (!g_handWOk[aim]) return;
    float h = g_handAimHeading + g_shotYawTrimDeg / RAD2DEG, pt = g_handAimPitch + g_shotPitchTrimDeg / RAD2DEG;
    float f[3] = { sinf(h) * cosf(pt), cosf(h) * cosf(pt), -sinf(pt) };       /* where shots go */
    float up[3] = { g_handWR[aim][0][1], g_handWR[aim][1][1], g_handWR[aim][2][1] };
    float E[3], tubeR = 1.7f;
    const float *bnd = g_sightNode ? *(const float **)(g_sightNode + 0x20) : NULL;
    bool isLens = g_sightNode && !_stricmp(*(const char **)(g_sightNode + 0x08), "##ScopeLens");
    if (bnd && isLens && bnd[3] > 0.2f && bnd[3] < 6.0f) {   /* the model's own scope lens */
        memcpy(E, bnd, 12);
        tubeR = bnd[3];
    } else if (ScopeTopPart(g_weaponNode, up, f, E, &tubeR)) {   /* no lens in the model: the gun's highest part (where a scope sits) */
    } else if (bnd && bnd[3] > 1.0f && bnd[3] < 40.0f) {   /* rear end of the scope part along the barrel */
        float R = bnd[3];
        for (int i = 0; i < 3; i++) E[i] = bnd[i] - f[i] * R * 0.80f;
        tubeR = fminf(fmaxf(R * 0.16f, 1.0f), 2.6f);
    } else { const float *W = (const float *)(g_weaponNode + NIAV_WORLD_TRANSLATE); for (int i = 0; i < 3; i++) E[i] = W[i] + up[i] * 5.0f + f[i] * 4.0f; }

    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    float d[3]; V3Sub(d, E, C);
    float dist = V3Len(d);
    /* only when you're really sighting: eye close behind the lens, looking straight down the barrel */
    static bool sighting = false;
    float alignNeed = sighting ? 0.970f : 0.985f, distMax = sighting ? 40.0f : 32.0f;
    sighting = dist > 0.5f && dist < distMax && V3Dot(d, f) / dist > alignNeed;
    static int sightLogs = 0;
    static bool wasSighting = false;
    if (sighting != wasSighting && sightLogs < 12) { sightLogs++; Log("Scope: %s (eye %.1f units from the lens, %.1f deg off the barrel)", sighting ? "sighting through the scope" : "stopped sighting", dist, acosf(fminf(V3Dot(d, f) / fmaxf(dist, 0.01f), 1.0f)) * RAD2DEG); }
    if (sighting && !wasSighting) {
        static int episodes = 0;
        if (episodes++ < 2) g_capLogClears = 24;      /* log what the game clears while drawing (diagnostics) */
        if (g_capSnaps < 1) { g_capSnaps++; g_capSnapWanted = true; }
    }
    wasSighting = sighting;
    g_scopeSighting = sighting; memcpy(g_scopeEye, C, 12);
    if (!sighting) return;
    /* camera axes (head-driven camera, no roll) and the image's tangent range */
    M3 B; GameBasis(B, *PlayerRotZ(player), -*PlayerRotX(player));
    float R[3] = { B[0][0], B[1][0], B[2][0] }, U[3] = { B[0][1], B[1][1], B[2][1] }, F[3] = { -B[0][2], -B[1][2], -B[2][2] };
    {   /* use the camera's real orientation (what the picture was actually drawn with) */
        const float *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);   /* columns: view, up, right */
        float cF[3] = { Cw[0], Cw[3], Cw[6] }, cU[3] = { Cw[1], Cw[4], Cw[7] }, cR[3] = { Cw[2], Cw[5], Cw[8] };
        if (fabsf(V3Len(cF) - 1.0f) < 0.05f && fabsf(V3Len(cR) - 1.0f) < 0.05f) {
            static int bl = 0;
            if (bl++ < 3) Log("Scope: camera vs head basis: view %.2f deg apart, right %.2f deg apart",
                              acosf(fminf(V3Dot(cF, F), 1.0f)) * RAD2DEG, acosf(fminf(V3Dot(cR, R), 1.0f)) * RAD2DEG);
            memcpy(F, cF, 12); memcpy(U, cU, 12); memcpy(R, cR, 12);
        }
    }
    const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
    float l = fr[0], r = fr[1], t = fr[2], b = fr[3];
    if (!(r > l) || !(t > b)) return;
    #define PROJ(P, OX, OY, OK) do { float q[3]; V3Sub(q, P, C); float z = V3Dot(q, F); OK = z > 0.1f; \
        if (OK) { OX = (V3Dot(q, R) / z - l) / (r - l) * g_bbW; OY = (t - V3Dot(q, U) / z) / (t - b) * g_bbH; } } while (0)
    bool ok1, ok2;
    float cx = 0, cy = 0, sx = 0, sy = 0;
    {   static char *forW = NULL; if (forW != weap) { forW = weap; ScopeHeight_Load(weap); } }
    float Eo[3]; { float k = g_scopeOffY * g_unitsPerMeter / 100.0f; for (int i = 0; i < 3; i++) Eo[i] = E[i] + up[i] * k; }   /* this gun's picture height */
    PROJ(Eo, cx, cy, ok1);
    float aimPt[3]; for (int i = 0; i < 3; i++) aimPt[i] = C[i] + f[i] * 20000.0f;
    PROJ(aimPt, sx, sy, ok2);
    #undef PROJ
    if (!ok1 || !ok2) return;
    float z = V3Dot(d, F);
    float pxPerTan = g_bbW / (r - l);
    float rad = tubeR / fmaxf(z, 1.0f) * pxPerTan;           /* the scope tube's opening */
    rad = fminf(rad, g_bbH * 0.30f * fmaxf(g_scopeSize, 1.0f));
    if (rad < 12.0f) return;
    float sfov = *(const float *)(weap + 0x110);
    float zoom = (sfov > 1.0f && sfov < 80.0f) ? tanf(37.5f / RAD2DEG) / tanf(sfov * 0.5f / RAD2DEG) : 4.0f;
    zoom = fminf(fmaxf(zoom * 0.6f, 1.5f), g_scopeMaxZoom);
    rad *= g_scopeSize;   /* this gun's picture size */
    g_scope = (ScopeView){ true, cx, cy, rad, sx, sy, zoom };
    g_capSX = sx; g_capSY = sy; g_capWant = true;   /* grab the clean world picture while the frame is drawn */
}

/* Main thread, once per drawn frame (gameplay, no menu). */

/* ---- Climbing ---------------------------------------------------------------------------
   Grip with an empty hand while it touches a wall, rock, ledge or the ground and that hand holds on:
   your body moves so the hand stays where it grabbed, so pulling your hand down lifts you up. Both
   hands can hold on; with a gun in your right hand the left hand still climbs and hangs. Letting go
   drops you with the speed you were moving at (so you can pull yourself over a ledge). */
typedef struct __attribute__((aligned(16))) {
    float pos0[4], pos1[4];
    uint8_t b20, pad21[3];
    uint32_t filter;        /* 24: layer (low bits) + the player's collision group, so the ray skips you */
    uint32_t unk28[6];
    float frac;             /* 40: how far along the ray the hit is (1 = nothing hit) */
    uint32_t unk44[15];
    void *cdBody;           /* 80 */
    uint32_t unk84[3];
    float v90[4];
    uint32_t unkA0[3];
    uint8_t bAC, padAD[3];
} RayCastData;
#define HAVOK_UNIT 6.9991f
typedef char *(__attribute__((thiscall)) *TESPick_t)(void *tes, RayCastData *rc, int unk);

/* Cast a ray through the game's physics world (the same call JIP LN's GetRayCastPos uses). Returns the
   object hit (or NULL) and how far along the segment it was. */
static char *RayCast(const float *from, const float *to, float *fracOut)
{
    static RayCastData rc;
    char *tes = *(char **)TES_PTR_ADDR;
    char *player = (char *)GetPlayer();
    if (!tes || !player) return NULL;
    memset(&rc, 0, sizeof rc);
    for (int i = 0; i < 3; i++) { rc.pos0[i] = from[i] / HAVOK_UNIT; rc.pos1[i] = to[i] / HAVOK_UNIT; }
    rc.frac = 1.0f;
    rc.unk44[0] = 0xFFFFFFFF;            /* 44 */
    rc.unk44[3] = 0xFFFFFFFF;            /* 50 */
    uint32_t filt = 0;
    char *proc = *(char **)(player + 0x68);
    char *ctrl = proc ? *(char **)(proc + 0x138) : NULL;
    char *ph = ctrl ? *(char **)(ctrl + 0x594) : NULL;
    char *hph = ph ? *(char **)(ph + 0x08) : NULL;
    if (hph) filt = *(uint32_t *)(hph + 0x2C);
    rc.filter = (filt & 0xFFFF0000) | 6;   /* layer 6: what a bullet would hit */
    char *hit = ((TESPick_t)0x00458440)(tes, &rc, 1);
    if (fracOut) *fracOut = rc.frac;
    return hit;
}

/* The reference that owns a hit 3D object (NULL for terrain). */
static char *HitRef(char *obj)
{
    for (int i = 0; obj && i < 64; i++) {
        if (*(uintptr_t *)obj == 0x010A8F90) { char *r = *(char **)(obj + 0xCC); if (r) return r; }   /* BSFadeNode */
        obj = *(char **)(obj + 0x18);
    }
    return NULL;
}

/* Something you can hold on to: not a person/creature and not a loose object (those get grabbed). */
static bool Climbable(char *obj)
{
    if (!obj) return false;
    char *ref = HitRef(obj);
    if (!ref) return true;                                                  /* terrain */
    if ((*(uintptr_t **)ref)[0x100 / 4] == 0x008D0360) return false;        /* an actor */
    char *rb = RefBody(ref);
    if (rb) { uint8_t mt = *(uint8_t *)(rb + 0xE8); if (mt == 1 || mt == 2 || mt == 3 || mt == 6) return false; }   /* moves freely */
    return true;
}

/* Is this hand touching (or reaching into) something to hold on to? */
static bool Climb_Touch(char *cam, int h)
{
    const float *H = g_handW[h];
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE);
    float d[3]; V3Sub(d, H, C);
    float L = V3Len(d);
    if (L > 1.0f) {   /* eye to hand (and a little past): the hand is at or inside a surface */
        float to[3]; for (int i = 0; i < 3; i++) to[i] = H[i] + d[i] / L * 5.0f;
        float fr; char *o = RayCast(C, to, &fr);
        if (o && fr < 1.0f && Climbable(o)) return true;
    }
    static const float dirs[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    for (int k = 0; k < 6; k++) {   /* short feelers around the hand */
        float to[3]; for (int i = 0; i < 3; i++) to[i] = H[i] + dirs[k][i] * 5.0f;
        float fr; char *o = RayCast(H, to, &fr);
        if (o && fr < 1.0f && Climbable(o)) return true;
    }
    return false;
}

static void *g_scrSetPos = NULL; static bool g_scrSetPosFailed = false;
/* Moving the player (climbing, grappling) is only asked for here; the move itself runs from the game's main loop
   (Move_Flush), never in the middle of drawing a frame - a move that crosses into another area loads and unloads
   parts of the world, which mid-frame can crash the game. */
static float g_moveWant[3]; static bool g_moveHave = false; static char *g_movePlayer = NULL;
static bool Move_Run(char *player, const float *pos);
static bool Climb_MovePlayer(char *player, const float *pos)
{
    if (!g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt || g_scrSetPosFailed) return false;
    memcpy(g_moveWant, pos, 12); g_movePlayer = player; g_moveHave = true;
    return true;
}
static float g_actMoveWant[3]; static char *g_actMoveRef = NULL; static uint32_t g_actMoveID = 0; static bool g_actMoveHave = false;
static char *g_actPushRef = NULL; static uint32_t g_actPushID = 0;   /* knocked down when it arrives */
static void ActorMove_Flush(void)
{   /* (main loop) the grappled person moved toward you / knocked down on arrival */
    static void *mv = NULL, *push = NULL; static bool tried = false;
    if (!tried && g_scriptIntf && g_scriptIntf->CompileScript) { tried = true;
        mv = g_scriptIntf->CompileScript("ref rWho\nfloat x\nfloat y\nfloat z\nBegin Function { rWho, x, y, z }\nrWho.SetPosEx x y z\nEnd\n");
        push = g_scriptIntf->CompileScript("ref rWho\nBegin Function { rWho }\nplayer.PushActorAway rWho 3\nEnd\n");
        if (!mv) Log("Grapple: couldn't set up pulling people (SetPosEx missing?)"); }
    char *pl = (char *)GetPlayer();
    bool loading = ((const volatile bool *)0x011F308F)[1007];
    if (g_actMoveHave) {
        g_actMoveHave = false;
        if (mv && !loading && g_actMoveRef && LookupForm(g_actMoveID) == (void *)g_actMoveRef) {
            uint32_t a, b, c; memcpy(&a, &g_actMoveWant[0], 4); memcpy(&b, &g_actMoveWant[1], 4); memcpy(&c, &g_actMoveWant[2], 4);
            g_scriptIntf->CallFunctionAlt(mv, pl, 4, g_actMoveRef, a, b, c);
        }
    }
    if (g_actPushRef) {
        char *r = g_actPushRef; g_actPushRef = NULL;
        if (push && !loading && LookupForm(g_actPushID) == (void *)r) { g_scriptIntf->CallFunctionAlt(push, pl, 1, r); Log("Grapple: knocked them down"); }
    }
}
static void Move_Flush(void)
{
    ActorMove_Flush();
    if (!g_moveHave) return;
    g_moveHave = false;
    char *pl = *(char **)PLAYER_PTR_ADDR;
    if (!g_inGameplay || !pl || pl != g_movePlayer || ((const volatile bool *)0x011F308F)[1007]) return;   /* (not across a load) */
    if (!Move_Run(pl, g_moveWant)) { g_climb[0] = g_climb[1] = false; }
}
static bool Move_Run(char *player, const float *pos)
{
    if (!g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt || g_scrSetPosFailed) return false;
    if (!g_scrSetPos) {
        g_scrSetPos = g_scriptIntf->CompileScript("float x\nfloat y\nfloat z\nBegin Function { x, y, z }\nplayer.SetPosEx x y z\nEnd\n");
        if (!g_scrSetPos) { g_scrSetPosFailed = true; Log("Climbing: couldn't set up moving the player (JIP LN's SetPosEx missing?) - climbing off"); return false; }
    }
    uint32_t a, b, c; memcpy(&a, &pos[0], 4); memcpy(&b, &pos[1], 4); memcpy(&c, &pos[2], 4);
    return g_scriptIntf->CallFunctionAlt(g_scrSetPos, player, 3, a, b, c);
}

static float g_climbAnchor[2][3], g_climbVel[3];
static bool  g_climbing = false;
static float g_climbGravSave = 1.0f;

/* ---- Jump momentum: a running jump keeps its speed through the air and into the landing ---------- */
#define g_momentumOn g_momentumOnIni
static float g_momMinSpeed = 110.0f, g_momLandTime = 0.45f;
static void Momentum_Update(char *player, float dt)
{
    if (!g_momentumOn || !player || dt <= 0.0f || dt > 0.1f || g_climbing) return;
    char *proc = *(char **)(player + 0x68);
    char *ctrl = (proc && *(uint8_t *)(proc + 0x28) <= 1) ? *(char **)(proc + 0x138) : NULL;
    char *proxy = ctrl ? *(char **)(ctrl + 0x08) : NULL;
    if (!ctrl || !proxy) return;
    const float *P = (const float *)(player + 0x30);
    static float prev[2], vGround[2] = { 0, 0 }; static bool havePrev = false;
    static bool air = false; static float take[2] = { 0, 0 }, takeSpeed = 0, landT = -1;
    float vx = 0, vy = 0;
    if (havePrev) { vx = (P[0] - prev[0]) / dt; vy = (P[1] - prev[1]) / dt; }
    prev[0] = P[0]; prev[1] = P[1]; havePrev = true;
    if (vx * vx + vy * vy > 4000.0f * 4000.0f) { vGround[0] = vGround[1] = 0; air = false; landT = -1; return; }   /* teleport/load */
    bool inAir = *(float *)(ctrl + 0x548) > 0.08f;   /* falling time */
    if (!inAir) {
        if (air) {   /* just landed: carry the speed on for a moment */
            air = false; landT = 0;
            static int ll = 0; if (takeSpeed > 0 && ll++ < 10) Log("Jump momentum: landed at %.0f units/s - carrying it on", takeSpeed);
        }
        if (landT < 0) { vGround[0] += (vx - vGround[0]) * 0.35f; vGround[1] += (vy - vGround[1]) * 0.35f; }
    } else if (!air) {   /* just left the ground */
        air = true; landT = -1;
        float sp = sqrtf(vGround[0] * vGround[0] + vGround[1] * vGround[1]);
        if (sp > g_momMinSpeed) { takeSpeed = fminf(sp, 1200.0f); take[0] = vGround[0] / sp; take[1] = vGround[1] / sp;
            static int tl = 0; if (tl++ < 10) Log("Jump momentum: took off at %.0f units/s", takeSpeed); }
        else { takeSpeed = 0; static int sl = 0; if (sl++ < 10) Log("Jump momentum: took off at %.0f units/s (too slow to carry)", sp); }
    }
    float want = 0;
    if (air && takeSpeed > 0) want = takeSpeed;
    else if (landT >= 0 && takeSpeed > 0) {
        landT += dt;
        if (landT >= g_momLandTime) { landT = -1; takeSpeed = 0; vGround[0] = vx; vGround[1] = vy; }
        else want = takeSpeed * (1.0f - landT / g_momLandTime);
    }
    if (want <= 0) return;
    float *pv = (float *)(proxy + 0x10);   /* physics velocity (havok units) */
    float cx = pv[0] * HAVOK_UNIT, cy = pv[1] * HAVOK_UNIT;
    float along = cx * take[0] + cy * take[1];
    if (along < want) {   /* top it up along the jump direction (your steering still adds on) */
        float add = want - along;
        pv[0] = (cx + take[0] * add) / HAVOK_UNIT; pv[1] = (cy + take[1] * add) / HAVOK_UNIT;
        float *cv = (float *)(ctrl + 0x4F0);
        cv[0] = pv[0]; cv[1] = pv[1];
    }
}
static bool Scr_Num(void *scr, void *arg, double *out);
/* Sitting (a chair, the vigor tester in Doc Mitchell's house): the grip must stay the "use" button that stands you up -
   never a climb on the chair or the wall beside it. */
static bool Player_Sitting(char *player)
{
    static void *scr = NULL; static bool failed = false;
    if (failed || !g_scriptIntf || !g_scriptIntf->CompileScript) return false;
    if (!scr) { scr = g_scriptIntf->CompileScript("ref rWho\nfloat fVal\nBegin Function { rWho }\nset fVal to rWho.GetSitting\nSetFunctionValue fVal\nEnd\n");
        if (!scr) { failed = true; Log("Climbing: couldn't set up the sitting check"); return false; } }
    double v = 0; return Scr_Num(scr, player, &v) && v != 0.0;
}
static void Climb_Update(char *player, char *cam, float dt, bool noWeapon)
{
    int amH = g_leftHanded ? 0 : 1, mvH = 1 - amH;
    char *proc = *(char **)(player + 0x68);
    char *ctrl = (proc && *(uint8_t *)(proc + 0x28) <= 1) ? *(char **)(proc + 0x138) : NULL;
    for (int h = 0; h < 2; h++) {
        bool canUse = g_climbEnabled && g_handWOk[h] && !g_holding[h] && !g_grabWant[h] && (h == mvH || noWeapon) && !(h == mvH && g_twoHandWant);
        if (g_climb[h] && (!g_gripHeld[h] || !canUse)) { g_climb[h] = false; Log("Climbing: %s hand lets go", h ? "right" : "left"); }
        if (g_gripEdge[h]) {
            g_gripEdge[h] = false;
            bool sitting = !g_climb[h] && g_gripHeld[h] && canUse && Player_Sitting(player);
            if (sitting) { static int sl = 0; if (sl++ < 5) Log("Climbing: you're sitting - the grip stands you up instead"); }
            if (!g_climb[h] && g_gripHeld[h] && canUse && !sitting && cam && ctrl && Climb_Touch(cam, h)) {
                g_climb[h] = true;
                memcpy(g_climbAnchor[h], g_handW[h], 12);
                static int cl = 0;
                if (cl++ < 40) { char *pc = *(char **)(player + 0x40);
                    Log("Climbing: %s hand holds on at (%.0f, %.0f, %.0f) - player at (%.0f, %.0f, %.0f), location %08X", h ? "right" : "left", g_handW[h][0], g_handW[h][1], g_handW[h][2],
                        ((float *)(player + 0x30))[0], ((float *)(player + 0x30))[1], ((float *)(player + 0x30))[2], pc ? *(uint32_t *)(pc + 0x0C) : 0u); }
            }
        }
    }
    bool active = g_climb[0] || g_climb[1];
    float *P = (float *)(player + 0x30);
    if (active && ctrl) {
        float d[3] = { 0, 0, 0 }; int n = 0;
        for (int h = 0; h < 2; h++) if (g_climb[h]) { for (int i = 0; i < 3; i++) d[i] += g_climbAnchor[h][i] - g_handW[h][i]; n++; }
        for (int i = 0; i < 3; i++) d[i] /= n;
        float dl = V3Len(d);
        if (dl > 30.0f) for (int i = 0; i < 3; i++) d[i] *= 30.0f / dl;      /* never jump far in one frame */
        float np[3] = { P[0] + d[0], P[1] + d[1], P[2] + d[2] };
        if (!g_climbing) {
            g_climbing = true; g_climbGravSave = *(float *)(ctrl + 0x54C);
            if (!(g_climbGravSave > 0.0f)) g_climbGravSave = 1.0f;
            memset(g_climbVel, 0, sizeof g_climbVel);
        }
        if (dl > 0.01f && !Climb_MovePlayer(player, np)) { g_climb[0] = g_climb[1] = false; }
        static int ml = 0;
        if (ml++ < 3) Log("Climbing: moved %.1f units (player now at %.0f, %.0f, %.0f; wanted %.0f, %.0f, %.0f)", dl, P[0], P[1], P[2], np[0], np[1], np[2]);
        for (int i = 0; i < 3; i++) g_climbVel[i] += (d[i] / dt - g_climbVel[i]) * 0.3f;
        /* hang still: no gravity, no falling speed, no fall damage building up */
        *(float *)(ctrl + 0x54C) = 0.0f;
        *(float *)(ctrl + 0x544) = P[2];
        *(float *)(ctrl + 0x548) = 0.0f;
        memset(ctrl + 0x4F0, 0, 16);
        char *proxy = *(char **)(ctrl + 0x08);
        if (proxy) memset(proxy + 0x10, 0, 16);
    } else if (g_climbing) {
        g_climbing = false;
        if (ctrl) {
            *(float *)(ctrl + 0x54C) = g_climbGravSave;
            *(float *)(ctrl + 0x544) = P[2];
            *(float *)(ctrl + 0x548) = 0.0f;
            float v = V3Len(g_climbVel), vmax = 500.0f;
            if (v > vmax) for (int i = 0; i < 3; i++) g_climbVel[i] *= vmax / v;
            char *proxy = *(char **)(ctrl + 0x08);
            if (proxy) for (int i = 0; i < 3; i++) ((float *)(proxy + 0x10))[i] = g_climbVel[i] / HAVOK_UNIT;
        }
        Log("Climbing: let go, moving at %.0f units/s", V3Len(g_climbVel));
    }
}


/* ---- The grapple's rope: a thin dark cable from your left hand to the hook point, drawn over the picture.
   Worked out at the same moment the frame's camera is final, drawn at Present. */
static struct { bool on; float x[2], y[2], w[2]; double at; } g_rope;
static bool Rope_Project(char *cam, const float *A, const float *B)
{
    g_rope.on = false;
    if (!cam || !g_bbW) return false;
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float F[3] = { Cw[0], Cw[3], Cw[6] }, Up[3] = { Cw[1], Cw[4], Cw[7] }, Rt[3] = { Cw[2], Cw[5], Cw[8] };
    const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
    float l = fr[0], r = fr[1], t = fr[2], b = fr[3];
    if (!(r > l) || !(t > b)) return false;
    float qa[3], qb[3]; V3Sub(qa, A, C); V3Sub(qb, B, C);
    float za = V3Dot(qa, F), zb = V3Dot(qb, F), zn = 2.0f;
    if (za < zn && zb < zn) return false;
    if (za < zn || zb < zn) {   /* clip the part behind your eyes */
        float k = (zn - za) / (zb - za);
        float m[3]; for (int i = 0; i < 3; i++) m[i] = qa[i] + (qb[i] - qa[i]) * k;
        if (za < zn) { memcpy(qa, m, 12); za = zn; } else { memcpy(qb, m, 12); zb = zn; }
    }
    float *q[2] = { qa, qb }; float z[2] = { za, zb };
    float pxPerUnitAt1 = g_bbW / (r - l);   /* pixels per unit of sideways size at distance 1 */
    float thick = 0.45f;                     /* cable radius in game units (~6 mm) */
    for (int e = 0; e < 2; e++) {
        g_rope.x[e] = (V3Dot(q[e], Rt) / z[e] - l) / (r - l) * g_bbW;
        g_rope.y[e] = (t - V3Dot(q[e], Up) / z[e]) / (t - b) * g_bbH;
        g_rope.w[e] = fmaxf(thick * pxPerUnitAt1 / z[e], 1.2f);
    }
    g_rope.on = true; g_rope.at = QpcMs();
    return true;
}
static void Rope_Draw(IDirect3DDevice9 *dev)
{
    if (!g_rope.on || QpcMs() - g_rope.at > 100.0) { g_rope.on = false; return; }
    g_rope.on = false;
    float dx = g_rope.x[1] - g_rope.x[0], dy = g_rope.y[1] - g_rope.y[0], L = sqrtf(dx * dx + dy * dy);
    if (L < 1.0f || !g_bbW) return;
    IDirect3DSurface9 *bb = NULL;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    static const D3DRENDERSTATETYPE rsList[] = { D3DRS_ZENABLE, D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE,
        D3DRS_STENCILENABLE, D3DRS_SCISSORTESTENABLE, D3DRS_CULLMODE, D3DRS_LIGHTING, D3DRS_FOGENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_COLORWRITEENABLE,
        D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_BLENDOP, D3DRS_SEPARATEALPHABLENDENABLE };
    enum { NRS = sizeof rsList / sizeof rsList[0] };
    DWORD rsSave[NRS];
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_GetRenderState(dev, rsList[i], &rsSave[i]);
    DWORD t0c, t0a1, t0a, t0aa1, t1c;
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLOROP, &t0c);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_COLORARG1, &t0a1);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAOP, &t0a);
    IDirect3DDevice9_GetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, &t0aa1);
    IDirect3DDevice9_GetTextureStageState(dev, 1, D3DTSS_COLOROP, &t1c);
    IDirect3DSurface9 *rtSave = NULL; IDirect3DDevice9_GetRenderTarget(dev, 0, &rtSave);
    D3DVIEWPORT9 vpSave; IDirect3DDevice9_GetViewport(dev, &vpSave);
    IDirect3DVertexShader9 *vsSave = NULL; IDirect3DDevice9_GetVertexShader(dev, &vsSave);
    IDirect3DPixelShader9 *psSave = NULL; IDirect3DDevice9_GetPixelShader(dev, &psSave);
    IDirect3DVertexDeclaration9 *declSave = NULL; IDirect3DDevice9_GetVertexDeclaration(dev, &declSave);
    DWORD fvfSave = 0; IDirect3DDevice9_GetFVF(dev, &fvfSave);
    IDirect3DBaseTexture9 *texSave = NULL; IDirect3DDevice9_GetTexture(dev, 0, &texSave);
    IDirect3DVertexBuffer9 *vbSave = NULL; UINT vbOff = 0, vbStride = 0; IDirect3DDevice9_GetStreamSource(dev, 0, &vbSave, &vbOff, &vbStride);

    IDirect3DDevice9_SetRenderTarget(dev, 0, bb);
    D3DVIEWPORT9 vp = { 0, 0, g_bbW, g_bbH, 0.0f, 1.0f }; IDirect3DDevice9_SetViewport(dev, &vp);
    IDirect3DDevice9_SetVertexShader(dev, NULL); IDirect3DDevice9_SetPixelShader(dev, NULL);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, TRUE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_BLENDOP, D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SCISSORTESTENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_SRGBWRITEENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_COLORWRITEENABLE, 0x7);
    IDirect3DDevice9_SetTexture(dev, 0, NULL);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    if (SUCCEEDED(IDirect3DDevice9_BeginScene(dev))) {
        float nx = -dy / L, ny = dx / L;
        /* outer dark cable, then a lighter strand down the middle so it reads as a round rope */
        static const struct { float k; DWORD c; } layer[2] = { { 1.0f, 0xF0262019 }, { 0.38f, 0xC06E6250 } };
        for (int li = 0; li < 2; li++) {
            float w0 = g_rope.w[0] * layer[li].k, w1 = g_rope.w[1] * layer[li].k; DWORD c = layer[li].c;
            VtxC v[6] = {
                { g_rope.x[0] - nx * w0, g_rope.y[0] - ny * w0, 0, 1, c }, { g_rope.x[1] - nx * w1, g_rope.y[1] - ny * w1, 0, 1, c },
                { g_rope.x[1] + nx * w1, g_rope.y[1] + ny * w1, 0, 1, c }, { g_rope.x[0] - nx * w0, g_rope.y[0] - ny * w0, 0, 1, c },
                { g_rope.x[1] + nx * w1, g_rope.y[1] + ny * w1, 0, 1, c }, { g_rope.x[0] + nx * w0, g_rope.y[0] + ny * w0, 0, 1, c } };
            IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, 2, v, sizeof(VtxC));
        }
        IDirect3DDevice9_EndScene(dev);
    }
    for (int i = 0; i < NRS; i++) IDirect3DDevice9_SetRenderState(dev, rsList[i], rsSave[i]);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP, t0c);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, t0a1);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, t0a);
    IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, t0aa1);
    IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, t1c);
    if (rtSave) { IDirect3DDevice9_SetRenderTarget(dev, 0, rtSave); IDirect3DSurface9_Release(rtSave); }
    IDirect3DDevice9_SetViewport(dev, &vpSave);
    IDirect3DDevice9_SetVertexShader(dev, vsSave); if (vsSave) IDirect3DVertexShader9_Release(vsSave);
    IDirect3DDevice9_SetPixelShader(dev, psSave); if (psSave) IDirect3DPixelShader9_Release(psSave);
    if (declSave) { IDirect3DDevice9_SetVertexDeclaration(dev, declSave); IDirect3DVertexDeclaration9_Release(declSave); }
    else if (fvfSave) IDirect3DDevice9_SetFVF(dev, fvfSave);
    IDirect3DDevice9_SetTexture(dev, 0, texSave); if (texSave) IDirect3DBaseTexture9_Release(texSave);
    IDirect3DDevice9_SetStreamSource(dev, 0, vbSave, vbOff, vbStride); if (vbSave) IDirect3DVertexBuffer9_Release(vbSave);
    IDirect3DSurface9_Release(bb);
}

/* ---- Grappling hook (left hand, like Dying Light's) ----------------------------------------------------
   Make a fist with the left hand (grip) and pull the left trigger: a line shoots straight out where the hand
   points and pulls you to that spot, speeding up as you go. Let go of the trigger (or grip) on the way and
   you fly on with the speed you had - a launch. Reaching the spot gives a little hop up over the edge. */
static struct {
    bool  pull, fling, combo[2]; int mode, hand;          /* mode 0: physics velocity, 1: stepped moves (if physics won't move you) */
    float target[3], dir[3], vel[3], speed, t, stuckT, gravSave, lastP[3], flingH[2], flingT;
    double coolUntil;
    float missEnd[3]; double missAt;      /* a shot that hit nothing: the line flies out and snaps back */
} g_grap;
static struct { bool on; char *ref; uint32_t id; int hand; float speed, t; } g_yank;   /* a person on the line, being pulled to you */
static bool Grapple_Busy(void) { return g_grap.pull || g_grap.fling; }
static void Grap_Buzz(int h, unsigned short us) { if (g_vrSystem && g_handIdx[h] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, us); }
static void Grap_SetVel(char *ctrl, const float *v, bool withZ)
{
    char *proxy = *(char **)(ctrl + 0x08);
    if (!proxy) return;
    float *pv = (float *)(proxy + 0x10), *cv = (float *)(ctrl + 0x4F0);
    for (int i = 0; i < (withZ ? 3 : 2); i++) { pv[i] = v[i] / HAVOK_UNIT; cv[i] = pv[i]; }
}
static void Grap_End(char *player, char *ctrl, bool launch, const char *why)
{
    float *P = (float *)(player + 0x30);
    if (ctrl) {
        *(float *)(ctrl + 0x54C) = g_grap.gravSave > 0 ? g_grap.gravSave : 1.0f;
        *(float *)(ctrl + 0x544) = P[2];
        *(float *)(ctrl + 0x548) = 0.0f;
    }
    float v[3]; memcpy(v, g_grap.vel, sizeof v);
    bool arrived = !strcmp(why, "arrived");
    float keep = arrived ? 0.3f : launch ? g_grapLaunch : 0.15f;
    for (int i = 0; i < 3; i++) v[i] *= keep;
    float vmax = 60.0f * g_unitsPerMeter, L = V3Len(v);
    if (L > vmax) for (int i = 0; i < 3; i++) v[i] *= vmax / L;
    if (arrived) v[2] = fmaxf(v[2], g_grapHop * g_unitsPerMeter);   /* hop up over the edge */
    if (ctrl) Grap_SetVel(ctrl, v, true);
    g_grap.pull = false;
    float hs = sqrtf(v[0] * v[0] + v[1] * v[1]);
    g_grap.fling = hs > 1.5f * g_unitsPerMeter || v[2] > 2.0f * g_unitsPerMeter;
    g_grap.flingH[0] = v[0]; g_grap.flingH[1] = v[1]; g_grap.flingT = 0; memcpy(g_grap.lastP, P, 12);
    g_grap.coolUntil = QpcMs() + 250.0;
    Log("Grapple: %s - flying on at %.1f m/s (up %.1f m/s)", why, V3Len(v) / g_unitsPerMeter, v[2] / g_unitsPerMeter);
}
static bool Grap_HandFree(int h)
{
    if (!g_grapHand[h] || !g_handWOk[h] || g_holding[h] || g_climb[h]) return false;
    int ah = g_leftHanded ? 0 : 1;
    if (h == ah && g_weaponOutThisFrame && PlayerWeaponType() > 2) return false;   /* the gun hand: only without a gun out */
    return true;
}
static void Grapple_Update(char *player, char *cam, float dt)
{
    char *proc = *(char **)(player + 0x68);
    char *ctrl = (proc && *(uint8_t *)(proc + 0x28) <= 1) ? *(char **)(proc + 0x138) : NULL;
    float *P = (float *)(player + 0x30);
    g_grapPulling = g_grap.pull;
    bool ok = g_grapOn && g_inGameplay && ctrl && !g_pipOpen && !g_setOpen && !g_climbing;
    bool combo[2], edge[2];
    static bool gripFirst[2], gripWas[2];
    for (int k = 0; k < 2; k++) {   /* the grapple fist counts only grip first, then trigger (a trigger pulled first stays bullet time) */
        if (g_gripHeld[k] && !gripWas[k]) gripFirst[k] = g_grapTrigH[k] < 0.35f;
        if (!g_gripHeld[k]) gripFirst[k] = false;
        gripWas[k] = g_gripHeld[k];
    }
    for (int k = 0; k < 2; k++) {
        combo[k] = g_gripHeld[k] && (gripFirst[k] || g_grap.combo[k]) && g_grapTrigH[k] > (g_grap.combo[k] ? 0.35f : 0.70f);
        edge[k] = combo[k] && !g_grap.combo[k];
        g_grap.combo[k] = combo[k];
    }
    int fireH = -1;   /* a hand that just made the grapple fist (the newest press wins - swing from hand to hand) */
    for (int k = 0; k < 2; k++) if (edge[k] && Grap_HandFree(k) && !(g_grap.pull && g_grap.hand == k)) fireH = k;
    int h = g_grap.hand;

    if (g_yank.on) {   /* pulling a person to you */
        int yh = g_yank.hand;
        bool alive = g_yank.ref && LookupForm(g_yank.id) == (void *)g_yank.ref;
        if (!ok || !alive || !combo[yh] || !g_grapHand[yh]) { g_yank.on = false; Log("Grapple: let them go"); return; }
        g_yank.t += dt;
        const float *A = (const float *)(g_yank.ref + 0x30);
        float hd = *PlayerRotZ(player);
        float want[3] = { P[0] + sinf(hd) * 1.0f * g_unitsPerMeter, P[1] + cosf(hd) * 1.0f * g_unitsPerMeter, P[2] };   /* a metre in front of you */
        float to[3]; V3Sub(to, want, A);
        float dist = V3Len(to);
        float top = g_grapSpeed * g_unitsPerMeter;
        g_yank.speed = fminf(g_yank.speed + top / g_grapRamp * dt, top);
        float step = g_yank.speed * dt;
        if (dist < fmaxf(0.4f * g_unitsPerMeter, step) || g_yank.t > 6.0f) {
            g_yank.on = false;
            if (dist < 1.0f * g_unitsPerMeter) { g_actPushRef = g_yank.ref; g_actPushID = g_yank.id; }
            Grap_Buzz(yh, 3999);
            Log("Grapple: %s (%.1f s)", dist < 1.0f * g_unitsPerMeter ? "pulled them to you" : "took too long", g_yank.t);
            return;
        }
        float np[3]; for (int i = 0; i < 3; i++) np[i] = A[i] + to[i] / dist * step;
        memcpy(g_actMoveWant, np, 12); g_actMoveRef = g_yank.ref; g_actMoveID = g_yank.id; g_actMoveHave = true;
        if (g_grapRope && g_handWOk[yh]) { float tip[3] = { A[0], A[1], A[2] + 0.9f * g_unitsPerMeter }; Rope_Project(cam, g_handW[yh], tip); }
        static int bz = 0; if (g_grapRumble && (bz++ & 3) == 0) Grap_Buzz(yh, 500);
        return;
    }

    if (g_grap.pull && fireH < 0) {
        if (!ok || !g_grapHand[h]) { Grap_End(player, ctrl, false, "stopped"); return; }
        if (!combo[h]) { Grap_End(player, ctrl, true, "let go early"); return; }
        g_grap.t += dt;
        float chest[3] = { P[0], P[1], P[2] + (g_gameEyeUnits > 50.0f ? g_gameEyeUnits : 118.0f) * 0.72f };
        float to[3]; V3Sub(to, g_grap.target, chest);
        float dist = V3Len(to);
        float top = g_grapSpeed * g_unitsPerMeter;
        g_grap.speed = fminf(g_grap.speed + top / g_grapRamp * dt, top);
        float arrive = fmaxf(0.6f * g_unitsPerMeter, g_grap.speed * dt * 1.5f);
        if (dist < arrive || g_grap.t > 8.0f) { Grap_End(player, ctrl, true, dist < arrive ? "arrived" : "took too long"); return; }
        for (int i = 0; i < 3; i++) { g_grap.dir[i] = to[i] / dist; g_grap.vel[i] = g_grap.dir[i] * g_grap.speed; }
        /* did last frame's push actually move you? */
        float moved[3]; V3Sub(moved, P, g_grap.lastP); memcpy(g_grap.lastP, P, 12);
        float expect = g_grap.speed * dt;
        if (g_grap.t > 0.12f && V3Len(moved) < expect * 0.25f) g_grap.stuckT += dt; else g_grap.stuckT = 0;
        if (g_grap.stuckT > 0.08f) {
            if (g_grap.mode == 0) { g_grap.mode = 1; g_grap.stuckT = 0; Log("Grapple: the physics push isn't moving you - moving you step by step instead"); }
            else { Grap_End(player, ctrl, false, "blocked"); return; }
        }
        /* no gravity or fall damage while on the line */
        *(float *)(ctrl + 0x54C) = 0.0f;
        *(float *)(ctrl + 0x544) = P[2];
        *(float *)(ctrl + 0x548) = 0.0f;
        if (g_grap.mode == 0) Grap_SetVel(ctrl, g_grap.vel, true);
        else {
            float step[3], np[3], probeTo[3]; float sl = fminf(expect, dist);
            for (int i = 0; i < 3; i++) { step[i] = g_grap.dir[i] * sl; np[i] = P[i] + step[i]; probeTo[i] = chest[i] + g_grap.dir[i] * (sl + 25.0f); }
            float fr; char *o = RayCast(chest, probeTo, &fr);
            if (o && fr < 1.0f && dist > 2.0f * g_unitsPerMeter) { Grap_End(player, ctrl, false, "blocked"); return; }
            float zero[3] = { 0, 0, 0 }; Grap_SetVel(ctrl, zero, true);
            Climb_MovePlayer(player, np);
        }
        if (g_grapRope && g_handWOk[h]) {   /* the rope: shoots out over the first tenth of a second, then stays taut to the hook */
            float k = fminf(g_grap.t / 0.10f, 1.0f), tip[3];
            for (int i = 0; i < 3; i++) tip[i] = g_handW[h][i] + (g_grap.target[i] - g_handW[h][i]) * k;
            Rope_Project(cam, g_handW[h], tip);
        }
        static int bz = 0; if (g_grapRumble && (bz++ & 3) == 0) Grap_Buzz(h, 300);
        return;
    }
    if (g_grap.missAt > 0) {   /* a miss: out and back in a quarter second */
        int mh = g_grap.hand;
        float mt = (float)((QpcMs() - g_grap.missAt) / 1000.0);
        if (mt > 0.25f || !g_handWOk[mh]) g_grap.missAt = 0;
        else if (g_grapRope) {
            float k = mt < 0.12f ? mt / 0.12f : 1.0f - (mt - 0.12f) / 0.13f, tip[3];
            for (int i = 0; i < 3; i++) tip[i] = g_handW[mh][i] + (g_grap.missEnd[i] - g_handW[mh][i]) * k;
            Rope_Project(cam, g_handW[mh], tip);
        }
    }

    if (g_grap.fling && !g_grap.pull) {   /* flying on after letting go: keep the sideways speed (slowly fading) until you land */
        g_grap.flingT += dt;
        if (!ok) { g_grap.fling = false; return; }
        bool onGround = g_grap.flingT > 0.25f && *(float *)(ctrl + 0x548) < 0.02f;
        float moved[2] = { P[0] - g_grap.lastP[0], P[1] - g_grap.lastP[1] }; memcpy(g_grap.lastP, P, 12);
        float hs = sqrtf(g_grap.flingH[0] * g_grap.flingH[0] + g_grap.flingH[1] * g_grap.flingH[1]);
        bool blocked = g_grap.flingT > 0.15f && hs * dt > 1.0f && sqrtf(moved[0] * moved[0] + moved[1] * moved[1]) < hs * dt * 0.2f;
        if (onGround || blocked || g_grap.flingT > 5.0f || hs < 1.0f * g_unitsPerMeter) {
            g_grap.fling = false;
            Log("Grapple: landed (%s) after %.1f s", onGround ? "ground" : blocked ? "hit something" : "slowed down", g_grap.flingT);
        } else {
            float k = expf(-g_grapDrag * dt);
            g_grap.flingH[0] *= k; g_grap.flingH[1] *= k;
            Grap_SetVel(ctrl, g_grap.flingH, false);
            if (g_grapSafe) *(float *)(ctrl + 0x544) = P[2];   /* no fall damage from the launch */
        }
    }

    if (fireH < 0 || !ok || QpcMs() < g_grap.coolUntil) return;
    h = fireH;
    /* fire: straight out from where the hand points */
    float d[3]; for (int i = 0; i < 3; i++) d[i] = -g_handWR[h][i][2];
    float dl = V3Len(d); if (dl < 0.5f) return; for (int i = 0; i < 3; i++) d[i] /= dl;
    float range = g_grapRange * g_unitsPerMeter, from[3], to[3];
    for (int i = 0; i < 3; i++) { from[i] = g_handW[h][i] + d[i] * 8.0f; to[i] = from[i] + d[i] * range; }
    float fr = 1.0f; char *hit = RayCast(from, to, &fr);
    char *hitRef = hit ? HitRef(hit) : NULL;
    bool isActor = hitRef && (*(uintptr_t **)hitRef)[0x100 / 4] == 0x008D0360;
    if (!hit || fr >= 1.0f || (isActor && !g_grapActors)) {
        if (!g_grap.pull) {
            g_grap.hand = h;
            for (int i = 0; i < 3; i++) g_grap.missEnd[i] = from[i] + d[i] * fminf((hit && fr < 1.0f) ? fr * range : range, 25.0f * g_unitsPerMeter);
            g_grap.missAt = QpcMs();
        }
        Grap_Buzz(h, 1200); static int nl = 0; if (nl++ < 10) Log("Grapple: nothing to hook within %.0f m (%s hand)", g_grapRange, h ? "right" : "left"); return; }
    float hitD = fr * range;
    if (hitD < 1.5f * g_unitsPerMeter) { Grap_Buzz(h, 800); return; }
    if (isActor && g_grapYank && !g_grap.pull) {   /* a person: pull THEM to you */
        g_yank.on = true; g_yank.ref = hitRef; g_yank.id = *(uint32_t *)(hitRef + 0x0C); g_yank.hand = h;
        g_yank.speed = g_grapStart * g_grapSpeed * g_unitsPerMeter; g_yank.t = 0;
        g_grap.hand = h; g_grap.missAt = 0;
        Grap_Buzz(h, 3000);
        Log("Grapple: %s hand hooked someone %.1f m away (%08X) - pulling them to you", h ? "right" : "left", hitD / g_unitsPerMeter, g_yank.id);
        return;
    }
    bool wasPulling = g_grap.pull;
    for (int i = 0; i < 3; i++) { g_grap.target[i] = from[i] + d[i] * (hitD - 0.3f * g_unitsPerMeter); g_grap.dir[i] = d[i]; }
    if (!wasPulling) {
        g_grap.speed = g_grapStart * g_grapSpeed * g_unitsPerMeter;
        g_grap.gravSave = *(float *)(ctrl + 0x54C); if (!(g_grap.gravSave > 0.0f)) g_grap.gravSave = 1.0f;
        if (g_grap.fling) g_grap.speed = fmaxf(g_grap.speed, fminf(V3Len(g_grap.vel), g_grapSpeed * g_unitsPerMeter));   /* keep your speed when hooking again mid-air */
    }
    g_grap.pull = true; g_grap.fling = false; g_grap.missAt = 0; g_grap.hand = h; g_grap.t = 0; g_grap.stuckT = 0; g_grap.mode = 0;
    memcpy(g_grap.lastP, P, 12);
    if (!wasPulling && *(float *)(ctrl + 0x548) < 0.05f) {   /* standing: lift off the ground first so the pull isn't held down */
        float up[3] = { P[0], P[1], P[2] + 6.0f }; Climb_MovePlayer(player, up);
    }
    Grap_Buzz(h, 3000);
    Log("Grapple: %s hand hooked %.1f m away (%s)%s", h ? "right" : "left", hitD / g_unitsPerMeter,
        isActor ? "someone" : hitRef ? "an object" : "the ground/terrain", wasPulling ? " - switched hands mid-pull" : "");
    if (g_savePhotos && g_gpN < 3 && !g_gpStart) { g_gpN++; g_gpFrame = 0; g_gpStart = QpcMs(); }
}

/* Wearing power armor: falls never hurt (the fall height is counted from where you are every frame). */
static void PowerArmor_NoFall(char *player)
{
    char *proc = *(char **)(player + 0x68);
    if (!proc || *(uint8_t *)(proc + 0x28) != 0) return;   /* (the player's full process only) */
    bool pa = *(uint8_t *)(proc + 0x128) != 0;
    static int lastPa = -1;
    if (pa != lastPa) { lastPa = pa; Log("Power armor: %s%s", pa ? "wearing it" : "not wearing it", pa && g_paNoFall ? " - no fall damage" : ""); }
    if (!g_paNoFall || !pa) return;
    char *ctrl = *(char **)(proc + 0x138);
    if (!ctrl) return;
    *(float *)(ctrl + 0x544) = ((float *)(player + 0x30))[2];
}

/* ---- stimpaks (drawing side: holding it, stabbing) ---- */
static void Stim_Release(int h, int what)
{   /* the hand lets go of it; the main loop then uses it or puts it back */
    if (g_holding[h] && g_held[h].ref == g_stimRef) { Held_Collide(h); memset(&g_held[h], 0, sizeof g_held[h]); g_holding[h] = false; }
    g_stimDoRef = g_stimRef; g_stimDoID = g_stimRefID; g_stimDoType = g_stimType; g_stimDo = what;
    g_stimHand = -1; g_stimRef = NULL; g_stimHeld = false;
}
static void Stim_Update(float dt, bool noWeapon)
{
    if (g_stimHand < 0) return;
    int h = g_stimHand, o = 1 - h;
    double now = QpcMs();
    bool valid = g_stimRef && LookupForm(g_stimRefID) == (void *)g_stimRef;
    if (!valid) { Stim_Release(h, 2); return; }
    if (!g_stimHeld) {   /* (its 3D appears a moment after it's placed) */
        if (RefBody(g_stimRef) && g_handWOk[h]) { Grab_Start(h, g_stimRef); g_stimHeld = g_holding[h] && g_held[h].ref == g_stimRef; }
        if (!g_stimHeld && now - g_stimAt > 2000.0) { Log("Stimpak: couldn't put it in your hand - putting it back"); Stim_Release(h, 2); }
        return;
    }
    if (!g_holding[h] || g_held[h].ref != g_stimRef || !g_gripHeld[h]) { Stim_Release(h, 2); return; }   /* let go: back in the inventory */
    /* stab: the hand moving quickly into your other forearm or your chest */
    const float *P = g_vrHandPos[h];
    float fw[3] = { -sinf(g_vrHeadYaw), 0, -cosf(g_vrHeadYaw) }, chest[3];
    for (int i = 0; i < 3; i++) chest[i] = g_vrHeadPos[i] + fw[i] * g_chestFwd;
    chest[1] -= g_chestDown;
    float dc[3]; V3Sub(dc, P, chest);
    bool inChest = V3Len(dc) < g_chestR;
    bool inArm = false;
    if (g_vrHandOk[o]) {   /* the forearm: from the other hand back toward its elbow (the controller's +Z) */
        float A[3], B[3], AB[3], AP[3];
        for (int i = 0; i < 3; i++) { A[i] = g_vrHandPos[o][i]; B[i] = A[i] + g_vrHandRot[o][i][2] * 0.26f; }
        V3Sub(AB, B, A); V3Sub(AP, P, A);
        float t = fminf(fmaxf(V3Dot(AP, AB) / fmaxf(V3Dot(AB, AB), 1e-6f), 0.0f), 1.0f), q[3];
        for (int i = 0; i < 3; i++) q[i] = A[i] + AB[i] * t - P[i];
        inArm = V3Len(q) < 0.09f;
    }
    static bool wasIn = false, armed = false; static double armFor = 0;
    if (armFor != g_stimAt) { armFor = g_stimAt; armed = false; wasIn = true; }   /* (a new one: the hand starts at your chest) */
    bool in = inChest || inArm;
    if (!in && now - g_stimAt > 250.0) armed = true;   /* it has to leave your chest and arm before a stab counts */
    if (in && !wasIn && armed && g_handSpeed[h] > 0.45f) {
        Log("Stimpak: stabbed into your %s (%.1f m/s)", inArm ? "arm" : "chest", g_handSpeed[h]);
        if (g_vrSystem) { g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, 3999); if (inArm && g_handIdx[o] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[o], 0, 3999); }
        Stim_Release(h, 1);
        wasIn = false; return;
    }
    wasIn = in;
    if (!noWeapon && g_holding[h]) Grab_Hold(h, dt);   /* (with a gun out the normal holding below doesn't run) */
}
static void Grab_Update(char *player, char *cam)
{
    LARGE_INTEGER now, fq; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&fq);
    float dt = g_grabLastQpc.QuadPart ? (float)(now.QuadPart - g_grabLastQpc.QuadPart) / (float)fq.QuadPart : 1.0f / 60.0f;
    g_grabLastQpc = now;
    dt = fminf(fmaxf(dt, 1.0f / 240.0f), 1.0f / 20.0f);

    Hands_ComputeWorld(player, cam);
    bool noWeapon = !g_weaponOutThisFrame || PlayerWeaponType() == -1;
    for (int h = 0; h < 2; h++) {
        bool keep = h == g_stimHand ? g_gripHeld[h] : g_grabWant[h];
        if (g_holding[h] && (!keep || (!noWeapon && h != g_stimHand) || !g_handWOk[h])) {
            if (g_grabWant[h]) Log("Grab: %s hand drops its object (%s)", h ? "right" : "left", !noWeapon ? "weapon drawn" : "hand tracking lost");
            Grab_Drop(h, g_handWOk[h]);
        }
    }
    static unsigned scanTick = 0;
    if ((scanTick++ % 3) == 0) Near_Scan(player, 160.0f);   /* the object search doesn't need every frame */
    Collide_Update(dt, !noWeapon);
    Climb_Update(player, cam, dt, noWeapon);
    Grapple_Update(player, cam, dt);
    PowerArmor_NoFall(player);
    if (!Grapple_Busy()) Momentum_Update(player, dt);
    Stim_Update(dt, noWeapon);
    if (!g_grabEnabled || !noWeapon) { g_grabCand[0] = g_grabCand[1] = NULL; return; }
    for (int h = 0; h < 2; h++) if (!g_grabWant[h]) g_grabBlock[h] = false;
    for (int h = 0; h < 2; h++)
        if (!g_holding[h] && g_grabWant[h] && !g_grabBlock[h] && g_grabCand[h] && g_handWOk[h] && Near_ValidRef(g_grabCand[h])) Grab_Start(h, g_grabCand[h]);
    for (int h = 0; h < 2; h++) if (g_holding[h]) Grab_Hold(h, dt);
    Grab_FindCandidates();
}


/* Interface drawn into the middle of the frame: the 2D menus and the HUD are flat pictures across the
   whole image, which in the full VR view would reach far into your peripheral vision. Narrowing the
   interface camera's viewport draws them, same shape, into the central part of the view instead. */
static void UI_Viewport(void)
{
    char *im = *(char **)0x011D8A80;
    if (!im) return;
    static bool logged = false;
    if (!logged) {
        logged = true;
        for (int k = 0; k < 2; k++) {
            char *sg = *(char **)(im + 4 + 4 * k);
            char *c = sg ? *(char **)(sg + 0xAC) : NULL;
            if (c) { const float *v = (const float *)(c + 0x100), *f = (const float *)(c + 0xDC);
                Log("Interface camera %d: viewport l%.2f r%.2f t%.2f b%.2f, frustum l%.1f r%.1f t%.1f b%.1f ortho %d", k, v[0], v[1], v[2], v[3], f[0], f[1], f[2], f[3], *(uint8_t *)(c + 0xF4)); }
        }
    }
    /* The interface is drawn as a flat picture over the whole frame, so in the headset the HUD sits out at the
       edges of your view and menus fill it. Both interface cameras' views are widened instead (the flat one
       draws most of it, the other draws parts like the dialogue topics), which draws the whole interface
       smaller, towards the middle (HUD size / Menu size). Their viewports are left full. */
    bool room = !g_inGameplay || g_farActive2 || (g_farGameMenus && IsMenuMode() && !g_pipOpen && !g_liveMenuNow);
    bool menuUp = g_liveMenuNow || (IsMenuMode() && !g_pipOpen);
    bool talking = ((const volatile bool *)0x011F308F)[1009];
    float sc = room || g_flatMenus ? 1.0f : talking ? g_dlgScale : menuUp ? g_menuScale : g_uiScale;
    if (!(sc > 0.1f && sc <= 1.0f)) sc = 1.0f;
    {   /* one picture of the first conversation (to check how its parts are drawn) */
        static double tAt = 0; static bool snapped = false;
        if (talking && !snapped) { if (!tAt) tAt = QpcMs(); else if (QpcMs() - tAt > 2500.0 && g_hookedDevice) { snapped = true; g_dlgSnapWant = true; } } else if (!talking) tAt = 0;
    }
    static float base[2][4], lastSet[2][4]; static bool haveBase[2];
    for (int k = 0; k < 2; k++) {
        char *sgk = *(char **)(im + 4 + 4 * k);
        char *c = sgk ? *(char **)(sgk + 0xAC) : NULL;
        if (!c || IsBadReadPtr(c, 0x110)) continue;
        float *v = (float *)(c + 0x100);
        static const float full[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
        if (memcmp(v, full, sizeof full)) memcpy(v, full, sizeof full);
        float *f = (float *)(c + NICAMERA_FRUSTUM);
        if (!haveBase[k] || memcmp(f, lastSet[k], sizeof lastSet[k])) {   /* (the game's own values - first time, or it changed them) */
            if (f[1] > f[0] && f[2] > f[3]) { memcpy(base[k], f, sizeof base[k]); haveBase[k] = true; }
        }
        if (!haveBase[k]) continue;
        float cx = (base[k][0] + base[k][1]) * 0.5f, cy = (base[k][2] + base[k][3]) * 0.5f;
        float hw = (base[k][1] - base[k][0]) * 0.5f / sc, hh = (base[k][2] - base[k][3]) * 0.5f / sc;
        if (menuUp && !room && !talking) cy -= g_menuYOff * 2.0f * hh;   /* pop-up menus raised or lowered to where you look */
        float want[4] = { cx - hw, cx + hw, cy + hh, cy - hh };
        memcpy(f, want, sizeof want); memcpy(lastSet[k], want, sizeof want);
        if (k == 0) { memcpy(g_uiXfBase, base[0], sizeof g_uiXfBase); memcpy(g_uiXfCur, want, sizeof g_uiXfCur); g_uiXfOn = memcmp(base[0], want, sizeof want) != 0; }
    }
    static float lastSc = -1; if (sc != lastSc) { lastSc = sc; Log("Interface: drawn at %.0f%% size (%s; both interface cameras)", sc * 100.0f, room ? "menu screen / full" : menuUp ? "menu" : "HUD"); }
}

/* ---- Pip-Boy recorder: a short picture sequence of every Pip-Boy open (first 3 opens per session) ---- */
static double g_recStart; static int g_recOpen, g_recFrame;
static IDirect3DSurface9 *g_recRT = NULL, *g_recSys = NULL;
#define REC_W 1920
#define REC_H 1080
static void Rec_Release9(void) { Release9(&g_recRT); Release9(&g_recSys); }
static void Rec_Project(const float *P, float *x, float *y, float *z)
{
    *x = *y = -1; *z = 0;
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR, *cam = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
    if (!cam || !P) return;
    const float *C = (const float *)(cam + NIAV_WORLD_TRANSLATE), *Cw = (const float *)(cam + NIAV_WORLD_ROTATE);
    float F[3] = { Cw[0], Cw[3], Cw[6] }, U[3] = { Cw[1], Cw[4], Cw[7] }, R[3] = { Cw[2], Cw[5], Cw[8] };
    const float *fr = (const float *)(cam + NICAMERA_FRUSTUM);
    float q[3]; V3Sub(q, P, C); *z = V3Dot(q, F);
    if (*z > 0.1f) { *x = (V3Dot(q, R) / *z - fr[0]) / (fr[1] - fr[0]); *y = (fr[2] - V3Dot(q, U) / *z) / (fr[2] - fr[3]); }
}
static bool g_rtTestOff = false;
static void Pip_Record(IDirect3DDevice9 *dev)
{
    if (g_rtTestOff && (!g_recStart || !g_pipOpen)) { g_pipRealTime = true; g_rtTestOff = false; Log("REC TEST: real-time Pip-Boy back on"); }
    if (!g_recStart) return;
    double t = QpcMs() - g_recStart;
    if (g_recFrame >= 16) { g_recStart = 0; return; }
    if (t < g_recFrame * 450.0) return;
    if (!g_recRT) IDirect3DDevice9_CreateRenderTarget(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_recRT, NULL);
    if (!g_recSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_recSys, NULL);
    IDirect3DSurface9 *bb = NULL;
    if (!g_recRT || !g_recSys || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { g_recStart = 0; return; }
    bool ok = SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_recRT, NULL, D3DTEXF_LINEAR)) && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_recRT, g_recSys));
    IDirect3DSurface9_Release(bb);
    if (ok) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(g_recSys, &lr, NULL, D3DLOCK_READONLY))) {
            char name[64]; snprintf(name, sizeof name, "FNVR_rec_%d_%02d.bmp", g_recOpen, g_recFrame);
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, REC_W, REC_H);
            IDirect3DSurface9_UnlockRect(g_recSys);
        }
    }
    /* what's where in that frame */
    float gx = -1, gy = -1, gz = 0, qx = -1, qy = -1, qz = 0, hx = -1, hy = -1, hz = 0;
    float gc[4] = { 0 }, qc[4] = { 0 };
    if (g_pScr && Geo_Center(g_pScr, gc, &gc[3], 0)) Rec_Project(gc, &gx, &gy, &gz);
    if (g_pOn && Geo_Center(g_pOn, qc, &qc[3], 0)) Rec_Project(qc, &qx, &qy, &qz);
    char *lh = g_bones[0][2];
    if (lh) Rec_Project((const float *)(lh + NIAV_WORLD_TRANSLATE), &hx, &hy, &hz);
    float hd[3]; V3Sub(hd, g_vrHandPos[0], g_vrHeadPos);
    Log("REC %d.%02d t=%.1fs mode %d open %d | Pip-Boy glass at %.2f,%.2f (d %.0f) | menu picture at %.2f,%.2f (d %.0f, size %.1f, %s) | left hand at %.2f,%.2f | real left hand %.2f m from head | wrist roll %.0f deg",
        g_recOpen, g_recFrame, t / 1000.0, g_pipModeRaw, (int)g_pipOpen, gx, gy, gz, qx, qy, qz, qc[3],
        g_pOn ? ((*(uint32_t *)(g_pOn + 0x30) & 1) ? "hidden" : "shown") : "none", hx, hy, V3Len(hd), g_twRoll * RAD2DEG);
    /* Test: photos 9-19 with the world paused (the game's normal Pip-Boy drawing), to compare with real-time. */
    if (false && g_recFrame == 8 && g_pipRealTime && g_pipOpen) { g_pipRealTime = false; g_rtTestOff = true; Log("REC TEST: real-time Pip-Boy OFF for photos 9-19"); }
    if (g_recFrame == 19 && g_rtTestOff) { g_pipRealTime = true; g_rtTestOff = false; Log("REC TEST: real-time Pip-Boy back on"); }
    /* Which part is the floating menu picture? Hide one candidate per test frame and see which picture loses it. */
    {
        static char *cand[4]; static int hiddenIdx = -1; static uint32_t savedFlag;
        if (hiddenIdx >= 0 && cand[hiddenIdx]) { *(uint32_t *)(cand[hiddenIdx] + 0x30) = savedFlag; hiddenIdx = -1; }
        if (g_recFrame == 3) {
            char *im = *(char **)0x011D8A80;
            char *pm = (im && !IsBadReadPtr(im + 0x174, 4)) ? *(char **)(im + 0x174) : NULL;
            cand[0] = (pm && !IsBadReadPtr(pm, 0x1C)) ? *(char **)(pm + 0x04) : NULL;
            cand[1] = (pm && !IsBadReadPtr(pm, 0x1C)) ? *(char **)(pm + 0x08) : NULL;
            cand[2] = g_pFor ? FindNameCI(g_pFor, "pipboyon", 0) : NULL;
            cand[3] = g_pScr;
            const float *sb = g_pScr ? *(const float **)(g_pScr + 0x20) : NULL;
            for (int i = 0; i < 4; i++) {
                char *c = cand[i];
                if (!c || IsBadReadPtr(c, 0xC0)) { cand[i] = NULL; Log("PIPTEST cand %d: none", i); continue; }
                const char *nm = *(const char **)(c + 0x08); if (!nm || IsBadReadPtr(nm, 1)) nm = "?";
                char *par = *(char **)(c + NIAV_PARENT); const char *pn = (par && !IsBadReadPtr(par, 0x10)) ? *(const char **)(par + 0x08) : NULL; if (!pn || IsBadReadPtr(pn, 1)) pn = "?";
                const float *b = *(const float **)(c + 0x20);
                float dist = -1, rad = -1;
                if (b && !IsBadReadPtr(b, 16)) { rad = b[3]; if (sb && !IsBadReadPtr(sb, 16)) { float d[3]; V3Sub(d, b, sb); dist = V3Len(d); } }
                Log("PIPTEST cand %d: '%s' (vt %08X, parent '%s') size %.1f, %.1f units from pipboyscreen, %s - hidden in picture %d",
                    i, nm, *(uint32_t *)c, pn, rad, dist, (*(uint32_t *)(c + 0x30) & 1) ? "already hidden" : "shown", 4 + i * 2);
            }
        }
        int next = g_recFrame + 1;
        if (false && next >= 4 && next <= 10 && !(next & 1) && g_pipModeRaw == 3) {
            int k = (next - 4) / 2;
            if (cand[k]) { savedFlag = *(uint32_t *)(cand[k] + 0x30); *(uint32_t *)(cand[k] + 0x30) |= 1u; hiddenIdx = k; }
        }
    }
    g_recFrame++;
}
/* ---- Grab recorder: pictures of the first 3 pick-ups per session ---- */

static void Grab_Record(IDirect3DDevice9 *dev)
{
    if (!g_grecStart) return;
    double t = QpcMs() - g_grecStart;
    if (g_grecFrame >= 10) { g_grecStart = 0; return; }
    if (t < g_grecFrame * 320.0) return;
    if (!g_recRT) IDirect3DDevice9_CreateRenderTarget(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_recRT, NULL);
    if (!g_recSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_recSys, NULL);
    IDirect3DSurface9 *bb = NULL;
    if (!g_recRT || !g_recSys || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { g_grecStart = 0; return; }
    bool ok = SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_recRT, NULL, D3DTEXF_LINEAR)) && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_recRT, g_recSys));
    IDirect3DSurface9_Release(bb);
    if (ok) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(g_recSys, &lr, NULL, D3DLOCK_READONLY))) {
            char name[64]; snprintf(name, sizeof name, "FNVR_grab_%d_%02d.bmp", g_grecN, g_grecFrame);
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, REC_W, REC_H);
            IDirect3DSurface9_UnlockRect(g_recSys);
        }
    }
    int h = g_grecHand; Held *g = &g_held[h];
    float hx = -1, hy = -1, hz = 0, ox = -1, oy = -1, oz = 0, rel[3] = { 0, 0, 0 };
    char *hb = g_bones[h][B_HAND];
    if (hb) Rec_Project((const float *)(hb + NIAV_WORLD_TRANSLATE), &hx, &hy, &hz);
    if (g_holding[h] && g->rb) {
        float P[3]; BodyCom(g->rb, P); Rec_Project(P, &ox, &oy, &oz);
        if (hb) {   /* object centre in the drawn hand's own axes */
            const float *R = (const float *)(hb + NIAV_WORLD_ROTATE), *Tb = (const float *)(hb + NIAV_WORLD_TRANSLATE);
            float d[3]; V3Sub(d, P, Tb);
            for (int i = 0; i < 3; i++) rel[i] = R[0 * 3 + i] * d[0] + R[1 * 3 + i] * d[1] + R[2 * 3 + i] * d[2];
        }
    }
    Palm *pm = &g_palm[h];
    Log("GRAB %d.%02d t=%.1fs holding %d | hand at %.2f,%.2f (d %.0f) | object at %.2f,%.2f (d %.0f) | object from hand (hand axes) %.1f %.1f %.1f | palm faces %.2f %.2f %.2f, knuckles %.1f %.1f %.1f | grip radius %.1f curl %.2f",
        g_grecN, g_grecFrame, t / 1000.0, (int)g_holding[h], hx, hy, hz, ox, oy, oz, rel[0], rel[1], rel[2],
        pm->n[0], pm->n[1], pm->n[2], pm->kn[0], pm->kn[1], pm->kn[2], g->gripR, g->curl);
    g_grecFrame++;
}
/* ---- Grapple recorder: a photo burst of the first 3 grapples per session ---- */
static void Grap_Record(IDirect3DDevice9 *dev)
{
    if (!g_gpStart) return;
    double t = QpcMs() - g_gpStart;
    if (g_gpFrame >= 12) { g_gpStart = 0; return; }
    if (t < g_gpFrame * 200.0) return;
    if (!g_recRT) IDirect3DDevice9_CreateRenderTarget(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_recRT, NULL);
    if (!g_recSys) IDirect3DDevice9_CreateOffscreenPlainSurface(dev, REC_W, REC_H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &g_recSys, NULL);
    IDirect3DSurface9 *bb = NULL;
    if (!g_recRT || !g_recSys || FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb))) { g_gpStart = 0; return; }
    bool ok = SUCCEEDED(IDirect3DDevice9_StretchRect(dev, bb, NULL, g_recRT, NULL, D3DTEXF_LINEAR)) && SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, g_recRT, g_recSys));
    IDirect3DSurface9_Release(bb);
    if (ok) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(IDirect3DSurface9_LockRect(g_recSys, &lr, NULL, D3DLOCK_READONLY))) {
            char name[64]; snprintf(name, sizeof name, "FNVR_grapple_%d_%02d.bmp", g_gpN, g_gpFrame);
            SaveBMP(name, (const BYTE *)lr.pBits, lr.Pitch, REC_W, REC_H);
            IDirect3DSurface9_UnlockRect(g_recSys);
        }
    }
    g_gpFrame++;
}
static void __attribute__((fastcall)) Hook_DoRenderFrame(void *osGlobals, void *unusedEdx, void *renderer, uint32_t a, uint32_t b)
{
    HeadingLock_Apply();   /* (again right before drawing, after the game has moved things this frame) */
    (void)unusedEdx;
    IDirect3DDevice9 *dev = GetGameDevice();
    char *sg = *(char **)SCENEGRAPH_PTR_ADDR;
    char *cam = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
    void *player = GetPlayer();

    g_renderCallsSinceLog++;
    uintptr_t ret = (uintptr_t)__builtin_return_address(0);
    if (ret == 0x008721A9) g_renderCallsResize++; else if (ret == 0x00879179) g_renderCallsMain++; else g_renderCallsOther = ret;
    bool doStereo = g_stereo && g_inGameplay && !g_menuCached && dev && cam && player && g_vrCompositor && !g_displayFailed && g_d11;
    if (g_inGameplay && !g_menuCached && cam && player) {   /* the character's standing eye height: the most common height above its feet */
        static unsigned hist[201]; static double histAt = 0;
        float e = ((const float *)(cam + NIAV_WORLD_TRANSLATE))[2] - ((const float *)((char *)player + 0x30))[2];
        if (e > 60.0f && e < 200.0f) hist[(int)(e + 0.5f)]++;
        if (QpcMs() - histAt > 2000.0) {
            histAt = QpcMs();
            int best = 0; for (int i = 90; i <= 200; i++) if (hist[i] > hist[best]) best = i;
            if (best && hist[best] >= 60 && fabsf(best - g_gameEyeUnits) > 0.75f) {
                g_gameEyeUnits = (float)best;
                static int el = 0; if (el++ < 4) Log("Body: the character's eyes are %d game units above its feet", best);
            }
        }
    }
    if (HeadsetDisplayActive()) UI_Viewport();
    LockFovForDraw();
    if (!doStereo) {
        double t0 = QpcMs(), t1, t2, t3, t4, t5;
        bool physQuiet = false;
        {   /* a loading screen or a new location (doors, fast travel): the old cell's objects are being deleted - let go of
               everything and leave the physics alone for 2 s, so nothing touches an object that no longer exists */
            static char *lastCell = NULL; static double quietUntil = 0;
            char *cell = player ? *(char **)((char *)player + 0x40) : NULL;
            bool loading = ((const volatile bool *)0x011F308F)[1007];
            if (loading || cell != lastCell) {
                if (g_nearCount || g_holding[0] || g_holding[1] || g_climb[0] || g_climb[1] || g_thrown.ref)
                    Log("Physics: %s - letting go of everything for 2 s", loading ? "loading screen" : "new location");
                Grab_ClearAll(); g_climb[0] = g_climb[1] = false; memset(&g_thrown, 0, sizeof g_thrown); g_yank.on = false; g_actMoveHave = false;
                quietUntil = QpcMs() + 2000.0; lastCell = cell;
            }
            physQuiet = QpcMs() < quietUntil;
        }
        if (g_inGameplay && cam && player && !g_menuCached && g_bodyValid) {
            HeadOffset_Apply((char *)player, cam);
            t1 = QpcMs(); g_tHead += t1 - t0;
            g_weaponOutThisFrame = PlayerWeaponOut();
            if (!physQuiet) { Grab_Update((char *)player, cam); Thrown_Update((char *)player); }
        }
        t1 = QpcMs(); g_tGrab += t1 - t0;
        bool armsDone = g_handAimValid && g_inGameplay && cam && player && !g_menuCached && !g_adsActive &&
                        Arms_Apply((char *)player, cam);
        t2 = QpcMs(); g_tArms += t2 - t1;
        bool grabVis = false;
        if (g_inGameplay && cam && player && !g_menuCached && g_bodyValid) {
            if (armsDone) Palm_Update();
            if (armsDone && QpcMs() >= g_weaponSettleUntil) Scope_Prepare((char *)player, cam); else g_scope.active = false;
            if (armsDone) PipScr_Prepare((char *)player, cam); else g_pipQuad.ok = false;
            /* sighting through the scope: leave the gun out of the picture so the magnified view shows
               the target instead of the inside of the scope (the scope view takes its place) */

            Grab_Visual();
            if (armsDone) MeleeAngle_Apply();
            Holster_Models((char *)player);
            {   /* grappling: the body below you is hidden (it can't keep up at grapple speed); back a moment after */
                static double pullEnd = 0;
                if (g_grapPulling || g_climbing) pullEnd = QpcMs();   /* (climbing too) */
                g_bodyHidden = NULL;
                if (g_hideBodyGrap && pullEnd > 0 && QpcMs() - pullEnd < 1200.0) {
                    char *rs = *(char **)((char *)player + 0x64); char *root = rs && !IsBadReadPtr(rs, 0x18) ? *(char **)(rs + 0x14) : NULL;
                    if (root && !IsBadReadPtr(root, 0x34) && !(*(uint32_t *)(root + 0x30) & 1)) { *(uint32_t *)(root + 0x30) |= 1; g_bodyHidden = root; }
                }
            }
            grabVis = true;
        }
        t3 = QpcMs(); g_tVis += t3 - t2;
        if (!armsDone) g_capWant = false;
        Trk_Update();
        g_capCount = 0; g_inRenderFrame = true;
        o_DoRenderFrame(osGlobals, renderer, a, b);
        g_inRenderFrame = false;
        t4 = QpcMs(); g_tGame += t4 - t3;
        MeleeAngle_Restore();
        if (g_bodyHidden) { *(uint32_t *)(g_bodyHidden + 0x30) &= ~1u; g_bodyHidden = NULL; }
        if (grabVis) Grab_VisualRestore();
        if (armsDone) Arms_Restore();
        HeadOffset_Restore();
        t5 = QpcMs(); g_tArms += t5 - t4;
        g_tFrames++;
        {
            static double lastEnd = 0;
            if (lastEnd > 0 && t5 - lastEnd > 250.0) {
                static int hitchLogs = 0;
                if (hitchLogs++ < 30)
                    Log("Hitch: %.0f ms between frames (spent outside drawing - game logic/loading). This frame: game drawing %.1f ms, FNVR %.2f ms",
                        t5 - lastEnd - (t5 - t0), t4 - t3, (t3 - t0) + (t5 - t4));
            }
            lastEnd = t5;
        }
        return;
    }
    if (!g_loggedStereoActive) {
        g_loggedStereoActive = true;
        Log("Stereo rendering active: eye separation %.2f game units (IPD %.1f mm x %.1f units/m)",
            g_ipdMeters * g_unitsPerMeter, g_ipdMeters * 1000.0f, g_unitsPerMeter);
    }

    /* Eyes sit left/right of the camera along its horizontal right vector. */
    float heading = *PlayerRotZ(player);                    /* + = clockwise from north (+Y) */
    float rx = cosf(heading), ry = -sinf(heading);
    float half = 0.5f * g_ipdMeters * g_unitsPerMeter;

    /* Alternate-eye rendering: each frame draws exactly once, from the left or right eye in turn.
       (Drawing twice per frame overloads the game - its per-frame bookkeeping isn't built for it.)
       The camera is moved in world space and in its parent's (local) space, so the offset survives
       if the game re-derives the camera's world position while drawing. */
    float *T = (float *)(cam + NIAV_WORLD_TRANSLATE);
    float *L = (float *)(cam + NIAV_LOCAL_TRANSLATE);
    char *parent = *(char **)(cam + NIAV_PARENT);
    float savedT[3] = { T[0], T[1], T[2] }, savedL[3] = { L[0], L[1], L[2] };
    int eye = g_aerEye;
    float side = eye == 0 ? -1.0f : 1.0f;
    float w[3] = { rx * side * half, ry * side * half, 0.0f };   /* world-space offset */
    float l[3] = { w[0], w[1], w[2] };
    if (parent) {
        /* local = R_parent^T * world / scale_parent */
        const float *R = (const float *)(parent + NIAV_WORLD_ROTATE);   /* row-major 3x3 */
        float sc = *(const float *)(parent + NIAV_WORLD_SCALE);
        if (!(sc > 0.0001f)) sc = 1.0f;
        for (int k = 0; k < 3; k++) l[k] = (R[0 * 3 + k] * w[0] + R[1 * 3 + k] * w[1] + R[2 * 3 + k] * w[2]) / sc;
    }
    for (int k = 0; k < 3; k++) { T[k] = savedT[k] + w[k]; L[k] = savedL[k] + l[k]; }
    float expect[3] = { T[0], T[1], T[2] };
    o_DoRenderFrame(osGlobals, renderer, a, b);
    if (!g_loggedStereoCheck) {
        g_loggedStereoCheck = true;
        float d = fabsf(T[0] - expect[0]) + fabsf(T[1] - expect[1]) + fabsf(T[2] - expect[2]);
        Log("Stereo check: camera %s while drawing (moved %.3f units; parent %p)",
            d > 0.01f ? "position was CHANGED by the game" : "position kept", d, (void *)parent);
    }
    bool ok = Capture_Eye(dev, 0);
    if (ok && g_fillSlot) g_fillSlot->targetEye = eye;
    for (int k = 0; k < 3; k++) { T[k] = savedT[k]; L[k] = savedL[k]; }
    g_aerEye ^= 1;
    g_stereoCapturedThisFrame = ok;
    if (ok) g_stereoFramesSinceLog++;
}

static void InstallRenderHook(void)
{
    static bool done = false;
    if (done) return;
    done = true;
    BYTE *fn = (BYTE *)DORENDERFRAME_ADDR;
    static const BYTE expected[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };   /* push ebp; mov ebp,esp; sub esp,8 */
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) { Log("Stereo: VirtualAlloc failed - stereo disabled"); g_stereo = false; return; }

    if (memcmp(fn, expected, 6) == 0) {
        /* Trampoline: the 6 original bytes, then jump back into the function. */
        memcpy(tramp, fn, 6);
        tramp[6] = 0xE9;
        *(int32_t *)(tramp + 7) = (int32_t)((DORENDERFRAME_ADDR + 6) - ((uintptr_t)tramp + 11));
    } else if (fn[0] == 0xE9) {
        /* Another mod already hooked it with a jump - chain through that mod. */
        uint32_t other = (uint32_t)(DORENDERFRAME_ADDR + 5) + *(int32_t *)(fn + 1);
        tramp[0] = 0xE9;
        *(int32_t *)(tramp + 1) = (int32_t)(other - ((uintptr_t)tramp + 5));
        Log("Stereo: render function already hooked by another mod (jump to %08X) - chaining", other);
    } else {
        Log("Stereo: unexpected bytes at %08X (%02X %02X %02X %02X %02X %02X) - stereo disabled",
            DORENDERFRAME_ADDR, fn[0], fn[1], fn[2], fn[3], fn[4], fn[5]);
        g_stereo = false;
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), tramp, 64);
    o_DoRenderFrame = (DoRenderFrame_t)(uintptr_t)tramp;

    DWORD old;
    VirtualProtect(fn, 6, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9;
    *(int32_t *)(fn + 1) = (int32_t)((uintptr_t)Hook_DoRenderFrame - (DORENDERFRAME_ADDR + 5));
    fn[5] = 0x90;
    VirtualProtect(fn, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 6);
    Log("Stereo: render hook installed at function entry %08X", DORENDERFRAME_ADDR);
}



/* ---- Instant Pip-Boy: no raise/lower animation waits ------------------------------------
   The game holds the Pip-Boy in "opening" until the arm-raise animation finishes, and in "closing" until
   the lowering one does. With the Pip-Boy simply living on your tracked wrist those waits are just delay,
   so skip them: the menu comes up (and goes away) at once. [Controls] bInstantPipBoy=1 */
static bool g_pipInstant = true;
static void InstallInstantPipBoy(void)
{
    if (!g_pipInstant || !g_wristPip) { Log("Instant Pip-Boy: off"); return; }
    static const BYTE openExp[6] = { 0x0F, 0x85, 0x03, 0x02, 0x00, 0x00 };   /* jne (open animation not finished) */
    static const BYTE closeExp[2] = { 0x75, 0x63 };                           /* jne (close animation still playing) */
    BYTE *po = (BYTE *)0x0070F0AC, *pc = (BYTE *)0x0070F3E4;
    int ok = 0; DWORD old;
    if (!memcmp(po, openExp, 6)) { VirtualProtect(po, 6, PAGE_EXECUTE_READWRITE, &old); memset(po, 0x90, 6); VirtualProtect(po, 6, old, &old); ok++; }
    if (!memcmp(pc, closeExp, 2)) { VirtualProtect(pc, 2, PAGE_EXECUTE_READWRITE, &old); memset(pc, 0x90, 2); VirtualProtect(pc, 2, old, &old); ok++; }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    Log("Instant Pip-Boy: %d of 2 animation waits removed", ok);
}

/* ---- Real-time Pip-Boy -----------------------------------------------------------------
   The game pauses the world for every menu. Its main loop does keep the world running while a menu is
   open when one engine flag (0x11DEA2D) says so; while the Pip-Boy is up FNVR sets that flag, so people,
   creatures, combat, the clock and the weather carry on. Menu controls stay menu controls.
   [Controls] bPipBoyRealTime=1 (F10 toggles it in game). */
static bool g_pipRealTime = true;
typedef uint8_t (__attribute__((thiscall)) *RealTimeCheck_t)(void *self, int slot);
static RealTimeCheck_t o_RealTimeCheck = (RealTimeCheck_t)0x00701450;
static uint8_t __attribute__((thiscall)) Hook_RealTimeCheck(void *self, int slot)
{
    uint8_t r = o_RealTimeCheck(self, slot);
    if (!r && g_pipRealTime && g_wristPip) {
        int m = PipBoyMode();
        if (m == 2 || m == 3 || m == 5) return 1;
    }
    return r;
}
/* Safer way: right after the main loop records "a menu is open", clear that note while the Pip-Boy is fully
   open, so the loop runs exactly its normal-gameplay path (the Pip-Boy itself still works as a menu). */
typedef uint8_t (__cdecl *AfterMenuFlag_t)(void);
static AfterMenuFlag_t o_AfterMenuFlag = (AfterMenuFlag_t)0x007050D0;
/* The game runs NPC thinking on helper threads that stall while a menu is open, so while the world runs
   behind the Pip-Boy, switch the game to its single-thread path (iNumHWThreads = 1). Switched only while
   those threads are idle (0x11DFA19 clear), so a running job is never cut off. */
static int g_hwThreadsSaved = -1;
static bool g_menuFlagCleared;
static uint8_t __cdecl Hook_AfterMenuFlag(void)
{
    int *hw = (int *)(0x011C3EA4 + 4);
    bool idle = *(volatile uint8_t *)0x011DFA19 == 0;
    g_menuFlagCleared = false;
    static double open3 = 0;
    if (PipBoyMode() == 3) { if (!open3) open3 = QpcMs(); } else open3 = 0;
    bool pipWant = g_pipRealTime && g_wristPip && open3 != 0;
    bool liveMenu = !pipWant && PipBoyMode() == 0 && g_inGameplay && LiveMenu_Check();
    {   /* a live menu fading out: stay live a moment longer, so leaving a conversation doesn't flash the flat menu screen */
        static double lastLive = 0;
        if (liveMenu) lastLive = QpcMs();
        else if (g_liveMenuNow && !g_liveBlocked && !pipWant && QpcMs() - lastLive < 800.0 && IsMenuMode()) liveMenu = true;   /* (only while the game is still in its menu state) */
    }
    if (liveMenu != g_liveMenuNow) { g_liveMenuNow = liveMenu; Log("Live menus: %s", liveMenu ? "a menu is up - the world keeps running, you stay in it" : "menu closed"); }
    bool want = pipWant || liveMenu;
    if (want) {
        if (g_hwThreadsSaved < 0 && idle) { g_hwThreadsSaved = *hw; *hw = 1; Log("Real-time Pip-Boy: world running (single-thread AI while open, was %d threads)", g_hwThreadsSaved); }
        if (g_hwThreadsSaved >= 0) { *(volatile uint8_t *)0x011DEA2B = 0; g_menuFlagCleared = true; }
    } else if (g_hwThreadsSaved >= 0 && idle) {
        *hw = g_hwThreadsSaved; g_hwThreadsSaved = -1;
        Log("Real-time Pip-Boy: world paused/normal again");
    }
    return o_AfterMenuFlag();
}
/* The frame-drawing dispatch picks the menu drawing path (which also renders the Pip-Boy's screen) only
   while the menu flag is set - so set it back just for that call. */
typedef void (__attribute__((thiscall)) *RenderDispatch_t)(void *self);
static RenderDispatch_t o_RenderDispatch = (RenderDispatch_t)0x0086F450;
static void __attribute__((thiscall)) Hook_RenderDispatch(void *self)
{
    bool restore = g_menuFlagCleared && !g_liveMenuNow && *(volatile uint8_t *)0x011DEA2B == 0;   /* (live menus: the normal frame, the menu is part of the interface it draws) */
    if (restore) *(volatile uint8_t *)0x011DEA2B = 1;
    /* the game re-places its Pip-Boy menu shape (and its blending) during its own update: put ours back
       right before the frame is drawn, so the menu stays exactly where you set it */
    if (g_pipOpen && g_pipModeRaw == 3 && g_lastFP) {
        char *im = *(char **)0x011D8A80; char *pm = Rd(im, 0x178) ? *(char **)(im + 0x174) : NULL;
        g_ppApplyOnly = true; PipScreen_Offset(g_lastFP); g_ppApplyOnly = false;
        {   /* diagnostics every few seconds: is FNVR drawing the menu, and where */
            static double nl = 0; char *menu = Rd(pm, 8) ? *(char **)(pm + 4) : NULL;
            if (QpcMs() > nl && IsNiObj(menu)) { nl = QpcMs() + 5000;
                Log("PIPDIAG FNVR draws the menu %d, game copy scale %.4f, screen spot %s %.2f %.2f %.2f size %.2f x %.2f, picture area %.2f-%.2f",
                    (int)g_pipScrDrawing, *(float *)(menu + NIAV_LOCAL_SCALE), g_pipScrUser ? "saved" : "auto",
                    g_pipScrC[0], g_pipScrC[1], g_pipScrC[2], g_pipScrW, g_pipScrH, g_pipScrUV[0], g_pipScrUV[2]); }
        }
    }
    if (g_pipOpen && g_pipModeRaw == 3 && QpcMs() - g_pipQuadLast.at > 40.0) {   /* (the screen spot, in case this frame isn't posed by FNVR) */
        char *pl = (char *)GetPlayer(); char *sg = *(char **)SCENEGRAPH_PTR_ADDR; char *cm = sg ? *(char **)(sg + SCENEGRAPH_CAMERA) : NULL;
        if (pl && cm) PipScr_Prepare(pl, cm);
    }
    bool live = restore && g_pipWorldDraw && g_pipOpen && g_pipModeRaw == 3;
    {   /* the one-frame trace: a moment after the Pip-Boy opens, once for paused and once for running */
        static double openAt = 0; static bool doneLive = false, donePaused = false;
        bool open = g_pipOpen && g_pipModeRaw == 3;
        if (!open) openAt = 0; else if (!openAt) openAt = QpcMs();
        bool isLive = restore != 0;
        if (open && openAt && QpcMs() - openAt > 1500.0 && !g_trOn && !(isLive ? doneLive : donePaused)) {
            if (isLive) doneLive = true; else donePaused = true;
            g_trMode = isLive ? "running" : "paused"; g_trOn = 1; g_trLines = 0; g_trDraws = 0;
            Log("TRACE ===== one frame with the Pip-Boy open, world %s (live world pass %s) =====", g_trMode, g_pipWorldDraw ? "on" : "off");
            IDirect3DSurface9 *cur = NULL; if (g_hookedDevice && SUCCEEDED(IDirect3DDevice9_GetRenderTarget(g_hookedDevice, 0, &cur)) && cur) { Tr_Surf("(already drawing into)", cur); IDirect3DSurface9_Release(cur); }
        }
    }
    if (g_trOn) { Tr_Flush(); Log("TRACE >>> game frame pass 1 (menu flag %d)", (int)*(volatile uint8_t *)0x011DEA2B); }
    if (live && g_pipWorldFirst) {
        /* the live world first, then the Pip-Boy path on top of it: that path doesn't clear the picture, it only
           draws your Pip-Boy (and its menu) over whatever is there - so the menu ends up over the moving world */
        *(volatile uint8_t *)0x011DEA2B = 0;
        o_RenderDispatch(self);
        *(volatile uint8_t *)0x011DEA2B = 1;
        static int wl = 0; if (wl++ < 2) Log("Real-time Pip-Boy: live world drawn first, then the Pip-Boy and its menu on top");
        bool pmWrap = PM_Wanted();
        if (pmWrap) PM_Begin(2);
        o_RenderDispatch(self);
        if (pmWrap) PM_End();
        *(volatile uint8_t *)0x011DEA2B = 0;
        return;
    }
    bool pmWrap = PM_Wanted();
    if (pmWrap) PM_Begin(2);
    o_RenderDispatch(self);   /* (the menu drawing path: it draws the Pip-Boy menu picture - but not the world) */
    if (pmWrap) PM_End();
    if (g_trOn == 1) { Tr_Flush(); Log("TRACE <<< end of pass 1"); char nm[64]; snprintf(nm, sizeof nm, "FNVR_trace_%s_after_pass1.bmp", g_trMode); if (g_hookedDevice) SaveBackbuffer(g_hookedDevice, nm); g_trOn = 2; }
    if (restore) *(volatile uint8_t *)0x011DEA2B = 0;
    if (restore && g_pipWorldDraw && g_pipOpen && g_pipModeRaw == 3) {

        /* then draw the frame again the normal way, so the world behind the Pip-Boy keeps moving instead of
           standing still as a picture */
        static int wl = 0; if (wl++ < 2) Log("Real-time Pip-Boy: drawing the live world behind the Pip-Boy too");
        bool pmWrap2 = PM_Wanted();
        if (g_trOn) { Tr_Flush(); Log("TRACE >>> game frame pass 2 - the live world (menu flag %d)", (int)*(volatile uint8_t *)0x011DEA2B); }
        if (pmWrap2) PM_Begin(2);
        o_RenderDispatch(self);
        if (pmWrap2) PM_End();
        if (g_trOn) { Tr_Flush(); Log("TRACE <<< end of pass 2"); }
    }
}
/* ---- Quick-loot menu in the middle of your view ----------------------------------------------------
   The quick-loot window (JIP Loot Menu) sits at the right of the screen, which in the headset ends up past
   your right hand. While it's showing, its left/right position is set so it's centred in front of you. */
static const char *Tile_Name(char *t) { const char *n = Rd(t, 0x28) ? *(const char **)(t + 0x20) : NULL; return Rd((void *)n, 1) ? n : ""; }
static float *Tile_Val(char *t, uint32_t id)
{
    if (!Rd(t, 0x30)) return NULL;
    char **vals = *(char ***)(t + 0x14); uint32_t n = *(uint32_t *)(t + 0x18);
    if (!vals || n > 4096 || !Rd(vals, n * 4)) return NULL;
    for (uint32_t i = 0; i < n; i++) { char *v = vals[i]; if (Rd(v, 0x10) && *(uint32_t *)v == id) return (float *)(v + 8); }
    return NULL;
}
static char *Tile_FindName(char *t, int depth, int maxDepth)
{
    if (!Rd(t, 0x30) || depth > maxDepth) return NULL;
    const char *nm = Tile_Name(t);
    if (depth > 0) {
        char low[64]; snprintf(low, sizeof low, "%s", nm); for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
        if (strstr(low, "jlm") || strstr(low, "lootmenu") || strstr(low, "quickloot")) return t;
    }
    char *node = *(char **)(t + 0x04);   /* first child */
    for (int k = 0; node && k < 400 && Rd(node, 12); k++, node = *(char **)node) {
        char *r = Tile_FindName(*(char **)(node + 8), depth + 1, maxDepth);
        if (r) return r;
    }
    return NULL;
}
/* What you're looking at: the game's crosshair target. Container = the quick-loot window is for it. */
static int Loot_CrossKind(char **refOut)
{
    char *im = *(char **)0x011D8A80; *refOut = NULL;
    char *ref = Rd(im, 0xD4) ? *(char **)(im + 0xD0) : NULL;
    if (!Rd(ref, 0x28)) return 0;
    uint8_t rt = *(uint8_t *)(ref + 4); if (rt < 0x3A || rt > 0x3C) return 0;
    char *base = *(char **)(ref + 0x20); if (!Rd(base, 8)) return 0;
    *refOut = ref;
    uint8_t bt = *(uint8_t *)(base + 4);
    if (bt == 0x1B) return 1;                    /* container */
    if (rt == 0x3B || rt == 0x3C) return 2;      /* person / creature */
    return 3;
}
/* ---- HUD parts on/off ----------------------------------------------------------------------------
   The game's HUD in the headset: each part can be hidden ([HUD] in FNVR.ini, or the VR settings). The parts
   are found by name on the HUD, so HUD mods that rename them may need a look at the log (HUD parts are listed). */
static bool g_hudShow = true, g_hudHealth = true, g_hudAP = true, g_hudSneak = true, g_hudCompass = true, g_hudAmmo = true, g_hudCross = true;
/* Each HUD part is a list of the game's HUD pieces by exact name ("Group/piece" = a piece inside a group;
   "Group/*-skip" = every piece of a group except one - the vanilla health group also holds the compass). */
#define HUD_MAXT 12
typedef struct { const char *names[6]; bool *on; char *tile[HUD_MAXT]; float saved[HUD_MAXT]; int n; bool hidden; } HudPart;
static HudPart g_hudParts[] = {
    { { "HitPoints/*-compass_window", NULL }, &g_hudHealth },
    { { "ActionPoints/meter", "ActionPoints/MeterBackground", "ActionPoints/right_bracket", NULL }, &g_hudAP },
    { { "SneakMeter", NULL }, &g_hudSneak },
    { { "HitPoints/compass_window", NULL }, &g_hudCompass },
    { { "ActionPoints/justify_right_text", "AmmoTypeLabel", "CNDArrows", NULL }, &g_hudAmmo },
    { { "ReticleCenter", "JDC", NULL }, &g_hudCross },
};
static const char *g_hudPartName[] = { "health", "AP", "sneak", "compass", "ammo", "crosshair" };
/* ---- Fire rate (all guns) and the Ranger Sequoia's cylinder ----------------------------------------
   [Controls] iFireRatePercent: 100 = the game's own. The gun in your hand gets its fire rate (automatic guns)
   and attack animation speed (everything else) scaled; its own values come back when you put it away.
   The Ranger Sequoia is set back to its 5-round cylinder ([Controls] bSequoiaFiveRounds). */
static int g_fireRatePct = 100; static bool g_seqFive = true; static bool g_revFast = true;   /* [Controls] bRevolverFastFire */
/* A revolver (by its name or its model file): fires as fast as you pull the trigger. */
static bool Weap_IsRevolver(char *w)
{
    if (!w) return false;
    char t[200] = "";
    const char *nm = (!IsBadReadPtr(w + 0x34, 4) && *(const char **)(w + 0x34) && !IsBadReadPtr(*(const char **)(w + 0x34), 1)) ? *(const char **)(w + 0x34) : "";
    const char *mp = (!IsBadReadPtr(w + 0x40, 4) && *(const char **)(w + 0x40) && !IsBadReadPtr(*(const char **)(w + 0x40), 1)) ? *(const char **)(w + 0x40) : "";
    snprintf(t, sizeof t, "%s|%s", nm, mp); for (char *c = t; *c; c++) *c = (char)tolower((unsigned char)*c);
    static const char *keys[] = { "revolver", "sequoia", "magnum", "police pistol", ".357", "357", ".44", "lucky", "that gun", "thatgun", "5.56mm pistol", "556pistol", "mysterious", "colt", "schofield", "scofield", "peacemaker", "single action", "six-shooter", "sixshooter", "s&w", "smith & wesson", "smith and wesson", "smith&wesson", "smithwesson", ".500", "500 magnum", "500magnum", "model 500", "m500", "stranger", "wesson" };
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) if (strstr(t, keys[i])) return true;
    return false;
}
static uint32_t FBits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }   /* (script float arguments go in as their raw 4 bytes) */
static bool Scr_Num(void *scr, void *arg, double *out)
{   /* run a little function and get the number it returns */
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    if (!g_scriptIntf || !g_scriptIntf->CallFunction || !scr) return false;
    struct { double num; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    bool ok = ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 1, arg);
    if (!ok || res.type != 1) return false;
    *out = res.num; return true;
}
static char *Scr_Ref(void *scr, void *arg)
{   /* run a little function and get the reference it returns */
    typedef bool (*CallFn_t)(void *, void *, void *, void *, uint8_t, ...);
    if (!g_scriptIntf || !g_scriptIntf->CallFunction || !scr) return NULL;
    struct { union { double num; void *form; } u; uint8_t type; uint8_t pad[23]; } res; memset(&res, 0, sizeof res);
    bool ok = ((CallFn_t)g_scriptIntf->CallFunction)(scr, GetPlayer(), NULL, &res, 1, arg);
    if (!ok || res.type != 2) return NULL;
    return (char *)res.u.form;
}
/* ---- stimpaks (main loop side: inventory and using) ---- */
static const char *const g_stimNames[2] = { "Stimpak", "SuperStimpak" };
static int Stim_Pick(void)
{   /* which one to pull out: a Stimpak if you have one, else a Super Stimpak; -1 = none */
    static void *cnt[2]; static bool tried = false;
    if (!tried) { tried = true;
        for (int i = 0; i < 2; i++) { char b[200]; snprintf(b, sizeof b, "ref rWho\nint iVal\nBegin Function { rWho }\nset iVal to rWho.GetItemCount %s\nSetFunctionValue iVal\nEnd\n", g_stimNames[i]);
            cnt[i] = g_scriptIntf && g_scriptIntf->CompileScript ? g_scriptIntf->CompileScript(b) : NULL;
            if (!cnt[i]) Log("Stimpak: couldn't set up counting %s", g_stimNames[i]); } }
    for (int i = 0; i < 2; i++) { double v = 0; if (cnt[i] && Scr_Num(cnt[i], GetPlayer(), &v) && v >= 1.0) return i; }
    return -1;
}
static bool Stim_Begin(int h, int type)
{   /* take one out of your inventory and put it in the world, to be held in your hand */
    static void *place[2]; static bool tried = false;
    if (!tried) { tried = true;
        for (int i = 0; i < 2; i++) { char b[260]; snprintf(b, sizeof b, "ref rWho\nref rNew\nBegin Function { rWho }\nrWho.RemoveItem %s 1 1\nset rNew to rWho.PlaceAtMe %s 1\nSetFunctionValue rNew\nEnd\n", g_stimNames[i], g_stimNames[i]);
            place[i] = g_scriptIntf && g_scriptIntf->CompileScript ? g_scriptIntf->CompileScript(b) : NULL; } }
    if (type < 0 || type > 1 || !place[type]) return false;
    char *ref = Scr_Ref(place[type], GetPlayer());
    if (!ref) { Log("Stimpak: couldn't take one out"); return false; }
    g_stimRef = ref; g_stimRefID = *(uint32_t *)(ref + 0x0C); g_stimType = type; g_stimHand = h; g_stimAt = QpcMs(); g_stimHeld = false;
    Log("Stimpak: %s pulled out with the %s hand (ref %08X)", type ? "Super Stimpak" : "Stimpak", h ? "right" : "left", g_stimRefID);
    return true;
}
static void Stim_Flush(void)
{   /* use it (stabbed) or put it back (let go) - removing the one held in the world either way */
    int what = g_stimDo; if (!what) return;
    g_stimDo = 0;
    static void *useS[2], *backS[2], *backNoRef[2]; static bool tried = false;
    if (!tried) { tried = true;
        for (int i = 0; i < 2; i++) { char b[300];
            snprintf(b, sizeof b, "ref rWho\nref rStim\nBegin Function { rWho, rStim }\nrStim.Disable\nrStim.MarkForDelete\nrWho.AddItem %s 1 1\nrWho.EquipItem %s 0 1\nEnd\n", g_stimNames[i], g_stimNames[i]);
            useS[i] = g_scriptIntf->CompileScript(b);
            snprintf(b, sizeof b, "ref rWho\nref rStim\nBegin Function { rWho, rStim }\nrStim.Disable\nrStim.MarkForDelete\nrWho.AddItem %s 1 1\nEnd\n", g_stimNames[i]);
            backS[i] = g_scriptIntf->CompileScript(b);
            snprintf(b, sizeof b, "ref rWho\nBegin Function { rWho }\nrWho.AddItem %s 1 1\nEnd\n", g_stimNames[i]);
            backNoRef[i] = g_scriptIntf->CompileScript(b); } }
    int t = g_stimDoType & 1; char *pl = (char *)GetPlayer();
    bool refOk = g_stimDoRef && LookupForm(g_stimDoID) == (void *)g_stimDoRef;
    if (what == 1 && refOk && useS[t]) { g_scriptIntf->CallFunctionAlt(useS[t], pl, 2, pl, g_stimDoRef); Log("Stimpak: used (%s)", t ? "Super Stimpak" : "Stimpak"); }
    else if (refOk && backS[t]) { g_scriptIntf->CallFunctionAlt(backS[t], pl, 2, pl, g_stimDoRef); Log("Stimpak: put back"); }
    else if (backNoRef[t]) { g_scriptIntf->CallFunctionAlt(backNoRef[t], pl, 1, pl); Log("Stimpak: put back (the one in your hand was gone)"); }
    g_stimDoRef = NULL;
}
static void FireRate_Tick(void)
{
    if (!g_inGameplay || !g_scriptIntf || !g_scriptIntf->CompileScript || !g_scriptIntf->CallFunctionAlt) return;
    static void *getFR, *getAM, *setBoth, *seq; static bool failed = false, seqDone = false, tried = false;
    if (!tried) {   /* (each one on its own, so one that doesn't compile doesn't take the others with it) */
        tried = true;
        getFR = g_scriptIntf->CompileScript("ref rWeap\nfloat fVal\nBegin Function { rWeap }\nset fVal to GetWeaponFireRate rWeap\nSetFunctionValue fVal\nEnd\n");
        getAM = g_scriptIntf->CompileScript("ref rWeap\nfloat fVal\nBegin Function { rWeap }\nset fVal to GetWeaponAnimMult rWeap\nSetFunctionValue fVal\nEnd\n");
        setBoth = g_scriptIntf->CompileScript("ref rWeap\nfloat fRate\nfloat fMult\nBegin Function { rWeap, fRate, fMult }\nSetWeaponFireRate fRate rWeap\nSetWeaponAnimMult fMult rWeap\nEnd\n");
        if (!setBoth) setBoth = g_scriptIntf->CompileScript("ref rWeap\nfloat fRate\nfloat fMult\nBegin Function { rWeap, fRate, fMult }\nSetWeaponFireRate fRate rWeap\nEnd\n");
        seq = g_scriptIntf->CompileScript("ref rWeap\nBegin Function { }\nset rWeap to WeapNVRangerSequoia\nSetWeaponClipRounds 5 rWeap\nEnd\n");
        Log("Fire rate: scripts - read rate %s, read attack speed %s, set %s; Sequoia %s", getFR ? "ok" : "FAILED", getAM ? "ok" : "FAILED", setBoth ? "ok" : "FAILED", seq ? "ok" : "FAILED");
        if (!setBoth || (!getFR && !getAM)) failed = true;
    }
    if (!seqDone && g_seqFive && seq) { seqDone = true; g_scriptIntf->CallFunctionAlt(seq, GetPlayer(), 0); Log("Ranger Sequoia: cylinder set to 5 rounds"); }
    if (failed) return;
    static void *cur = NULL; static float fr0 = 0, am0 = 0; static int curPct = 100;
    void *w = g_weaponOutThisFrame ? PlayerWeaponForm() : NULL;
    static bool curRev = false, curRevOn = false;
    if (w != cur || (w && (curPct != g_fireRatePct || curRevOn != g_revFast))) {
        if (cur && (fr0 > 0 || am0 > 0) && (curPct != 100 || (curRev && curRevOn))) g_scriptIntf->CallFunctionAlt(setBoth, GetPlayer(), 3, cur, FBits(fr0), FBits(am0));   /* the old gun's own values back */
        if (w != cur) { cur = w; double a = 0, b = 0; fr0 = getFR && Scr_Num(getFR, w, &a) ? (float)a : 0.0f; am0 = getAM && Scr_Num(getAM, w, &b) ? (float)b : 0.0f;
            if (w && fr0 <= 0 && am0 <= 0) { static int nl = 0; if (nl++ < 5) Log("Fire rate: couldn't read this gun's own rate"); } curPct = 100; curRev = Weap_IsRevolver((char *)w);
            if (curRev) Log("Fire rate: revolver in your hand - it fires as fast as you pull the trigger%s", g_revFast ? "" : " (off in settings)"); }
        curRevOn = g_revFast;
        float k = g_fireRatePct / 100.0f;
        if (curRev && g_revFast) k = fmaxf(k, 4.0f);   /* revolvers: the hammer cycle sped up so every pull fires */
        if (cur && k != 1.0f && (fr0 > 0 || am0 > 0)) {
            g_scriptIntf->CallFunctionAlt(setBoth, GetPlayer(), 3, cur, FBits(fr0 > 0 ? fr0 * k : fr0), FBits(am0 > 0 ? am0 * k : 1.0f * k));
            static int nl = 0; if (nl++ < 10) Log("Fire rate: %d%% on the gun in your hand (rate %.2f -> %.2f, attack speed %.2f -> %.2f)", g_fireRatePct, fr0, fr0 * k, am0, am0 * k);
        }
        curPct = g_fireRatePct;
    }
}
static void Hud_Load(void)
{
    g_hudShow = GetPrivateProfileIntA("HUD", "bShowHUD", 1, g_iniPath) != 0;
    g_fireRatePct = GetPrivateProfileIntA("Controls", "iFireRatePercent", 100, g_iniPath); if (g_fireRatePct < 25 || g_fireRatePct > 400) g_fireRatePct = 100;
    g_seqFive = GetPrivateProfileIntA("Controls", "bSequoiaFiveRounds", 1, g_iniPath) != 0;
    g_revFast = GetPrivateProfileIntA("Controls", "bRevolverFastFire", 1, g_iniPath) != 0;
    g_hudHealth = GetPrivateProfileIntA("HUD", "bHealth", 1, g_iniPath) != 0;
    g_hudAP = GetPrivateProfileIntA("HUD", "bActionPoints", 1, g_iniPath) != 0;
    g_hudSneak = GetPrivateProfileIntA("HUD", "bSneak", 1, g_iniPath) != 0;
    g_hudCompass = GetPrivateProfileIntA("HUD", "bCompass", 1, g_iniPath) != 0;
    g_hudAmmo = GetPrivateProfileIntA("HUD", "bAmmo", 1, g_iniPath) != 0;
    g_hudCross = GetPrivateProfileIntA("HUD", "bCrosshair", 1, g_iniPath) != 0;
}
static char *Hud_Child(char *t, const char *name, int idx, int *count)
{   /* the idx-th child named 'name' (or any child but 'name' when name starts with '*-') */
    char *node = Rd(t, 0x30) ? *(char **)(t + 0x04) : NULL; int seen = 0;
    bool except = name[0] == '*' && name[1] == '-'; const char *nm = except ? name + 2 : name;
    for (int m = 0; node && m < 60 && Rd(node, 12); m++, node = *(char **)node) {
        char *c = *(char **)(node + 8);
        bool eq = !_stricmp(Tile_Name(c), nm);
        if (except ? !eq : eq) { if (seen == idx) { if (count) *count = seen + 1; return c; } seen++; }
    }
    if (count) *count = seen; return NULL;
}
static void Hud_Tick(void)
{
    if (!g_inGameplay) return;
    char *hud = *(char **)0x011D96C0; char *root = Rd(hud, 8) ? *(char **)(hud + 4) : NULL;
    if (!Rd(root, 0x30)) return;
    static char *forRoot = NULL;
    if (forRoot != root) {   /* (new HUD: find its parts by name) */
        forRoot = root;
        for (unsigned k = 0; k < sizeof g_hudParts / sizeof g_hudParts[0]; k++) {
            HudPart *hp = &g_hudParts[k]; hp->n = 0; hp->hidden = false;
            for (int q = 0; q < 6 && hp->names[q]; q++) {
                char grp[48]; snprintf(grp, sizeof grp, "%s", hp->names[q]); char *slash = strchr(grp, '/'); if (slash) *slash = 0;
                for (int gi = 0; gi < 4; gi++) {
                    char *g = Hud_Child(root, grp, gi, NULL); if (!g) break;
                    if (!slash) { if (hp->n < HUD_MAXT) hp->tile[hp->n++] = g; continue; }
                    for (int ci = 0; ci < HUD_MAXT; ci++) { char *c = Hud_Child(g, slash + 1, ci, NULL); if (!c) break; if (hp->n < HUD_MAXT) hp->tile[hp->n++] = c; }
                }
            }
            Log("HUD: %s - %d piece(s) found", g_hudPartName[k], hp->n);
        }
    }
    for (unsigned k = 0; k < sizeof g_hudParts / sizeof g_hudParts[0]; k++) {
        HudPart *hp = &g_hudParts[k];
        bool hide = !g_hudShow || !*hp->on;
        for (int q = 0; q < hp->n; q++) {
            char *t = hp->tile[q]; if (!Rd(t, 0x30)) continue;
            float *vis = Tile_Val(t, 0xFA3);
            if (hide) {
                if (!hp->hidden) hp->saved[q] = vis ? *vis : 1.0f;
                if (!vis || *vis != 0.0f) ((TileSetFloat_t)0x00A012D0)(t, 0xFA3, 0.0f, true);
            } else if (hp->hidden) ((TileSetFloat_t)0x00A012D0)(t, 0xFA3, hp->saved[q] != 0.0f ? hp->saved[q] : 1.0f, true);
        }
        hp->hidden = hide;
    }
}
static void LootMenu_Tick(void)
{
    g_lootShown = false;
    bool diag = g_lootDiag; g_lootDiag = false;
    if (!g_inGameplay) return;
    char *xref = NULL; int xk = Loot_CrossKind(&xref);
    char *hud = *(char **)0x011D96C0; char *root = Rd(hud, 8) ? *(char **)(hud + 4) : NULL;
    if (!Rd(root, 0x30)) return;
    static char *tile = NULL, *forRoot = NULL; static double nextFind = 0; static bool listed = false;
    if (forRoot != root) { forRoot = root; tile = NULL; nextFind = 0; }
    if (!tile && QpcMs() > nextFind) {
        nextFind = QpcMs() + 3000.0;
        tile = Tile_FindName(root, 0, 5);
        if (tile) Log("Loot menu: found '%s' on the HUD - it will be centred in front of you", Tile_Name(tile));
        else if (!listed) {
            listed = true;
            char buf[600] = ""; char *node = *(char **)(root + 0x04);
            for (int k = 0; node && k < 60 && Rd(node, 12); k++, node = *(char **)node) {
                size_t l = strlen(buf); if (l + 40 > sizeof buf) break;
                snprintf(buf + l, sizeof buf - l, "%s%s", l ? ", " : "", Tile_Name(*(char **)(node + 8)));
            }
            Log("Loot menu: not on the HUD yet (HUD parts: %s)", buf);
        }
    }
    if (!tile || !Rd(tile, 0x30)) { tile = NULL; if (xk == 1) g_lootShown = true; return; }
    float *vis = Tile_Val(tile, 0xFA3), *x = Tile_Val(tile, 0xFA1), *w = Tile_Val(tile, 0xFB1);
    float *al = Tile_Val(tile, 0xFA9);
    bool tileUp = x && !(vis && *vis == 0.0f) && (!al || *al > 1.0f);
    /* shown: the window says so, or you're looking at a container (the window is up for those) */
    g_lootShown = tileUp || xk == 1;
    if (diag) { static int nd = 0; if (nd++ < 40) {
        int kids = 0; char *node = *(char **)(tile + 0x04); for (; node && kids < 99 && Rd(node, 12); node = *(char **)node) kids++;
        Log("Loot check on right grip: window visible %.0f alpha %.0f x %.0f width %.0f parts %d | looking at %s | -> %s",
            vis ? *vis : -1.0f, al ? *al : -1.0f, x ? *x : -1.0f, w ? *w : -1.0f, kids,
            xk == 1 ? "a container" : xk == 2 ? "a person/creature" : xk == 3 ? "something else" : "nothing",
            g_lootShown ? "loot window UP (grip takes)" : "no loot window (grip activates)"); } }
    if (!x || (vis && *vis == 0.0f)) return;
    {   static bool was = false; if (g_lootShown && !was && g_lootControls && g_motionControls) {
            static int hl = 0; if (hl++ < 3) UIMessage("Loot: A take, right stick scroll, B take all, grip open"); }
        was = g_lootShown; }
    if (!g_lootCenter) return;
    /* where its parent starts, and how wide the screen is (in menu units) */
    float parentX = 0, screenW = 0; char *p = *(char **)(tile + 0x28);
    for (int k = 0; p && k < 12 && Rd(p, 0x30); k++, p = *(char **)(p + 0x28)) {
        float *px = Tile_Val(p, 0xFA1), *pw = Tile_Val(p, 0xFB1);
        if (px) parentX += *px;
        if (pw && *pw > screenW) screenW = *pw;
    }
    if (!(screenW > 100.0f)) screenW = 1280.0f;
    float tw = w && *w > 1.0f ? *w : 300.0f;
    float want = (screenW - tw) * 0.5f - parentX + g_lootOffX;
    if (fabsf(*x - want) > 0.5f) {
        ((TileSetFloat_t)0x00A012D0)(tile, 0xFA1, want, true);
        static int nl = 0; if (nl++ < 3) Log("Loot menu: moved to the middle (x %.0f, width %.0f of %.0f)", want, tw, screenW);
    }
}
/* ======================================================================= */
/* Kid-friendly options                                                    */
/* ======================================================================= */
/* Bloody Mess bodies bursting, limbs coming off, and blood. Each is a switch; turning one back on puts the
   game's own values back exactly.
   - Bloody Mess / dismemberment: the game settings that give the chance of a body or limb bursting or being
     cut off are set to 0.
   - Blood: on-screen blood splatter and the blood left by cut-off limbs are set to 0, and every "hit flesh"
     effect (the blood spray and the blood decal a bullet or blade leaves on a body or wall) is swapped for a
     copy with no blood: it keeps the hit sound and uses the same set's cloth/dust puff and no decal. */
typedef struct { uintptr_t addr; const char *name; int grp; uint32_t orig; bool have; } KidGS;
enum { KG_MESS = 1, KG_DISM, KG_BLOOD, KG_GORE, KG_BLOODSTR };
static KidGS g_kidGS[] = {
    { 0x11CF18C, "iMessTorsoExplodeChance", KG_MESS }, { 0x11CEE3C, "iMessIntactLimbExplodeBonus", KG_MESS },
    { 0x11CE188, "iMessTargetedLimbExplodeBonus", KG_MESS }, { 0x11CFE90, "iMessCrippledLimbExplodeBonus", KG_MESS },
    { 0x11CE2B4, "iMessIntactLimbDismemberChance", KG_DISM }, { 0x11CF2EC, "iCombatDismemberPartChance", KG_DISM },
    { 0x11CE2FC, "iCombatExplodePartChance", KG_DISM },
    { 0x11DF7F8, "bDisableAllGore:General", KG_GORE },
    { 0x11CF2B8, "iBloodSplatterMaxCount", KG_BLOOD }, { 0x11CF4C8, "iDismemberBloodDecalCount", KG_BLOOD },
    { 0x11CEE24, "fBloodSplatterCountBase", KG_BLOOD }, { 0x11CED28, "fBloodSplatterCountDamageMult", KG_BLOOD },
    { 0x11CEBB0, "sBloodParticleDefault", KG_BLOODSTR }, { 0x11CE1A0, "sBloodParticleMeleeDefault", KG_BLOODSTR },
};
#define KID_NGS ((int)(sizeof g_kidGS / sizeof g_kidGS[0]))
static bool Kid_GSOk(KidGS *g)
{
    char *st = (char *)g->addr;
    if (!Rd(st, 12)) return false;
    const char *nm = *(const char **)(st + 8);
    size_t L = strlen(g->name);
    return Rd((void *)nm, L + 1) && !_strnicmp(nm, g->name, L + 1);
}
/* The game's own "take this limb off" routine: every bursting body and severed limb goes through it.
   With dismemberment off it does nothing; with only Bloody Mess bursting off, bursting calls do nothing. */
typedef int (__attribute__((thiscall)) *Dismember_t)(void *actor, uint32_t a1, uint32_t part, uint32_t explode, uint32_t a4, uint32_t a5, uint32_t a6);
static Dismember_t o_Dismember = NULL;
static int __attribute__((thiscall)) Hook_Dismember(void *actor, uint32_t a1, uint32_t part, uint32_t explode, uint32_t a4, uint32_t a5, uint32_t a6)
{
    bool block = !g_kidDismember || ((explode & 0xFF) && !g_kidMess);
    static int nl = 0;
    if (nl++ < 12) Log("Kid-friendly: the game %s a %s (part %u)%s", block ? "wanted" : "is doing", (explode & 0xFF) ? "body/limb burst" : "limb cut-off", part, block ? " - stopped" : "");
    if (block) return 0;
    return o_Dismember(actor, a1, part, explode, a4, a5, a6);
}
static int Hook_InsnLen(const BYTE *p)
{
    BYTE b = p[0];
    if (b == 0x55 || b == 0x53 || b == 0x56 || b == 0x57 || b == 0x51 || b == 0x50 || b == 0x52) return 1;
    if (b == 0x8B && (p[1] == 0xEC || p[1] == 0xFF || p[1] == 0xF1 || p[1] == 0xD9 || p[1] == 0xC1)) return 2;
    if (b == 0x83 && p[1] == 0xEC) return 3;
    if (b == 0x81 && p[1] == 0xEC) return 6;
    if (b == 0x6A) return 2;
    if (b == 0x68) return 5;
    if (b == 0x64 && p[1] == 0xA1) return 6;
    return 0;
}
static void InstallDismemberHook(void)
{
    static bool done = false; if (done) return; done = true;
    BYTE *fn = (BYTE *)0x008B52A0;
    if (!Rd(fn, 16)) return;
    Log("Kid-friendly: limb routine starts %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", fn[0], fn[1], fn[2], fn[3], fn[4], fn[5], fn[6], fn[7], fn[8], fn[9]);
    BYTE *tramp = (BYTE *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return;
    int n = 0;
    if (fn[0] == 0xE9) {   /* another mod got there first: go through it */
        uint32_t other = (uint32_t)(uintptr_t)fn + 5 + *(int32_t *)(fn + 1);
        tramp[0] = 0xE9; *(int32_t *)(tramp + 1) = (int32_t)(other - ((uintptr_t)tramp + 5)); n = 5;
    } else {
        while (n < 5) { int L = Hook_InsnLen(fn + n); if (!L) { Log("Kid-friendly: limb routine not recognised - relying on the game settings only"); return; } n += L; }
        memcpy(tramp, fn, n);
        tramp[n] = 0xE9; *(int32_t *)(tramp + n + 1) = (int32_t)(((uintptr_t)fn + n) - ((uintptr_t)tramp + n + 5));
    }
    FlushInstructionCache(GetCurrentProcess(), tramp, 64);
    o_Dismember = (Dismember_t)(uintptr_t)tramp;
    DWORD old; VirtualProtect(fn, 8, PAGE_EXECUTE_READWRITE, &old);
    fn[0] = 0xE9; *(int32_t *)(fn + 1) = (int32_t)((uintptr_t)Hook_Dismember - ((uintptr_t)fn + 5));
    for (int i = 5; i < n; i++) fn[i] = 0x90;
    VirtualProtect(fn, 8, old, &old); FlushInstructionCache(GetCurrentProcess(), fn, 8);
    Log("Kid-friendly: limb routine hooked");
}
#define VTBL_IMPACTDATASET 0x0103323C
#define VTBL_IMPACTDATA    0x01032F6C
static struct { char *set; int mat; char *orig; } g_kidSwap[4096]; static int g_kidSwapN;
static struct { char *orig, *clean; } g_kidClone[512]; static int g_kidCloneN;
static bool g_kidSwapped;
static const char *g_kidCleanPath = NULL;   /* a clean (non-blood) hit effect model, for the blood particle settings */
static bool Kid_IsImpact(char *d) { return Rd(d, 0x78) && *(uint32_t *)d == VTBL_IMPACTDATA; }
static char *Kid_Clean(char *org, char *set)
{
    for (int i = 0; i < g_kidCloneN; i++) if (g_kidClone[i].orig == org) return g_kidClone[i].clean;
    static const int pref[] = { 7, 1, 0, 5, 2, 4, 9, 3 };   /* cloth, dirt, stone, wood, grass, metal... - anything not flesh */
    char *src = NULL;
    for (int k = 0; k < 8 && !src; k++) { char *c = *(char **)(set + 0x1C + 4 * pref[k]); if (Kid_IsImpact(c)) src = c; }
    if (!src || g_kidCloneN >= 512) return NULL;
    char *cl = (char *)malloc(0x78);
    if (!cl) return NULL;
    memcpy(cl, org, 0x78);
    memcpy(cl + 0x18, src + 0x18, 0x18);   /* the effect model of the clean hit */
    if (!g_kidCleanPath) { const char *mp = *(const char **)(src + 0x1C); if (Rd((void *)mp, 4) && mp[0]) { g_kidCleanPath = mp; Log("Kid-friendly: clean hit effect '%s'", mp); } }
    *(BYTE *)(cl + 0x44) = 1;               /* no decal */
    *(char **)(cl + 0x48) = NULL;           /* no blood texture */
    g_kidClone[g_kidCloneN].orig = org; g_kidClone[g_kidCloneN].clean = cl; g_kidCloneN++;
    return cl;
}
static void Kid_Blood(bool off)
{
    if (off == g_kidSwapped) return;
    if (!off) {
        for (int i = 0; i < g_kidSwapN; i++) *(char **)(g_kidSwap[i].set + 0x1C + 4 * g_kidSwap[i].mat) = g_kidSwap[i].orig;
        Log("Kid-friendly: blood hit effects back to normal (%d)", g_kidSwapN);
        g_kidSwapN = 0; g_kidSwapped = false; return;
    }
    char *map = *(char **)0x011C54C0;
    if (!Rd(map, 0x10)) { Log("Kid-friendly: the game's form list isn't there - blood effects left alone"); return; }
    uint32_t nb = *(uint32_t *)(map + 4); char **buckets = *(char ***)(map + 8);
    if (!nb || nb > 0x1000000 || !Rd(buckets, nb * 4)) return;
    int sets = 0;
    for (uint32_t b = 0; b < nb; b++)
        for (char *e = buckets[b]; e && Rd(e, 12); e = *(char **)e) {
            char *f = *(char **)(e + 8);
            if (!Rd(f, 0x4C) || *(uint32_t *)f != VTBL_IMPACTDATASET) continue;
            sets++;
            static const int flesh[] = { 6, 10, 11 };   /* organic, organic bug, organic glow */
            for (int k = 0; k < 3; k++) {
                char **slot = (char **)(f + 0x1C + 4 * flesh[k]);
                if (!Kid_IsImpact(*slot) || g_kidSwapN >= 4096) continue;
                char *cl = Kid_Clean(*slot, f);
                if (!cl) continue;
                g_kidSwap[g_kidSwapN].set = f; g_kidSwap[g_kidSwapN].mat = flesh[k]; g_kidSwap[g_kidSwapN].orig = *slot; g_kidSwapN++;
                *slot = cl;
            }
        }
    g_kidSwapped = true;
    Log("Kid-friendly: blood hit effects removed (%d hit kinds in %d sets, %d clean copies)", g_kidSwapN, sets, g_kidCloneN);
}
static void Kids_Apply(void)
{
    if (!g_gameOk) return;
    InstallDismemberHook();
    Kid_Blood(!g_kidBlood);
    int set = 0;
    for (int i = 0; i < KID_NGS; i++) {
        KidGS *g = &g_kidGS[i];
        if (!g->have) {
            if (!Kid_GSOk(g)) { static int nl = 0; if (nl++ < 6) Log("Kid-friendly: game setting %s not found", g->name); continue; }
            g->orig = *(uint32_t *)(g->addr + 4); g->have = true;
            if (g->grp == KG_BLOODSTR) { const char *sv = (const char *)(uintptr_t)g->orig; Log("Kid-friendly: %s = '%s'", g->name, Rd((void *)sv, 4) ? sv : "?"); }
        }
        uint32_t v = g->orig;
        switch (g->grp) {
        case KG_MESS:  if (!g_kidMess || !g_kidDismember) v = 0; break;
        case KG_DISM:  if (!g_kidDismember) v = 0; break;
        case KG_GORE:  if (!g_kidDismember) v = 1; break;
        case KG_BLOOD: if (!g_kidBlood) v = 0; break;
        case KG_BLOODSTR: if (!g_kidBlood && g_kidCleanPath) v = (uint32_t)(uintptr_t)g_kidCleanPath; break;
        }
        *(uint32_t *)(g->addr + 4) = v;
        set++;
    }
    Log("Kid-friendly: Bloody Mess bursting %s, dismemberment %s, blood %s (%d game settings, limb routine %s)",
        g_kidMess && g_kidDismember ? "on" : "OFF", g_kidDismember ? "on" : "OFF", g_kidBlood ? "on" : "OFF", set, o_Dismember ? "hooked" : "not hooked");
}

/* ======================================================================= */
/* In-game settings panel, body calibration and profiles                   */
/* ======================================================================= */
/* Hold both thumbsticks in for a second (or press F11): a settings panel appears in front of you. Point
   at a line with your right hand and pull the trigger, or use either stick (up/down chooses, left/right
   changes). B goes back, and closes the panel from the first page. Every change is saved to FNVR.ini
   straight away and works at once - the few that need a game restart say so.
   Body & profiles: up to four people can each keep their own standing height and arm reach (and
   handedness, aim angle and world size). Calibrating makes the world match your height and sizes the
   in-game arms to yours, so a straight arm is straight in the game too. */

static float IniF(const char *sec, const char *key, float def)
{
    char d[32], b[48]; snprintf(d, sizeof d, "%g", def);
    GetPrivateProfileStringA(sec, key, d, b, sizeof b, g_iniPath);
    float v = (float)atof(b); return isfinite(v) ? v : def;
}
static void IniFW(const char *sec, const char *key, float v, const char *fmt)
{
    char b[48]; snprintf(b, sizeof b, fmt ? fmt : "%.3f", v);
    WritePrivateProfileStringA(sec, key, b, g_iniPath);
}

/* a length for people to read: metres, or feet and inches when US units are chosen */
static void FmtLen(char *out, size_t cap, float m, int dec)
{
    if (!g_imperial) { snprintf(out, cap, "%.*f m", dec, m); return; }
    float in = m / 0.0254f;
    if (fabsf(in) >= 36.0f) {
        int neg = in < 0; float a = fabsf(in); int ft = (int)(a / 12.0f); float r = a - ft * 12.0f;
        if (r >= 11.95f) { ft++; r = 0; }
        snprintf(out, cap, "%s%d ft %.1f in", neg ? "-" : "", ft, r);
    } else snprintf(out, cap, "%.1f in", in);
}
/* ---- body & profiles ---- */

/* The head height that counts as "standing at full height": standing play - your standing eye height; seated play -
   your sitting eye height (sit up straight = the character stands). */
static float Body_StandRef(void) { return g_seated ? g_seatEye : g_bodyEye; }
static void Body_Apply(void)
{
    float upm = g_worldScaleManual;
    float worldEye = g_bodyEye > 0.8f ? g_bodyEye : (g_seated ? 1.65f : 0.0f);   /* (seated: sized like an average standing person unless measured) */
    if (g_bodyScaleWorld && worldEye > 0.8f) {
        float ge = g_gameEyeUnits > 60.0f ? g_gameEyeUnits : 120.0f;
        upm = ge / worldEye;
    }
    if (upm < 30.0f) upm = 30.0f; if (upm > 160.0f) upm = 160.0f;
    if (fabsf(upm - g_unitsPerMeter) > 0.01f) { Log("Body: world size now %.1f game units per metre", upm); g_unitsPerMeter = upm; }
    { float ref = Body_StandRef(); if (ref > 0.3f && (g_standY < 0 || fabsf(g_standY - ref) > 0.25f)) g_standY = ref; }
}
static void Profile_Save(void)
{
    int s = g_profActive; char sec[16]; snprintf(sec, sizeof sec, "Profile%d", s);
    WritePrivateProfileStringA(sec, "sName", g_profName[s], g_iniPath);
    IniFW(sec, "fEyeHeight", g_bodyEye, "%.3f");
    IniFW(sec, "fArmReachL", g_bodyReach[0], "%.3f");
    IniFW(sec, "fArmReachR", g_bodyReach[1], "%.3f");
    SaveInt(sec, "bMatchWorld", g_bodyScaleWorld ? 1 : 0);
    SaveInt(sec, "bMatchArms", g_bodyArmMatch ? 1 : 0);
    IniFW(sec, "fWorldScale", g_worldScaleManual, "%.1f");
    SaveInt(sec, "bLeftHanded", g_leftHanded ? 1 : 0);
    IniFW(sec, "fAimPitchOffset", g_aimPitchOffsetDeg, "%.1f");
    SaveInt(sec, "bSeated", g_seated ? 1 : 0);
    IniFW(sec, "fSeatedEyeHeight", g_seatEye, "%.3f");
    /* keep the old single-person keys in step (older FNVR versions read these) */
    IniFW("Stereo", "fWorldScale", g_worldScaleManual, "%.1f");
    SaveInt("Controls", "bLeftHanded", g_leftHanded ? 1 : 0);
    IniFW("Controls", "fAimPitchOffset", g_aimPitchOffsetDeg, "%.1f");
}
static void Profile_Load(int s)
{
    char sec[16]; snprintf(sec, sizeof sec, "Profile%d", s);
    GetPrivateProfileStringA(sec, "sName", "", g_profName[s], sizeof g_profName[s], g_iniPath);
    if (!g_profName[s][0]) snprintf(g_profName[s], sizeof g_profName[s], "Player %d", s);
    bool exists = (int)GetPrivateProfileIntA(sec, "bMatchWorld", -1, g_iniPath) != -1;
    if (exists) {
        g_bodyEye = IniF(sec, "fEyeHeight", 0); if (!(g_bodyEye > 0.8f && g_bodyEye < 2.4f)) g_bodyEye = 0;
        g_bodyReach[0] = IniF(sec, "fArmReachL", 0); g_bodyReach[1] = IniF(sec, "fArmReachR", 0);
        for (int i = 0; i < 2; i++) if (!(g_bodyReach[i] > 0.2f && g_bodyReach[i] < 1.5f)) g_bodyReach[i] = 0;
        g_bodyScaleWorld = GetPrivateProfileIntA(sec, "bMatchWorld", 1, g_iniPath) != 0;
        g_bodyArmMatch = GetPrivateProfileIntA(sec, "bMatchArms", 1, g_iniPath) != 0;
        float w = IniF(sec, "fWorldScale", g_worldScaleManual); if (w > 20.0f && w < 200.0f) g_worldScaleManual = w;
        g_leftHanded = GetPrivateProfileIntA(sec, "bLeftHanded", g_leftHanded ? 1 : 0, g_iniPath) != 0;
        float a = IniF(sec, "fAimPitchOffset", g_aimPitchOffsetDeg); if (a >= -90 && a <= 90) g_aimPitchOffsetDeg = a;
        g_seated = GetPrivateProfileIntA(sec, "bSeated", 0, g_iniPath) != 0;
        g_seatEye = IniF(sec, "fSeatedEyeHeight", 0); if (!(g_seatEye > 0.4f && g_seatEye < 2.0f)) g_seatEye = 0;
    } else {   /* a new person: start from the current settings, body not measured yet */
        g_bodyEye = 0; g_bodyReach[0] = g_bodyReach[1] = 0; g_seated = false; g_seatEye = 0;
    }
    g_profActive = s;
    SaveInt("Profiles", "iActive", s);
    if (!exists) Profile_Save();
    Body_Apply();
    Log("Profile %d '%s': %s, eye height %.2f m (sitting %.2f m), arm reach %.2f / %.2f m, world %s, arms %s, %s-handed", s, g_profName[s],
        g_seated ? "SEATED" : "standing", g_bodyEye, g_seatEye,
        g_bodyReach[0], g_bodyReach[1], g_bodyScaleWorld ? "matched to height" : "fixed size", g_bodyArmMatch ? "sized to yours" : "game length",
        g_leftHanded ? "left" : "right");
}
static void Profiles_Init(void)
{
    g_worldScaleManual = g_unitsPerMeter;
    for (int s = 1; s <= 4; s++) {
        char sec[16]; snprintf(sec, sizeof sec, "Profile%d", s);
        GetPrivateProfileStringA(sec, "sName", "", g_profName[s], sizeof g_profName[s], g_iniPath);
        if (!g_profName[s][0]) snprintf(g_profName[s], sizeof g_profName[s], "Player %d", s);
    }
    int a = GetPrivateProfileIntA("Profiles", "iActive", 1, g_iniPath);
    if (a < 1 || a > 4) a = 1;
    Profile_Load(a);
}
/* the arm length used for the in-game arm this frame (1 = the game's own) */
static float Body_ArmScale(int side, float gameLen)
{
    if (!g_bodyArmMatch || !(g_bodyReach[side] > 0.2f) || !(gameLen > 1.0f)) return 1.0f;
    float k = g_bodyReach[side] * g_unitsPerMeter / gameLen;
    return k < 0.75f ? 0.75f : k > 1.3f ? 1.3f : k;
}

/* ---- the option list ---- */
#define SET_W 1024
#define SET_H 1280
#define SET_ROW_Y0 150
#define SET_ROW_H 62
#define SET_ROWS 14
enum { SP_MAIN, SP_BODY, SP_PIP, SP_CONTROLS, SP_SPOTS, SP_HANDS, SP_DISPLAY, SP_WEAPONS, SP_MOVE, SP_KIDS, SP_HUD, SP_START, SP_COUNT };
static const char *const g_setPageName[SP_COUNT] = { "FNVR SETTINGS", "BODY & PROFILES", "PIP-BOY", "CONTROLS", "HOLSTERS & GESTURES", "HANDS & OBJECTS",
                                                     "DISPLAY & MENUS", "WEAPONS & SCOPES", "MOVEMENT & COMFORT", "KID FRIENDLY STUFF", "HUD & MENUS", "WELCOME TO FNVVRMC" };
enum { ST_BOOL, ST_FLOAT, ST_INT, ST_PAGE, ST_ACTION, ST_SIGN, ST_PROFILE, ST_INFO };
enum { SF_RESTART = 1, SF_PROFILE = 2, SF_FOV = 4, SF_BODY = 8, SF_KIDS = 16, SF_MELEE = 32, SF_HOLM = 64 };
enum { SA_CLOSE = 1, SA_BACK, SA_CAL_BODY, SA_CAL_HEIGHT, SA_CAL_ARMS, SA_PIP_ADJUST, SA_PIP_RESET, SA_START_PLAY };
typedef struct {
    int page, type; const char *label; const char *sec, *key; void *var;
    float mn, mx, step; const char *fmt; int flags, arg; const char *help;
} SetOpt;
static const SetOpt g_setOpts[] = {
    { SP_START, ST_PROFILE, "Who's playing", 0, 0, 0, 0, 0, 0, 0, 0, 0, "Pick your profile with left/right. A profile that isn't calibrated yet is a new player - up to four, each with their own body, play style and hand." },
    { SP_START, ST_BOOL, "Play style", 0, 0, &g_seated, 0, 0, 0, 0, SF_PROFILE | SF_BODY, 0, "STANDING: play on your feet - really crouch to reach low things. SEATED: play sitting down at your character's full height; hold the right stick DOWN to crouch and reach low things." },
    { SP_START, ST_BOOL, "Left-handed", 0, 0, &g_leftHanded, 0, 0, 0, 0, SF_PROFILE, 0, "Swap hands: the left hand aims and shoots, the right stick moves." },
    { SP_START, ST_ACTION, "Calibrate height and arms", 0, 0, 0, 0, 0, 0, 0, 0, SA_CAL_BODY, "Two quick steps: your height (standing straight, or sitting up straight for seated play), then both arms straight out in front." },
    { SP_START, ST_ACTION, "Start playing", 0, 0, 0, 0, 0, 0, 0, 0, SA_START_PLAY, "Go to the main menu. A profile that isn't calibrated is calibrated first. (Change any of this later: hold both thumbsticks in for a second.)" },
    { SP_MAIN, ST_PAGE, "Body & profiles", 0, 0, 0, 0, 0, 0, 0, 0, SP_BODY, "Your height and arm reach, so the world and your arms fit you. Up to four people, each with their own." },
    { SP_MAIN, ST_PAGE, "Pip-Boy", 0, 0, 0, 0, 0, 0, 0, 0, SP_PIP, "The Pip-Boy on your wrist: its screen, how it opens, how your forearm turns." },
    { SP_MAIN, ST_PAGE, "Controls", 0, 0, 0, 0, 0, 0, 0, 0, SP_CONTROLS, "Motion controls, aiming, melee swings, buttons and fingers." },
    { SP_MAIN, ST_PAGE, "Holsters & gestures", 0, 0, 0, 0, 0, 0, 0, 0, SP_SPOTS, "Where the hip and shoulder holsters and the chest (pause menu) grab are on your body." },
    { SP_MAIN, ST_PAGE, "Hands & objects", 0, 0, 0, 0, 0, 0, 0, 0, SP_HANDS, "Picking things up, throwing, pushing, climbing." },
    { SP_MAIN, ST_PAGE, "HUD & menus", 0, 0, 0, 0, 0, 0, 0, 0, SP_HUD, "The HUD's size and which parts show; pop-up menus (talking, trading, looting) and the quick-loot window." },
    { SP_MAIN, ST_PAGE, "Display & menus", 0, 0, 0, 0, 0, 0, 0, 0, SP_DISPLAY, "How the game is shown in the headset and where menus appear." },
    { SP_MAIN, ST_PAGE, "Weapons & scopes", 0, 0, 0, 0, 0, 0, 0, 0, SP_WEAPONS, "See-through scopes and their zoom and brightness." },
    { SP_MAIN, ST_PAGE, "Movement & comfort", 0, 0, 0, 0, 0, 0, 0, 0, SP_MOVE, "Real crouching and leaning, sprinting and jumping." },
    { SP_MAIN, ST_PAGE, "Kid friendly stuff", 0, 0, 0, 0, 0, 0, 0, 0, SP_KIDS, "Turn off bodies bursting, limbs coming off, and blood." },
    { SP_MAIN, ST_PAGE, "Startup setup screen", 0, 0, 0, 0, 0, 0, 0, 0, SP_START, "The screen shown before the main menu: who's playing, standing or seated, calibration." },
    { SP_MAIN, ST_ACTION, "Close settings", 0, 0, 0, 0, 0, 0, 0, 0, SA_CLOSE, "Hold both thumbsticks in for a second (or press F11) to open this panel again." },

    { SP_BODY, ST_BOOL, "US units (feet & inches)", "Display", "bUSUnits", &g_imperial, 0, 0, 0, 0, 0, 0, "ON: heights, reaches and distances in this panel are shown in feet and inches (speeds in mph) instead of metres." },
    { SP_BODY, ST_PROFILE, "Profile", 0, 0, 0, 0, 0, 0, 0, 0, 0, "Who is playing. Each profile keeps its own height, arm reach, handedness, aim angle and world size. Left/right to switch." },
    { SP_BODY, ST_BOOL, "Play style", 0, 0, &g_seated, 0, 0, 0, 0, SF_PROFILE | SF_BODY, 0, "STANDING: play on your feet. SEATED: play sitting down at your character's full height; hold the right stick DOWN to crouch and reach low things. Calibrate again after switching." },
    { SP_BODY, ST_FLOAT, "Sitting eye height", 0, 0, &g_seatEye, 0.0f, 1.9f, 0.01f, "%.2f m", SF_PROFILE | SF_BODY, 0, "Seated play: your eye height sitting up straight (it counts as your character standing). Measured by the calibration." },
    { SP_BODY, ST_FLOAT, "Seated crouch depth", "Seated", "fCrouchDepth", &g_seatCrouchDepth, 0.2f, 1.4f, 0.05f, "%.2f m", 0, 0, "Seated play: how far down holding the right stick DOWN takes you." },
    { SP_BODY, ST_BOOL, "Setup screen at startup", "Startup", "bShowSetup", &g_startSetup, 0, 0, 0, 0, 0, 0, "ON: each time the game starts, choose who's playing and standing or seated before the main menu." },
    { SP_BODY, ST_ACTION, "Calibrate my body", 0, 0, 0, 0, 0, 0, 0, 0, SA_CAL_BODY, "Two quick steps: stand straight (height), then hold both arms straight out in front (arm reach). Saved to this profile." },
    { SP_BODY, ST_FLOAT, "Eye height", 0, 0, &g_bodyEye, 0.0f, 2.3f, 0.01f, "%.2f m", SF_PROFILE | SF_BODY, 0, "Your standing eye height (the headset when you stand straight). 0 = not measured. Fine-tune with left/right." },
    { SP_BODY, ST_FLOAT, "Left arm reach", 0, 0, &g_bodyReach[0], 0.0f, 1.4f, 0.01f, "%.2f m", SF_PROFILE | SF_BODY, 0, "How far your left hand reaches with the arm straight. Measured by the calibration; 0 = use the game's arm length." },
    { SP_BODY, ST_FLOAT, "Right arm reach", 0, 0, &g_bodyReach[1], 0.0f, 1.4f, 0.01f, "%.2f m", SF_PROFILE | SF_BODY, 0, "How far your right hand reaches with the arm straight. Measured by the calibration; 0 = use the game's arm length." },
    { SP_BODY, ST_BOOL, "Match world to my height", 0, 0, &g_bodyScaleWorld, 0, 0, 0, 0, SF_PROFILE | SF_BODY, 0, "ON: your eyes are at your character's eye height and your real floor is the game's floor, whatever your size." },
    { SP_BODY, ST_BOOL, "Size arms to mine", 0, 0, &g_bodyArmMatch, 0, 0, 0, 0, SF_PROFILE | SF_BODY, 0, "ON: the in-game arms are as long as yours, so your arm straight = its arm straight, bent = bent." },
    { SP_BODY, ST_FLOAT, "World size (when not matched)", 0, 0, &g_worldScaleManual, 40.0f, 120.0f, 0.5f, "%.1f", SF_PROFILE | SF_BODY, 0, "Game units per real metre when the world isn't matched to your height. Higher = the world feels smaller. Default 70." },
    { SP_BODY, ST_BOOL, "Left-handed", 0, 0, &g_leftHanded, 0, 0, 0, 0, SF_PROFILE, 0, "Swap hands: the left hand aims and shoots, the right stick moves." },
    { SP_BODY, ST_INFO, "World size now", 0, 0, &g_unitsPerMeter, 0, 0, 0, "%.1f units/m", 0, 0, "The world size in use right now (game units per real metre)." },

    { SP_PIP, ST_BOOL, "Pip-Boy on your wrist", "Controls", "bWristPipBoy", &g_wristPip, 0, 0, 0, 0, 0, 0, "Raise your left wrist to your face to open the Pip-Boy on your arm. OFF: the game's normal flat Pip-Boy." },
    { SP_PIP, ST_BOOL, "Menu drawn on the screen", "PipScreen", "bDrawMenu", &g_pipScrOn, 0, 0, 0, 0, 0, 0, "FNVR draws the Pip-Boy menu right on the Pip-Boy 2000's screen, without the dark box around it." },
    { SP_PIP, ST_BOOL, "Black box: Pip-Boy screen box", "PipScreen", "bShowScreenBox", &g_pipBoxShow, 0, 0, 0, 0, 0, 0, "The dark screen box of the Pip-Boy model. OFF hides it (the menu may need it to show - try both)." },
    { SP_PIP, ST_BOOL, "Black box: menu background", "PipScreen", "bMenuBackground", &g_pipMenuBg, 0, 0, 0, 0, 0, 0, "The dark background behind the menu itself. OFF: only the glowing menu shows (may not show over the live world)." },
    { SP_PIP, ST_FLOAT, "Screen brightness", "PipScreen", "fBrightness", &g_pipScrBright, 0.3f, 3.0f, 0.1f, "%.1f", 0, 0, "How bright the menu glows on the Pip-Boy screen." },
    { SP_PIP, ST_ACTION, "Adjust screen position", 0, 0, 0, 0, 0, 0, 0, 0, SA_PIP_ADJUST, "Open your Pip-Boy, then click the LEFT stick: left stick moves the menu, right stick sizes it, A switches to tilting, B saves." },
    { SP_PIP, ST_ACTION, "Reset screen position", 0, 0, 0, 0, 0, 0, 0, 0, SA_PIP_RESET, "Forget your adjusted spot and put the menu back on the 2000's glass automatically." },
    { SP_PIP, ST_BOOL, "World keeps running", "Controls", "bPipBoyRealTime", &g_pipRealTime, 0, 0, 0, 0, 0, 0, "ON: the world doesn't pause while you look at your Pip-Boy (like Fallout 4 VR)." },
    { SP_PIP, ST_BOOL, "Live world behind the Pip-Boy", "Controls", "bPipBoyLiveWorld", &g_pipWorldDraw, 0, 0, 0, 0, 0, 0, "ON: the world keeps being drawn while the Pip-Boy is open (needs 'World keeps running'). Turn OFF if it stutters or the menu stops updating." },
    { SP_HUD, ST_BOOL, "Quick-loot window controls", "Controls", "bLootMenuControls", &g_lootControls, 0, 0, 0, 0, 0, 0, "While the quick-loot window is up: right A takes the item, right stick scrolls, right B takes all, right grip opens the full loot menu." },
    { SP_HUD, ST_BOOL, "Quick-loot window in the middle", "Display", "bCenterLootMenu", &g_lootCenter, 0, 0, 0, 0, 0, 0, "The quick-loot window (for boxes and bodies) is moved from the right edge to the middle of your view." },
    { SP_HUD, ST_FLOAT, "Quick-loot window sideways", "Display", "fLootMenuOffset", &g_lootOffX, -600.0f, 600.0f, 20.0f, "%.0f", 0, 0, "Nudge the quick-loot window left (-) or right (+) from the middle." },
    { SP_PIP, ST_BOOL, "Live world: Pip-Boy drawn last", "Controls", "bPipBoyWorldFirst", &g_pipWorldFirst, 0, 0, 0, 0, 0, 0, "ON: the moving world is drawn first and the Pip-Boy with its menu on top (keeps the menu visible). OFF: the old order." },
    { SP_PIP, ST_BOOL, "Opens instantly", "Controls", "bInstantPipBoy", &g_pipInstant, 0, 0, 0, 0, SF_RESTART, 0, "Skip the game's slow raise/lower animation. Takes effect after restarting the game." },
    { SP_PIP, ST_BOOL, "Arm stays where your hand is", "Controls", "bPipBoyNoAnimation", &g_pipNoAnim, 0, 0, 0, 0, 0, 0, "ON: no Pip-Boy body animation; your arm follows your real hand while the Pip-Boy is open." },
    { SP_PIP, ST_BOOL, "Hold Pip-Boy in front of face", "Controls", "bPipBoyLockPose", &g_pipLock, 0, 0, 0, 0, 0, 0, "ON: while the wrist gesture is held, the game holds the Pip-Boy square in front of your face." },
    { SP_PIP, ST_FLOAT, "Forearm turn limit", "Controls", "fForearmRollLimit", &g_forearmRollLimit, 30.0f, 150.0f, 5.0f, "%.0f deg", 0, 0, "How far the forearm can roll each way. A real arm manages about 90; it never flips over past this." },
    { SP_PIP, ST_FLOAT, "Forearm turn near elbow", "Controls", "fForearmTwistShare", &g_twShare, 0.0f, 1.0f, 0.05f, "%.2f", 0, 0, "How much of your wrist roll the whole forearm takes (1 = all of it, so the Pip-Boy turns fully with your wrist)." },
    { SP_PIP, ST_FLOAT, "Elbow follows hand", "Controls", "fArmFollowHand", &g_armFollowHand, 0.0f, 1.0f, 0.05f, "%.2f", 0, 0, "How much the elbow goes where your hand's angle says it is (0 = the game's pose, 1 = fully)." },

    { SP_CONTROLS, ST_BOOL, "Motion controls", "Controls", "bMotionControls", &g_motionControls, 0, 0, 0, 0, 0, 0, "Your VR controllers work as the game's controller (sticks, buttons, triggers)." },
    { SP_CONTROLS, ST_BOOL, "Aim with your hand", "Controls", "bHandAim", &g_handAim, 0, 0, 0, 0, 0, 0, "ON: guns shoot where your hand points; your head only looks around." },
    { SP_CONTROLS, ST_FLOAT, "Aim angle", "Controls", "fAimPitchOffset", &g_aimPitchOffsetDeg, -90.0f, 90.0f, 0.5f, "%.1f deg", SF_PROFILE, 0, "Tilt between the controller and where you aim - differs between controller models. Quick way: point at the centre of your view and click both sticks." },
    { SP_CONTROLS, ST_FLOAT, "Shot left/right correction", "Controls", "fShotYawTrim", &g_shotYawTrimDeg, -10.0f, 10.0f, 0.25f, "%.2f deg", 0, 0, "Fine correction if shots land left or right of where you point." },
    { SP_CONTROLS, ST_FLOAT, "Shot up/down correction", "Controls", "fShotPitchTrim", &g_shotPitchTrimDeg, -10.0f, 10.0f, 0.25f, "%.2f deg", 0, 0, "Fine correction if shots land high or low." },
    { SP_CONTROLS, ST_BOOL, "Melee swings", "Controls", "bMeleeSwings", &g_meleeSwings, 0, 0, 0, 0, 0, 0, "Swing a melee weapon or punch with a real swing of your hand." },
    { SP_CONTROLS, ST_BOOL, "Right trigger attacks with melee", "Controls", "bMeleeTrigger", &g_meleeTrigger, 0, 0, 0, 0, 0, 0, "ON: the right trigger attacks with melee weapons and fists too (as well as swinging). Automatic melee weapons like the chainsaw always use the trigger." },
    { SP_CONTROLS, ST_FLOAT, "Swing speed needed", "Controls", "fSwingSpeed", &g_swingSpeed, 0.5f, 6.0f, 0.1f, "%.1f m/s", 0, 0, "How fast your hand must move to count as a swing. Lower = easier." },
    { SP_CONTROLS, ST_BOOL, "Left trigger = bullet time", "Controls", "bLeftTriggerBulletTime", &g_ltBullet, 0, 0, 0, 0, 0, 0, "The left trigger presses the bullet-time key (needs a bullet-time mod)." },
    { SP_CONTROLS, ST_BOOL, "Left trigger aims down sights", "Controls", "bAimDownSights", &g_adsEnabled, 0, 0, 0, 0, 0, 0, "Use the game's own aim-down-sights view with the left trigger (aimed with your head)." },
    { SP_CONTROLS, ST_BOOL, "Finger tracking", "Controls", "bFingerTracking", &g_fingerTracking, 0, 0, 0, 0, 0, 0, "Fingers follow your controller's touch sensors (trigger, grip, thumb)." },
    { SP_CONTROLS, ST_SIGN, "Fingers bend the other way", "Controls", "bFingerBendReverse", &g_fingerSign, 0, 0, 0, 0, 0, 0, "Turn on if the in-game fingers bend backwards." },
    { SP_CONTROLS, ST_INT, "Finger bend axis", "Controls", "iFingerBendAxis", &g_fingerAxis, 0, 2, 1, "%.0f", 0, 0, "If fingers bend sideways, try another axis (0, 1 or 2)." },
    { SP_CONTROLS, ST_BOOL, "Menu pointer revolver", "Controls", "bMenuPointer", &g_mmPointer, 0, 0, 0, 0, 0, 0, "In menus, a revolver in your right hand points at the menu; the trigger selects." },
    { SP_CONTROLS, ST_BOOL, "Menu pointer shot", "Controls", "bMenuPointerShot", &g_mmShot, 0, 0, 0, 0, 0, 0, "Each menu click fires a Ranger Sequoia shot (sound, recoil, flash)." },
    { SP_CONTROLS, ST_FLOAT, "Menu shot volume", "Controls", "fMenuShotVolume", &g_mmShotVol, 0.0f, 1.0f, 0.05f, "%.2f", 0, 0, "How loud the menu revolver's shot is (1 = a real gunshot)." },

    { SP_SPOTS, ST_BOOL, "Chest grab: stimpak", "Stimpak", "bChestStimpak", &g_stimOn, 0, 0, 0, 0, 0, 0, "ON: a free hand grabbing at your chest pulls out a Stimpak (or a Super Stimpak) if you have one - stab it into your other forearm or your chest to use it, let go to put it back. With none, the chest grab opens the pause menu." },
    { SP_SPOTS, ST_BOOL, "Holsters", "Controls", "bHolsters", &g_holstersOn, 0, 0, 0, 0, 0, 0, "Right grip at your right hip (pistols, one-hand melee) or over your right shoulder (long guns) holsters or draws." },
    { SP_SPOTS, ST_FLOAT, "Hip holster: below eyes", "Controls", "fHipHolsterDown", &g_hipDown, 0.2f, 1.2f, 0.02f, "%.2f m", 0, 0, "How far below your eyes the hip holster is." },
    { SP_SPOTS, ST_FLOAT, "Hip holster: to the side", "Controls", "fHipHolsterSide", &g_hipSide, 0.0f, 0.6f, 0.02f, "%.2f m", 0, 0, "How far out to your right the hip holster is." },
    { SP_SPOTS, ST_FLOAT, "Shoulder holster: below eyes", "Controls", "fShoulderHolsterDown", &g_shDown, -0.2f, 0.6f, 0.02f, "%.2f m", 0, 0, "How far below your eyes the shoulder holster is." },
    { SP_SPOTS, ST_FLOAT, "Shoulder holster: to the side", "Controls", "fShoulderHolsterSide", &g_shSide, 0.0f, 0.5f, 0.02f, "%.2f m", 0, 0, "How far to your right the shoulder holster is." },
    { SP_SPOTS, ST_FLOAT, "Shoulder holster: behind", "Controls", "fShoulderHolsterBack", &g_shBack, -0.2f, 0.5f, 0.02f, "%.2f m", 0, 0, "How far behind your head the shoulder holster is." },
    { SP_SPOTS, ST_BOOL, "Show holstered weapons", "HolsterModel", "bShow", &g_holShow, 0, 0, 0, 0, 0, 0, "A weapon put away in a holster is shown there - on your hip or over your shoulder." },
    { SP_SPOTS, ST_FLOAT, "Hip weapon: turn", 0, 0, &g_holAng[1][0], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Turns the weapon shown on your hip around the up axis." },
    { SP_SPOTS, ST_FLOAT, "Hip weapon: tilt", 0, 0, &g_holAng[1][1], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Tilts the weapon shown on your hip forward/back." },
    { SP_SPOTS, ST_FLOAT, "Hip weapon: roll", 0, 0, &g_holAng[1][2], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Rolls the weapon shown on your hip." },
    { SP_SPOTS, ST_FLOAT, "Shoulder weapon: turn", 0, 0, &g_holAng[2][0], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Turns the weapon shown over your shoulder around the up axis." },
    { SP_SPOTS, ST_FLOAT, "Shoulder weapon: tilt", 0, 0, &g_holAng[2][1], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Tilts the weapon shown over your shoulder." },
    { SP_SPOTS, ST_FLOAT, "Shoulder weapon: roll", 0, 0, &g_holAng[2][2], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_HOLM, 0, "Rolls the weapon shown over your shoulder." },
    { SP_SPOTS, ST_FLOAT, "Holster size", "Controls", "fHolsterRadius", &g_holsterR, 0.05f, 0.4f, 0.01f, "%.2f m", 0, 0, "How close your hand must be to a holster spot." },
    { SP_SPOTS, ST_BOOL, "Grab chest = pause menu", "Controls", "bChestPause", &g_chestPause, 0, 0, 0, 0, 0, 0, "Squeeze a grip with your hand at the middle of your chest to open (and close) the pause menu." },
    { SP_SPOTS, ST_FLOAT, "Chest spot: below eyes", "Controls", "fChestDown", &g_chestDown, 0.1f, 0.8f, 0.02f, "%.2f m", 0, 0, "How far below your eyes the chest grab spot is." },
    { SP_SPOTS, ST_FLOAT, "Chest spot: in front", "Controls", "fChestForward", &g_chestFwd, -0.1f, 0.4f, 0.02f, "%.2f m", 0, 0, "How far in front of your head the chest grab spot is." },
    { SP_SPOTS, ST_FLOAT, "Chest spot size", "Controls", "fChestRadius", &g_chestR, 0.05f, 0.4f, 0.01f, "%.2f m", 0, 0, "How close your hand must be to the chest spot." },

    { SP_HANDS, ST_BOOL, "Pick up objects", "Grab", "bGrabObjects", &g_grabEnabled, 0, 0, 0, 0, 0, 0, "With no weapon out, squeeze a grip next to a loose object to pick it up." },
    { SP_HANDS, ST_FLOAT, "Grab reach", "Grab", "fGrabReach", &g_grabReach, 2.0f, 40.0f, 1.0f, "%.0f units", 0, 0, "How close your hand must be to an object to grab it (about 1.4 cm per unit)." },
    { SP_HANDS, ST_FLOAT, "Heaviest object", "Grab", "fGrabMaxMass", &g_grabMaxMass, 10.0f, 1000.0f, 10.0f, "%.0f", 0, 0, "The heaviest thing you can pick up." },
    { SP_HANDS, ST_FLOAT, "Throw strength", "Grab", "fThrowStrength", &g_throwStrength, 0.0f, 3.0f, 0.1f, "%.1f", 0, 0, "How hard things fly when you let go while moving your hand." },
    { SP_HANDS, ST_FLOAT, "Thrown object damage", "Grab", "fThrowDamage", &g_throwDmg, 0.0f, 3.0f, 0.1f, "%.1fx", 0, 0, "Things you throw hurt whoever they hit: more for faster, heavier throws and with higher Melee Weapons + Unarmed (both maxed: a good throw kills a deathclaw). 0 = no damage." },
    { SP_HANDS, ST_BOOL, "Thrown objects hurt companions", "Grab", "bThrowHurtsCompanions", &g_throwHurtFriends, 0, 0, 0, 0, 0, 0, "OFF: your companions (and your dog) are never hurt by what you throw." },
    { SP_HANDS, ST_BOOL, "Throwing also punches", "Grab", "bThrowAttacks", &g_throwPunch, 0, 0, 0, 0, 0, 0, "OFF: the throwing motion is just a throw - it no longer also counts as a punch (which used to hit whoever was near, like your dog)." },
    { SP_HANDS, ST_BOOL, "Hands push objects", "Grab", "bHandCollision", &g_handCollision, 0, 0, 0, 0, 0, 0, "Your hands and weapons knock loose objects around." },
    { SP_HANDS, ST_FLOAT, "Push strength", "Grab", "fCollisionStrength", &g_collisionStrength, 0.0f, 3.0f, 0.1f, "%.1f", 0, 0, "How hard your hands push things (1 = a solid hand)." },
    { SP_HANDS, ST_BOOL, "Hands stop at walls", "Grab", "bHandWallCollision", &g_handWalls, 0, 0, 0, 0, 0, 0, "Your hand rests on walls and tables instead of passing through them." },
    { SP_HANDS, ST_BOOL, "Climbing", "Grab", "bClimbing", &g_climbEnabled, 0, 0, 0, 0, 0, 0, "Grab ledges and walls with the grips to climb." },
    { SP_HANDS, ST_FLOAT, "Held object along palm", "Grab", "fHoldPalmAlong", &g_holdAlong, 0.0f, 1.0f, 0.02f, "%.2f", 0, 0, "Where held objects sit, from the wrist (0) to the knuckles (1)." },
    { SP_HANDS, ST_FLOAT, "Held object gap", "Grab", "fHoldPalmGap", &g_holdGap, -3.0f, 5.0f, 0.1f, "%.1f", 0, 0, "Gap between your palm and a held object (game units)." },
    { SP_HANDS, ST_FLOAT, "Finger wrap", "Grab", "fHoldFingerWrap", &g_holdCurl, 0.0f, 1.5f, 0.05f, "%.2f", 0, 0, "How tightly your fingers wrap held objects (1 = touching)." },

    { SP_DISPLAY, ST_BOOL, "US units (feet & inches)", "Display", "bUSUnits", &g_imperial, 0, 0, 0, 0, 0, 0, "ON: heights, reaches and distances in this panel are shown in feet and inches (speeds in mph) instead of metres." },
    { SP_DISPLAY, ST_BOOL, "Game in the headset", "Display", "bHeadsetDisplay", &g_headsetDisplay, 0, 0, 0, 0, SF_RESTART, 0, "OFF: head tracking only, the game stays on the monitor. Takes effect after a restart." },
    { SP_DISPLAY, ST_BOOL, "Match headset field of view", "Display", "bMatchHeadsetFOV", &g_matchHeadsetFov, 0, 0, 0, 0, SF_FOV, 0, "Widen the game's view so it fills your headset (F9 toggles it too)." },
    { SP_DISPLAY, ST_INT, "Sharpness", "Display", "iPixelDensityPercent", &g_pixelDensityPct, 0, 200, 5, "%.0f %%", SF_RESTART, 0, "Headset sharpness. 0 = automatic (matches the game's resolution, recommended). Restart to apply." },
    { SP_DISPLAY, ST_BOOL, "Copy frames in background", "Display", "bAsyncCapture", &g_asyncCapture, 0, 0, 0, 0, SF_RESTART, 0, "Faster frame copying to the headset. Restart to apply." },
    { SP_DISPLAY, ST_BOOL, "Curved menus and HUD", "Display", "bCurvedUI", &g_curvedUI, 0, 0, 0, 0, SF_RESTART, 0, "Menus and the HUD on a curved panel in front of you. Restart to apply." },
    { SP_DISPLAY, ST_BOOL, "Main menu on a big screen", "Display", "bMainMenuScreen", &g_farMenu, 0, 0, 0, 0, 0, 0, "The main menu and loading screens on a big curved screen a few feet away." },
    { SP_HUD, ST_BOOL, "Live menus (talking, trading, looting)", "Display", "bLiveMenus", &g_liveMenus, 0, 0, 0, 0, 0, 0, "ON: talking, trading, containers and pop-up messages don't freeze the game - the world keeps running and you stay in it, the menu in front of you." },
    { SP_HUD, ST_FLOAT, "Look away to close menus", "Display", "fLookAwayClose", &g_lookAwayDeg, 0.0f, 180.0f, 5.0f, "%.0f deg", 0, 0, "With live menus: turn your head this far away from a menu and it closes. 0 = never." },
    { SP_DISPLAY, ST_BOOL, "Game menus on the big screen", "Display", "bMenusOnScreen", &g_farGameMenus, 0, 0, 0, 0, 0, 0, "Pause menu, messages, containers and other in-game menus on the same screen." },
    { SP_HUD, ST_FLOAT, "HUD size", "Display", "fUIScale", &g_uiScale, 0.2f, 1.0f, 0.05f, "%.2f", 0, 0, "How much of your view the HUD covers. Smaller = health, AP and compass come in from the edges where you can see them." },
    { SP_HUD, ST_FLOAT, "Dialogue size", "Display", "fDialogueScale2", &g_dlgScale, 0.2f, 1.0f, 0.05f, "%.2f", 0, 0, "How much of your view talking to people takes up - the box and its text shrink together." },
    { SP_HUD, ST_FLOAT, "Menu height", "Display", "fMenuHeight", &g_menuYOff, -0.5f, 0.5f, 0.02f, "%+.2f", 0, 0, "Moves pop-up menus (trading, companions, containers, messages) up (+) or down (-) to where you look straight ahead." },
    { SP_HUD, ST_FLOAT, "Menu size", "Display", "fMenuScale", &g_menuScale, 0.2f, 1.0f, 0.05f, "%.2f", 0, 0, "How much of your view pop-up menus (talking, trading, containers, messages) cover. Smaller = further away." },
    { SP_HUD, ST_BOOL, "Show the HUD", "HUD", "bShowHUD", &g_hudShow, 0, 0, 0, 0, 0, 0, "OFF: no health, AP, sneak, compass, ammo or crosshair on screen (pop-up messages and menus still show)." },
    { SP_HUD, ST_BOOL, "HUD: health bar", "HUD", "bHealth", &g_hudHealth, 0, 0, 0, 0, 0, 0, "The health bar." },
    { SP_HUD, ST_BOOL, "HUD: action points bar", "HUD", "bActionPoints", &g_hudAP, 0, 0, 0, 0, 0, 0, "The AP bar." },
    { SP_HUD, ST_BOOL, "HUD: sneak indicator", "HUD", "bSneak", &g_hudSneak, 0, 0, 0, 0, 0, 0, "The [HIDDEN] / [CAUTION] / [DANGER] sneak indicator." },
    { SP_HUD, ST_BOOL, "HUD: compass", "HUD", "bCompass", &g_hudCompass, 0, 0, 0, 0, 0, 0, "The compass along the bottom." },
    { SP_HUD, ST_BOOL, "HUD: ammo and condition", "HUD", "bAmmo", &g_hudAmmo, 0, 0, 0, 0, 0, 0, "Ammo count and weapon condition." },
    { SP_HUD, ST_BOOL, "HUD: crosshair", "HUD", "bCrosshair", &g_hudCross, 0, 0, 0, 0, 0, 0, "The crosshair in the middle (you aim with your hand anyway)." },
    { SP_DISPLAY, ST_FLOAT, "Big screen distance", "Display", "fMainMenuDistance", &g_mmDist, 0.5f, 6.0f, 0.1f, "%.1f m", 0, 0, "How far away the main menu / loading screen is (applies the next time it opens)." },
    { SP_DISPLAY, ST_FLOAT, "Big screen width", "Display", "fMainMenuWidth", &g_mmWidth, 0.5f, 8.0f, 0.1f, "%.1f m", 0, 0, "How wide the main menu / loading screen is." },
    { SP_DISPLAY, ST_FLOAT, "Big screen curve", "Display", "fMainMenuCurveRadius", &g_mmCurveR, 0.5f, 20.0f, 0.5f, "%.1f m", 0, 0, "How much the big screen curves around you (bigger = flatter)." },
    { SP_DISPLAY, ST_FLOAT, "Menu revolver: up/down", "Controls", "fMenuGunUp", &g_mmGunUpIni, -0.1f, 0.15f, 0.005f, "%.3f m", 0, 0, "Where the menu revolver sits in your hand: raise or lower it." },
    { SP_DISPLAY, ST_FLOAT, "Menu revolver: forward/back", "Controls", "fMenuGunForward", &g_mmGunFwdIni, -0.1f, 0.15f, 0.005f, "%.3f m", 0, 0, "Where the menu revolver sits in your hand: forward or back." },
    { SP_DISPLAY, ST_BOOL, "Menu revolver turned around", "Controls", "bMenuGunTurnAround", &g_seqFlip, 0, 0, 0, 0, 0, 0, "The barrel end is found from the gun's shape. Turn on only if it still points backwards." },
    { SP_DISPLAY, ST_FLOAT, "Menu revolver roll", "Controls", "fMenuGunRoll", &g_seqRoll, -180.0f, 180.0f, 5.0f, "%.0f deg", 0, 0, "Turn the menu revolver around its barrel if it sits tilted in your hand." },
    { SP_DISPLAY, ST_FLOAT, "Flat menu width", "Display", "fMenuScreenFOV", &g_menuScreenFov, 30.0f, 110.0f, 5.0f, "%.0f deg", 0, 0, "Width of the flat screen used for some menus and loading screens." },

    { SP_WEAPONS, ST_FLOAT, "Left-handed gun: forward/back", "LeftHanded", "fGunForward", &g_lhGunFwd, -15.0f, 15.0f, 0.5f, "%.1f cm", 0, 0, "Left-handed play: move the gun in your left hand forward (+) or back (-)." },
    { SP_WEAPONS, ST_FLOAT, "Left-handed gun: up/down", "LeftHanded", "fGunUp", &g_lhGunUp, -15.0f, 15.0f, 0.5f, "%.1f cm", 0, 0, "Left-handed play: move the gun in your left hand up (+) or down (-)." },
    { SP_WEAPONS, ST_FLOAT, "Left-handed gun: left/right", "LeftHanded", "fGunSide", &g_lhGunSide, -15.0f, 15.0f, 0.5f, "%.1f cm", 0, 0, "Left-handed play: move the gun in your left hand to the right (+) or left (-)." },
    { SP_WEAPONS, ST_BOOL, "See-through scopes", "Controls", "bSeeThroughScopes", &g_scopesEnabled, 0, 0, 0, 0, SF_RESTART, 0, "Look through a scope on your gun to see a magnified view. Turning it ON needs a restart." },
    { SP_WEAPONS, ST_BOOL, "Raise to eye to aim scopes", "Controls", "bRaiseToEyeScopes", &g_autoScope, 0, 0, 0, 0, 0, 0, "Bring a scoped gun up to your eye and the game's own scope view comes up." },
    { SP_WEAPONS, ST_INT, "Fire rate", "Controls", "iFireRatePercent", &g_fireRatePct, 25, 400, 5, "%.0f%%", 0, 0, "How fast guns fire, for the gun in your hand. 100% = the game's own; higher = faster, lower = slower. (Automatic guns fire faster; others' attack is quicker.)" },
    { SP_WEAPONS, ST_BOOL, "Revolvers fire as fast as you pull", "Controls", "bRevolverFastFire", &g_revFast, 0, 0, 0, 0, 0, 0, "Revolvers (Ranger Sequoia, Lucky, .357, .44, Police Pistol...) fire every time you pull the trigger, as fast as you can pull it." },
    { SP_WEAPONS, ST_FLOAT, "Melee weapon tilt (this weapon)", 0, 0, &g_melA[0], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_MELEE, 0, "Turns the melee weapon in your hand (first axis). Hold the weapon out and adjust until it sits straight. Saved for that weapon." },
    { SP_WEAPONS, ST_FLOAT, "Melee weapon turn (this weapon)", 0, 0, &g_melA[1], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_MELEE, 0, "Turns the melee weapon in your hand (second axis). Saved for that weapon." },
    { SP_WEAPONS, ST_FLOAT, "Melee weapon roll (this weapon)", 0, 0, &g_melA[2], -180.0f, 180.0f, 5.0f, "%+.0f deg", SF_MELEE, 0, "Turns the melee weapon in your hand (third axis). Saved for that weapon." },
    { SP_WEAPONS, ST_BOOL, "Melee swings", "Controls", "bMeleeSwings", &g_meleeSwings, 0, 0, 0, 0, 0, 0, "Swing a melee weapon or punch with a real swing of your hand." },
    { SP_WEAPONS, ST_BOOL, "Right trigger attacks with melee", "Controls", "bMeleeTrigger", &g_meleeTrigger, 0, 0, 0, 0, 0, 0, "ON: the right trigger attacks with melee weapons and fists too (as well as swinging). Automatic melee weapons like the chainsaw always use the trigger." },
    { SP_WEAPONS, ST_FLOAT, "Scope picture height (this gun)", "ScopeHeightCm", g_scopeKey, &g_scopeOffY, -30.0f, 30.0f, 0.5f, "%+.1f cm", 0, 0, "Moves the picture in the scope up (+) or down (-) for the gun in your hand. Saved for that gun only. Hold the gun out once so it's known." },
    { SP_WEAPONS, ST_FLOAT, "Scope picture size (this gun)", "ScopeSize", g_scopeKey, &g_scopeSize, 0.2f, 4.0f, 0.05f, "%.2fx", 0, 0, "Makes the picture in the scope bigger or smaller for the gun in your hand, to fill its lens. Saved for that gun only." },
    { SP_WEAPONS, ST_FLOAT, "Scope brightness", "Controls", "fScopeBrightness", &g_scopeBright, 0.1f, 4.0f, 0.1f, "%.1f", 0, 0, "How bright the view through the scope is." },
    { SP_WEAPONS, ST_FLOAT, "Scope zoom limit", "Controls", "fScopeMaxZoom", &g_scopeMaxZoom, 1.0f, 10.0f, 0.5f, "%.1fx", 0, 0, "The most a scope magnifies." },

    { SP_MOVE, ST_BOOL, "Real crouch and lean", "Tracking", "bPhysicalCrouchLean", &g_physicalMove, 0, 0, 0, 0, 0, 0, "Really crouching or leaning moves your view and arms (reach the floor, lean over tables)." },
    { SP_MOVE, ST_FLOAT, "Lean before the body follows", "Tracking", "fMaxLean", &g_maxLean, 0.0f, 1.0f, 0.05f, "%.2f m", 0, 0, "How far you can lean before your body moves with you." },
    { SP_MOVE, ST_FLOAT, "Sprint speed", "Controls", "fSprintSpeed", &g_sprintMult, 1.0f, 3.0f, 0.1f, "%.1fx", 0, 0, "Speed while the left stick is clicked in." },
    { SP_MOVE, ST_BOOL, "Running jumps keep speed", "Controls", "bJumpMomentum", &g_momentumOnIni, 0, 0, 0, 0, 0, 0, "A running jump carries your speed through the air." },
    { SP_MOVE, ST_BOOL, "Run by swinging your arms", "Controls", "bArmSwing", &g_armSwing, 0, 0, 0, 0, 0, 0, "Pump both arms like you're running (one forward, one back) to move where you look, like Blade & Sorcery. The stick still works." },
    { SP_MOVE, ST_FLOAT, "Arm swing sensitivity", "Controls", "fArmSwingSensitivity", &g_armSwingSens, 0.3f, 3.0f, 0.1f, "%.1fx", 0, 0, "Higher = smaller swings already move you, and you reach full speed sooner." },
    { SP_MOVE, ST_BOOL, "Hard arm swings sprint", "Controls", "bArmSwingSprint", &g_armSwingSprint, 0, 0, 0, 0, 0, 0, "Swinging your arms hard makes you sprint." },
    { SP_MOVE, ST_BOOL, "No fall damage in power armor", "Controls", "bPowerArmorNoFallDamage", &g_paNoFall, 0, 0, 0, 0, 0, 0, "While you wear power armor, falling from any height doesn't hurt you." },
    { SP_MOVE, ST_BOOL, "Grappling hook", "Grapple", "bEnabled", &g_grapOn, 0, 0, 0, 0, 0, 0, "Fist (grip) + trigger: shoot a line where your hand points and get pulled there. Let go early to launch." },
    { SP_MOVE, ST_BOOL, "Grapple: left hand", "Grapple", "bLeftHand", &g_grapHand[0], 0, 0, 0, 0, 0, 0, "Left grip + left trigger fires the grapple." },
    { SP_MOVE, ST_BOOL, "Grapple: right hand", "Grapple", "bRightHand", &g_grapHand[1], 0, 0, 0, 0, 0, 0, "Right grip + right trigger fires the grapple - only while no gun is out (with a gun the trigger shoots). Fire from the other hand mid-pull to swing on." },
    { SP_MOVE, ST_FLOAT, "Grapple range", "Grapple", "fRange", &g_grapRange, 5.0f, 150.0f, 5.0f, "%.0f m", 0, 0, "How far the line reaches." },
    { SP_MOVE, ST_FLOAT, "Grapple top speed", "Grapple", "fTopSpeed", &g_grapSpeed, 5.0f, 80.0f, 1.0f, "%.0f m/s", 0, 0, "How fast the line pulls you once it's up to speed (it starts slower and speeds up)." },
    { SP_MOVE, ST_FLOAT, "Grapple launch", "Grapple", "fLaunch", &g_grapLaunch, 0.0f, 3.0f, 0.1f, "%.1fx", 0, 0, "How much of your speed you keep when you let go early (more = bigger launch)." },
    { SP_MOVE, ST_BOOL, "No fall damage after grappling", "Grapple", "bSafeLanding", &g_grapSafe, 0, 0, 0, 0, 0, 0, "Landing from a grapple launch doesn't hurt." },
    { SP_MOVE, ST_FLOAT, "Grapple starting speed", "Grapple", "fStartSpeed", &g_grapStart, 0.05f, 1.0f, 0.05f, "%.2f", 0, 0, "How fast the pull starts, as a share of the top speed (1 = full speed at once)." },
    { SP_MOVE, ST_FLOAT, "Grapple time to top speed", "Grapple", "fTimeToTop", &g_grapRamp, 0.05f, 5.0f, 0.05f, "%.2f s", 0, 0, "How long the pull takes to reach its top speed." },
    { SP_MOVE, ST_FLOAT, "Grapple hop at the end", "Grapple", "fEndHop", &g_grapHop, 0.0f, 10.0f, 0.5f, "%.1f m/s", 0, 0, "The little upward hop when you reach the hook point (helps you up onto ledges)." },
    { SP_MOVE, ST_FLOAT, "Grapple air slowdown", "Grapple", "fAirDrag", &g_grapDrag, 0.0f, 3.0f, 0.05f, "%.2f", 0, 0, "How quickly a launch loses its sideways speed in the air (0 = never slows)." },
    { SP_MOVE, ST_BOOL, "Grapple onto people & creatures", "Grapple", "bHookActors", &g_grapActors, 0, 0, 0, 0, 0, 0, "The line can hook onto people and creatures." },
    { SP_MOVE, ST_BOOL, "Grapple pulls people to you", "Grapple", "bPullPeople", &g_grapYank, 0, 0, 0, 0, 0, 0, "ON: hooking a person or creature pulls THEM to you and knocks them down when they arrive. OFF: the line pulls you to them." },
    { SP_MOVE, ST_BOOL, "Hide your body (grapple, climbing)", "Grapple", "bHideBody", &g_hideBodyGrap, 0, 0, 0, 0, 0, 0, "Your body below you disappears while the line pulls you or you climb (and a moment after), so it can't get in front of you." },
    { SP_MOVE, ST_BOOL, "Show the grapple rope", "Grapple", "bShowRope", &g_grapRope, 0, 0, 0, 0, 0, 0, "Draw the rope from your hand to the hook point." },
    { SP_MOVE, ST_BOOL, "Grapple rumble", "Grapple", "bRumble", &g_grapRumble, 0, 0, 0, 0, 0, 0, "Controller buzz while the line is pulling you." },
    { SP_KIDS, ST_BOOL, "Bloody Mess bursts bodies", "KidFriendly", "bBloodyMessExplodes", &g_kidMess, 0, 0, 0, 0, SF_KIDS, 0, "OFF: with the Bloody Mess perk, bodies no longer burst into pieces (you still get its extra damage)." },
    { SP_KIDS, ST_BOOL, "Dismemberment", "KidFriendly", "bDismemberment", &g_kidDismember, 0, 0, 0, 0, SF_KIDS, 0, "OFF: arms, legs and heads never come off and bodies never burst - from any weapon or perk." },
    { SP_KIDS, ST_BOOL, "Blood effects & decals", "KidFriendly", "bBlood", &g_kidBlood, 0, 0, 0, 0, SF_KIDS, 0, "OFF: no blood spray or blood marks when anyone is hit, and no blood on your view. Hits still make their sound." },
};
enum { SET_NOPTS = sizeof g_setOpts / sizeof g_setOpts[0] };

/* ---- panel state ---- */
static bool   g_setDirty = true, g_setFailed = false;
static int    g_setPage = SP_MAIN, g_setSel = 0, g_setScroll = 0, g_setHover = -1, g_setHoverPart = 0;
static int    g_setPageSel[SP_COUNT];
static int    g_setCal = 0;               /* 0 none, 1 = height step, 2 = arms step */
static double g_setCalAt = 0;
static char   g_setToast[160]; static double g_setToastUntil = 0;
static VROverlayHandle_t g_setOv = 0, g_setCurOv = 0;
static ID3D11Texture2D *g_setTex11 = NULL, *g_setCurTex11 = NULL;
static HDC g_setDC = NULL; static HBITMAP g_setBmp = NULL; static uint32_t *g_setBits = NULL;
static HFONT g_setFontBig = NULL, g_setFont = NULL, g_setFontSmall = NULL;
static HmdMatrix34_t g_setPose; static float g_setWidthM = 0.46f;
static bool   g_setShown = false, g_setCurShown = false;

static int Set_PageRows(int page, int *idx, int max)
{
    int n = 0;
    for (int i = 0; i < SET_NOPTS && n < max; i++) if (g_setOpts[i].page == page) idx[n++] = i;
    if (page != SP_MAIN && page != SP_START && n < max) idx[n++] = -1;   /* "Back" */
    return n;
}
static void Set_Toast(const char *msg) { snprintf(g_setToast, sizeof g_setToast, "%s", msg); g_setToastUntil = QpcMs() + 3500.0; g_setDirty = true; }
static void Set_Buzz(unsigned short us) { int h = g_leftHanded ? 0 : 1; if (g_vrSystem && g_handIdx[h] < 16) g_vrSystem->TriggerHapticPulse(g_handIdx[h], 0, us); }

static void Set_ValueText(const SetOpt *o, char *out, size_t cap)
{
    switch (o->type) {
    case ST_BOOL:
        if (o->var == &g_seated) snprintf(out, cap, "%s", g_seated ? "SEATED" : "STANDING");
        else snprintf(out, cap, "%s", *(bool *)o->var ? "ON" : "OFF");
        break;
    case ST_SIGN: snprintf(out, cap, "%s", *(float *)o->var < 0 ? "ON" : "OFF"); break;
    case ST_FLOAT: case ST_INFO: {
        float v = *(float *)o->var;
        if ((o->var == &g_bodyEye || o->var == &g_seatEye) && v <= 0.0f) snprintf(out, cap, "not measured");
        else if ((o->var == &g_bodyReach[0] || o->var == &g_bodyReach[1]) && v <= 0.0f) snprintf(out, cap, "game's own");
        else if (g_imperial && o->fmt && strstr(o->fmt, " m/s")) snprintf(out, cap, "%.1f mph", v * 2.23694f);
        else if (g_imperial && o->fmt && strstr(o->fmt, "units/m")) snprintf(out, cap, "%.1f units/ft", v * 0.3048f);
        else if (g_imperial && o->fmt && strlen(o->fmt) >= 2 && !strcmp(o->fmt + strlen(o->fmt) - 2, " m")) {
            int dec = strstr(o->fmt, ".3") ? 3 : strstr(o->fmt, ".1") ? 1 : 2; FmtLen(out, cap, v, dec);
        }
        else snprintf(out, cap, o->fmt ? o->fmt : "%.2f", v);
        break; }
    case ST_INT: snprintf(out, cap, o->fmt ? o->fmt : "%.0f", (float)*(int *)o->var); break;
    case ST_PROFILE: snprintf(out, cap, "%s", g_profName[g_profActive]); break;
    case ST_PAGE: snprintf(out, cap, ">"); break;
    default: out[0] = 0; break;
    }
}
static void Set_Changed(const SetOpt *o)
{
    if (o->sec && o->key) {
        switch (o->type) {
        case ST_BOOL: SaveInt(o->sec, o->key, *(bool *)o->var ? 1 : 0); break;
        case ST_SIGN: SaveInt(o->sec, o->key, *(float *)o->var < 0 ? 1 : 0); break;
        case ST_INT: SaveInt(o->sec, o->key, *(int *)o->var); break;
        case ST_FLOAT: IniFW(o->sec, o->key, *(float *)o->var, "%.3f"); break;
        default: break;
        }
    }
    if (o->flags & SF_BODY) Body_Apply();
    if (o->flags & SF_PROFILE) Profile_Save();
    if (o->flags & SF_FOV) ApplyHeadsetFov();
    if (o->flags & SF_KIDS) Kids_Apply();
    if (o->flags & SF_MELEE) MeleeAngle_Save();
    if (o->flags & SF_HOLM) Hol_Save();
    if (o->var == &g_wristPip || o->var == &g_pipScrOn) g_pipScrDrawing = false;
    if (o->flags & SF_RESTART) Set_Toast("Saved - restart the game for this one to take effect");
    char v[64]; Set_ValueText(o, v, sizeof v);
    Log("Settings: %s = %s", o->label, v);
    g_setDirty = true;
}
static void Set_Step(const SetOpt *o, int dir, float mult)
{
    switch (o->type) {
    case ST_BOOL: *(bool *)o->var = !*(bool *)o->var; break;
    case ST_SIGN: *(float *)o->var = *(float *)o->var < 0 ? 1.0f : -1.0f; break;
    case ST_FLOAT: {
        float *v = (float *)o->var, step = o->step * mult;
        float nv = *v + dir * step;
        if ((o->var == &g_bodyEye || o->var == &g_seatEye || o->var == &g_bodyReach[0] || o->var == &g_bodyReach[1]) && *v <= 0.0f && dir > 0)
            nv = o->var == &g_bodyEye ? 1.60f : o->var == &g_seatEye ? 1.20f : 0.60f;   /* start a not-yet-measured value somewhere sensible */
        if (nv < o->mn) nv = o->mn; if (nv > o->mx) nv = o->mx;
        nv = roundf(nv / o->step) * o->step;
        if ((o->var == &g_seatEye && nv < 0.4f) || (o->var == &g_bodyEye && nv < 0.8f) || ((o->var == &g_bodyReach[0] || o->var == &g_bodyReach[1]) && nv < 0.2f)) nv = 0.0f;
        *v = nv; break; }
    case ST_INT: {
        int *v = (int *)o->var, step = (int)(o->step * (mult > 1 ? mult : 1));
        int nv = *v + dir * (step ? step : 1);
        if (nv < (int)o->mn) nv = (int)o->mn; if (nv > (int)o->mx) nv = (int)o->mx;
        *v = nv; break; }
    case ST_PROFILE: {
        int s = g_profActive + dir; if (s < 1) s = 4; if (s > 4) s = 1;
        Profile_Save(); Profile_Load(s);
        char m[96]; snprintf(m, sizeof m, "Profile: %s%s", g_profName[s], g_bodyEye > 0 ? "" : " (not calibrated yet)"); Set_Toast(m);
        Set_Buzz(1500); g_setDirty = true; return; }
    default: return;
    }
    Set_Changed(o);
    Set_Buzz(500);
}
static void Settings_Close(void);
static bool g_startAfterCal = false;   /* the startup screen's "Start playing" asked for a calibration first */
static bool Body_Calibrated(void) { return Body_StandRef() > 0.3f && g_bodyReach[0] > 0.2f && g_bodyReach[1] > 0.2f; }
static void Set_Activate(int optIdx, int part)
{
    if (optIdx < 0) { g_setPageSel[g_setPage] = g_setSel; g_setPage = SP_MAIN; g_setSel = g_setPageSel[SP_MAIN]; g_setScroll = 0; g_setDirty = true; Set_Buzz(900); return; }
    const SetOpt *o = &g_setOpts[optIdx];
    switch (o->type) {
    case ST_PAGE: g_setPageSel[g_setPage] = g_setSel; g_setPage = o->arg; g_setSel = g_setPageSel[o->arg]; g_setScroll = 0; g_setDirty = true; Set_Buzz(900); break;
    case ST_BOOL: case ST_SIGN: Set_Step(o, 1, 1); break;
    case ST_FLOAT: case ST_INT: if (part) Set_Step(o, part, 1); break;
    case ST_PROFILE: Set_Step(o, part ? part : 1, 1); break;
    case ST_ACTION:
        Set_Buzz(1200);
        switch (o->arg) {
        case SA_CLOSE: Settings_Close(); break;
        case SA_START_PLAY:
            if (Body_Calibrated()) { Profile_Save(); Settings_Close(); Log("Startup: %s is playing %s", g_profName[g_profActive], g_seated ? "seated" : "standing"); }
            else { g_startAfterCal = true; g_setCal = 1; g_setCalAt = QpcMs(); g_setDirty = true; }
            break;
        case SA_CAL_BODY: case SA_CAL_HEIGHT: g_setCal = 1; g_setCalAt = QpcMs(); g_setDirty = true; break;
        case SA_CAL_ARMS:
            g_setCal = 2; g_setCalAt = QpcMs(); g_setDirty = true; break;
        case SA_PIP_ADJUST:
            if (g_pipOpen && g_pipModeRaw == 3) { g_adjStartReq = true; Settings_Close(); }
            else Set_Toast("Open your Pip-Boy, then click the LEFT stick to adjust its screen");
            break;
        case SA_PIP_RESET: PipScr_Forget(); Set_Toast("Pip-Boy screen: back to the automatic spot"); break;
        }
        break;
    default: break;
    }
}

/* ---- drawing the panel (GDI into a picture, then onto the SteamVR overlay) ---- */
#define SC_BG   RGB(10, 14, 10)
#define SC_AMB  RGB(255, 186, 72)
#define SC_DIM  RGB(150, 112, 52)
#define SC_DARK RGB(26, 20, 8)
#define SC_LINE RGB(90, 66, 30)
static bool Set_InitGdi(void)
{
    if (g_setDC) return true;
    HDC screen = GetDC(NULL);
    g_setDC = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!g_setDC) return false;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = SET_W; bi.bmiHeader.biHeight = -SET_H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    g_setBmp = CreateDIBSection(g_setDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_setBmp || !bits) { DeleteDC(g_setDC); g_setDC = NULL; return false; }
    g_setBits = (uint32_t *)bits;
    SelectObject(g_setDC, g_setBmp);
    g_setFontBig = CreateFontA(-60, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_setFont = CreateFontA(-38, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_setFontSmall = CreateFontA(-30, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    SetBkMode(g_setDC, TRANSPARENT);
    return true;
}
static void Set_Fill(int x0, int y0, int x1, int y1, COLORREF c)
{
    RECT r = { x0, y0, x1, y1 }; HBRUSH b = CreateSolidBrush(c); FillRect(g_setDC, &r, b); DeleteObject(b);
}
static void Set_Text(HFONT f, COLORREF c, int x0, int y0, int x1, int y1, const char *s, UINT fmt)
{
    RECT r = { x0, y0, x1, y1 }; SelectObject(g_setDC, f); SetTextColor(g_setDC, c); DrawTextA(g_setDC, s, -1, &r, fmt);
}
static void Set_Draw(void)
{
    int idx[64]; int n = Set_PageRows(g_setPage, idx, 64);
    if (g_setSel >= n) g_setSel = n - 1; if (g_setSel < 0) g_setSel = 0;
    if (g_setSel < g_setScroll) g_setScroll = g_setSel;
    if (g_setSel >= g_setScroll + SET_ROWS) g_setScroll = g_setSel - SET_ROWS + 1;
    Set_Fill(0, 0, SET_W, SET_H, SC_BG);
    Set_Fill(0, 0, SET_W, 6, SC_AMB); Set_Fill(0, SET_H - 6, SET_W, SET_H, SC_AMB); Set_Fill(0, 0, 6, SET_H, SC_AMB); Set_Fill(SET_W - 6, 0, SET_W, SET_H, SC_AMB);
    /* title */
    Set_Text(g_setFontBig, SC_AMB, 40, 22, SET_W - 40, 100, g_setPageName[g_setPage], DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    char info[96]; snprintf(info, sizeof info, "FNVR %s  |  %s", FNVR_VERSION_SHORT, g_profName[g_profActive]);
    Set_Text(g_setFontSmall, SC_DIM, 40, 22, SET_W - 40, 100, info, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    Set_Fill(30, 118, SET_W - 30, 122, SC_LINE);
    /* rows */
    for (int r = 0; r < SET_ROWS && g_setScroll + r < n; r++) {
        int i = g_setScroll + r, oi = idx[i];
        int y0 = SET_ROW_Y0 + r * SET_ROW_H, y1 = y0 + SET_ROW_H - 6;
        bool sel = i == g_setSel;
        COLORREF tc = sel ? SC_DARK : SC_AMB, vc = sel ? SC_DARK : SC_AMB;
        if (sel) Set_Fill(24, y0, SET_W - 24, y1, SC_AMB);
        const char *label = oi < 0 ? "< Back" : g_setOpts[oi].label;
        char val[64] = ""; bool arrows = false;
        if (oi >= 0) {
            const SetOpt *o = &g_setOpts[oi];
            Set_ValueText(o, val, sizeof val);
            if (o->type == ST_BOOL || o->type == ST_SIGN) { if (!strcmp(val, "OFF")) vc = sel ? SC_DARK : SC_DIM; }
            arrows = o->type == ST_FLOAT || o->type == ST_INT || o->type == ST_PROFILE;
            if (o->type == ST_INFO) vc = sel ? SC_DARK : SC_DIM;
            if (o->flags & SF_RESTART) { char t[80]; snprintf(t, sizeof t, "%s *", val); snprintf(val, sizeof val, "%s", t); }
        }
        Set_Text(g_setFont, tc, 48, y0, SET_W - 410, y1, label, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (arrows) {   /* fixed arrows to point at: left half lowers, right half raises */
            Set_Text(g_setFont, vc, SET_W - 400, y0, SET_W - 350, y1, "<", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            Set_Text(g_setFont, vc, SET_W - 350, y0, SET_W - 98, y1, val, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            Set_Text(g_setFont, vc, SET_W - 98, y0, SET_W - 48, y1, ">", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else Set_Text(g_setFont, vc, SET_W - 400, y0, SET_W - 48, y1, val, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    if (g_setScroll > 0) Set_Text(g_setFontSmall, SC_DIM, 0, SET_ROW_Y0 - 30, SET_W, SET_ROW_Y0, "more above", DT_CENTER | DT_SINGLELINE);
    if (g_setScroll + SET_ROWS < n) Set_Text(g_setFontSmall, SC_DIM, 0, SET_ROW_Y0 + SET_ROWS * SET_ROW_H - 4, SET_W, SET_ROW_Y0 + SET_ROWS * SET_ROW_H + 26, "more below", DT_CENTER | DT_SINGLELINE);
    /* help for the chosen line */
    int hy = SET_H - 222;
    Set_Fill(30, hy - 8, SET_W - 30, hy - 4, SC_LINE);
    const char *help = (g_setSel < n && idx[g_setSel] >= 0) ? g_setOpts[idx[g_setSel]].help : "Back to the first page.";
    char startHelp[400];
    if (g_setPage == SP_START) {   /* who's playing and whether they're calibrated, above the line's help */
        char e1[32], e2[32], e3[32]; FmtLen(e1, sizeof e1, Body_StandRef(), 2); FmtLen(e2, sizeof e2, g_bodyReach[0], 2); FmtLen(e3, sizeof e3, g_bodyReach[1], 2);
        if (Body_Calibrated()) snprintf(startHelp, sizeof startHelp, "%s - %s, %s eye height %s, arms %s / %s.\n%s", g_profName[g_profActive], g_seated ? "seated" : "standing", g_seated ? "sitting" : "standing", e1, e2, e3, help ? help : "");
        else snprintf(startHelp, sizeof startHelp, "%s - %s, NOT CALIBRATED YET (Start playing calibrates first).\n%s", g_profName[g_profActive], g_seated ? "seated" : "standing", help ? help : "");
        help = startHelp;
    }
    if (QpcMs() < g_setToastUntil) { Set_Text(g_setFont, SC_AMB, 48, hy + 6, SET_W - 48, hy + 146, g_setToast, DT_LEFT | DT_WORDBREAK); }
    else Set_Text(g_setFontSmall, SC_AMB, 48, hy + 6, SET_W - 48, hy + 146, help ? help : "", DT_LEFT | DT_WORDBREAK);
    Set_Text(g_setFontSmall, SC_DIM, 40, SET_H - 66, SET_W - 40, SET_H - 16,
             g_setPage == SP_START ? "Point + trigger   |   stick: choose, change" : "Point + trigger   |   stick: choose, change   |   B: back   |   * = restart", DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    /* calibration steps cover the list */
    if (g_setCal) {
        Set_Fill(60, 330, SET_W - 60, 900, SC_DARK);
        Set_Fill(60, 330, SET_W - 60, 336, SC_AMB); Set_Fill(60, 894, SET_W - 60, 900, SC_AMB);
        const char *t1 = g_setCal == 1 ? "STEP 1: HEIGHT" : "STEP 2: ARM REACH";
        const char *t2 = g_setCal == 1
            ? (g_seated ? "Sit up straight in your seat and look straight ahead.\n\nPull the RIGHT trigger."
                        : "Stand up straight and look straight ahead.\n\nPull the RIGHT trigger.")
            : "Hold BOTH arms straight out in front of you at shoulder height, look straight ahead.\n\nPull BOTH triggers.";
        Set_Text(g_setFontBig, SC_AMB, 90, 360, SET_W - 90, 440, t1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        Set_Text(g_setFont, SC_AMB, 110, 470, SET_W - 110, 820, t2, DT_CENTER | DT_WORDBREAK);
        Set_Text(g_setFontSmall, SC_DIM, 90, 830, SET_W - 90, 880, "B = cancel", DT_CENTER | DT_SINGLELINE);
    }
    GdiFlush();
    for (int i = 0; i < SET_W * SET_H; i++) g_setBits[i] |= 0xFF000000u;   /* GDI leaves alpha at 0: the panel is solid */
}
static bool Set_EnsureOverlay(void)
{
    if (g_setFailed) return false;
    if (!g_vrOverlay || !g_vrSystem || !g_d11 || !g_d11ctx) return false;
    if (!g_setOv) {
        if (g_vrOverlay->CreateOverlay("fnvr.settings", "FNVR Settings", &g_setOv) != EVROverlayError_VROverlayError_None) { g_setFailed = true; Log("Settings: no overlay"); return false; }
        g_vrOverlay->SetOverlaySortOrder(g_setOv, 20);
        g_vrOverlay->SetOverlayWidthInMeters(g_setOv, g_setWidthM);
    }
    if (!g_setCurOv && g_vrOverlay->CreateOverlay("fnvr.settings.cursor", "FNVR Settings Cursor", &g_setCurOv) == EVROverlayError_VROverlayError_None) {
        g_vrOverlay->SetOverlaySortOrder(g_setCurOv, 21);
        g_vrOverlay->SetOverlayWidthInMeters(g_setCurOv, 0.016f);
        D3D11_TEXTURE2D_DESC td = { 32, 32, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        uint32_t px[32 * 32];
        for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
            float dx = x - 15.5f, dy = y - 15.5f, r = sqrtf(dx * dx + dy * dy);
            float a = r < 9.0f ? 1.0f : r < 13.0f ? (13.0f - r) / 4.0f : 0.0f;
            uint32_t A = (uint32_t)(a * 255.0f), inner = r < 6.0f;
            px[y * 32 + x] = A << 24 | (inner ? 0x00FFE0A0u : 0x00FFB040u);
        }
        D3D11_SUBRESOURCE_DATA sd = { px, 32 * 4, 0 };
        if (FAILED(ID3D11Device_CreateTexture2D(g_d11, &td, &sd, &g_setCurTex11))) g_setCurTex11 = NULL;
        if (g_setCurTex11) { Texture_t t = { g_setCurTex11, ETextureType_TextureType_DirectX, EColorSpace_ColorSpace_Auto }; g_vrOverlay->SetOverlayTexture(g_setCurOv, &t); }
    }
    if (!g_setTex11) {
        D3D11_TEXTURE2D_DESC td = { SET_W, SET_H, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        if (FAILED(ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_setTex11))) { g_setTex11 = NULL; g_setFailed = true; Log("Settings: no texture"); return false; }
    }
    if (!Set_InitGdi()) { g_setFailed = true; Log("Settings: couldn't make the drawing surface"); return false; }
    return true;
}
static void Settings_Open(void)
{
    if (g_setOpen) return;
    if (!Set_EnsureOverlay()) { UIMessage("FNVR settings need SteamVR"); return; }
    TrackedDevicePose_t hp; float yaw = 0, pitch = 0, pos[3] = { 0, 1.6f, 0 };
    g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &hp, 1);
    if (hp.bPoseIsValid) { ExtractYawPitch(&hp.mDeviceToAbsoluteTracking, &yaw, &pitch); for (int k = 0; k < 3; k++) pos[k] = hp.mDeviceToAbsoluteTracking.m[k][3]; }
    float dist = 0.65f;
    float c[3] = { pos[0] - sinf(yaw) * dist, pos[1] - 0.10f, pos[2] - cosf(yaw) * dist };
    BuildSubmitPose(&g_setPose, yaw, -0.15f, c);
    g_vrOverlay->SetOverlayWidthInMeters(g_setOv, g_setWidthM);
    g_vrOverlay->SetOverlayTransformAbsolute(g_setOv, ETrackingUniverseOrigin_TrackingUniverseStanding, &g_setPose);
    g_setOpen = true; g_setDirty = true; g_setCal = 0; g_setPage = SP_MAIN; g_setSel = g_setPageSel[SP_MAIN]; g_setScroll = 0; g_setHover = -1;
    if (g_bodyEye <= 0.0f) Set_Toast("Tip: Body & profiles > Calibrate my body - so the world and your arms fit you");
    Set_Buzz(2500);
    Log("Settings: opened");
}
/* The startup screen: once per launch, on the main menu, before you can use it - who's playing, standing or seated,
   calibration. The game gets no controls until "Start playing". */
static void Settings_OpenStart(void)
{
    Settings_Open();
    if (!g_setOpen) return;
    g_setPage = SP_START; g_setSel = Body_Calibrated() ? 4 : 0; g_setScroll = 0; g_setDirty = true;
    g_setToastUntil = 0;
    Log("Startup: setup screen shown (%s, %s, %s)", g_profName[g_profActive], g_seated ? "seated" : "standing", Body_Calibrated() ? "calibrated" : "not calibrated");
}
static void StartSetup_Tick(void)
{
    static bool done = false; static double seenAt = 0;
    if (done || !g_startSetup) return;
    if (g_inGameplay) { done = true; return; }   /* (loaded straight into a game: no startup screen) */
    const volatile bool *vis = (const volatile bool *)0x011F308F;
    bool mainMenu = vis[1013] && !g_newGameWait && HeadsetDisplayActive() && g_vrOverlay;   /* (the loading picture stays up behind the main menu) */
    if (!mainMenu) { seenAt = 0; return; }
    if (!seenAt) { seenAt = QpcMs(); return; }
    if (QpcMs() - seenAt < 1500.0) return;
    done = true;
    if (!g_setOpen) Settings_OpenStart();
}
static void Settings_Close(void)
{
    if (!g_setOpen) return;
    g_setOpen = false; g_setCal = 0; g_setSwallow = true;
    if (g_vrOverlay) { if (g_setOv) g_vrOverlay->HideOverlay(g_setOv); if (g_setCurOv) g_vrOverlay->HideOverlay(g_setCurOv); }
    g_setShown = g_setCurShown = false;
    Set_Buzz(1500);
    Log("Settings: closed");
}
static void Settings_Toggle(void) { if (g_setOpen) Settings_Close(); else Settings_Open(); }

/* Called from the controller poll. Returns true while the panel has the controls (the game gets none). */
static bool Settings_Update(const VRControllerState_t *mv, const VRControllerState_t *am, bool okMv, bool okAm)
{
    if (!g_setOpen) return false;
    static bool trigPrev = false, aPrev = false, bPrev = false, yPrev = false, bothTrigPrev = false;
    static double navAt = 0, valAt = 0, valSince = 0; static int navDir = 0, valDir = 0;
    double now = QpcMs();
    bool trig = okAm && am->rAxis[1].x > (trigPrev ? 0.35f : 0.7f);
    bool trigL = okMv && mv->rAxis[1].x > 0.6f;
    bool a = okAm && Pressed(am, EVRButtonId_k_EButton_A), b = okAm && Pressed(am, EVRButtonId_k_EButton_ApplicationMenu);
    bool y = okMv && Pressed(mv, EVRButtonId_k_EButton_ApplicationMenu);
    bool trigEdge = trig && !trigPrev, aEdge = a && !aPrev, bEdge = (b && !bPrev) || (y && !yPrev);
    bool bothTrig = trig && trigL, bothTrigEdge = bothTrig && !bothTrigPrev;
    trigPrev = trig; aPrev = a; bPrev = b; yPrev = y; bothTrigPrev = bothTrig;
    /* calibration steps */
    if (g_setCal) {
        if (bEdge) { g_setCal = 0; g_startAfterCal = false; g_setDirty = true; Set_Toast("Calibration cancelled"); return true; }
        if (now - g_setCalAt < 600.0) return true;   /* (the click that started it) */
        if (g_setCal == 1 && trigEdge && !trigL) {
            TrackedDevicePose_t hp; g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, &hp, 1);
            float hy = hp.mDeviceToAbsoluteTracking.m[1][3];
            bool ok = hp.bPoseIsValid && (g_seated ? (hy > 0.4f && hy < 2.0f) : (hy > 0.8f && hy < 2.4f));
            if (ok) {
                if (g_seated) g_seatEye = hy; else g_bodyEye = hy;
                g_standY = hy; g_haveLeanAnchor = false;
                Body_Apply(); Profile_Save(); Set_Buzz(3000);
                Log("Body: %s eye height measured %.3f m (world %.1f units/m)", g_seated ? "sitting" : "standing", hy, g_unitsPerMeter);
                g_setCal = 2; g_setCalAt = now;
            } else Set_Toast("Couldn't read the headset height - try again");
            g_setDirty = true; return true;
        }
        if (g_setCal == 2 && (!g_inGameplay || g_menuCached)) {   /* main menu: measured from the headset and controllers */
            if (bothTrigEdge) {
                TrackedDevicePose_t ps[16];
                g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, ps, 16);
                if (ps[0].bPoseIsValid) {
                    float yaw = 0, pitch = 0; ExtractYawPitch(&ps[0].mDeviceToAbsoluteTracking, &yaw, &pitch);
                    float H[3] = { ps[0].mDeviceToAbsoluteTracking.m[0][3], ps[0].mDeviceToAbsoluteTracking.m[1][3], ps[0].mDeviceToAbsoluteTracking.m[2][3] };
                    float fw[3] = { -sinf(yaw), 0, -cosf(yaw) }, rt[3] = { cosf(yaw), 0, -sinf(yaw) };
                    /* the shoulders sit where the game's do relative to the eyes (in proportion to your height) */
                    float E = g_bodyEye > 0.8f ? g_bodyEye : (g_seated ? 1.65f : H[1]);
                    int got = 0;
                    for (int sd = 0; sd < 2; sd++) {
                        uint32_t di = g_handIdx[sd];
                        if (di >= 16 || !ps[di].bPoseIsValid) continue;
                        float S[3], d[3];
                        for (int i = 0; i < 3; i++) S[i] = H[i] + rt[i] * (sd ? 1.0f : -1.0f) * 0.116f * E - fw[i] * 0.087f * E;
                        S[1] -= 0.204f * E;
                        for (int i = 0; i < 3; i++) d[i] = ps[di].mDeviceToAbsoluteTracking.m[i][3] - S[i];
                        float r = V3Len(d);
                        if (r > 0.3f && r < 1.3f) { g_bodyReach[sd] = r; got++; }
                    }
                    if (got) {
                        g_setCal = 0; Body_Apply(); Profile_Save(); Set_Buzz(3500);
                        char e1[32], e2[32], e3[32]; FmtLen(e1, sizeof e1, Body_StandRef(), 2); FmtLen(e2, sizeof e2, g_bodyReach[0], 2); FmtLen(e3, sizeof e3, g_bodyReach[1], 2);
                        char m[160]; snprintf(m, sizeof m, "Saved to %s: %s eye height %s, arm reach %s / %s", g_profName[g_profActive], g_seated ? "sitting" : "standing", e1, e2, e3);
                        Set_Toast(m); Log("Body: %s (arms measured on the main menu)", m);
                        if (g_startAfterCal && Body_Calibrated()) { g_startAfterCal = false; Settings_Close(); Log("Startup: %s is playing %s", g_profName[g_profActive], g_seated ? "seated" : "standing"); }
                    } else Set_Toast("Couldn't see your controllers - hold both arms out and try again");
                    g_setDirty = true;
                }
            }
            return true;
        }
        if (g_setCal == 2) {
            static double maskAt = 0;
            if (bothTrigEdge && !g_armCalMask) { g_armCalMask = 3; g_armCalDone = false; maskAt = now; Set_Buzz(2000); }
            if (g_armCalMask && now - maskAt > 1500.0) { g_armCalMask = 0; g_armCalDone = true; }   /* (a hand out of view: keep what was measured) */
            if (g_armCalDone) {
                g_armCalDone = false; g_setCal = 0; Profile_Save(); Set_Buzz(3500);
                char e1[32], e2[32], e3[32]; FmtLen(e1, sizeof e1, g_bodyEye, 2); FmtLen(e2, sizeof e2, g_bodyReach[0], 2); FmtLen(e3, sizeof e3, g_bodyReach[1], 2);
                char m[160]; snprintf(m, sizeof m, "Saved to %s: eye height %s, arm reach %s / %s", g_profName[g_profActive], e1, e2, e3);
                Set_Toast(m); Log("Body: %s", m);
                if (g_startAfterCal && Body_Calibrated()) { g_startAfterCal = false; Settings_Close(); }
            }
            return true;
        }
        return true;
    }
    int idx[64]; int n = Set_PageRows(g_setPage, idx, 64);
    /* pointing with the aiming hand */
    g_setHover = -1; g_setHoverPart = 0;
    {
        TrackedDevicePose_t ps[16]; int h = g_leftHanded ? 0 : 1;
        g_vrSystem->GetDeviceToAbsoluteTrackingPose(ETrackingUniverseOrigin_TrackingUniverseStanding, 0.0f, ps, 16);
        bool hit = false;
        if (g_handIdx[h] < 16 && ps[g_handIdx[h]].bPoseIsValid) {
            const float (*m)[4] = ps[g_handIdx[h]].mDeviceToAbsoluteTracking.m;
            float o = g_aimPitchOffsetDeg / RAD2DEG, org[3], dir[3];
            for (int k = 0; k < 3; k++) { org[k] = m[k][3]; dir[k] = -m[k][2] * cosf(o) + m[k][1] * sinf(o); }
            float C[3], X[3], Y[3], Z[3];
            for (int k = 0; k < 3; k++) { C[k] = g_setPose.m[k][3]; X[k] = g_setPose.m[k][0]; Y[k] = g_setPose.m[k][1]; Z[k] = g_setPose.m[k][2]; }
            float dn = V3Dot(dir, Z);
            if (dn < -0.05f) {
                float d[3]; V3Sub(d, C, org); float t = V3Dot(d, Z) / dn;
                if (t > 0.02f && t < 4.0f) {
                    float P[3]; for (int k = 0; k < 3; k++) P[k] = org[k] + dir[k] * t;
                    float q[3]; V3Sub(q, P, C);
                    float W = g_setWidthM, H = g_setWidthM * SET_H / SET_W;
                    float u = V3Dot(q, X) / W + 0.5f, v = 0.5f - V3Dot(q, Y) / H;
                    if (u >= 0 && u <= 1 && v >= 0 && v <= 1) {
                        hit = true;
                        float px = u * SET_W, py = v * SET_H;
                        int r = (int)floorf((py - SET_ROW_Y0) / SET_ROW_H);
                        if (py >= SET_ROW_Y0 && r >= 0 && r < SET_ROWS && g_setScroll + r < n) {
                            g_setHover = g_setScroll + r;
                            g_setHoverPart = px > SET_W - 410 ? (px < SET_W - 224 ? -1 : 1) : 0;
                        }
                        HmdMatrix34_t cm = g_setPose;
                        for (int k = 0; k < 3; k++) cm.m[k][3] = P[k] + Z[k] * 0.004f;
                        if (g_setCurOv) {
                            g_vrOverlay->SetOverlayTransformAbsolute(g_setCurOv, ETrackingUniverseOrigin_TrackingUniverseStanding, &cm);
                            if (!g_setCurShown) { g_vrOverlay->ShowOverlay(g_setCurOv); g_setCurShown = true; }
                        }
                    }
                }
            }
        }
        if (!hit && g_setCurShown && g_setCurOv) { g_vrOverlay->HideOverlay(g_setCurOv); g_setCurShown = false; }
    }
    if (g_setHover >= 0 && g_setHover != g_setSel) { g_setSel = g_setHover; g_setDirty = true; Set_Buzz(250); }
    /* sticks: whichever is pushed further */
    float sy = 0, sx = 0;
    if (okMv) { sy = mv->rAxis[0].y; sx = mv->rAxis[0].x; }
    if (okAm && fabsf(am->rAxis[0].y) > fabsf(sy)) sy = am->rAxis[0].y;
    if (okAm && fabsf(am->rAxis[0].x) > fabsf(sx)) sx = am->rAxis[0].x;
    int nd = fabsf(sy) > 0.6f && fabsf(sy) > fabsf(sx) ? (sy > 0 ? -1 : 1) : 0;
    int vd = fabsf(sx) > 0.6f && fabsf(sx) >= fabsf(sy) ? (sx > 0 ? 1 : -1) : 0;
    if (nd != navDir) { navDir = nd; navAt = now; if (nd) { g_setSel = (g_setSel + nd + n) % n; g_setDirty = true; Set_Buzz(300); } }
    else if (nd && now - navAt > 380.0) { navAt = now - 290.0; g_setSel = (g_setSel + nd + n) % n; g_setDirty = true; Set_Buzz(200); }
    if (vd != valDir) { valDir = vd; valAt = now; valSince = now; if (vd && g_setSel < n && idx[g_setSel] >= 0) {
            const SetOpt *o = &g_setOpts[idx[g_setSel]];
            if (o->type == ST_FLOAT || o->type == ST_INT || o->type == ST_PROFILE || o->type == ST_BOOL || o->type == ST_SIGN) Set_Step(o, vd, 1); } }
    else if (vd && now - valAt > 400.0 && g_setSel < n && idx[g_setSel] >= 0) {
        const SetOpt *o = &g_setOpts[idx[g_setSel]];
        if (o->type == ST_FLOAT || o->type == ST_INT) { valAt = now - 330.0; Set_Step(o, vd, now - valSince > 1800.0 ? 5.0f : 1.0f); }
    }
    /* trigger / A: use the line you point at (or the chosen one) */
    if (trigEdge || aEdge) {
        int i = (trigEdge && g_setHover >= 0) ? g_setHover : g_setSel;
        int part = (trigEdge && g_setHover >= 0) ? g_setHoverPart : 1;
        if (i >= 0 && i < n) Set_Activate(idx[i], part);
    }
    if (bEdge) {
        if (g_setPage == SP_START) Set_Toast("Choose \"Start playing\" when you're ready");
        else if (g_setPage == SP_MAIN) Settings_Close(); else Set_Activate(-1, 0);
    }
    return true;
}

/* Present: redraw the panel picture when something changed, and keep it shown. */
/* ---- intro skip ring: a small ring in front of you that fills while the right grip is held ---- */
#define SK_W 512
#define SK_H 320
static VROverlayHandle_t g_skOv = 0; static ID3D11Texture2D *g_skTex11 = NULL; static bool g_skFailed = false, g_skShown = false;
static HDC g_skDC = NULL; static HBITMAP g_skBmp = NULL; static uint32_t *g_skBits = NULL; static HFONT g_skFont = NULL, g_skFontBig = NULL;
static uint32_t g_skOut[SK_W * SK_H];
static void IntroSkip_Draw(float prog, bool done)
{
    memset(g_skBits, 0, SK_W * SK_H * 4);
    SetBkMode(g_skDC, TRANSPARENT); SetTextColor(g_skDC, RGB(255, 255, 255));
    RECT r1 = { 0, 228, SK_W, 290 };
    SelectObject(g_skDC, g_skFont);
    DrawTextA(g_skDC, done ? "Skipping the intro..." : "Hold right grip to skip the intro", -1, &r1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    if (!done) {
        char sec[8]; snprintf(sec, sizeof sec, "%d", (int)ceilf(6.0f * (1.0f - prog)));
        RECT r2 = { 156, 15, 356, 215 }; SelectObject(g_skDC, g_skFontBig);
        DrawTextA(g_skDC, sec, -1, &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    }
    GdiFlush();
    const float cx = 256.0f, cy = 115.0f, rOut = 96.0f, rIn = 78.0f;
    for (int y = 0; y < SK_H; y++) for (int x = 0; x < SK_W; x++) {
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy, rr = sqrtf(dx * dx + dy * dy);
        float ring = fminf(fminf(rr - rIn, rOut - rr) + 0.5f, 1.0f); if (ring < 0) ring = 0;
        float ang = atan2f(dx, -dy); if (ang < 0) ang += 6.2831853f;   /* 0 at the top, clockwise */
        bool filled = done || ang <= prog * 6.2831853f;
        float bg = rr < rIn ? 0.55f : 0.0f;   /* dark disc behind the number */
        float t = (float)(g_skBits[y * SK_W + x] & 0xFF) / 255.0f;   /* text coverage */
        float ra = ring * (filled ? 1.0f : 0.35f);
        float a = fmaxf(fmaxf(ra, t), bg);
        float lum = a > 0 ? fmaxf(ra, t) / a : 0;   /* amber where ring/text, black where only the disc */
        uint32_t R = (uint32_t)(255 * lum), G = (uint32_t)(182 * lum), B = (uint32_t)(64 * lum);
        if (t > ra) { R = 255; G = (uint32_t)(182 + 60 * t); if (G > 255) G = 255; B = 120; }
        g_skOut[y * SK_W + x] = (uint32_t)(a * 255.0f) << 24 | R << 16 | G << 8 | B;
    }
}
/* The intro movie stops the game's main loop (where the controllers are normally read), so the hold is read here,
   on the drawing side, which keeps running while the movie plays. */
static void IntroSkip_Poll(void)
{
    static double since = 0; static bool fired = false, lg = false;
    Key_RealTick();   /* (release keys pressed below - the main loop that normally does this isn't running) */
    bool intro = g_newGameWait && g_newGameReadyAt == 0;
    bool grip = false;
    uint32_t ri = g_handIdx[g_leftHanded ? 0 : 1];
    if (intro && g_vrSystem && ri != 0xFFFFFFFF) {
        VRControllerState_t cs;
        if (g_vrSystem->GetControllerState(ri, &cs, sizeof cs)) grip = Pressed(&cs, EVRButtonId_k_EButton_Grip) || cs.rAxis[2].x > 0.7f;
    }
    if (intro && grip && !lg) { lg = true; Log("Intro skip: right grip held during the intro - keep holding for 6 s"); }
    if (!intro || !grip) { since = 0; fired = false; g_skipProg = 0.0f; return; }
    if (fired) return;
    if (!since) since = QpcMs();
    float pr = (float)((QpcMs() - since) / 6000.0);
    g_skipProg = pr > 1.0f ? 1.0f : pr;
    if (pr >= 1.0f) {
        fired = true; g_skipProg = 0.0f; g_skipDoneAt = QpcMs();
        Key_Real(1); Key_Real(57);   /* Esc + Space: the movie's own skip keys */
        g_vrSystem->TriggerHapticPulse(ri, 0, 3500);
        Log("Intro skip: held 6 s - pressed Esc and Space to skip the intro movie");
    }
}
static void IntroSkip_Frame(void)
{
    IntroSkip_Poll();
    float prog = g_skipProg; bool done = g_skipDoneAt > 0 && QpcMs() - g_skipDoneAt < 2500.0;
    bool want = prog > 0.0f || done;
    if (!want) { if (g_skShown && g_skOv) { g_vrOverlay->HideOverlay(g_skOv); g_skShown = false; } return; }
    if (g_skFailed || !g_vrOverlay || !g_d11 || !g_d11ctx) return;
    if (!g_skOv) {
        if (g_vrOverlay->CreateOverlay("fnvr.introskip", "FNVR Intro Skip", &g_skOv) != EVROverlayError_VROverlayError_None) { g_skFailed = true; Log("Intro skip: no overlay"); return; }
        g_vrOverlay->SetOverlaySortOrder(g_skOv, 30);
        g_vrOverlay->SetOverlayWidthInMeters(g_skOv, 0.34f);
        HmdMatrix34_t m; memset(&m, 0, sizeof m);
        m.m[0][0] = m.m[1][1] = m.m[2][2] = 1.0f; m.m[1][3] = -0.10f; m.m[2][3] = -0.75f;   /* in front of your eyes, a little low */
        g_vrOverlay->SetOverlayTransformTrackedDeviceRelative(g_skOv, 0, &m);
        D3D11_TEXTURE2D_DESC td = { SK_W, SK_H, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        if (FAILED(ID3D11Device_CreateTexture2D(g_d11, &td, NULL, &g_skTex11))) { g_skTex11 = NULL; g_skFailed = true; Log("Intro skip: no texture"); return; }
        BITMAPINFO bi; memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = SK_W; bi.bmiHeader.biHeight = -SK_H;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        g_skDC = CreateCompatibleDC(NULL);
        g_skBmp = g_skDC ? CreateDIBSection(g_skDC, &bi, DIB_RGB_COLORS, (void **)&g_skBits, NULL, 0) : NULL;
        if (!g_skBmp || !g_skBits) { g_skFailed = true; Log("Intro skip: no drawing surface"); return; }
        SelectObject(g_skDC, g_skBmp);
        g_skFont = CreateFontA(29, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
        g_skFontBig = CreateFontA(96, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
    }
    static double drawn = 0; static int lastKey = -1;
    int key = done ? 1000 : (int)(prog * 200.0f);
    if (key != lastKey || QpcMs() - drawn > 500.0) {
        lastKey = key; drawn = QpcMs();
        IntroSkip_Draw(prog, done);
        ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_skTex11, 0, NULL, g_skOut, SK_W * 4, 0);
        ID3D11DeviceContext_Flush(g_d11ctx);
        Texture_t t = { g_skTex11, ETextureType_TextureType_DirectX, EColorSpace_ColorSpace_Auto };
        g_vrOverlay->SetOverlayTexture(g_skOv, &t);
    }
    if (!g_skShown) { g_vrOverlay->ShowOverlay(g_skOv); g_skShown = true; }
}

static void Settings_Frame(void)
{
    static double lastBody = 0;
    if (QpcMs() - lastBody > 1000.0) { lastBody = QpcMs(); Body_Apply(); }
    if (!g_setOpen) return;
    if (!Set_EnsureOverlay()) { g_setOpen = false; return; }
    static double toastWas = 0;
    bool toastNow = QpcMs() < g_setToastUntil;
    if (toastWas && !toastNow) g_setDirty = true;
    toastWas = toastNow ? 1 : 0;
    if (g_setDirty) {
        g_setDirty = false;
        Set_Draw();
        ID3D11DeviceContext_UpdateSubresource(g_d11ctx, (ID3D11Resource *)g_setTex11, 0, NULL, g_setBits, SET_W * 4, 0);
        ID3D11DeviceContext_Flush(g_d11ctx);
        Texture_t t = { g_setTex11, ETextureType_TextureType_DirectX, EColorSpace_ColorSpace_Auto };
        g_vrOverlay->SetOverlayTexture(g_setOv, &t);
    }
    if (!g_setShown) { g_vrOverlay->ShowOverlay(g_setOv); g_setShown = true; }
}

static void InstallRealTimePipBoy(void)
{
    {   /* drawing dispatch call in the main loop */
        BYTE *p = (BYTE *)0x0086E8FD;
        if (p[0] == 0xE8 && (DWORD)(0x0086E8FD + 5 + *(int32_t *)(p + 1)) == 0x0086F450) {
            DWORD old; VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
            *(int32_t *)(p + 1) = (int32_t)((uintptr_t)Hook_RenderDispatch - (0x0086E8FD + 5));
            VirtualProtect(p, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), p, 5);
        } else Log("Real-time Pip-Boy: drawing dispatch call changed by another mod - Pip-Boy screen may stay dark");
    }
    static const DWORD sites[2] = { 0x0086E6E7, 0x0086E7CE };
    int ok = 0;
    for (int i = 0; i < 2; i++) {
        BYTE *p = (BYTE *)sites[i];
        if (p[0] != 0xE8 || (DWORD)(sites[i] + 5 + *(int32_t *)(p + 1)) != 0x007050D0) {
            Log("Real-time Pip-Boy: unexpected code at %08lX (another mod changed it) - not installed there", (unsigned long)sites[i]);
            continue;
        }
        DWORD old; VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
        *(int32_t *)(p + 1) = (int32_t)((uintptr_t)Hook_AfterMenuFlag - (sites[i] + 5));
        (void)Hook_RealTimeCheck;
        VirtualProtect(p, 5, old, &old);
        FlushInstructionCache(GetCurrentProcess(), p, 5);
        ok++;
    }
    Log("Real-time Pip-Boy: %d of 2 hooks installed (%s)", ok, g_pipRealTime ? "on" : "off - F10 to turn on");
}

/* ---- D3D9 hooks --------------------------------------------------------- */

typedef HRESULT (STDMETHODCALLTYPE *DevPresent_t)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);
typedef HRESULT (STDMETHODCALLTYPE *DevReset_t)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
typedef HRESULT (STDMETHODCALLTYPE *SwapPresent_t)(IDirect3DSwapChain9 *, const RECT *, const RECT *, HWND, const RGNDATA *, DWORD);

static DevPresent_t  o_DevPresent = NULL;
static DevReset_t    o_DevReset = NULL;
static SwapPresent_t o_SwapPresent = NULL;
static IDirect3DDevice9 *g_hookedDevice = NULL;
static bool g_loggedPresentPath = false;

static HRESULT STDMETHODCALLTYPE Hook_DevPresent(IDirect3DDevice9 *dev, const RECT *a, const RECT *b, HWND w, const RGNDATA *r)
{
    if (g_inPresent) return o_DevPresent(dev, a, b, w, r);
    g_inPresent = true;
    if (!g_loggedPresentPath) { g_loggedPresentPath = true; Log("Frames are presented through IDirect3DDevice9::Present"); }
    InterlockedExchange(&g_inMainLoopWork, 2);
    Display_Frame(dev);
    InterlockedExchange(&g_inMainLoopWork, 3);
    HRESULT hr = o_DevPresent(dev, a, b, w, r);
    InterlockedExchange(&g_inMainLoopWork, 4);
    Display_WaitForNextFrame();
    InterlockedExchange(&g_inMainLoopWork, 0);
    g_inPresent = false;
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_SwapPresent(IDirect3DSwapChain9 *sc, const RECT *a, const RECT *b, HWND w, const RGNDATA *r, DWORD f)
{
    if (g_inPresent) return o_SwapPresent(sc, a, b, w, r, f);
    g_inPresent = true;
    if (!g_loggedPresentPath) { g_loggedPresentPath = true; Log("Frames are presented through IDirect3DSwapChain9::Present"); }
    IDirect3DDevice9 *dev = NULL;
    if (SUCCEEDED(IDirect3DSwapChain9_GetDevice(sc, &dev)) && dev) {
        Display_Frame(dev);
        IDirect3DDevice9_Release(dev);
    }
    HRESULT hr = o_SwapPresent(sc, a, b, w, r, f);
    Display_WaitForNextFrame();
    g_inPresent = false;
    return hr;
}

static HRESULT STDMETHODCALLTYPE Hook_DevReset(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp)
{
    Log("D3D9 device reset (%ux%u) - releasing capture surfaces", pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0);
    Display_Release9();
    g_loggedFrustum = false;
    return o_DevReset(dev, pp);
}

static void *PatchVtable(void **vtbl, int index, void *hook)
{
    DWORD old;
    void *orig = vtbl[index];
    if (orig == hook) return NULL;
    VirtualProtect(&vtbl[index], sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
    vtbl[index] = hook;
    VirtualProtect(&vtbl[index], sizeof(void *), old, &old);
    return orig;
}

static void InstallD3D9Hooks(void)
{
    if (g_hookedDevice || !g_headsetDisplay) return;
    IDirect3DDevice9 *dev = GetGameDevice();
    if (!dev) { Log("D3D9 device not available yet"); return; }
    g_hookedDevice = dev;

    void **vtbl = *(void ***)dev;
    o_DevReset   = (DevReset_t)PatchVtable(vtbl, 16, (void *)Hook_DevReset);
    o_DevPresent = (DevPresent_t)PatchVtable(vtbl, 17, (void *)Hook_DevPresent);
    o_DevClear = (DevClear_t)PatchVtable(vtbl, 43, (void *)Hook_DevClear);
    o_SetRT = (DevSetRT_t)PatchVtable(vtbl, 37, (void *)Hook_SetRT);   /* (always: the Pip-Boy menu capture needs it too) */
    o_SetTex = (DevSetTex_t)PatchVtable(vtbl, 65, (void *)Hook_SetTex);
    o_DP = (DevDP_t)PatchVtable(vtbl, 81, (void *)Hook_DP);
    o_DIP = (DevDIP_t)PatchVtable(vtbl, 82, (void *)Hook_DIP);
    o_SetScissor = (DevSetScissor_t)PatchVtable(vtbl, 75, (void *)Hook_SetScissor);
    if (g_curvedUI && !g_flatMenus) {
        o_SetDS = (DevSetDS_t)PatchVtable(vtbl, 39, (void *)Hook_SetDS);
        o_SetVP = (DevSetVP_t)PatchVtable(vtbl, 47, (void *)Hook_SetVP);
        o_SetRS = (DevSetRS_t)PatchVtable(vtbl, 57, (void *)Hook_SetRS);
        Log("Curved interface: drawing hooks %s", o_SetRT && o_SetDS && o_SetVP && o_SetRS ? "installed" : "INCOMPLETE");
        if (!(o_SetRT && o_SetDS && o_SetVP && o_SetRS)) g_curvedActive = false;
    }

    IDirect3DSwapChain9 *sc = NULL;
    if (SUCCEEDED(IDirect3DDevice9_GetSwapChain(dev, 0, &sc)) && sc) {
        void **svt = *(void ***)sc;
        o_SwapPresent = (SwapPresent_t)PatchVtable(svt, 3, (void *)Hook_SwapPresent);
        IDirect3DSwapChain9_Release(sc);
    }

    D3DDEVICE_CREATION_PARAMETERS cp;
    IDirect3DDevice9_GetCreationParameters(dev, &cp);
    Log("Hooked D3D9 device %p (adapter %u): Present=%d Reset=%d SwapChain::Present=%d Clear=%d",
        (void *)dev, cp.AdapterOrdinal, o_DevPresent != NULL, o_DevReset != NULL, o_SwapPresent != NULL, o_DevClear != NULL);
}

/* ======================================================================= */
/* Freeze watchdog: if the game's main loop stops for 5+ seconds, log where   */
/* the main thread is stuck (so freezes can be diagnosed from the log).       */
/* ======================================================================= */

static HANDLE g_mainThread = NULL;
static volatile DWORD g_lastLoopTick = 0;

static void DescribeAddress(DWORD addr, char *out, size_t n)
{
    HMODULE mod = NULL;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)(uintptr_t)addr, &mod) && mod) {
        GetModuleFileNameA(mod, name, MAX_PATH);
        const char *base = strrchr(name, '\\');
        snprintf(out, n, "%08lX (%s+%lX)", (unsigned long)addr, base ? base + 1 : name, (unsigned long)(addr - (DWORD)(uintptr_t)mod));
    } else {
        snprintf(out, n, "%08lX", (unsigned long)addr);
    }
}

/* If the game is about to crash, write where it happened into FNVR.log (the game's own crash logger
   doesn't always catch it). Only notes it - the game handles the crash as before. */
static LONG CALLBACK CrashNote(EXCEPTION_POINTERS *ep)
{
    static volatile LONG notes = 0;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION) return EXCEPTION_CONTINUE_SEARCH;
    void *at = ep->ExceptionRecord->ExceptionAddress;
    HMODULE mod = NULL; char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)at, &mod) && mod) GetModuleFileNameA(mod, path, sizeof path);
    const char *base = strrchr(path, '\\'); base = base ? base + 1 : path;
    if (!_stricmp(base, "KERNELBASE.dll") || !_stricmp(base, "kernel32.dll") || !_stricmp(base, "ntdll.dll")) return EXCEPTION_CONTINUE_SEARCH;   /* (safe pointer checks) */
    if (InterlockedIncrement(&notes) > 6) return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t off = mod ? (uintptr_t)at - (uintptr_t)mod : 0;
    Log("CRASH? %s at %s+%X (reading %p) - eax %08lX ecx %08lX edx %08lX esi %08lX edi %08lX",
        code == EXCEPTION_ACCESS_VIOLATION ? "bad memory access" : code == EXCEPTION_STACK_OVERFLOW ? "stack overflow" : "illegal instruction",
        base, (unsigned)off, ep->ExceptionRecord->NumberParameters > 1 ? (void *)ep->ExceptionRecord->ExceptionInformation[1] : NULL,
        (unsigned long)ep->ContextRecord->Eax, (unsigned long)ep->ContextRecord->Ecx, (unsigned long)ep->ContextRecord->Edx, (unsigned long)ep->ContextRecord->Esi, (unsigned long)ep->ContextRecord->Edi);
    {   /* a few return addresses from the stack, to see the way in */
        uintptr_t *sp = (uintptr_t *)ep->ContextRecord->Esp; char line[400] = ""; int n = 0;
        for (int i = 0; i < 64 && n < 10 && !IsBadReadPtr(sp + i, 4); i++) {
            HMODULE m2 = NULL; char p2[MAX_PATH];
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)sp[i], &m2) && m2 && GetModuleFileNameA(m2, p2, sizeof p2)) {
                const char *b2 = strrchr(p2, '\\'); b2 = b2 ? b2 + 1 : p2;
                char t[48]; snprintf(t, sizeof t, " %s+%X", b2, (unsigned)(sp[i] - (uintptr_t)m2)); strncat(line, t, sizeof line - strlen(line) - 1); n++;
            }
        }
        Log("CRASH?   stack:%s", line);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI WatchdogThread(LPVOID unused)
{
    (void)unused;
    int dumps = 0;
    DWORD lastDumpTick = 0;
    for (;;) {
        Sleep(150);
        DWORD last = g_lastLoopTick;
        if (!last || !g_mainThread) continue;
        DWORD stalled = GetTickCount() - last;
        /* sample freezes of ~0.8 s and longer (shooting / Pip-Boy hitches) - a few per session */
        if (stalled < 800 || dumps >= 16 || last == lastDumpTick) continue;
        lastDumpTick = last;
        dumps++;
        if (SuspendThread(g_mainThread) == (DWORD)-1) continue;
        CONTEXT ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (GetThreadContext(g_mainThread, &ctx)) {
            char d[128];
            DescribeAddress(ctx.Eip, d, sizeof d);
            Log("WATCHDOG: main loop stalled %.1fs (inside FNVR work: %ld). Main thread at %s", stalled / 1000.0f, g_inMainLoopWork, d);
            /* Return addresses on the stack that point into code */
            DWORD *sp = (DWORD *)(uintptr_t)ctx.Esp;
            int found = 0;
            for (int i = 0; i < 1024 && found < 16; i++) {
                DWORD v;
                if (IsBadReadPtr(sp + i, 4)) break;
                v = sp[i];
                MEMORY_BASIC_INFORMATION mbi;
                if (v < 0x10000 || !VirtualQuery((void *)(uintptr_t)v, &mbi, sizeof mbi)) continue;
                if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) continue;
                DescribeAddress(v, d, sizeof d);
                Log("WATCHDOG:   stack[%d] %s", i, d);
                found++;
            }
        }
        ResumeThread(g_mainThread);
    }
    return 0;
}

static void StartWatchdog(void)
{
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    AddVectoredExceptionHandler(0, CrashNote);
    HANDLE t = CreateThread(NULL, 0, WatchdogThread, NULL, 0, NULL);
    if (t) CloseHandle(t);
}

/* ======================================================================= */
/* NVSE glue                                                               */
/* ======================================================================= */

static PluginHandle g_handle = 0;
static bool g_loggedLoop = false;

static const char *MsgName(uint32_t t)
{
    static const char *names[] = {
        "PostLoad", "ExitGame", "ExitToMainMenu", "LoadGame", "SaveGame",
        "ScriptPrecompile", "PreLoadGame", "ExitGame_Console", "PostLoadGame",
        "PostPostLoad", "RuntimeScriptError", "DeleteGame", "RenameGame",
        "RenameNewGame", "NewGame", "DeleteGameName", "RenameGameName",
        "RenameNewGameName", "DeferredInit", "ClearScriptDataCache", "MainGameLoop",
        "ScriptCompile", "EventListDestroyed", "PostQueryPlugins", "OnFramePresent",
    };
    return t < sizeof(names) / sizeof(names[0]) ? names[t] : "Other";
}

/* New game: switch gameplay on only once the player is really in the world (loaded body, in a cell, no loading screen) for 2 s. */
static void NewGame_Tick(void)
{
    if (!g_newGameWait) return;
    char *pl = *(char **)PLAYER_PTR_ADDR;
    char *rd = (pl && !IsBadReadPtr(pl, 0x70)) ? *(char **)(pl + 0x64) : NULL;
    char *node = (rd && !IsBadReadPtr(rd, 0x18)) ? *(char **)(rd + 0x14) : NULL;
    char *cell = pl ? *(char **)(pl + 0x40) : NULL;
    bool loading = ((const volatile bool *)0x011F308F)[1007];
    if (!node || !cell || loading) { g_newGameReadyAt = 0; return; }
    if (!g_newGameReadyAt) { g_newGameReadyAt = QpcMs(); return; }
    if (QpcMs() - g_newGameReadyAt < 2000.0) return;
    g_newGameWait = false;
    g_inGameplay = true;
    Kids_Apply();
    g_reequipAt = QpcMs() + 2000.0;
    g_headSnapAt = QpcMs() + 5000.0;
    UIMessage(HeadsetDisplayActive() ? "FNVR loaded - headset display active" : "FNVR loaded");
    Log("New game: you're in the world - VR features on");
}

static void OnNVSEMessage(NVSEMessage *msg)
{
    if (!msg) return;
    switch (msg->type) {
    case kMessage_MainGameLoop:
        if (!g_loggedLoop) { Log("First MainGameLoop tick received"); g_loggedLoop = true; InstallD3D9Hooks(); }
        g_lastLoopTick = GetTickCount();
        InterlockedExchange(&g_inMainLoopWork, 1);
        { double t0 = QpcMs(); VR_Tick(); g_tTick += QpcMs() - t0; }
        NewGame_Tick();
        Move_Flush();
        Stim_Flush();
        Far_UiTick();
        Holster_Tick();
        LootMenu_Tick(); Hud_Tick(); FireRate_Tick();
        if (g_seqWantAt > 0 && !g_seqTried && QpcMs() > g_seqWantAt) { Log("Menu Sequoia: loading the model"); Room_LoadSequoia(); }
        InterlockedExchange(&g_inMainLoopWork, 0);
        return;
    case kMessage_ScriptCompile:
    case kMessage_EventListDestroyed:
    case kMessage_OnFramePresent:
    case kMessage_ScriptPrecompile:
    case kMessage_RuntimeScriptError:
        return;   /* very frequent, not interesting (script errors come from other mods) */
    default:
        break;
    }
    Log("NVSE message: %s (%u)", MsgName(msg->type), msg->type);

    switch (msg->type) {
    case kMessage_DeferredInit:
        InstallShaderCache();
        InstallD3D9Hooks();
        InstallRenderHook();
        if (g_motionControls) InstallXInputThunkHook();
        if (g_motionControls && g_handAim) InstallFireHook();
        InstallRealTimePipBoy();
        InstallPipMenuCapture();
        InstallInstantPipBoy();
        break;
    case kMessage_NewGame:   /* sent when "New" is clicked - before the intro and before the world exists: wait for it */
        Grab_ClearAll();
        HeadOffset_Reset();
        g_inGameplay = false;
        g_newGameWait = true; g_newGameReadyAt = 0;
        Log("New game: waiting until you're in the world before switching the VR features on");
        break;
    case kMessage_PostLoadGame:
        g_newGameWait = false;
        g_inGameplay = true;
        Kids_Apply();
        g_reequipAt = QpcMs() + 2000.0;
        g_headSnapAt = QpcMs() + 5000.0;
        UIMessage(HeadsetDisplayActive() ? "FNVR loaded - headset display active" : "FNVR loaded");
        break;
    case kMessage_ExitToMainMenu:
        g_newGameWait = false;
        Grab_ClearAll();
        HeadOffset_Reset();
        RestoreGameFov("exit to main menu");
        g_inGameplay = false;
        break;
    case kMessage_PreLoadGame:
        g_newGameWait = false;
        Grab_ClearAll();
        HeadOffset_Reset();
        RestoreGameFov("loading");
        g_inGameplay = false;
        break;
    case kMessage_ExitGame:
    case kMessage_ExitGame_Console:
        Log("Game exiting - shutting down cleanly");
        VR_Stop("game exiting");
        break;
    default:
        break;
    }
}

__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface *nvse, PluginInfo *info)
{
    info->infoVersion = 1;
    info->name = "FNVR";
    info->version = FNVR_VERSION;

    if (nvse->isEditor) return false;

    LogOpen();
    Log("FNVR %s", FNVR_VERSION_STR);
    Log("NVSE version: %08X  runtime: %08X", nvse->nvseVersion, nvse->runtimeVersion);

    if (nvse->runtimeVersion != RUNTIME_VERSION_1_4_0_525) {
        Log("ERROR: unsupported game version (expected 1.4.0.525 / %08X). Plugin disabled.", RUNTIME_VERSION_1_4_0_525);
        return false;
    }
    g_gameOk = true;
    return true;
}

__declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface *nvse)
{
    Log("NVSEPlugin_Load called");
    LoadSettings();
    if (g_motionControls) { InstallXInputHook(); LoadGameBindings(); }
    StartWatchdog();
    g_handle = nvse->GetPluginHandle();
    g_scriptIntf = (NVSEScriptIntf *)nvse->QueryInterface(6);
    Log("Script interface: %s", g_scriptIntf ? "available" : "missing");

    NVSEMessagingInterface *msgIntf = (NVSEMessagingInterface *)nvse->QueryInterface(kInterface_Messaging);
    if (!msgIntf) { Log("ERROR: could not get NVSE messaging interface"); return false; }
    if (!msgIntf->RegisterListener(g_handle, "NVSE", OnNVSEMessage)) { Log("ERROR: RegisterListener failed"); return false; }
    Log("Registered for NVSE messages. Load complete.");
    return true;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) g_selfModule = inst;
    if (reason == DLL_PROCESS_DETACH && g_log) {
        Log("DLL unloading");
        fclose(g_log);
        g_log = NULL;
    }
    return TRUE;
}
