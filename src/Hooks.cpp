// Hooks pour kick automatiquement les EOSID bannis à la connexion.
//
// Stratégie:
//   - On accroche AShooterGameMode::PostLogin (signature standard UE).
//     C'est appelé quand un PlayerController est prêt après login.
//   - On extrait l'EOSID via AsaApi.
//   - Si banni, on appelle KickPlayerController avec le message configuré.
//
// Note: on a essayé HandleNewPlayer_Implementation mais AsaApi v1.21
// ne trouve pas son offset binaire (probablement inlinée). PostLogin
// est plus stable et tout aussi efficace pour le ban-on-join.

#include "Plugin.h"
#include "Logger.h"

#include "API/ARK/Ark.h"

namespace EOSBanManager {

    // Signature: void AShooterGameMode::PostLogin(APlayerController* NewPlayer)
    DECLARE_HOOK(AShooterGameMode_PostLogin,
                 void,
                 AShooterGameMode*,
                 APlayerController*);

    void Hook_AShooterGameMode_PostLogin(AShooterGameMode* gm,
                                         APlayerController* pc_base) {
        // On laisse Ark exécuter sa logique de login en premier
        AShooterGameMode_PostLogin_original(gm, pc_base);

        auto* pc = static_cast<AShooterPlayerController*>(pc_base);
        if (!pc) return;

        const std::string eos = Plugin::GetEOSID(pc);
        if (eos.empty()) return;

        if (Plugin::Get().Db().IsBanned(eos)) {
            BanEntry entry;
            Plugin::Get().Db().GetBan(eos, entry);

            std::string msg = Plugin::Get().Cfg().kick_message;
            if (!entry.reason.empty()) {
                msg += " (" + entry.reason + ")";
            }
            FString fmsg = FString(msg.c_str());

            EOSLog::Info("Kick au login: EOS=%s nom=%s raison=%s",
                         eos.c_str(),
                         Plugin::GetPlayerName(pc).c_str(),
                         entry.reason.c_str());

            // v1.21: KickPlayerController prend (APlayerController*, const FString&)
            gm->KickPlayerController(pc, fmsg);
        }
    }

    // AsaApi v1.21 utilise le nom de fonction COMPLET avec la signature
    // des paramètres pour identifier l'offset binaire.
    static constexpr const char* kHookName =
        "AShooterGameMode.PostLogin(APlayerController*)";

    void RegisterHooks() {
        AsaApi::GetHooks().SetHook(kHookName,
                                   &Hook_AShooterGameMode_PostLogin,
                                   &AShooterGameMode_PostLogin_original);
    }

    void UnregisterHooks() {
        AsaApi::GetHooks().DisableHook(kHookName,
                                       &Hook_AShooterGameMode_PostLogin);
    }

} // namespace EOSBanManager
