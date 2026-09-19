#pragma once

#include "bridge/game_snapshot.hpp"
#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

void DrawZombieEsp(const bridge::FrameSnapshot& frame, const VisualSettings& settings);

}  // namespace pztrainer::features::visual
