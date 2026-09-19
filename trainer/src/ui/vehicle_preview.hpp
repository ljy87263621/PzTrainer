#pragma once

#include <imgui.h>

#include "features/visual/vehicle_visual_settings.hpp"

namespace pztrainer::ui {

void DrawVehiclePreview(const ImVec2& size,
                        const features::visual::VehicleVisualSettings& settings);

}  // namespace pztrainer::ui
