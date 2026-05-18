#include "Plugin.h"
#include "Logger.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp> // fourni par AsaApi (vendored)

namespace EOSBanManager {

    using json = nlohmann::json;

    Plugin& Plugin::Get() {
        static Plugin instance;
        return instance;
    }

    static std::filesystem::path ConfigPath() {
        // AsaApi place les DLL plugins dans:
        //   ShooterGame/Binaries/Win64/ArkApi/Plugins/EOSBanManager/
        // On cherche config.json à côté de la DLL.
        return std::filesystem::path(ArkApi::Tools::GetCurrentDir())
            / "ArkApi" / "Plugins" / "EOSBanManager" / "config.json";
    }

    bool Plugin::ReloadConfig() {
        const auto path = ConfigPath();
        if (!std::filesystem::exists(path)) {
            Log::Error("config.json introuvable: %s", path.string().c_str());
            return false;
        }

        try {
            std::ifstream f(path);
            json j;
            f >> j;

            cfg_.mysql_host     = j.value("/MySQL/Host"_json_pointer,     cfg_.mysql_host);
            cfg_.mysql_port     = j.value("/MySQL/Port"_json_pointer,     cfg_.mysql_port);
            cfg_.mysql_user     = j.value("/MySQL/User"_json_pointer,     cfg_.mysql_user);
            cfg_.mysql_password = j.value("/MySQL/Password"_json_pointer, cfg_.mysql_password);
            cfg_.mysql_database = j.value("/MySQL/Database"_json_pointer, cfg_.mysql_database);
            cfg_.mysql_table    = j.value("/MySQL/Table"_json_pointer,    cfg_.mysql_table);

            cfg_.kick_message       = j.value("/Messages/KickMessage"_json_pointer,    cfg_.kick_message);
            cfg_.ban_broadcast      = j.value("/Messages/BanBroadcast"_json_pointer,   cfg_.ban_broadcast);
            cfg_.unban_broadcast    = j.value("/Messages/UnbanBroadcast"_json_pointer, cfg_.unban_broadcast);
            cfg_.broadcast_on_ban   = j.value("/Messages/BroadcastOnBan"_json_pointer,   cfg_.broadcast_on_ban);
            cfg_.broadcast_on_unban = j.value("/Messages/BroadcastOnUnban"_json_pointer, cfg_.broadcast_on_unban);

            cfg_.require_admin = j.value("/Permissions/RequireAdmin"_json_pointer, cfg_.require_admin);
        }
        catch (const std::exception& e) {
            Log::Error("Erreur de parsing config.json: %s", e.what());
            return false;
        }
        Log::Info("Config rechargée.");
        return true;
    }

    bool Plugin::Load() {
        if (loaded_) return true;
        Log::Info("Chargement du plugin EOSBanManager...");

        if (!ReloadConfig()) {
            Log::Error("Chargement annulé : config invalide.");
            return false;
        }

        if (!db_.Init(cfg_.mysql_host, cfg_.mysql_port,
                      cfg_.mysql_user, cfg_.mysql_password,
                      cfg_.mysql_database, cfg_.mysql_table)) {
            Log::Error("Impossible de se connecter à MySQL — plugin désactivé.");
            return false;
        }

        loaded_ = true;
        Log::Info("Plugin EOSBanManager chargé.");
        return true;
    }

    void Plugin::Unload() {
        if (!loaded_) return;
        db_.Shutdown();
        loaded_ = false;
        Log::Info("Plugin EOSBanManager déchargé.");
    }

    std::string Plugin::GetEOSID(AShooterPlayerController* controller) {
        if (!controller) return "";
        FString eos;
        // AsaApi expose un helper officiel pour récupérer l'EOSID
        // depuis l'UniqueNetId du joueur.
        if (controller->GetEOSIdFromController(controller, &eos)) {
            return std::string(TCHAR_TO_UTF8(*eos));
        }
        return "";
    }

    std::string Plugin::GetPlayerName(AShooterPlayerController* controller) {
        if (!controller) return "";
        FString name;
        AsaApi::GetApiUtils().GetShooterGameMode();
        controller->GetPlayerCharacterName(&name);
        return std::string(TCHAR_TO_UTF8(*name));
    }

    void Plugin::KickPlayerByEOS(const std::string& eos_id, const std::string& reason) {
        // Parcourt les controllers connectés et kick le bon.
        auto& world = AsaApi::GetApiUtils();
        auto controllers = world.GetWorld()->PlayerControllerListField();
        for (TWeakObjectPtr<APlayerController>& weak : controllers) {
            APlayerController* pc = weak.Get();
            auto* shooter = static_cast<AShooterPlayerController*>(pc);
            if (!shooter) continue;
            if (GetEOSID(shooter) == eos_id) {
                FString r = FString(reason.c_str());
                shooter->ClientNotifyKicked(&r, true, false);
                // Force déconnexion
                world.GetShooterGameMode()->KickPlayerController(shooter, &r);
                Log::Info("Kické joueur EOS=%s (%s)", eos_id.c_str(), reason.c_str());
                return;
            }
        }
    }

} // namespace EOSBanManager
