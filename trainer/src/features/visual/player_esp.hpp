#pragma once

#include "bridge/game_snapshot.hpp"
#include "features/visual/player_visual_settings.hpp"

namespace pztrainer::features::visual {

void DrawPlayerEsp(const bridge::FrameSnapshot& frame,
                   const PlayerVisualSettings& settings);

}  // namespace pztrainer::features::visual
