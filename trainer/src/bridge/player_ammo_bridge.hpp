#pragma once

#include <cstdint>
#include <string>

#include "bridge/player_resource_bridge.hpp"

namespace pztrainer::bridge {

struct PlayerAmmoStatus {
    PlayerResourceSessionMode session_mode = PlayerResourceSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool enabled = false;
    bool firearm_ready = false;
    int current_ammo = 0;
    int maximum_ammo = 0;
    int refill_ammo = 0;
    std::uint64_t refill_count = 0;
    std::uint64_t server_sync_count = 0;
    std::string message = "等待玩家对象";
};

void UpdatePlayerAmmoBridge();
const PlayerAmmoStatus& GetPlayerAmmoStatus();
void SetInfiniteAmmoEnabled(bool enabled);

}  // namespace pztrainer::bridge
