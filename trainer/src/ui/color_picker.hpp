#pragma once

#include <imgui.h>

#include "features/visual/visual_settings.hpp"

namespace pztrainer::ui {

bool DrawColorSwatch(const char* id, ImVec4* color, const ImVec2& size);
bool DrawColorSwatch(const char* id, features::visual::StateColor* colors, const ImVec2& size);
void DrawColorPickerOverlay(bool host_visible, const ImVec2& host_minimum, const ImVec2& host_maximum);

}  // namespace pztrainer::ui
