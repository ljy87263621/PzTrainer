#pragma once

#include <string>

namespace pztrainer::bridge {

struct PlayerCarryStatus {
    bool initialized = false;
    bool player_ready = false;
    bool multiplayer = false;
    bool enabled = false;
    bool available = false;
    float multiplier = 1.85f;
    std::string message = "无限负重桥接尚未初始化";
};

void UpdatePlayerCarryBridge();
const PlayerCarryStatus& GetPlayerCarryStatus();
void SetUnlimitedCarryEnabled(bool enabled);
void SetCarryWeightMultiplier(float multiplier);

}  // namespace pztrainer::bridge
