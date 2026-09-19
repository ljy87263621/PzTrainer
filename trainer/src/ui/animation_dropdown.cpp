#include "ui/animation_dropdown.hpp"

#include <imgui.h>

#include <algorithm>

#include "ui/animation.hpp"
#include "ui/components.hpp"
#include "ui/glass_blur.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animated_dropdown.hpp"

namespace pztrainer::ui {
namespace {

constexpr const char* kAnimationLabels[]{"待机", "行走", "奔跑", "扑咬"};
constexpr const char* kPlayerAnimationLabels[]{"待机", "行走", "奔跑", "冲刺"};
constexpr const char* kAnimalAnimationLabels[]{"待机", "行走", "奔跑", "进食"};

float U(float value) {
    return value * settings::UiScale();
}

void DrawChevron(ImDrawList* draw, const ImVec2& center, ImU32 color, bool open) {
    const float direction = open ? -1.0f : 1.0f;
    draw->AddLine(ImVec2(center.x - U(5.0f), center.y - U(2.5f) * direction),
                  ImVec2(center.x, center.y + U(2.5f) * direction), color, U(1.7f));
    draw->AddLine(ImVec2(center.x, center.y + U(2.5f) * direction),
                  ImVec2(center.x + U(5.0f), center.y - U(2.5f) * direction), color, U(1.7f));
}

}  // namespace

bool DrawAnimationDropdown(int* selected_animation, bool player_preview,
                           bool animal_preview) {
    const char* const* labels = animal_preview
        ? kAnimalAnimationLabels
        : player_preview ? kPlayerAnimationLabels : kAnimationLabels;
    ImGui::PushID("PreviewAnimationDropdown");
    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    const float row_width = ImGui::GetContentRegionAvail().x;
    const int safe_selection = std::clamp(*selected_animation, 0, 3);
    const char* row_label = settings::Translate("预览动画");
    const char* selected_label = settings::Translate(labels[safe_selection]);
    const float label_width = ImGui::CalcTextSize(row_label).x;
    const float preferred_control_width = std::max(
        U(108.0f), ImGui::CalcTextSize(selected_label).x + U(44.0f));
    const float inline_control_width =
        row_width - label_width - U(10.0f);
    const bool stacked = inline_control_width < U(88.0f);
    const float control_width = stacked
        ? row_width
        : std::min(preferred_control_width, inline_control_width);
    const ImVec2 control_min(
        stacked ? row_start.x : row_start.x + row_width - control_width,
        stacked ? row_start.y + U(26.0f) : row_start.y);
    const ImVec2 resolved_control_max(
        control_min.x + control_width, control_min.y + U(42.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();

    if (!stacked) {
        draw->PushClipRect(
            row_start,
            ImVec2(control_min.x - U(6.0f), row_start.y + U(42.0f)), true);
    }
    draw->AddText(
        ImVec2(row_start.x, row_start.y + (stacked ? 0.0f : U(11.0f))),
        ImGui::GetColorU32(components::kText), row_label);
    if (!stacked) draw->PopClipRect();
    ImGui::SetCursorScreenPos(control_min);
    ImGui::InvisibleButton("AnimationSelector", ImVec2(control_width, U(42.0f)));
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiID dropdown_id = AnimatedDropdownId("AnimationPopup");
    if (ImGui::IsItemClicked()) ToggleAnimatedDropdown(dropdown_id);
    const bool open = IsAnimatedDropdownOpen(dropdown_id);
    const float hover = animation::Clamp01(animation::Spring(
        ImGui::GetID("AnimationSelectorHover"), hovered || open ? 1.0f : 0.0f, 250.0f, 22.0f));

    draw->AddRectFilled(control_min, resolved_control_max,
                        ImGui::GetColorU32(ImVec4(0.055f + hover * 0.018f,
                                                  0.058f + hover * 0.020f,
                                                  0.075f + hover * 0.030f, 1.0f)), U(8.0f));
    draw->AddRect(control_min, resolved_control_max,
                  ImGui::GetColorU32(ImVec4(0.18f, 0.20f, 0.27f, 0.55f + hover * 0.30f)),
                  U(8.0f), 0, U(1.0f));
    draw->AddText(ImVec2(control_min.x + U(13.0f), control_min.y + U(11.0f)),
                  ImGui::GetColorU32(components::kText),
                  selected_label);
    DrawChevron(draw, ImVec2(resolved_control_max.x - U(17.0f), control_min.y + U(21.0f)),
                 ImGui::GetColorU32(components::kMuted), open);

    bool changed = false;
    if (BeginAnimatedDropdownPopup(
            dropdown_id, control_min, resolved_control_max,
            ImVec2(control_width, U(166.0f)), hovered)) {
        const ImVec2 popup_minimum = ImGui::GetWindowPos();
        const ImVec2 popup_size = ImGui::GetWindowSize();
        const ImVec2 popup_maximum(
            popup_minimum.x + popup_size.x, popup_minimum.y + popup_size.y);
        DrawGlassPanel(ImGui::GetWindowDrawList(), popup_minimum, popup_maximum, 12.0f);
        for (int index = 0; index < 4; ++index) {
            ImGui::PushID(index);
            const ImVec2 item_start = ImGui::GetCursorScreenPos();
            const float item_width = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("AnimationItem", ImVec2(item_width, U(38.0f)));
            const bool item_hovered = ImGui::IsItemHovered();
            if (item_hovered) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    item_start, ImVec2(item_start.x + item_width, item_start.y + U(38.0f)),
                    ImGui::GetColorU32(ImVec4(0.13f, 0.36f, 0.95f, 0.14f)), 7.0f);
            }
            if (index == safe_selection) {
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(item_start.x + 8.0f, item_start.y + 20.0f),
                    ImVec2(item_start.x + 13.0f, item_start.y + 25.0f),
                    ImGui::GetColorU32(ImVec4(0.86f, 0.90f, 1.0f, 1.0f)), 2.2f);
                ImGui::GetWindowDrawList()->AddLine(
                    ImVec2(item_start.x + 13.0f, item_start.y + 25.0f),
                    ImVec2(item_start.x + 22.0f, item_start.y + 14.0f),
                    ImGui::GetColorU32(ImVec4(0.86f, 0.90f, 1.0f, 1.0f)), 2.2f);
            }
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(item_start.x + U(31.0f), item_start.y + U(10.0f)),
                ImGui::GetColorU32(index == safe_selection ? components::kText : components::kMuted),
                settings::Translate(labels[index]));
            if (ImGui::IsItemClicked()) {
                *selected_animation = index;
                changed = true;
                CloseAnimatedDropdown();
            }
            ImGui::PopID();
        }
        EndAnimatedDropdown();
    }
    ImGui::SetCursorScreenPos(ImVec2(
        row_start.x, row_start.y + U(stacked ? 74.0f : 48.0f)));
    ImGui::Dummy(ImVec2(row_width, 1.0f));
    ImGui::PopID();
    return changed;
}

}  // namespace pztrainer::ui
