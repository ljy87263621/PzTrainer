#pragma once

#include "bridge/game_snapshot.hpp"
#include "features/visual/vehicle_visual_settings.hpp"

namespace pztrainer::features::visual {

void DrawVehicleEsp(const bridge::FrameSnapshot& frame,
                    const VehicleVisualSettings& settings);

}  // namespace pztrainer::features::visual
