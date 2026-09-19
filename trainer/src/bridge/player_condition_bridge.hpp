#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

struct PlayerConditionStatus {
    bool initialized = false;
    bool player_ready = false;
    bool multiplayer = false;
    bool server_stat_sync_available = false;
    bool fatigue_available = false;
    bool panic_available = false;
    bool hunger_available = false;
    bool thirst_available = false;
    bool negative_moodles_available = false;
    bool infection_immunity_available = false;
    bool fatigue_enabled = false;
    bool panic_enabled = false;
    bool hunger_enabled = false;
    bool thirst_enabled = false;
    bool negative_moodles_enabled = false;
    bool infection_immunity_enabled = false;
    std::uint64_t stat_restore_count = 0;
    std::uint64_t body_restore_count = 0;
    std::uint64_t server_sync_count = 0;
    std::string message = "玩家状态保护桥接尚未初始化";
};

void UpdatePlayerConditionBridge();
const PlayerConditionStatus& GetPlayerConditionStatus();
void SetFatigueProtectionEnabled(bool enabled);
void SetPanicProtectionEnabled(bool enabled);
void SetHungerProtectionEnabled(bool enabled);
void SetThirstProtectionEnabled(bool enabled);
void SetNegativeMoodlesProtectionEnabled(bool enabled);
void SetInfectionImmunityEnabled(bool enabled);
bool IsFatigueProtectionRequested();
bool IsPanicProtectionRequested();
bool IsHungerProtectionRequested();
bool IsThirstProtectionRequested();
bool IsNegativeMoodlesProtectionRequested();
bool IsInfectionImmunityRequested();

}  // namespace pztrainer::bridge
