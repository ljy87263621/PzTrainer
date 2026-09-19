#pragma once

#include <imgui.h>

#include "features/visual/player_visual_settings.hpp"

namespace pztrainer::ui {

void DrawPlayerPreview(
    const ImVec2& size,
    const features::visual::PlayerVisualSettings& settings);

}  // namespace pztrainer::ui
