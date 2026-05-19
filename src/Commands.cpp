#include "Plugin.h"
#include "Logger.h"

#include "API/ARK/Ark.h"
// Note: les commands AsaApi sont déjà incluses via Ark.h dans v1.21

#include <sstream>
#include <vector>
#include <string>

namespace EOSBanManager {

    // -------- Utilitaires de parsing --------

    static std::vector<std::string> SplitArgs(const FString& fs) {
        std::vector<std::string> out;
        std::string s = TCHAR_TO_UTF8(*fs);
        std::istringstream iss(s);
        std::string tok;
        while (iss >> tok) out.push_back(tok);
        return out;
    }

    static std::string JoinFrom(const std::vector<std::string>& args, size_t from) {
        std::string out;
        for (size_t i = from; i < args.size(); ++i) {
            if (!out.empty()) out += " ";
            out += args[i];
        }
        return out;
    }

    static FString ToFString(const std::string& s) {
        return FString(s.c_str());
    }

    // -------- Implémentations partagées --------

    // executor : nom affiché de l'admin (ou "RCON" / "Console")
    // sendBack : callback pour répondre à l'auteur (admin/chat/RCON)
    template <typename SendBack>
    static void HandleBan(const std::vector<std::string>& args,
                          const std::string& executor,
                          SendBack&&         sendBack) {
        if (args.size() < 2) {
            sendBack("Usage: BanEOS <eos_id> [raison...]");
            return;
        }
        const std::string eos = args[1];
        const std::string reason = (args.size() >= 3) ? JoinFrom(args, 2) : "No reason";

        auto& plugin = Plugin::Get();

        // Tentative de récupérer le nom si le joueur est en ligne
        std::string player_name;
        auto& utils = AsaApi::GetApiUtils();
        auto controllers = utils.GetWorld()->PlayerControllerListField();
        for (TWeakObjectPtr<APlayerController>& weak : controllers) {
            auto* shooter = static_cast<AShooterPlayerController*>(weak.Get());
            if (!shooter) continue;
            if (Plugin::GetEOSID(shooter) == eos) {
                player_name = Plugin::GetPlayerName(shooter);
                break;
            }
        }

        if (!plugin.Db().BanEOS(eos, player_name, reason, executor)) {
            sendBack("Erreur lors de l'enregistrement du ban.");
            return;
        }

        // Kick si connecté
        plugin.KickPlayerByEOS(eos, plugin.Cfg().kick_message);

        std::ostringstream ok;
        ok << "EOS " << eos << " banni"
           << (player_name.empty() ? "" : (" (" + player_name + ")"))
           << " — raison: " << reason;
        sendBack(ok.str());

        if (plugin.Cfg().broadcast_on_ban) {
            std::string msg = plugin.Cfg().ban_broadcast;
            auto replace = [&](const std::string& key, const std::string& val) {
                size_t p; while ((p = msg.find(key)) != std::string::npos) msg.replace(p, key.size(), val);
            };
            replace("{name}",   player_name.empty() ? eos : player_name);
            replace("{eos}",    eos);
            replace("{reason}", reason);
            FString fmsg = ToFString(msg);
            utils.SendChatMessageToAll(FString("[Ban]"), *fmsg);
        }
    }

    template <typename SendBack>
    static void HandleUnban(const std::vector<std::string>& args,
                            const std::string& executor,
                            SendBack&&         sendBack) {
        if (args.size() < 2) {
            sendBack("Usage: UnbanEOS <eos_id>");
            return;
        }
        const std::string eos = args[1];

        auto& plugin = Plugin::Get();
        if (!plugin.Db().UnbanEOS(eos)) {
            sendBack("Aucun ban trouvé pour " + eos);
            return;
        }
        sendBack("EOS " + eos + " débanni par " + executor);

        if (plugin.Cfg().broadcast_on_unban) {
            std::string msg = plugin.Cfg().unban_broadcast;
            size_t p; while ((p = msg.find("{eos}")) != std::string::npos) msg.replace(p, 5, eos);
            FString fmsg = ToFString(msg);
            AsaApi::GetApiUtils().SendChatMessageToAll(FString("[Unban]"), *fmsg);
        }
    }

    template <typename SendBack>
    static void HandleList(SendBack&& sendBack) {
        auto& plugin = Plugin::Get();
        auto bans = plugin.Db().ListBans();
        if (bans.empty()) {
            sendBack("Aucun ban actif.");
            return;
        }
        std::ostringstream out;
        out << "Bans actifs (" << bans.size() << "):\n";
        for (const auto& b : bans) {
            out << " - " << b.eos_id;
            if (!b.player_name.empty()) out << " [" << b.player_name << "]";
            out << " par " << b.banned_by
                << " : " << b.reason << "\n";
        }
        sendBack(out.str());
    }

    // -------- Vérif admin --------

    static bool IsAdmin(AShooterPlayerController* pc) {
        if (!pc) return false;
        // AsaApi v1.21: bIsAdmin() est exposé sur APrimalPlayerController
        // (classe parente). Retourne un BitFieldValue<bool, uint32> cast implicite.
        return pc->bIsAdmin();
    }

    // -------- Wrappers AsaApi (Console / Chat / RCON) --------

