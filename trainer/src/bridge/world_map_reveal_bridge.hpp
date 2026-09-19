#pragma once

#include <string>

namespace pztrainer::bridge {

struct WorldMapRevealStatus {
    bool initialized = false;
    bool enabled = false;
    bool map_open = false;
    bool applied = false;
    std::string message = "世界地图迷雾桥接尚未初始化";
};

void UpdateWorldMapRevealBridge();
const WorldMapRevealStatus& GetWorldMapRevealStatus();
void SetWorldMapRevealEnabled(bool enabled);

}  // namespace pztrainer::bridge
