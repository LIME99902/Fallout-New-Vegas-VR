/* FNVR - SteamVR Input: the controls FNVR reads, as SteamVR "actions" (FNVR\input\actions.json and a default binding for
   each kind of controller), so every controller's buttons land where they should - and players can change them in
   SteamVR's controller bindings. Read by FNVR-VRHost.exe for the game, or by FNVR itself when OpenVR runs in the game.
   Copyright (C) 2026 FNVVRMC contributors. GPL-3.0-or-later.

   Only plain numbers cross between the 32-bit game and the 64-bit host (VrInFrame has no pointers). */
#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>

enum { VA_STICK, VA_TRIGGER, VA_GRIP, VA_GRIPCLICK, VA_TRIGGERCLICK, VA_PRIMARY, VA_SECONDARY, VA_STICKCLICK, VA_THUMBTOUCH,
       VA_SKEL_L, VA_SKEL_R, VA_HAPTIC, VA_N };
static const char *const k_vrinAct[VA_N] = {
    "/actions/fnvr/in/stick", "/actions/fnvr/in/trigger", "/actions/fnvr/in/grip", "/actions/fnvr/in/gripclick",
    "/actions/fnvr/in/triggerclick", "/actions/fnvr/in/primary", "/actions/fnvr/in/secondary", "/actions/fnvr/in/stickclick",
    "/actions/fnvr/in/thumbtouch", "/actions/fnvr/in/skel_left", "/actions/fnvr/in/skel_right", "/actions/fnvr/out/haptic" };

typedef struct {   /* one hand */
    float stick[2], trigger, grip, curl[5];   /* (curl: thumb, index, middle, ring, little - 0 open .. 1 closed) */
    int32_t gripDown, triggerDown, primary, secondary, stickDown, thumbTouch;
    int32_t active;   /* (any of this hand's controls bound and live) */
    int32_t skel;     /* (finger curls from the hand's skeleton) */
} VrInHand;
typedef struct { VrInHand h[2]; int32_t ok; int32_t err; } VrInFrame;
typedef struct { uint64_t set, act[VA_N], src[2]; int32_t ok; } VrInHandles;

#ifdef VRIN_IMPL
static int VrIn_Setup(struct VR_IVRInput_FnTable *I, const char *manifest, VrInHandles *H, char *msg, int cap)
{
    memset(H, 0, sizeof *H);
    if (!I || !I->SetActionManifestPath) { snprintf(msg, (size_t)cap, "no SteamVR Input interface"); return 0; }
    int e = (int)I->SetActionManifestPath((char *)manifest);
    if (e) { snprintf(msg, (size_t)cap, "SteamVR didn't take the action manifest %s (error %d)", manifest, e); return 0; }
    e = (int)I->GetActionSetHandle("/actions/fnvr", &H->set);
    if (e || !H->set) { snprintf(msg, (size_t)cap, "no action set (error %d)", e); return 0; }
    for (int i = 0; i < VA_N; i++) {
        e = (int)I->GetActionHandle((char *)k_vrinAct[i], &H->act[i]);
        if (e) { snprintf(msg, (size_t)cap, "action %s: error %d", k_vrinAct[i], e); return 0; }
    }
    I->GetInputSourceHandle("/user/hand/left", &H->src[0]);
    I->GetInputSourceHandle("/user/hand/right", &H->src[1]);
    H->ok = 1;
    snprintf(msg, (size_t)cap, "actions ready (hands %llu / %llu)", (unsigned long long)H->src[0], (unsigned long long)H->src[1]);
    return 1;
}
static void VrIn_Read(struct VR_IVRInput_FnTable *I, VrInHandles *H, VrInFrame *f)
{
    memset(f, 0, sizeof *f);
    if (!I || !H->ok) return;
    struct VRActiveActionSet_t as; memset(&as, 0, sizeof as); as.ulActionSet = H->set;
    int e = (int)I->UpdateActionState(&as, sizeof as, 1);
    if (e) { f->err = e; return; }
    for (int h = 0; h < 2; h++) {
        VrInHand *o = &f->h[h]; VRInputValueHandle_t src = H->src[h];
        struct InputAnalogActionData_t a; struct InputDigitalActionData_t d;
#define VRIN_AN(i) (memset(&a, 0, sizeof a), I->GetAnalogActionData(H->act[i], &a, sizeof a, src) == 0 && a.bActive)
#define VRIN_DG(i) (memset(&d, 0, sizeof d), I->GetDigitalActionData(H->act[i], &d, sizeof d, src) == 0 && d.bActive)
        if (VRIN_AN(VA_STICK))       { o->stick[0] = a.x; o->stick[1] = a.y; o->active = 1; }
        if (VRIN_AN(VA_TRIGGER))     { o->trigger = a.x; o->active = 1; }
        if (VRIN_AN(VA_GRIP))        { o->grip = a.x; o->active = 1; }
        if (VRIN_DG(VA_GRIPCLICK))   { o->gripDown = d.bState; o->active = 1; }
        if (VRIN_DG(VA_TRIGGERCLICK)){ o->triggerDown = d.bState; o->active = 1; }
        if (VRIN_DG(VA_PRIMARY))     { o->primary = d.bState; o->active = 1; }
        if (VRIN_DG(VA_SECONDARY))   { o->secondary = d.bState; o->active = 1; }
        if (VRIN_DG(VA_STICKCLICK))  { o->stickDown = d.bState; o->active = 1; }
        if (VRIN_DG(VA_THUMBTOUCH))  { o->thumbTouch = d.bState; }
#undef VRIN_AN
#undef VRIN_DG
        struct VRSkeletalSummaryData_t s; memset(&s, 0, sizeof s);
        if (I->GetSkeletalSummaryData && I->GetSkeletalSummaryData(H->act[h ? VA_SKEL_R : VA_SKEL_L], EVRSummaryType_VRSummaryType_FromDevice, &s) == 0) {
            bool any = false; for (int k = 0; k < 5; k++) { float c = s.flFingerCurl[k]; o->curl[k] = c < 0 ? 0 : c > 1 ? 1 : c; if (c > 0.001f) any = true; }
            o->skel = any || o->active;
        }
    }
    f->ok = 1;
}
static void VrIn_Haptic(struct VR_IVRInput_FnTable *I, VrInHandles *H, int hand, float seconds, float freq, float amp)
{
    if (!I || !H->ok || hand < 0 || hand > 1 || !I->TriggerHapticVibrationAction) return;
    I->TriggerHapticVibrationAction(H->act[VA_HAPTIC], 0.0f, seconds, freq, amp, H->src[hand]);
}
#endif
