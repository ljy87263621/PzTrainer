#pragma once

#include <string>

namespace pztrainer::bridge {

struct TimedActionStatus {
    bool initialized = false;
    bool player_ready = false;
    bool multiplayer = false;
    bool available = false;
    bool enabled = false;
    bool applied = false;
    std::string message = "定时动作桥接尚未初始化";
};

void UpdateTimedActionBridge();
const TimedActionStatus& GetTimedActionStatus();
void SetTimedActionInstantEnabled(bool enabled);

}  // namespace pztrainer::bridge
