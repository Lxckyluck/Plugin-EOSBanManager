// Hooks pour kick automatiquement les EOSID bannis à la connexion.
//
// Stratégie:
//   - On accroche AShooterGameMode::HandleNewPlayer_Implementation
//     C'est appelé une fois que le PlayerController est prêt côté serveur
//     mais avant que le joueur ait spawn — c'est le bon endroit pour kick.
//   - On extrait l'EOSID via AsaApi (GetEOSIdFromController).
//   - Si banni, on appelle KickPlayerController avec le message configuré.

#include "Plugin.h"
#include "Logger.h"

#include "API/ARK/Ark.h"
#include "API/Hooks.h"

namespace EOSBanManager {

    // Signature originale telle qu'exposée par AsaApi
    DECLARE_HOOK(AShooterGameMode_HandleNewPlayer,
                 bool,
                 AShooterGameMode*,
                 AShooterPlayerController*,
                 UPrimalPlayerData*,
                 AShooterCharacter*,
                 bool);

    bool Hook_AShooterGameMode_HandleNewPlayer(AShooterGameMode* gm,
                                               AShooterPlayerController* pc,
                                               UPrimalPlayerData* data,
                                               AShooterCharacter* character,
                                               bool isFromLogin) {
        // On laisse Ark exécuter sa logique en premier
        const bool result = AShooterGameMode_HandleNewPlayer_original(
            gm, pc, data, character, isFromLogin);

        if (pc) {
            const std::string eos = Plugin::GetEOSID(pc);
            if (!eos.empty() && Plugin::Get().Db().IsBanned(eos)) {
                BanEntry entry;
                Plugin::Get().Db().GetBan(eos, entry);

                std::string msg = Plugin::Get().Cfg().kick_message;
                if (!entry.reason.empty()) {
                    msg += " (" + entry.reason + ")";
                }
                FString fmsg = FString(msg.c_str());

                Log::Info("Kick au login: EOS=%s nom=%s raison=%s",
                          eos.c_str(),
                          Plugin::GetPlayerName(pc).c_str(),
                          entry.reason.c_str());

                pc->ClientNotifyKicked(&fmsg, true, false);
                gm->KickPlayerController(pc, &fmsg);
            }
        }
        return result;
    }

    void RegisterHooks() {
        AsaApi::GetHooks().SetHook("AShooterGameMode.HandleNewPlayer_Implementation",
                                   &Hook_AShooterGameMode_HandleNewPlayer,
                                   &AShooterGameMode_HandleNewPlayer_original);
    }

    void UnregisterHooks() {
        AsaApi::GetHooks().DisableHook("AShooterGameMode.HandleNewPlayer_Implementation",
                                       &Hook_AShooterGameMode_HandleNewPlayer);
    }

} // namespace EOSBanManager
