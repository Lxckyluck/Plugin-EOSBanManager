# Contexte du projet EOSBanManager — Handoff

Ce document résume tout l'état du projet pour reprendre dans une autre session Claude. Colle ce fichier dans le nouveau chat avec ton prochain message.

## Objectif

Plugin C++ pour serveur ARK: Survival Ascended, basé sur **AsaApi v1.21** (fork Pelayori). Permet de bannir des joueurs par **EOSID**, avec stockage **MySQL 8.0.27**, commandes admin (console / chat / RCON), liste des bans, unban, et kick automatique au login.

## Environnement

- **OS** : Windows Server 2022 (compte `Administrator`)
- **IDE** : Visual Studio 2026 — toolset `v143` installé en plus pour matcher AsaApi
- **MySQL** : 8.0.27 (zip), racine `C:\mysql-8.0.27-winx64\mysql-8.0.27-winx64\`
- **vcpkg** : installé à `C:\vcpkg\`, intégré (`vcpkg integrate install` fait)

## Structure des dossiers

```
C:\Users\Administrator\Documents\Code\
├── AsaApi\                                <- SDK cloné de Pelayori, v1.21
│   ├── AsaApi.sln                          (solution Visual Studio)
│   ├── x64\Release\AsaApi.lib              (généré après build de AsaApi)
│   ├── AsaApi\
│   │   ├── Core\Public\                    (headers : Ark.h, json.hpp, Logger/, etc.)
│   │   │   ├── API\
│   │   │   │   ├── ARK\Ark.h
│   │   │   │   └── UE\CoreTypes.h
│   │   │   ├── Logger\spdlog\fmt\bundled\  (fmt vendored ici aussi)
│   │   │   └── json.hpp                    (nlohmann/json à plat, namespace nlohmann::)
│   │   └── vcpkg_installed\x64-windows-1439-static-md\x64-windows-1439-static-md\
│   │       ├── include\fmt\                (fmt depuis vcpkg)
│   │       ├── include\openssl\
│   │       └── lib\
│   └── vcpkg.json (copié à la racine pour activation manifest mode)
└── Plugin-EOSBanManager\                   <- mon plugin, repo GitHub
    ├── EOSBanManager.vcxproj
    ├── CMakeLists.txt
    ├── src\ {dllmain.cpp, Plugin.cpp, Database.cpp, Commands.cpp, Hooks.cpp}
    ├── include\ {Plugin.h, Database.h, Logger.h}
    └── config\ {config.json, config.example.json, PluginInfo.json}
```

## Configuration de build (EOSBanManager.vcxproj)

```xml
<AsaApiRoot>$(SolutionDir)..\AsaApi</AsaApiRoot>
<MySQLRoot>C:\mysql-8.0.27-winx64\mysql-8.0.27-winx64</MySQLRoot>
<PlatformToolset>$(DefaultPlatformToolset)</PlatformToolset>
<RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary>   <!-- /MD pour matcher AsaApi -->
```

**Include directories** :
- `$(ProjectDir)include`
- `$(AsaApiRoot)\AsaApi\Core\Public`
- `$(AsaApiRoot)\AsaApi\Core\Public\API`
- `$(AsaApiRoot)\AsaApi\Core\Public\API\UE`
- `$(AsaApiRoot)\AsaApi\vcpkg_installed\x64-windows-1439-static-md\x64-windows-1439-static-md\include`
- `$(MySQLRoot)\include`

**Library directories** :
- `$(AsaApiRoot)\x64\Release`
- `$(AsaApiRoot)\AsaApi\vcpkg_installed\x64-windows-1439-static-md\x64-windows-1439-static-md\lib`
- `$(MySQLRoot)\lib`

**Linker deps** : `AsaApi.lib;libmysql.lib`

## Décisions clés prises

- Stockage MySQL (pas SQLite/JSON) — permet de partager les bans entre serveurs.
- Storage minimal : table `eos_bans` créée auto au premier connect.
- Commandes triple : console admin (`cheat BanEOS X`), chat admin (`/baneos X`), RCON (`BanEOS X`).
- Kick auto au login via hook `AShooterGameMode.HandleNewPlayer_Implementation`.
- Namespace de log renommé `EOSLog::` (conflit avec `Log::` d'AsaApi).
- json fourni par AsaApi vendored → `#include "json.hpp"` direct (pas `nlohmann/json.hpp`).

## ✅ Fixes appliqués pour les 6 erreurs v1.21 (vérifie après build)

- `KickPlayerController(pc, &fmsg)` → `KickPlayerController(pc, fmsg)` dans Plugin.cpp et Hooks.cpp
- Handlers console : signature changée à `void(APlayerController*, FString*, bool)` avec cast interne
- Handlers chat : signature changée à `void(AShooterPlayerController*, FString*, int, int)`
- `controller->GetEOSIdFromController(...)` → `AsaApi::GetApiUtils().GetEOSIDFromController(controller)` *(à vérifier — nom exact à confirmer dans les headers AsaApi)*
- `ArkApi::Tools::GetCurrentDir()` → `std::filesystem::current_path()`
- Tous les appels `pc->ClientNotifyKicked(...)` supprimés (`KickPlayerController` suffit)

## ERREURS DE COMPIL POTENTIELLES — si elles reviennent

Voici la liste actuelle d'erreurs avec mes hypothèses de correctif :

