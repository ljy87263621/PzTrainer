#pragma once

#include <imgui.h>

#include "features/visual/animal_visual_settings.hpp"

namespace pztrainer::ui {

void DrawAnimalPreview(const ImVec2& size,
                       const features::visual::AnimalVisualSettings& settings);

}  // namespace pztrainer::ui
