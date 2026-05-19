// Point d'entrée du plugin AsaApi.
//
// AsaApi recherche les exports suivants dans la DLL:
//   Plugin_Init()   appelé au chargement
//   Plugin_Unload() appelé au déchargement / shutdown
//
// La macro DllMain reste le point d'entrée Windows classique.

#include <Windows.h>

#include "Plugin.h"
#include "Logger.h"

namespace EOSBanManager {
    void RegisterCommands();
    void UnregisterCommands();
    void RegisterHooks();
    void UnregisterHooks();
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/,
                      DWORD   ul_reason_for_call,
                      LPVOID  /*lpReserved*/) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

// Exports attendus par AsaApi
extern "C" __declspec(dllexport) void Plugin_Init() {
    EOSLog::Info("Plugin_Init: démarrage EOSBanManager");
    if (!EOSBanManager::Plugin::Get().Load()) {
        EOSLog::Error("Plugin_Init: échec du chargement.");
        return;
    }
    EOSBanManager::RegisterCommands();
    EOSBanManager::RegisterHooks();
    EOSLog::Info("Plugin_Init: prêt.");
}

extern "C" __declspec(dllexport) void Plugin_Unload() {
    EOSLog::Info("Plugin_Unload: arrêt du plugin EOSBanManager");
    EOSBanManager::UnregisterHooks();
    EOSBanManager::UnregisterCommands();
    EOSBanManager::Plugin::Get().Unload();
}
