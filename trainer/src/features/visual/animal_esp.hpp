#pragma once

#include "bridge/game_snapshot.hpp"
#include "features/visual/animal_visual_settings.hpp"

namespace pztrainer::features::visual {

void DrawAnimalEsp(const bridge::FrameSnapshot& frame,
                   const AnimalVisualSettings& settings);

}  // namespace pztrainer::features::visual
