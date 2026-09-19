#pragma once

#include <imgui.h>

namespace pztrainer::ui {

bool InitializeGlassBlur();
void PrepareGlassBlur();
void DrawGlassPanel(ImDrawList* draw_list, const ImVec2& minimum,
                    const ImVec2& maximum, float rounding);
const char* GlassBlurError();

}  // namespace pztrainer::ui
