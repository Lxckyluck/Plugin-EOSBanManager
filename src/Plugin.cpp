#include "Plugin.h"
#include "Logger.h"

#include <fstream>
#include <filesystem>
#include "json.hpp" // fourni par AsaApi (vendored, namespace nlohmann)

namespace EOSBanManager {

    using json = nlohmann::json;

    Plugin& Plugin::Get() {
        static Plugin instance;
        return instance;
    }

    static std::filesystem::path ConfigPath() {
        // AsaApi place les DLL plugins dans:
        //   ShooterGame/Binaries/Win64/ArkApi/Plugins/EOSBanManager/
        // On cherche config.json à côté de la DLL via le chemin courant du serveur.
        return std::filesystem::current_path()
            / "ArkApi" / "Plugins" / "EOSBanManager" / "config.json";
    }

    bool Plugin::ReloadConfig() {
        const auto path = ConfigPath();
        if (!std::filesystem::exists(path)) {
            EOSLog::Error("config.json introuvable: %s", path.string().c_str());
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
            EOSLog::Error("Erreur de parsing config.json: %s", e.what());
            return false;
        }
        EOSLog::Info("Config rechargée.");
        return true;
    }

    bool Plugin::Load() {
        if (loaded_) return true;
        EOSLog::Info("Chargement du plugin EOSBanManager...");

        if (!ReloadConfig()) {
            EOSLog::Error("Chargement annulé : config invalide.");
            return false;
        }

        if (!db_.Init(cfg_.mysql_host, cfg_.mysql_port,
                      cfg_.mysql_user, cfg_.mysql_password,
                      cfg_.mysql_database, cfg_.mysql_table)) {
            EOSLog::Error("Impossible de se connecter à MySQL — plugin désactivé.");
            return false;
        }

        loaded_ = true;
        EOSLog::Info("Plugin EOSBanManager chargé.");
        return true;
    }

    void Plugin::Unload() {
        if (!loaded_) return;
        db_.Shutdown();
        loaded_ = false;
        EOSLog::Info("Plugin EOSBanManager déchargé.");
    }

    std::string Plugin::GetEOSID(AShooterPlayerController* controller) {
        if (!controller) return "";
        // AsaApi v1.21 expose un helper via ApiUtils pour récupérer l'EOSID
        // depuis l'UniqueNetId du PlayerController.
        FString eos = AsaApi::GetApiUtils().GetEOSIDFromController(controller);
        return std::string(TCHAR_TO_UTF8(*eos));
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
                // v1.21: KickPlayerController prend (APlayerController*, const FString&)
                world.GetShooterGameMode()->KickPlayerController(shooter, r);
                EOSLog::Info("Kické joueur EOS=%s (%s)", eos_id.c_str(), reason.c_str());
                return;
            }
        }
    }

} // namespace EOSBanManager
