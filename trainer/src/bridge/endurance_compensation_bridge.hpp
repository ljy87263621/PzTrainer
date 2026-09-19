#pragma once

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

struct EnduranceCompensationStatus {
    bool initialized = false;
    bool enabled = false;
    bool multiplayer = false;
    bool armed = false;
    std::uint64_t compensated_updates = 0;
    std::string message = "联机休息补偿待命";
};

void UpdateEnduranceCompensationBridge(bool enabled);
const EnduranceCompensationStatus& GetEnduranceCompensationStatus();

}  // namespace pztrainer::bridge
