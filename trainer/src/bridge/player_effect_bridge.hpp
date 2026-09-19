#pragma once

#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class PlayerEffectKind {
    TimedMedication,
    CharacterStat,
};

enum class PlayerEffectSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

struct PlayerEffectEntry {
    PlayerEffectKind kind = PlayerEffectKind::CharacterStat;
    std::string id;
    std::string display_name;
    std::string source_item_id;
    float value = 0.0f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float default_value = 0.0f;
    float duration_seconds = 0.0f;
    float strength = 0.0f;
    bool active = false;
    bool editable = false;
    unsigned int texture_id = 0;
    float uv_min_x = 0.0f;
    float uv_min_y = 0.0f;
    float uv_max_x = 1.0f;
    float uv_max_y = 1.0f;
    int texture_width = 0;
    int texture_height = 0;
};

struct PlayerEffectStatus {
    PlayerEffectSessionMode session_mode = PlayerEffectSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool server_stat_sync_available = false;
    std::string message = "等待玩家对象";
    std::vector<PlayerEffectEntry> entries;
};

void UpdatePlayerEffectBridge();
const PlayerEffectStatus& GetPlayerEffectStatus();
bool SetTimedPlayerEffect(const std::string& id, float duration_seconds,
                          float strength);
bool SetPlayerCharacterStat(const std::string& id, float value);
bool ResetPlayerCharacterStat(const std::string& id);

}  // namespace pztrainer::bridge
