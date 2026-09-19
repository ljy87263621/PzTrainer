#include "ui/components.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "ui/animation.hpp"
#include "ui/brand_icon.hpp"
#include "ui/color_picker.hpp"
#include "ui/theme.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"

namespace pztrainer::ui::components {
namespace {

constexpr ImVec4 kRowHover{0.12f, 0.13f, 0.16f, 0.42f};
constexpr ImVec4 kDivider{0.70f, 0.72f, 0.78f, 0.10f};
constexpr ImVec4 kTrackOff{0.055f, 0.065f, 0.082f, 1.0f};

float U(float value) {
    return value * settings::UiScale();
}

ImU32 Color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

void CenteredText(ImDrawList* draw, const ImVec2& minimum, const ImVec2& maximum, const char* text, ImU32 color) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    draw->AddText(
        ImVec2(minimum.x + (maximum.x - minimum.x - size.x) * 0.5f,
               minimum.y + (maximum.y - minimum.y - size.y) * 0.5f),
        color,
        text);
}

}  // namespace

void DrawBrandBadge(const char* letters) {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 end(start.x + U(42.0f), start.y + U(42.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImTextureID texture = BrandIconTexture();
    if (texture != 0) {
        const ImVec2 icon_size(U(36.0f), U(22.0f));
        const ImVec2 icon_minimum(
            start.x + (end.x - start.x - icon_size.x) * 0.5f,
            start.y + (end.y - start.y - icon_size.y) * 0.5f);
        draw->AddImageRounded(
            ImTextureRef(texture), icon_minimum,
            ImVec2(icon_minimum.x + icon_size.x, icon_minimum.y + icon_size.y),
            ImVec2(0.110f, 0.265f), ImVec2(0.903f, 0.723f),
            Color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), U(3.0f));
    } else {
        if (SemiboldFont() != nullptr) ImGui::PushFont(SemiboldFont(), 0.0f);
        CenteredText(
            draw, start, end, letters,
            Color(ImVec4(230.0f / 255.0f, 238.0f / 255.0f, 1.0f, 1.0f)));
        if (SemiboldFont() != nullptr) ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(U(42.0f), U(42.0f)));
}

void DrawUserAvatar() {
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 center(start.x + U(21.0f), start.y + U(21.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 color = Color(ImVec4(
        232.0f / 255.0f, 237.0f / 255.0f, 248.0f / 255.0f, 1.0f));
    draw->AddCircleFilled(
        ImVec2(center.x, center.y - U(7.0f)), U(5.0f), color);
    draw->AddBezierCubic(
        ImVec2(center.x - U(10.0f), center.y + U(10.0f)),
        ImVec2(center.x - U(9.0f), center.y - U(1.0f)),
        ImVec2(center.x + U(9.0f), center.y - U(1.0f)),
        ImVec2(center.x + U(10.0f), center.y + U(10.0f)),
        color, U(2.2f));
    ImGui::Dummy(ImVec2(U(42.0f), U(42.0f)));
}

struct ValueFormatParts {
    std::string number = "%.3f";
    std::string suffix;
};

std::string UnescapePercent(std::string value) {
    for (std::size_t offset = value.find("%%");
         offset != std::string::npos;
         offset = value.find("%%", offset + 1)) {
        value.replace(offset, 2, "%");
    }
    return value;
}

ValueFormatParts SplitValueFormat(const char* format) {
    const char* translated = settings::Translate(format);
    const std::string value = translated != nullptr ? translated : "%.3f";
    std::size_t conversion = value.find('%');
    while (conversion != std::string::npos &&
           conversion + 1 < value.size() && value[conversion + 1] == '%') {
        conversion = value.find('%', conversion + 2);
    }
    const std::size_t end = conversion == std::string::npos
        ? std::string::npos : value.find('f', conversion + 1);
    if (conversion == std::string::npos || end == std::string::npos) {
        return {};
    }
    ValueFormatParts result;
    result.number = value.substr(conversion, end - conversion + 1);
    result.suffix = UnescapePercent(value.substr(end + 1));
    return result;
}

void FinishFixedHeightRow(const ImVec2& start, float width, float height) {
    const float submitted_height = std::max(
        0.0f, height - ImGui::GetStyle().ItemSpacing.y);
    ImGui::SetCursorScreenPos(start);
    ImGui::Dummy(ImVec2(width, submitted_height));
}

void SectionLabel(const char* label) {
    const float original_x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(original_x + U(10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(settings::Translate(label));
    ImGui::PopStyleColor();
    ImGui::SetCursorPosX(original_x);
}

bool TopTab(const char* label, bool selected, float width) {
    ImGui::PushID(label);
    const ImGuiID hover_animation_id = ImGui::GetID("tab_hover");
    const ImGuiID selection_animation_id = ImGui::GetID("tab_selection");
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("tab_button", ImVec2(width, U(34.0f)));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const float hover_amount = animation::Clamp01(animation::Spring(hover_animation_id, hovered ? 1.0f : 0.0f, 260.0f, 24.0f));
    const float selected_amount = animation::Clamp01(animation::Spring(selection_animation_id, selected ? 1.0f : 0.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec4 base(0.025f, 0.027f, 0.035f, 0.55f);
    const ImVec4 hover_fill = animation::Lerp(base, kRowHover, hover_amount);
    const ImVec4 fill = animation::Lerp(hover_fill, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.22f), selected_amount);
    draw->AddRectFilled(
        start, ImVec2(start.x + width, start.y + U(34.0f)),
        Color(fill), U(7.0f));
    if (selected_amount > 0.001f) {
        draw->AddRect(start, ImVec2(start.x + width, start.y + U(34.0f)),
                      Color(ImVec4(kAccent.x, kAccent.y, kAccent.z, selected_amount)),
                      U(7.0f), 0, U(1.0f));
    }
    draw->AddText(ImVec2(start.x + U(13.0f), start.y + U(8.0f)),
                  Color(animation::Lerp(kMuted, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), selected_amount)),
                  settings::Translate(label));
    ImGui::PopID();
    return clicked;
}

bool MoreOptionsButton(const char* id, const ImVec2& size) {
    ImGui::PushID(id);
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("more_options", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const float amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("more_options_hover"), hovered ? 1.0f : 0.0f,
        260.0f, 22.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (amount > 0.001f) {
        draw->AddRectFilled(
            minimum, ImVec2(minimum.x + size.x, minimum.y + size.y),
            Color(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.16f * amount)),
            7.0f);
    }
    const ImU32 dot_color = Color(animation::Lerp(kMuted, kText, amount));
    const float center_y = minimum.y + size.y * 0.5f;
    const float center_x = minimum.x + size.x * 0.5f;
    for (int index = -1; index <= 1; ++index) {
        draw->AddCircleFilled(
            ImVec2(center_x + static_cast<float>(index) * 6.0f, center_y),
            2.0f, dot_color);
    }
    ImGui::PopID();
    return clicked;
}

void BeginCardInternal(const char* id, const char* heading, const ImVec2& size,
                       const ImVec2& padding) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.050f, 0.047f, 0.062f, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ImGui::GetStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGuiChildFlags flags = ImGuiChildFlags_Borders;
    ImVec2 resolved_size = size;
    if (resolved_size.y > 0.0f) resolved_size.y = U(resolved_size.y);
    if (resolved_size.y <= 0.0f) {
        flags |= ImGuiChildFlags_AutoResizeY;
    }
    ImGui::BeginChild(id, resolved_size, flags);
    if (heading != nullptr && heading[0] != '\0') {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.65f, 0.73f, 1.0f));
        ImGui::TextUnformatted(settings::Translate(heading));
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }
}

void BeginCard(const char* id, const char* heading, const ImVec2& size) {
    BeginCardInternal(id, heading, size, ImVec2(U(13.0f), U(11.0f)));
}

void BeginCompactCard(const char* id, const char* heading, const ImVec2& size) {
    BeginCardInternal(id, heading, size, ImVec2(U(9.0f), U(7.0f)));
}

void EndCard() {
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

bool ToggleRowInternal(const char* label, bool* value,
                       features::visual::StateColor* colors, ImVec4* color,
                       bool show_divider, bool compact) {
    ImGui::PushID(label);
    const ImGuiID hover_animation_id = ImGui::GetID("toggle_hover");
    const ImGuiID toggle_animation_id = ImGui::GetID("toggle_value");
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool has_color = colors != nullptr || color != nullptr;
    const float row_height = U(compact ? 32.0f : 40.0f);
    const float track_width = U(compact ? 34.0f : 42.0f);
    const float track_height = U(compact ? 20.0f : 24.0f);
    const float track_right = U(compact ? 9.0f : 11.0f);
    const float track_top = U(compact ? 6.0f : 8.0f);
    const float knob_radius = U(compact ? 7.0f : 9.0f);
    const float label_width = width - (has_color
        ? U(compact ? 83.0f : 101.0f)
        : U(compact ? 47.0f : 58.0f));
    ImGui::InvisibleButton("toggle_label", ImVec2(label_width, row_height));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    const ImVec2 track_min(
        start.x + width - track_width - track_right, start.y + track_top);
    const ImVec2 track_max(
        track_min.x + track_width, track_min.y + track_height);
    ImGui::SetCursorScreenPos(track_min);
    ImGui::InvisibleButton("toggle_switch", ImVec2(track_width, track_height));
    hovered |= ImGui::IsItemHovered();
    clicked |= ImGui::IsItemClicked();
    if (clicked) {
        *value = !*value;
    }
    const float hover_amount = animation::Clamp01(animation::Spring(hover_animation_id, hovered ? 1.0f : 0.0f, 260.0f, 24.0f));
    const float toggle_amount = animation::Spring(toggle_animation_id, *value ? 1.0f : 0.0f, 280.0f, 18.0f);
    const float toggle_color_amount = animation::Clamp01(toggle_amount);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hover_amount > 0.001f) {
        ImVec4 hover_color = kRowHover;
        hover_color.w *= hover_amount;
        draw->AddRectFilled(
            start, ImVec2(start.x + width, start.y + row_height - 1.0f),
            Color(hover_color), U(7.0f));
    }
    draw->AddText(
        ImVec2(start.x + U(compact ? 10.0f : 13.0f) + hover_amount,
               start.y + U(compact ? 7.0f : 11.0f)),
        Color(kText), settings::Translate(label));

    draw->AddRectFilled(track_min, track_max,
                        Color(animation::Lerp(kTrackOff, kAccent, toggle_color_amount)),
                        track_height * 0.5f);
    const float knob_x = animation::Lerp(
        track_min.x + U(3.0f),
        track_max.x - knob_radius * 2.0f - U(3.0f),
        toggle_amount);
    draw->AddCircleFilled(
        ImVec2(knob_x + knob_radius, track_min.y + track_height * 0.5f),
        knob_radius,
                          Color(animation::Lerp(ImVec4(0.55f, 0.60f, 0.66f, 1.0f),
                                                ImVec4(1.0f, 1.0f, 1.0f, 1.0f), toggle_color_amount)));
    if (colors != nullptr) {
        const float color_size = U(compact ? 22.0f : 26.0f);
        ImGui::SetCursorScreenPos(ImVec2(
            start.x + width - U(compact ? 75.0f : 91.0f),
            start.y + U(compact ? 5.0f : 7.0f)));
        DrawColorSwatch("row_color", colors, ImVec2(color_size, color_size));
    } else if (color != nullptr) {
        const float color_size = U(compact ? 22.0f : 26.0f);
        ImGui::SetCursorScreenPos(ImVec2(
            start.x + width - U(compact ? 75.0f : 91.0f),
            start.y + U(compact ? 5.0f : 7.0f)));
        DrawColorSwatch("row_color", color, ImVec2(color_size, color_size));
    }
    if (show_divider) {
        const float inset = U(compact ? 9.0f : 11.0f);
        draw->AddLine(ImVec2(start.x + inset, start.y + row_height - 0.5f),
                      ImVec2(start.x + width - inset, start.y + row_height - 0.5f),
                      Color(kDivider), 0.65f);
    }
    FinishFixedHeightRow(start, width, row_height);
    ImGui::PopID();
    return clicked;
}

bool ToggleRow(const char* label, bool* value,
               features::visual::StateColor* colors, bool show_divider) {
    return ToggleRowInternal(label, value, colors, nullptr, show_divider, false);
}

bool ToggleRow(const char* label, bool* value, ImVec4* color, bool show_divider) {
    return ToggleRowInternal(label, value, nullptr, color, show_divider, false);
}

bool CompactToggleRow(const char* label, bool* value, bool show_divider) {
    return ToggleRowInternal(
        label, value, nullptr, nullptr, show_divider, true);
}

bool CompactToggleRow(const char* label, bool* value,
                      features::visual::StateColor* colors,
                      bool show_divider) {
    return ToggleRowInternal(
        label, value, colors, nullptr, show_divider, true);
}

bool CompactToggleRow(const char* label, bool* value, ImVec4* color,
                      bool show_divider) {
    return ToggleRowInternal(
        label, value, nullptr, color, show_divider, true);
}

void RoundedTooltip(const char* text) {
    if (text == nullptr || text[0] == '\0') return;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(11.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.060f, 0.078f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.30f, 0.42f, 0.72f, 0.48f));
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 19.0f);
    ImGui::TextUnformatted(settings::Translate(text));
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void DisabledToggleRowInternal(const char* label, bool value, const char* tooltip,
                               bool show_divider, bool compact) {
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float row_height = U(compact ? 32.0f : 40.0f);
    const float track_width = U(compact ? 34.0f : 42.0f);
    const float track_height = U(compact ? 20.0f : 24.0f);
    const float knob_radius = U(compact ? 7.0f : 9.0f);
    ImGui::InvisibleButton("disabled_toggle", ImVec2(width, row_height));
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hovered) {
        draw->AddRectFilled(
            start, ImVec2(start.x + width, start.y + row_height - 1.0f),
            Color(ImVec4(0.12f, 0.13f, 0.16f, 0.26f)), U(7.0f));
        RoundedTooltip(tooltip);
    }

    const ImU32 disabled_text = Color(ImVec4(0.36f, 0.38f, 0.44f, 1.0f));
    draw->AddText(
        ImVec2(start.x + U(compact ? 10.0f : 13.0f),
               start.y + U(compact ? 7.0f : 11.0f)),
        disabled_text, settings::Translate(label));
    const ImVec2 track_min(
        start.x + width - track_width - U(compact ? 9.0f : 11.0f),
        start.y + U(compact ? 6.0f : 8.0f));
    const ImVec2 track_max(
        track_min.x + track_width, track_min.y + track_height);
    draw->AddRectFilled(
        track_min, track_max,
        Color(value ? ImVec4(0.13f, 0.20f, 0.34f, 1.0f)
                    : ImVec4(0.045f, 0.050f, 0.063f, 1.0f)),
        track_height * 0.5f);
    const float knob_x = value
        ? track_max.x - knob_radius * 2.0f - U(3.0f)
        : track_min.x + U(3.0f);
    draw->AddCircleFilled(
        ImVec2(knob_x + knob_radius, track_min.y + track_height * 0.5f),
        knob_radius,
        Color(ImVec4(0.29f, 0.31f, 0.36f, 1.0f)));
    if (show_divider) {
        const float inset = U(compact ? 9.0f : 11.0f);
        draw->AddLine(ImVec2(start.x + inset, start.y + row_height - 0.5f),
                      ImVec2(start.x + width - inset, start.y + row_height - 0.5f),
                      Color(kDivider), 0.65f);
    }
    FinishFixedHeightRow(start, width, row_height);
    ImGui::PopID();
}

