#include "Database.h"

#include <chrono>
#include <sstream>
#include <stdexcept>

#include "Logger.h"

namespace EOSBanManager {

    Database::Database() = default;

    Database::~Database() {
        Shutdown();
    }

    bool Database::Init(const std::string& host,
                        unsigned int       port,
                        const std::string& user,
                        const std::string& password,
                        const std::string& dbname,
                        const std::string& table) {
        std::lock_guard<std::mutex> lk(mtx_);

        host_     = host;
        port_     = port;
        user_     = user;
        password_ = password;
        dbname_   = dbname;
        table_    = table;

        conn_ = mysql_init(nullptr);
        if (!conn_) {
            EOSLog::Error("mysql_init a renvoyé null");
            return false;
        }

        // Reconnexion automatique (my_bool a disparu dans MySQL 8 → on utilise bool)
        bool reconnect = true;
        mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

        // UTF-8
        mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

        if (!mysql_real_connect(conn_,
                                host_.c_str(),
                                user_.c_str(),
                                password_.c_str(),
                                dbname_.c_str(),
                                port_,
                                nullptr,
                                0)) {
            EOSLog::Error("Connexion MySQL échouée: %s", mysql_error(conn_));
            mysql_close(conn_);
            conn_ = nullptr;
            return false;
        }

        // Création de la table si absente
        std::ostringstream q;
        q << "CREATE TABLE IF NOT EXISTS `" << table_ << "` ("
          << "  `eos_id`      VARCHAR(64)  NOT NULL PRIMARY KEY,"
          << "  `player_name` VARCHAR(128) NOT NULL DEFAULT '',"
          << "  `reason`      VARCHAR(255) NOT NULL DEFAULT '',"
          << "  `banned_by`   VARCHAR(128) NOT NULL DEFAULT '',"
          << "  `banned_at`   BIGINT       NOT NULL DEFAULT 0,"
          << "  INDEX `idx_banned_at` (`banned_at`)"
          << ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("Création de la table échouée: %s", mysql_error(conn_));
            return false;
        }

        EOSLog::Info("Database initialisée (table `%s`)", table_.c_str());
        return true;
    }

    void Database::Shutdown() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (conn_) {
            mysql_close(conn_);
            conn_ = nullptr;
        }
    }

    bool Database::EnsureConnection() {
        if (!conn_) return false;
        if (mysql_ping(conn_) != 0) {
            EOSLog::Warn("MySQL ping échoué, tentative de reconnexion: %s", mysql_error(conn_));
            mysql_close(conn_);
            conn_ = mysql_init(nullptr);
            bool reconnect = true;
            mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);
            mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
            if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(),
                                    password_.c_str(), dbname_.c_str(),
                                    port_, nullptr, 0)) {
                EOSLog::Error("Reconnexion MySQL échouée: %s", mysql_error(conn_));
                mysql_close(conn_);
                conn_ = nullptr;
                return false;
            }
        }
        return true;
    }

    std::string Database::Escape(const std::string& in) {
        if (!conn_) return in;
        std::string out;
        out.resize(in.size() * 2 + 1);
        unsigned long len = mysql_real_escape_string(conn_, &out[0], in.c_str(),
                                                     static_cast<unsigned long>(in.size()));
        out.resize(len);
        return out;
    }

    bool Database::BanEOS(const std::string& eos_id,
                          const std::string& player_name,
                          const std::string& reason,
                          const std::string& banned_by) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!EnsureConnection()) return false;

        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();

        std::ostringstream q;
        q << "INSERT INTO `" << table_ << "` "
          << "(`eos_id`,`player_name`,`reason`,`banned_by`,`banned_at`) VALUES ('"
          << Escape(eos_id)     << "','"
          << Escape(player_name)<< "','"
          << Escape(reason)     << "','"
          << Escape(banned_by)  << "',"
          << now
          << ") ON DUPLICATE KEY UPDATE "
          << "`player_name`=VALUES(`player_name`),"
          << "`reason`=VALUES(`reason`),"
          << "`banned_by`=VALUES(`banned_by`),"
          << "`banned_at`=VALUES(`banned_at`);";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("BanEOS échoué: %s", mysql_error(conn_));
            return false;
        }
        return true;
    }

    bool Database::UnbanEOS(const std::string& eos_id) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!EnsureConnection()) return false;

        std::ostringstream q;
        q << "DELETE FROM `" << table_ << "` WHERE `eos_id`='"
          << Escape(eos_id) << "';";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("UnbanEOS échoué: %s", mysql_error(conn_));
            return false;
        }
        return mysql_affected_rows(conn_) > 0;
    }

    bool Database::IsBanned(const std::string& eos_id) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!EnsureConnection()) return false;

        std::ostringstream q;
        q << "SELECT 1 FROM `" << table_ << "` WHERE `eos_id`='"
          << Escape(eos_id) << "' LIMIT 1;";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("IsBanned échoué: %s", mysql_error(conn_));
            return false;
        }
        MYSQL_RES* res = mysql_store_result(conn_);
        if (!res) return false;
        bool banned = (mysql_num_rows(res) > 0);
        mysql_free_result(res);
        return banned;
    }

    std::vector<BanEntry> Database::ListBans() {
        std::vector<BanEntry> out;
        std::lock_guard<std::mutex> lk(mtx_);
        if (!EnsureConnection()) return out;

        std::ostringstream q;
        q << "SELECT `eos_id`,`player_name`,`reason`,`banned_by`,`banned_at` FROM `"
          << table_ << "` ORDER BY `banned_at` DESC;";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("ListBans échoué: %s", mysql_error(conn_));
            return out;
        }
        MYSQL_RES* res = mysql_store_result(conn_);
        if (!res) return out;

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr) {
            BanEntry e;
            e.eos_id      = row[0] ? row[0] : "";
            e.player_name = row[1] ? row[1] : "";
            e.reason      = row[2] ? row[2] : "";
            e.banned_by   = row[3] ? row[3] : "";
            e.banned_at   = row[4] ? std::stoll(row[4]) : 0;
            out.push_back(std::move(e));
        }
        mysql_free_result(res);
        return out;
    }

    bool Database::GetBan(const std::string& eos_id, BanEntry& out) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!EnsureConnection()) return false;

        std::ostringstream q;
        q << "SELECT `eos_id`,`player_name`,`reason`,`banned_by`,`banned_at` FROM `"
          << table_ << "` WHERE `eos_id`='" << Escape(eos_id) << "' LIMIT 1;";

        if (mysql_query(conn_, q.str().c_str()) != 0) {
            EOSLog::Error("GetBan échoué: %s", mysql_error(conn_));
            return false;
        }
        MYSQL_RES* res = mysql_store_result(conn_);
        if (!res) return false;
        MYSQL_ROW row = mysql_fetch_row(res);
        bool found = false;
        if (row) {
            out.eos_id      = row[0] ? row[0] : "";
            out.player_name = row[1] ? row[1] : "";
            out.reason      = row[2] ? row[2] : "";
            out.banned_by   = row[3] ? row[3] : "";
            out.banned_at   = row[4] ? std::stoll(row[4]) : 0;
            found = true;
        }
        mysql_free_result(res);
        return found;
    }

} // namespace EOSBanManager
