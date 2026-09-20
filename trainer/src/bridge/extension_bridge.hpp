#pragma once

#include <string>
#include <vector>

namespace pztrainer::bridge {

struct ExtensionOptions {
    int flags = 0;
    int kill_range = 10;
};

struct CreationOption {
    std::string kind;
    std::string key;
    std::string label;
};

ExtensionOptions& GetExtensionOptions();
const std::string& GetExtensionStatus();
const std::vector<CreationOption>& GetCreationOptions();
bool ExtensionBridgeReady();
std::string ExtensionDisabledReason(const std::string& key);
void UpdateExtensionBridge(bool controls_blocked);
bool QueueExtensionCommand(const char* action, const std::string& payload = {}, int radius = 10);

}  // namespace pztrainer::bridge
