#pragma once

#include <imgui.h>

namespace pztrainer::features::visual {
struct VisualSettings;
}

namespace pztrainer::ui {

void DrawZombiePreview(const ImVec2& size, const features::visual::VisualSettings& settings);

}  // namespace pztrainer::ui