    // CONSOLE (admin tapant la commande in-game avec ShowCheatManager / cheat XYZ)
    // Signature v1.21: void(APlayerController*, FString*, bool)
    static void Console_BanEOS(APlayerController* pc_base, FString* cmd, bool /*shouldLog*/) {
        auto* pc = static_cast<AShooterPlayerController*>(pc_base);
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        auto args = SplitArgs(*cmd);
        std::string executor = Plugin::GetPlayerName(pc);
        if (executor.empty()) executor = "Console";
        HandleBan(args, executor, [&](const std::string& reply) {
            AsaApi::GetApiUtils().SendNotification(pc, FColorList::Green, 1.3f, 5.f, nullptr, "%s", reply.c_str());
        });
    }

    static void Console_UnbanEOS(APlayerController* pc_base, FString* cmd, bool) {
        auto* pc = static_cast<AShooterPlayerController*>(pc_base);
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        auto args = SplitArgs(*cmd);
        std::string executor = Plugin::GetPlayerName(pc);
        if (executor.empty()) executor = "Console";
        HandleUnban(args, executor, [&](const std::string& reply) {
            AsaApi::GetApiUtils().SendNotification(pc, FColorList::Green, 1.3f, 5.f, nullptr, "%s", reply.c_str());
        });
    }

    static void Console_ListBans(APlayerController* pc_base, FString*, bool) {
        auto* pc = static_cast<AShooterPlayerController*>(pc_base);
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        HandleList([&](const std::string& reply) {
            AsaApi::GetApiUtils().SendNotification(pc, FColorList::White, 1.2f, 8.f, nullptr, "%s", reply.c_str());
        });
    }

    // CHAT (joueur admin qui tape /baneos ... dans le chat)
    // Signature v1.21: void(AShooterPlayerController*, FString*, int, int)
    static void Chat_BanEOS(AShooterPlayerController* pc, FString* msg, int /*chatType*/, int /*unk*/) {
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        auto args = SplitArgs(*msg);
        std::string executor = Plugin::GetPlayerName(pc);
        HandleBan(args, executor.empty() ? "Chat" : executor, [&](const std::string& reply) {
            AsaApi::GetApiUtils().SendChatMessage(pc, FString("[Ban]"), reply.c_str());
        });
    }

    static void Chat_UnbanEOS(AShooterPlayerController* pc, FString* msg, int, int) {
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        auto args = SplitArgs(*msg);
        std::string executor = Plugin::GetPlayerName(pc);
        HandleUnban(args, executor.empty() ? "Chat" : executor, [&](const std::string& reply) {
            AsaApi::GetApiUtils().SendChatMessage(pc, FString("[Unban]"), reply.c_str());
        });
    }

    static void Chat_ListBans(AShooterPlayerController* pc, FString*, int, int) {
        if (Plugin::Get().Cfg().require_admin && !IsAdmin(pc)) return;
        HandleList([&](const std::string& reply) {
            AsaApi::GetApiUtils().SendChatMessage(pc, FString("[Bans]"), reply.c_str());
        });
    }

    // RCON
    static void Rcon_BanEOS(RCONClientConnection* conn, RCONPacket* packet, UWorld*) {
        FString cmd = packet->Body;
        auto args = SplitArgs(cmd);
        HandleBan(args, "RCON", [&](const std::string& reply) {
            FString f = ToFString(reply);
            conn->SendMessageW(packet->Id, 0, &f);
        });
    }

    static void Rcon_UnbanEOS(RCONClientConnection* conn, RCONPacket* packet, UWorld*) {
        FString cmd = packet->Body;
        auto args = SplitArgs(cmd);
        HandleUnban(args, "RCON", [&](const std::string& reply) {
            FString f = ToFString(reply);
            conn->SendMessageW(packet->Id, 0, &f);
        });
    }

    static void Rcon_ListBans(RCONClientConnection* conn, RCONPacket* packet, UWorld*) {
        HandleList([&](const std::string& reply) {
            FString f = ToFString(reply);
            conn->SendMessageW(packet->Id, 0, &f);
        });
    }

    // -------- Enregistrement / désenregistrement --------

    void RegisterCommands() {
        auto& cmds = AsaApi::GetCommands();
        cmds.AddConsoleCommand("BanEOS",     &Console_BanEOS);
        cmds.AddConsoleCommand("UnbanEOS",   &Console_UnbanEOS);
        cmds.AddConsoleCommand("ListBans",   &Console_ListBans);

        cmds.AddChatCommand("/baneos",   &Chat_BanEOS);
        cmds.AddChatCommand("/unbaneos", &Chat_UnbanEOS);
        cmds.AddChatCommand("/listbans", &Chat_ListBans);

        cmds.AddRconCommand("BanEOS",    &Rcon_BanEOS);
        cmds.AddRconCommand("UnbanEOS",  &Rcon_UnbanEOS);
        cmds.AddRconCommand("ListBans",  &Rcon_ListBans);
    }

    void UnregisterCommands() {
        auto& cmds = AsaApi::GetCommands();
        cmds.RemoveConsoleCommand("BanEOS");
        cmds.RemoveConsoleCommand("UnbanEOS");
        cmds.RemoveConsoleCommand("ListBans");

        cmds.RemoveChatCommand("/baneos");
        cmds.RemoveChatCommand("/unbaneos");
        cmds.RemoveChatCommand("/listbans");

        cmds.RemoveRconCommand("BanEOS");
        cmds.RemoveRconCommand("UnbanEOS");
        cmds.RemoveRconCommand("ListBans");
    }

} // namespace EOSBanManager
