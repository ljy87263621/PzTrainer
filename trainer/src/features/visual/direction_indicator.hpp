#pragma once

#include <imgui.h>

namespace pztrainer::features::visual {

void DrawDirectionIndicator(ImDrawList* draw, const ImVec2& origin,
                            const ImVec2& target, const ImVec4& color,
                            const char* trailing_label = nullptr);

void DrawNearbyDirectionIndicator(ImDrawList* draw, const ImVec2& origin,
                                  const ImVec2& target, const ImVec4& color,
                                  const char* trailing_label = nullptr);

}  // namespace pztrainer::features::visual
