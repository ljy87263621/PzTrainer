#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

struct PlayerAimingDelayStatus {
    bool enabled = false;
    std::uint64_t cleared_count = 0;
    std::string message = "快速结束瞄准延迟已关闭";
};

void SetFastAimingDelayEnabled(bool enabled);
void UpdateFastAimingDelay(bool world_ready);
const PlayerAimingDelayStatus& GetPlayerAimingDelayStatus();

}  // namespace pztrainer::bridge
