#pragma once

#include <string>

namespace pztrainer::bridge {

struct FreeBuildStatus {
    bool initialized = false;
    bool enabled = false;
    bool applied = false;
    bool operation_pending = false;
    std::string message = "免费建造桥接尚未初始化";
};

void UpdateFreeBuildBridge();
const FreeBuildStatus& GetFreeBuildStatus();
void SetFreeBuildEnabled(bool enabled);

}  // namespace pztrainer::bridge
