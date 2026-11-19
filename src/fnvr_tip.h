#pragma once
#include <stdio.h>
#include <string.h>
/* FNVR: the pointing ('tip') and holding ('handgrip') places of a controller, from SteamVR's description of its model -
   asked for as a string property no runtime has (FNVR_TIP_PROP); answer "tx ty tz fx fy fz gx gy gz" (positions and the tip's
   forward, in the controller's own space) */
#define FNVR_TIP_PROP 0x7F00F001
static uint32_t FNVR_TipString(struct VR_IVRSystem_FnTable *sys, struct VR_IVRRenderModels_FnTable *rm, TrackedDeviceIndex_t d, char *buf, uint32_t n)
{
    if (!sys || !rm || !buf || n < 8) return 0;
    char model[256] = ""; ETrackedPropertyError e = ETrackedPropertyError_TrackedProp_Success;
    sys->GetStringTrackedDeviceProperty(d, ETrackedDeviceProperty_Prop_RenderModelName_String, model, sizeof model, &e);
    if (!model[0]) return 0;
    VRControllerState_t cs; memset(&cs, 0, sizeof cs); struct RenderModel_ControllerMode_State_t ms; ms.bScrollWheelVisible = false;
    RenderModel_ComponentState_t tip, grip; memset(&tip, 0, sizeof tip); memset(&grip, 0, sizeof grip);
    if (!rm->GetComponentState(model, "tip", &cs, &ms, &tip)) return 0;
    bool g = rm->GetComponentState(model, "handgrip", &cs, &ms, &grip);
    const float (*t)[4] = tip.mTrackingToComponentLocal.m, (*h)[4] = grip.mTrackingToComponentLocal.m;
    return (uint32_t)snprintf(buf, n, "%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f", t[0][3], t[1][3], t[2][3], -t[0][2], -t[1][2], -t[2][2],
                              g ? h[0][3] : 0.0f, g ? h[1][3] : 0.0f, g ? h[2][3] : 0.0f) + 1;
}