void DisabledToggleRow(const char* label, bool value, const char* tooltip,
                       bool show_divider) {
    DisabledToggleRowInternal(label, value, tooltip, show_divider, false);
}

void CompactDisabledToggleRow(const char* label, bool value,
                              const char* tooltip, bool show_divider) {
    DisabledToggleRowInternal(label, value, tooltip, show_divider, true);
}

bool StepperRow(const char* label, float* value, float minimum, float maximum, float step,
                const char* format, bool show_divider,
                bool* interaction_active) {
    ImGui::PushID(label);
    if (interaction_active != nullptr) *interaction_active = false;
    const ImGuiID slider_animation_id = ImGui::GetID("slider_value");
    const ImGuiID hover_animation_id = ImGui::GetID("slider_hover");
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const char* translated_label = settings::Translate(label);

    const ValueFormatParts value_format = SplitValueFormat(format);
    const float value_width = U(68.0f);
    const float value_left = start.x + width - value_width - U(11.0f);
    const float text_width = ImGui::CalcTextSize(translated_label).x;
    const float maximum_label_width = std::max(
        U(40.0f), value_left - start.x - U(12.0f) - U(64.0f));
    const float label_width = std::min(
        std::max(width * 0.32f, text_width + U(26.0f)),
        maximum_label_width);
    const float track_left = start.x + label_width;
    const float track_width = std::max(
        U(18.0f), value_left - U(12.0f) - track_left);
    const float track_right = track_left + track_width;
    const float track_y = start.y + U(22.0f);
    bool changed = false;

    draw->PushClipRect(
        ImVec2(start.x, start.y),
        ImVec2(track_left - U(8.0f), start.y + U(46.0f)), true);
    draw->AddText(
        ImVec2(start.x + U(13.0f), start.y + U(14.0f)), Color(kText),
        translated_label);
    draw->PopClipRect();

    const ImVec2 value_min(value_left, start.y + U(8.0f));
    const ImVec2 value_max(
        value_left + value_width, value_min.y + U(28.0f));
    draw->AddRectFilled(
        value_min, value_max,
        Color(ImVec4(0.07f, 0.075f, 0.095f, 1.0f)), U(7.0f));
    const float suffix_width = value_format.suffix.empty()
        ? 0.0f : ImGui::CalcTextSize(value_format.suffix.c_str()).x;
    const float suffix_reservation = value_format.suffix.empty()
        ? 0.0f : suffix_width + U(10.0f);
    const float input_width = std::max(
        U(24.0f), value_width - suffix_reservation);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(U(7.0f), U(4.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::SetCursorScreenPos(value_min);
    ImGui::SetNextItemWidth(input_width);
    const bool value_changed = ImGui::InputFloat(
        "##value", value, 0.0f, 0.0f, value_format.number.c_str(),
        ImGuiInputTextFlags_CharsDecimal);
    const bool value_input_hovered = ImGui::IsItemHovered();
    const bool value_input_active = ImGui::IsItemActive();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    if (value_changed) {
        if (!std::isfinite(*value)) *value = minimum;
        *value = std::clamp(*value, minimum, maximum);
        changed = true;
    }
    if (!value_format.suffix.empty()) {
        draw->AddText(
            ImVec2(value_max.x - suffix_width - U(7.0f),
                   value_min.y + (U(28.0f) - ImGui::GetTextLineHeight()) * 0.5f),
            Color(kText), value_format.suffix.c_str());
    }
    if (value_input_hovered || value_input_active) {
        draw->AddRect(
            value_min, value_max,
            Color(ImVec4(kAccent.x, kAccent.y, kAccent.z,
                         value_input_active ? 0.72f : 0.34f)),
            U(7.0f), 0, U(1.0f));
    }

    ImGui::SetCursorScreenPos(ImVec2(track_left, track_y - U(9.0f)));
    ImGui::InvisibleButton("slider_track", ImVec2(track_width, U(18.0f)));
    const bool slider_hovered = ImGui::IsItemHovered();
    const bool slider_active = ImGui::IsItemActive();
    if (slider_active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float position = std::clamp((ImGui::GetIO().MousePos.x - track_left) / track_width, 0.0f, 1.0f);
        const float raw_value = minimum + position * (maximum - minimum);
        const float snapped_value = step > 0.0f
            ? minimum + std::round((raw_value - minimum) / step) * step
            : raw_value;
        const float next_value = std::clamp(snapped_value, minimum, maximum);
        if (*value != next_value) {
            *value = next_value;
            changed = true;
        }
    }

    const float target_position = std::clamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float animated_position = animation::Clamp01(
        animation::Spring(slider_animation_id, target_position, 270.0f, 19.0f));
    const float hover_amount = animation::Clamp01(
        animation::Spring(hover_animation_id, slider_hovered ? 1.0f : 0.0f, 260.0f, 23.0f));
    const float knob_x = track_left + track_width * animated_position;
    draw->AddRectFilled(
        ImVec2(track_left, track_y - U(2.0f)),
        ImVec2(track_right, track_y + U(2.0f)),
        Color(ImVec4(0.10f, 0.11f, 0.14f, 1.0f)), 2.0f);
    draw->AddRectFilled(
        ImVec2(track_left, track_y - U(2.0f)),
        ImVec2(knob_x, track_y + U(2.0f)),
        Color(kAccent), 2.0f);
    if (hover_amount > 0.001f) {
        draw->AddCircleFilled(
            ImVec2(knob_x, track_y), U(10.0f) + hover_amount * U(2.0f),
            Color(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.10f * hover_amount)));
    }
    draw->AddCircleFilled(
        ImVec2(knob_x, track_y), U(7.0f) + hover_amount,
        Color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));

    if (show_divider) {
        draw->AddLine(ImVec2(start.x + U(11.0f), start.y + U(45.5f)),
                      ImVec2(start.x + width - U(11.0f),
                             start.y + U(45.5f)),
                      Color(kDivider), 0.65f);
    }
    if (interaction_active != nullptr) {
        *interaction_active = value_input_active || slider_active;
    }
    FinishFixedHeightRow(start, width, U(46.0f));
    ImGui::PopID();
    return changed;
}

}  // namespace pztrainer::ui::components
