#pragma once

#include <string>

namespace pztrainer::bridge {

enum class PlayerMovementSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

struct PlayerMovementStatus {
    PlayerMovementSessionMode session_mode = PlayerMovementSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool no_clip_enabled = false;
    bool no_clip_available = false;
    bool network_state_ready = false;
    std::string message = "等待玩家对象";
};

void UpdatePlayerMovementBridge();
const PlayerMovementStatus& GetPlayerMovementStatus();
void SetNoClipEnabled(bool enabled);

}  // namespace pztrainer::bridge
