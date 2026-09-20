#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

enum class PlayerHealthSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

struct PlayerHealthStatus {
    PlayerHealthSessionMode session_mode = PlayerHealthSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool infinite_health_enabled = false;
    bool invincibility_enabled = false;
    bool invincibility_available = false;
    std::string invincibility_message = "等待无敌模式接口初始化";
    int last_restored_parts = 0;
    std::uint64_t restore_count = 0;
    std::uint64_t server_sync_count = 0;
    std::string message = "等待玩家对象";
};

void UpdatePlayerHealthBridge();
const PlayerHealthStatus& GetPlayerHealthStatus();
void SetInfiniteHealthEnabled(bool enabled);
void SetInvincibilityEnabled(bool enabled);

}  // namespace pztrainer::bridge