### 1. `KickPlayerController` attend `const FString&`, pas `FString*`
**Fichiers concernés** : `src/Plugin.cpp` ligne ~119, `src/Hooks.cpp` ligne ~54
**Fix** : remplacer `gm->KickPlayerController(pc, &fmsg)` par `gm->KickPlayerController(pc, fmsg)` (passer la FString par valeur, pas l'adresse). Idem pour `world.GetShooterGameMode()->KickPlayerController(shooter, &r)` → `KickPlayerController(shooter, r)`.

### 2. `AddConsoleCommand` veut `APlayerController*`, pas `AShooterPlayerController*`
**Fichier** : `src/Commands.cpp`
**Fix** : changer les signatures des handlers `Console_BanEOS`, `Console_UnbanEOS`, `Console_ListBans` de `void(AShooterPlayerController* pc, ...)` à `void(APlayerController* pc, ...)`, puis cast en interne :
```cpp
static void Console_BanEOS(APlayerController* pc_base, FString* cmd, bool shouldLog) {
    auto* pc = static_cast<AShooterPlayerController*>(pc_base);
    ...
}
```

### 3. `AddChatCommand` signature : `(AShooterPlayerController*, FString*, int, int)`
**Fichier** : `src/Commands.cpp`
**Fix** : changer la signature des `Chat_BanEOS`, etc. de `void(AShooterPlayerController*, FString*, EChatSendMode::Type)` à `void(AShooterPlayerController* pc, FString* msg, int chat_type, int unknown)`. On peut ignorer les deux derniers paramètres.

### 4. `GetEOSIdFromController` n'est PAS membre de `AShooterPlayerController`
**Fichier** : `src/Plugin.cpp` méthode `GetEOSID`
**Fix probable** : c'est exposé via `AsaApi::GetApiUtils()`. Essayer :
```cpp
FString eos = AsaApi::GetApiUtils().GetEOSIDFromPlayerController(controller);
return std::string(TCHAR_TO_UTF8(*eos));
```
**À vérifier dans v1.21** — chercher dans `AsaApi\Core\Public\API\Helpers\Helpers.h` ou équivalent la méthode exacte. Peut aussi être `GetEOSIDFromController` (cas).

### 5. `GetCurrentDir` identifier not found
**Fichier** : `src/Plugin.cpp` fonction `ConfigPath()`
**Fix probable** : changer `ArkApi::Tools::GetCurrentDir()` en `AsaApi::Tools::GetCurrentDir()` ou utiliser `std::filesystem::current_path()`. Mieux : récupérer le dossier de la DLL via `GetModuleFileName` Windows API.

### 6. `ClientNotifyKicked` n'est PAS membre de `AShooterPlayerController`
**Fichiers** : `src/Plugin.cpp`, `src/Hooks.cpp`
**Fix** : retirer les appels à `pc->ClientNotifyKicked(...)` — le `KickPlayerController` côté GameMode suffit. Ou utiliser la méthode équivalente exposée par AsaApi v1.21 (à chercher).

## Fixes DÉJÀ appliqués (pour mémoire, ne pas refaire)

- ✅ Renommage namespace `Log::` → `EOSLog::` (Logger.h + tous les .cpp)
- ✅ Retrait de `#include "API/Commands.h"` et `#include "API/Hooks.h"` (inclus via Ark.h)
- ✅ Changement `#include <nlohmann/json.hpp>` → `#include "json.hpp"`
- ✅ `my_bool reconnect = 1` → `bool reconnect = true` (MySQL 8 a viré my_bool)
- ✅ Runtime `/MT` → `/MD` (match avec triplet vcpkg `static-md` d'AsaApi)
- ✅ Toolset `v143` → `$(DefaultPlatformToolset)` (pour VS 2026 sans installer v143)
  - **Note** : a quand même fallu installer v143 en composant individuel parce qu'AsaApi pin v143 dans ses property sheets
- ✅ Include paths ajustés pour la structure AsaApi v1.21 réelle (`AsaApi\Core\Public\...`)

## Étapes APRÈS compilation réussie

1. **Déployer la DLL** dans `ShooterGame\Binaries\Win64\ArkApi\Plugins\EOSBanManager\` avec `PluginInfo.json` et `config.json`.
2. **Copier `libmysql.dll`** (depuis `C:\mysql-8.0.27-winx64\mysql-8.0.27-winx64\lib\`) à côté de `ArkAscendedServer.exe`.
3. **Créer la DB MySQL** :
   ```sql
   CREATE DATABASE ark_bans CHARACTER SET utf8mb4;
   CREATE USER 'ark_ban'@'localhost' IDENTIFIED WITH mysql_native_password BY '<mot_de_passe>';
   GRANT ALL PRIVILEGES ON ark_bans.* TO 'ark_ban'@'localhost';
   FLUSH PRIVILEGES;
   ```
4. **Remplir `config.json`** avec ces creds (ne pas utiliser `root`).
5. **Relancer le serveur ARK**, vérifier dans la console : `[EOSBanManager][INFO ] Plugin_Init: prêt.`

## Commandes en jeu (une fois déployé)

- Console (cheat) : `BanEOS <eos_id> [raison]`, `UnbanEOS <eos_id>`, `ListBans`
- Chat (admin) : `/baneos <eos_id> [raison]`, `/unbaneos <eos_id>`, `/listbans`
- RCON : `BanEOS <eos_id>`, `UnbanEOS <eos_id>`, `ListBans`

## Comment reprendre

Dans la nouvelle session :
1. Colle ce fichier en début de chat
2. Précise : "Continue depuis ce point — les erreurs en cours sont dans la section 'ERREURS DE COMPIL EN COURS'"
3. Connecte à nouveau le dossier du projet (`request_cowork_directory` sur `C:\Users\denis\Documents\Code\EOSBanManager`)
