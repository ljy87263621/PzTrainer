#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

enum class PlayerResourceSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

struct PlayerResourceStatus {
    PlayerResourceSessionMode session_mode = PlayerResourceSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool endurance_recovery_enabled = false;
    bool durability_protection_enabled = false;
    float endurance = 0.0f;
    int primary_condition = 0;
    int primary_condition_max = 0;
    int secondary_condition = 0;
    int secondary_condition_max = 0;
    std::uint64_t endurance_restore_count = 0;
    std::uint64_t durability_restore_count = 0;
    std::uint64_t server_sync_count = 0;
    std::string message = "等待玩家对象";
};

void UpdatePlayerResourceBridge();
const PlayerResourceStatus& GetPlayerResourceStatus();
void SetEnduranceRecoveryEnabled(bool enabled);
void SetDurabilityProtectionEnabled(bool enabled);

}  // namespace pztrainer::bridge
