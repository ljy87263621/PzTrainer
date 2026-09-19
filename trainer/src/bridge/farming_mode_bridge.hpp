#pragma once

#include <string>

namespace pztrainer::bridge {

struct FarmingModeStatus {
    bool initialized = false;
    bool enabled = false;
    bool applied = false;
    std::string message = "作物耕种桥接尚未初始化";
};

void UpdateFarmingModeBridge();
const FarmingModeStatus& GetFarmingModeStatus();
void SetFarmingModeEnabled(bool enabled);

}  // namespace pztrainer::bridge
