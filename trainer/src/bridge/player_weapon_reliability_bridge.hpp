#pragma once

#include <cstdint>
#include <string>

#include "bridge/player_resource_bridge.hpp"

namespace pztrainer::bridge {

struct PlayerWeaponReliabilityStatus {
    PlayerResourceSessionMode session_mode = PlayerResourceSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool enabled = false;
    bool firearm_ready = false;
    bool jammed = false;
    float original_jam_chance = 0.0f;
    std::uint64_t unjam_count = 0;
    std::uint64_t server_sync_count = 0;
    std::string message = "等待玩家对象";
};

void UpdatePlayerWeaponReliabilityBridge();
const PlayerWeaponReliabilityStatus& GetPlayerWeaponReliabilityStatus();
void SetNoWeaponJamEnabled(bool enabled);

}  // namespace pztrainer::bridge
