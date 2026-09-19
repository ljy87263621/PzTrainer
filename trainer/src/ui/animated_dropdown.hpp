#pragma once

#include <imgui.h>

namespace pztrainer::ui {

bool BeginAnimatedCombo(const char* id, const char* preview,
                        float popup_height, float popup_width = 0.0f);

ImGuiID AnimatedDropdownId(const char* id);
void ToggleAnimatedDropdown(ImGuiID id);
bool IsAnimatedDropdownOpen(ImGuiID id);
float AnimatedDropdownAmount(ImGuiID id);
bool BeginAnimatedDropdownPopup(ImGuiID id, const ImVec2& anchor_minimum,
                                const ImVec2& anchor_maximum,
                                const ImVec2& popup_size,
                                bool control_hovered = false);
void CloseAnimatedDropdown();
void EndAnimatedDropdown();

}  // namespace pztrainer::ui
