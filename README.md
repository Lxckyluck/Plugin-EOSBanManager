# EOSBanManager

Plugin **AsaApi** pour ARK: Survival Ascended permettant de bannir / débannir des joueurs par leur **EOSID**, avec stockage **MySQL** et kick automatique au login.

## Fonctionnalités

- Ban / Unban / Liste des bans via **commandes console admin**, **commandes chat admin** et **RCON**
- Stockage **MySQL** (table auto-créée au premier lancement)
- **Kick automatique** au login d'un EOSID banni
- Diffusion (broadcast) optionnelle du ban dans le chat global

## Commandes

| Type     | Commande                                  | Exemple                                          |
|----------|-------------------------------------------|--------------------------------------------------|
| Console  | `cheat BanEOS <eos_id> [raison...]`       | `cheat BanEOS 0002abcd1234... toxicité`          |
| Console  | `cheat UnbanEOS <eos_id>`                 | `cheat UnbanEOS 0002abcd1234...`                 |
| Console  | `cheat ListBans`                          | `cheat ListBans`                                 |
| Chat     | `/baneos <eos_id> [raison...]`            | `/baneos 0002abcd1234... cheats`                 |
| Chat     | `/unbaneos <eos_id>`                      | `/unbaneos 0002abcd1234...`                      |
| Chat     | `/listbans`                               | `/listbans`                                      |
| RCON     | `BanEOS <eos_id> [raison...]`             | (via ton outil RCON / bot Discord)               |
| RCON     | `UnbanEOS <eos_id>`                       |                                                  |
| RCON     | `ListBans`                                |                                                  |

Les commandes console et chat exigent par défaut que l'auteur soit admin connecté (`bIsAdmin`). RCON n'a pas de contrôle (par design, c'est déjà authentifié par mot de passe).

## Prérequis

- Serveur ARK: Survival Ascended avec **AsaApi** installé : https://github.com/ServersHub/AsaApi
- Visual Studio 2022 avec composants C++ (toolset v143)
- **MySQL Connector/C 6.1+** (ou compatible: libmysql.lib + mysql.h)
- Un serveur MySQL/MariaDB accessible depuis la machine du serveur ARK

## Compilation

1. Cloner ou télécharger ce dossier `EOSBanManager/`
2. Récupérer le SDK AsaApi à côté (structure suggérée) :
   ```
   MesPlugins/
     AsaApi/                 <- SDK AsaApi (Public/, Lib/)
     EOSBanManager/          <- ce dossier
   ```
3. Vérifier les chemins dans `EOSBanManager.vcxproj` (propriétés `AsaApiRoot` et `MySQLRoot`) ou dans `CMakeLists.txt`.
4. Ouvrir `EOSBanManager.vcxproj` dans Visual Studio, choisir la configuration **Release | x64**, puis **Build**.
   - Alternative CMake :
     ```
     cmake -S . -B build -A x64
     cmake --build build --config Release
     ```
5. Récupérer `EOSBanManager.dll` dans `x64/Release/` (ou `build/Release/`).

## Installation sur le serveur

1. Créer le dossier :
   ```
   ShooterGame/Binaries/Win64/ArkApi/Plugins/EOSBanManager/
   ```
2. Y copier :
   - `EOSBanManager.dll`
   - `config.json` (depuis `config/config.json`, à éditer)
   - `PluginInfo.json` (depuis `config/PluginInfo.json`)
3. Copier `libmysql.dll` (du Connector MySQL) à côté de `ArkAscendedServer.exe` ou dans le dossier du plugin.
4. Éditer `config.json` avec tes identifiants MySQL.
5. Relancer le serveur (ou taper `cheat ReloadPlugin EOSBanManager` si tu as déjà AsaApi chargé).

## Base MySQL

Le plugin crée tout seul la table à la première connexion. Schéma :

```sql
CREATE TABLE IF NOT EXISTS `eos_bans` (
  `eos_id`      VARCHAR(64)  NOT NULL PRIMARY KEY,
  `player_name` VARCHAR(128) NOT NULL DEFAULT '',
  `reason`      VARCHAR(255) NOT NULL DEFAULT '',
  `banned_by`   VARCHAR(128) NOT NULL DEFAULT '',
  `banned_at`   BIGINT       NOT NULL DEFAULT 0,
  INDEX `idx_banned_at` (`banned_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

Tu peux la partager entre plusieurs serveurs ARK pour un ban à l'échelle d'un cluster — il suffit que chaque serveur pointe vers la même DB.

## Configuration (config.json)

```json
{
  "MySQL": {
    "Host": "127.0.0.1",
    "Port": 3306,
    "User": "ark",
    "Password": "changeme",
    "Database": "ark_bans",
    "Table": "eos_bans"
  },
  "Messages": {
    "KickMessage": "Vous etes banni de ce serveur.",
    "BanBroadcast": "{name} a ete banni ({reason}).",
    "UnbanBroadcast": "{eos} a ete debanni.",
    "BroadcastOnBan": true,
    "BroadcastOnUnban": false
  },
  "Permissions": {
    "RequireAdmin": true
  }
}
```

Placeholders disponibles dans les messages : `{name}`, `{eos}`, `{reason}`.

## Comment trouver l'EOSID d'un joueur ?

En jeu, en tant qu'admin :
```
cheat ListPlayers
```
La sortie inclut l'EOSID pour chaque joueur connecté. Tu peux aussi le lire dans les logs serveur au login.

## Dépannage

- **"Connexion MySQL échouée"** : vérifie host/port/user/password et que `libmysql.dll` est trouvable (à côté de l'exécutable ou dans le PATH).
- **Le hook ne kick pas** : vérifie que la version d'AsaApi expose bien `AShooterGameMode.HandleNewPlayer_Implementation`. Sinon, adapter `Hooks.cpp` pour utiliser `AShooterGameMode.PostLogin` à la place.
- **Commande inconnue côté admin** : assure-toi d'avoir tapé `enablecheats <pass>` et d'être logué admin.

## Licence

MIT — fais-en ce que tu veux. PRs / suggestions bienvenues.
