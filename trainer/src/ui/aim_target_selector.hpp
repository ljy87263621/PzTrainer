#pragma once

#include <cstdint>

#include <imgui.h>

namespace pztrainer::ui {

void DrawAimTargetSelector(const ImVec2& size, std::uint32_t* target_points);

}  // namespace pztrainer::ui
