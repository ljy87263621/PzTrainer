#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

struct PlayerTeleportStatus {
    bool initialized = false;
    bool enabled = false;
    bool player_ready = false;
    bool map_open = false;
    bool multiplayer = false;
    float last_world_x = 0.0f;
    float last_world_y = 0.0f;
    float last_world_z = 0.0f;
    std::uint64_t teleport_count = 0;
    bool cooldown_active = false;
    std::uint32_t cooldown_remaining_seconds = 0;
    std::string message = "地图传送桥接尚未初始化";
};

void UpdatePlayerTeleportBridge(bool input_blocked);
const PlayerTeleportStatus& GetPlayerTeleportStatus();
void SetPlayerTeleportEnabled(bool enabled);

}  // namespace pztrainer::bridge
