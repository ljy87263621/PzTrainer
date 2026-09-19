#pragma once

#include <imgui.h>

#include "features/visual/visual_settings.hpp"

namespace pztrainer::ui::components {

inline constexpr ImVec4 kAccent{0.18f, 0.43f, 0.97f, 1.0f};
inline constexpr ImVec4 kText{0.88f, 0.89f, 0.93f, 1.0f};
inline constexpr ImVec4 kMuted{0.46f, 0.48f, 0.55f, 1.0f};

void DrawBrandBadge(const char* letters);
void DrawUserAvatar();
void SectionLabel(const char* label);
bool TopTab(const char* label, bool selected, float width);
bool MoreOptionsButton(const char* id, const ImVec2& size = ImVec2(30.0f, 30.0f));
void BeginCard(const char* id, const char* heading, const ImVec2& size);
void BeginCompactCard(const char* id, const char* heading, const ImVec2& size);
void EndCard();
bool ToggleRow(const char* label, bool* value, features::visual::StateColor* colors = nullptr,
               bool show_divider = true);
bool ToggleRow(const char* label, bool* value, ImVec4* color, bool show_divider = true);
bool CompactToggleRow(const char* label, bool* value, bool show_divider = true);
bool CompactToggleRow(const char* label, bool* value,
                      features::visual::StateColor* colors,
                      bool show_divider = true);
bool CompactToggleRow(const char* label, bool* value, ImVec4* color,
                      bool show_divider = true);
void DisabledToggleRow(const char* label, bool value, const char* tooltip,
                       bool show_divider = true);
void CompactDisabledToggleRow(const char* label, bool value, const char* tooltip,
                              bool show_divider = true);
void RoundedTooltip(const char* text);
bool StepperRow(const char* label, float* value, float minimum, float maximum, float step,
                const char* format, bool show_divider = true,
                bool* interaction_active = nullptr);

}  // namespace pztrainer::ui::components
