#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

// MySQL Connector/C++ (libmysql / mysql.h via Connector/C)
// On utilise l'API C de MySQL Connector pour rester léger.
// Tu peux remplacer par mysqlx ou mariadbpp si tu préfères.
#include <mysql.h>

namespace EOSBanManager {

    struct BanEntry {
        std::string eos_id;
        std::string player_name;   // optionnel — dernier nom connu
        std::string reason;
        std::string banned_by;
        long long   banned_at;     // unix timestamp
    };

    class Database {
    public:
        Database();
        ~Database();

        // Initialise la connexion et crée la table si elle n'existe pas
        bool Init(const std::string& host,
                  unsigned int       port,
                  const std::string& user,
                  const std::string& password,
                  const std::string& dbname,
                  const std::string& table);

        // Ferme la connexion proprement
        void Shutdown();

        // Ajoute un EOSID dans la liste de bans. Retourne false si déjà banni ou erreur.
        bool BanEOS(const std::string& eos_id,
                    const std::string& player_name,
                    const std::string& reason,
                    const std::string& banned_by);

        // Retire un EOSID de la liste de bans. Retourne true si une ligne a été supprimée.
        bool UnbanEOS(const std::string& eos_id);

        // True si l'EOSID est présent dans la table de bans.
        bool IsBanned(const std::string& eos_id);

        // Récupère tous les bans.
        std::vector<BanEntry> ListBans();

        // Récupère un ban précis (eos_id) — retourne false si absent.
        bool GetBan(const std::string& eos_id, BanEntry& out);

    private:
        // Reconnecte si la connexion a sauté (timeout, MySQL gone away, etc.)
        bool EnsureConnection();

        // Échappe une chaîne pour l'insertion SQL.
        std::string Escape(const std::string& in);

        MYSQL*       conn_ = nullptr;
        std::string  host_;
        unsigned int port_ = 3306;
        std::string  user_;
        std::string  password_;
        std::string  dbname_;
        std::string  table_;

        std::mutex   mtx_; // toutes les opérations DB sont sérialisées
    };

} // namespace EOSBanManager
