#pragma once

#include <imgui.h>

namespace pztrainer::ui::animation {

float DeltaTime();
float Spring(ImGuiID id, float target, float stiffness = 230.0f, float damping = 20.0f);
void SpringValue(float& value, float& velocity, float target, float stiffness, float damping);
float Clamp01(float value);
float Lerp(float from, float to, float amount);
ImVec4 Lerp(const ImVec4& from, const ImVec4& to, float amount);

}  // namespace pztrainer::ui::animation
