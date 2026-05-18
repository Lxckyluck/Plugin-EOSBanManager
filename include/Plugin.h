#pragma once

#include <memory>
#include <string>
#include "Database.h"

// API AsaApi : headers fournis par le SDK AsaApi (https://github.com/ServersHub/AsaApi)
// Ces includes sont fournis par le SDK lors de la compilation.
#include "API/ARK/Ark.h"

namespace EOSBanManager {

    struct Config {
        // MySQL
        std::string  mysql_host     = "127.0.0.1";
        unsigned int mysql_port     = 3306;
        std::string  mysql_user     = "ark";
        std::string  mysql_password = "changeme";
        std::string  mysql_database = "ark_bans";
        std::string  mysql_table    = "eos_bans";

        // Messages
        std::string  kick_message       = "You are banned from this server.";
        std::string  ban_broadcast      = "{name} a été banni ({reason}).";
        std::string  unban_broadcast    = "{name} a été débanni.";
        bool         broadcast_on_ban   = true;
        bool         broadcast_on_unban = false;

        // Permissions: niveau requis (Cheat = admin connecté)
        // Si tu utilises le plugin "Permissions" tu peux remplacer par un check de groupe.
        bool require_admin = true;
    };

    class Plugin {
    public:
        static Plugin& Get();

        bool Load();
        void Unload();

        Database& Db()        { return db_; }
        const Config& Cfg() const { return cfg_; }

        // Charge / recharge le config.json
        bool ReloadConfig();

        // Helpers pour kick un joueur déjà connecté à partir de son EOSID
        void KickPlayerByEOS(const std::string& eos_id, const std::string& reason);

        // Récupère l'EOSID d'un controller (utilisé dans le hook de login)
        static std::string GetEOSID(AShooterPlayerController* controller);

        // Récupère le pseudo
        static std::string GetPlayerName(AShooterPlayerController* controller);

    private:
        Plugin() = default;

        Database db_;
        Config   cfg_;
        bool     loaded_ = false;
    };

} // namespace EOSBanManager
