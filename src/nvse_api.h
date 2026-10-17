/* Minimal subset of the xNVSE plugin API (layout mirrors xNVSE nvse/PluginAPI.h). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef uint32_t PluginHandle;

typedef struct NVSEInterface {
    uint32_t nvseVersion;
    uint32_t runtimeVersion;
    uint32_t editorVersion;
    uint32_t isEditor;
    void    *RegisterCommand;
    void    *SetOpcodeBase;
    void   *(*QueryInterface)(uint32_t id);
    PluginHandle (*GetPluginHandle)(void);
    void    *RegisterTypedCommand;
    const char *(*GetRuntimeDirectory)(void);
    uint32_t isNogore;
} NVSEInterface;

typedef struct PluginInfo {
    uint32_t    infoVersion;   /* must be 1 */
    const char *name;
    uint32_t    version;
} PluginInfo;

typedef struct NVSEMessage {
    const char *sender;
    uint32_t    type;
    uint32_t    dataLen;
    void       *data;
} NVSEMessage;

typedef void (*EventCallback)(NVSEMessage *msg);

typedef struct NVSEMessagingInterface {
    uint32_t version;
    bool (*RegisterListener)(PluginHandle listener, const char *sender, EventCallback handler);
    bool (*Dispatch)(PluginHandle sender, uint32_t messageType, void *data, uint32_t dataLen, const char *receiver);
} NVSEMessagingInterface;

enum { kInterface_Messaging = 2 };

enum {
    kMessage_PostLoad = 0,
    kMessage_ExitGame,
    kMessage_ExitToMainMenu,
    kMessage_LoadGame,
    kMessage_SaveGame,
    kMessage_ScriptPrecompile,
    kMessage_PreLoadGame,
    kMessage_ExitGame_Console,
    kMessage_PostLoadGame,      /* 8 */
    kMessage_PostPostLoad,
    kMessage_RuntimeScriptError,
    kMessage_DeleteGame,
    kMessage_RenameGame,
    kMessage_RenameNewGame,
    kMessage_NewGame,           /* 14 */
    kMessage_DeleteGameName,
    kMessage_RenameGameName,
    kMessage_RenameNewGameName,
    kMessage_DeferredInit,      /* 18 */
    kMessage_ClearScriptDataCache,
    kMessage_MainGameLoop,      /* 20 */
    kMessage_ScriptCompile,     /* 21 */
    kMessage_EventListDestroyed,/* 22 */
    kMessage_PostQueryPlugins,  /* 23 */
    kMessage_OnFramePresent,    /* 24 */
};

#define RUNTIME_VERSION_1_4_0_525  0x040020D0u
