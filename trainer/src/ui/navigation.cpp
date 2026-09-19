#include "ui/navigation.hpp"

#include <algorithm>
#include <cmath>

#include "ui/animation.hpp"
#include "ui/components.hpp"
#include "ui/theme.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"

namespace pztrainer::ui::navigation {
namespace {

constexpr float kItemHeight = 36.0f;
constexpr ImVec4 kHover{0.20f, 0.31f, 0.43f, 0.28f};
constexpr ImVec4 kSelected{0.30f, 0.40f, 0.52f, 0.42f};
constexpr ImVec4 kSelectedBorder{0.48f, 0.67f, 0.88f, 0.20f};

ImU32 Color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

float U(float value) {
    return value * settings::UiScale();
}

void DrawIcon(ImDrawList* draw, Icon icon, const ImVec2& center, ImU32 color) {
    const float thickness = U(1.5f);
    switch (icon) {
        case Icon::Crosshair:
            draw->AddCircle(center, U(5.2f), color, 0, thickness);
            draw->AddCircleFilled(center, U(1.8f), color);
            draw->AddLine(ImVec2(center.x - U(8.0f), center.y),
                          ImVec2(center.x - U(3.5f), center.y), color, thickness);
            draw->AddLine(ImVec2(center.x + U(3.5f), center.y),
                          ImVec2(center.x + U(8.0f), center.y), color, thickness);
            draw->AddLine(ImVec2(center.x, center.y - U(8.0f)),
                          ImVec2(center.x, center.y - U(3.5f)), color, thickness);
            draw->AddLine(ImVec2(center.x, center.y + U(3.5f)),
                          ImVec2(center.x, center.y + U(8.0f)), color, thickness);
            break;
        case Icon::Mouse:
            draw->AddRect(ImVec2(center.x - U(5.0f), center.y - U(7.0f)),
                          ImVec2(center.x + U(5.0f), center.y + U(7.0f)), color,
                          U(5.0f), 0, thickness);
            draw->AddLine(ImVec2(center.x, center.y - U(6.5f)),
                          ImVec2(center.x, center.y - U(1.5f)), color, thickness);
            draw->AddLine(ImVec2(center.x - U(4.5f), center.y - U(0.5f)),
                          ImVec2(center.x + U(4.5f), center.y - U(0.5f)), color,
                          thickness);
            break;
        case Icon::Eye:
            draw->AddBezierCubic(
                ImVec2(center.x - U(7.0f), center.y),
                ImVec2(center.x - U(3.5f), center.y - U(5.0f)),
                ImVec2(center.x + U(3.5f), center.y - U(5.0f)),
                ImVec2(center.x + U(7.0f), center.y), color, thickness);
            draw->AddBezierCubic(
                ImVec2(center.x - U(7.0f), center.y),
                ImVec2(center.x - U(3.5f), center.y + U(5.0f)),
                ImVec2(center.x + U(3.5f), center.y + U(5.0f)),
                ImVec2(center.x + U(7.0f), center.y), color, thickness);
            draw->AddCircleFilled(center, U(2.5f), color);
            break;
        case Icon::World:
            draw->AddCircle(center, U(6.7f), color, 0, thickness);
            draw->AddEllipse(center, ImVec2(U(3.2f), U(6.7f)), color, 0.0f, 0, thickness);
            draw->AddLine(ImVec2(center.x - U(6.0f), center.y),
                          ImVec2(center.x + U(6.0f), center.y), color, thickness);
            draw->AddLine(ImVec2(center.x - U(5.0f), center.y - U(3.4f)),
                          ImVec2(center.x + U(5.0f), center.y - U(3.4f)), color, thickness);
            draw->AddLine(ImVec2(center.x - U(5.0f), center.y + U(3.4f)),
                          ImVec2(center.x + U(5.0f), center.y + U(3.4f)), color, thickness);
            break;
        case Icon::User:
            draw->AddCircle(
                ImVec2(center.x, center.y - U(4.0f)), U(3.2f),
                color, 0, thickness);
            draw->AddBezierCubic(
                ImVec2(center.x - U(6.0f), center.y + U(7.0f)),
                ImVec2(center.x - U(5.0f), center.y + U(1.0f)),
                ImVec2(center.x + U(5.0f), center.y + U(1.0f)),
                ImVec2(center.x + U(6.0f), center.y + U(7.0f)), color, thickness);
            break;
        case Icon::Package:
            draw->AddRect(ImVec2(center.x - U(6.0f), center.y - U(5.0f)),
                          ImVec2(center.x + U(6.0f), center.y + U(6.0f)),
                          color, U(1.5f), 0, thickness);
            draw->AddLine(ImVec2(center.x - U(6.0f), center.y - U(1.5f)),
                          ImVec2(center.x + U(6.0f), center.y - U(1.5f)), color, thickness);
            draw->AddLine(ImVec2(center.x, center.y - U(5.0f)),
                          ImVec2(center.x, center.y - U(1.5f)), color, thickness);
            break;
        case Icon::Spark:
            draw->AddLine(ImVec2(center.x, center.y - U(7.0f)),
                          ImVec2(center.x, center.y + U(7.0f)), color, thickness);
            draw->AddLine(ImVec2(center.x - U(5.0f), center.y),
                          ImVec2(center.x + U(5.0f), center.y), color, thickness);
            draw->AddLine(ImVec2(center.x - U(3.5f), center.y - U(4.5f)),
                          ImVec2(center.x + U(3.5f), center.y + U(4.5f)), color, thickness);
            draw->AddLine(ImVec2(center.x + U(3.5f), center.y - U(4.5f)),
                          ImVec2(center.x - U(3.5f), center.y + U(4.5f)), color, thickness);
            break;
        case Icon::Lua: {
            const ImU32 white = Color(ImVec4(
                245.0f / 255.0f, 248.0f / 255.0f, 1.0f, 1.0f));
            draw->AddCircle(center, U(6.6f), white, 0, thickness);
            draw->AddCircleFilled(
                ImVec2(center.x + U(3.3f), center.y - U(3.3f)),
                U(2.8f), Color(ImVec4(
                    18.0f / 255.0f, 22.0f / 255.0f,
                    31.0f / 255.0f, 1.0f)));
            draw->AddCircleFilled(
                ImVec2(center.x + U(6.0f), center.y - U(6.0f)),
                U(1.8f), white);
            break;
        }
        case Icon::Clock:
            draw->AddCircle(center, U(6.5f), color, 0, thickness);
            draw->AddLine(center, ImVec2(center.x, center.y - U(3.5f)), color, thickness);
            draw->AddLine(center, ImVec2(center.x + U(3.0f), center.y + U(1.5f)), color, thickness);
            break;
        case Icon::Settings:
            draw->AddCircle(center, U(3.2f), color, 0, thickness);
            draw->AddCircle(center, U(6.0f), color, 0, thickness);
            for (int index = 0; index < 8; ++index) {
                const float angle = static_cast<float>(index) * 3.14159265f * 0.25f;
                const ImVec2 inner(center.x + std::cos(angle) * U(6.0f),
                                   center.y + std::sin(angle) * U(6.0f));
                const ImVec2 outer(center.x + std::cos(angle) * U(7.8f),
                                   center.y + std::sin(angle) * U(7.8f));
                draw->AddLine(inner, outer, color, thickness);
            }
            break;
    }
}

}  // namespace

void BeginGroup(const char* id, int item_count) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(
        id,
        ImVec2(
            0.0f,
            U(kItemHeight) * static_cast<float>(std::max(item_count, 1))),
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
}

void EndGroup() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

bool ItemInternal(const char* label, Icon icon, bool selected,
                  IconDrawCallback draw_icon, const void* context) {
    ImGui::PushID(label);
    const ImGuiID hover_animation_id = ImGui::GetID("nav_hover");
    const ImGuiID selection_animation_id = ImGui::GetID("nav_selection");
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("nav_button", ImVec2(width, U(kItemHeight)));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const float hover_amount = animation::Clamp01(
        animation::Spring(hover_animation_id, hovered ? 1.0f : 0.0f, 260.0f, 24.0f));
    const float selected_amount = animation::Clamp01(
        animation::Spring(selection_animation_id, selected ? 1.0f : 0.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(start.x + width, start.y + U(kItemHeight));

    if (hover_amount > 0.001f) {
        ImVec4 hover_color = kHover;
        hover_color.w *= hover_amount;
        draw->AddRectFilled(start, end, Color(hover_color), U(7.0f));
    }
    if (selected_amount > 0.001f) {
        ImVec4 selected_fill = kSelected;
        selected_fill.w *= selected_amount;
        draw->AddRectFilled(
            start, end, Color(selected_fill), U(8.0f));
        ImVec4 selected_border = kSelectedBorder;
        selected_border.w *= selected_amount;
        draw->AddRect(
            start, end, Color(selected_border), U(8.0f), 0, U(1.0f));
    }

    const ImVec4 text_color = animation::Lerp(
        ImVec4(0.61f, 0.67f, 0.75f, 1.0f),
        ImVec4(0.94f, 0.97f, 1.0f, 1.0f), selected_amount);
    const ImVec4 icon_color = animation::Lerp(
        animation::Lerp(components::kMuted, components::kText, hover_amount),
        components::kAccent, selected_amount);
    const ImVec2 icon_center(
        start.x + U(18.0f), start.y + U(18.0f));
    if (draw_icon != nullptr) {
        draw_icon(draw, icon_center, Color(icon_color), context);
    } else {
        DrawIcon(draw, icon, icon_center, Color(icon_color));
    }
    ImFont* label_font = selected ? SemiboldFont() : RegularFont();
    const char* visible_label = settings::Translate(label);
    if (label_font != nullptr) {
        draw->AddText(
            label_font, ImGui::GetFontSize(),
            ImVec2(
                start.x + U(36.0f) + selected_amount,
                start.y + U(9.0f)),
            Color(text_color), visible_label);
    } else {
        draw->AddText(
            ImVec2(
                start.x + U(36.0f) + selected_amount,
                start.y + U(9.0f)),
            Color(text_color), visible_label);
    }

    ImGui::PopID();
    return clicked;
}

bool Item(const char* label, Icon icon, bool selected) {
    return ItemInternal(label, icon, selected, nullptr, nullptr);
}

bool Item(const char* label, bool selected, IconDrawCallback draw_icon,
          const void* context) {
    return ItemInternal(
        label, Icon::Lua, selected, draw_icon, context);
}

}  // namespace pztrainer::ui::navigation
