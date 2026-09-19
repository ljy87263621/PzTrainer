#pragma once

#include <string>

namespace pztrainer::bridge {

struct MultiHitStatus {
    bool initialized = false;
    bool player_ready = false;
    bool enabled = false;
    bool applied = false;
    bool original_value = false;
    std::string message = "多目标攻击桥接尚未初始化";
};

void UpdateMultiHitBridge();
const MultiHitStatus& GetMultiHitStatus();
void SetMultiHitEnabled(bool enabled);

}  // namespace pztrainer::bridge
