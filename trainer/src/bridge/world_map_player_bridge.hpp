#pragma once

#include <cstddef>
#include <string>

namespace pztrainer::bridge {

struct WorldMapPlayerStatus {
    bool initialized = false;
    bool enabled = false;
    bool applied = false;
    bool multiplayer = false;
    std::size_t synchronized_players = 0;
    std::string message = "地图玩家标记桥接尚未初始化";
};

void UpdateWorldMapPlayerBridge();
const WorldMapPlayerStatus& GetWorldMapPlayerStatus();
void SetWorldMapPlayerEnabled(bool enabled);

}  // namespace pztrainer::bridge
