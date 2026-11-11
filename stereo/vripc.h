/* FNVR Stereo - the game (32-bit) and FNVR-VRHost.exe (64-bit) talk through one block of shared memory.
   Copyright (C) 2026 FNVVRMC contributors. GPL-3.0-or-later.

   The game side (openvr_api.dll replacement) writes a request into `in`, sets `fn`, signals the request event and
   waits for the reply event; the host runs the real OpenVR call and writes the result into `out`.
   Only fixed-size types cross the boundary (no pointers): textures go across as shared D3D11 handles. */
#pragma once
#include <stdint.h>

#define VRIPC_VERSION 1
#define VRIPC_BUF     8192

enum {
    F_NONE = 0, F_HELLO, F_INIT, F_SHUTDOWN,
    /* IVRSystem */
    S_RTSIZE = 20, S_PROJMAT, S_PROJRAW, S_EYE2HEAD, S_DXGI, S_POSES, S_ROLEIDX, S_FLOATPROP, S_INT32PROP, S_BOOLPROP, S_STRINGPROP,
    S_EVENT, S_CTRLSTATE, S_HAPTIC, S_ACKQUIT, S_CONNECTED, S_DEVCLASS, S_VSYNC,
    /* IVRCompositor */
    C_TRACKSPACE = 60, C_WAITPOSES, C_SUBMIT, C_FOCUS, C_POSTPRESENT, C_LASTPOSES,
    /* IVROverlay */
    O_CREATE = 80, O_HIDE, O_SHOW, O_CURVE, O_FLAG, O_SORT, O_TEXTURE, O_XFORMABS, O_XFORMDEV, O_WIDTH, O_DESTROY, O_ALPHA, O_COLOR,
    /* SteamVR Input (vrinput.h): set up the actions, read every control of both hands at once, buzz a hand */
    I_SETUP = 100, I_READ, I_HAPTIC
};

typedef struct {
    volatile int32_t fn;          /* request id (0 = idle) */
    volatile int32_t hostReady;   /* host is up */
    volatile uint32_t hostPid;
    volatile int32_t version;
    volatile uint32_t calls;
    char hostLog[256];            /* last thing the host wants the game side to know */
    uint8_t in[VRIPC_BUF];
    uint8_t out[VRIPC_BUF];
} VrIpc;

/* names (formatted with the game's process id) */
#define VRIPC_SHM  "Local\\FNVR_VRHost_shm_%lu"
#define VRIPC_REQ  "Local\\FNVR_VRHost_req_%lu"
#define VRIPC_RSP  "Local\\FNVR_VRHost_rsp_%lu"

/* a texture handed across: a shared D3D11 handle (0 = none) and how to show it */
typedef struct { uint64_t shared; int32_t type, colorSpace; uint32_t w, h, fmt; } IpcTex;
