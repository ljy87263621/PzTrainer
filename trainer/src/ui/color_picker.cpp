#include "ui/color_picker.hpp"

#include <algorithm>
#include <cmath>

#include "ui/animation.hpp"
#include "ui/glass_blur.hpp"

namespace pztrainer::ui {
namespace {

struct PickerState {
    ImGuiID owner = 0;
    features::visual::StateColor* colors = nullptr;
    ImVec4* color = nullptr;
    ImVec2 anchor{};
    bool requested = false;
    bool just_opened = false;
    int selected_state = 0;
    int last_seen_frame = -1;
    float amount = 0.0f;
    float velocity = 0.0f;
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 1.0f;
    float alpha = 1.0f;
};

PickerState g_picker;

ImVec4* StateColorAt(features::visual::StateColor& colors, int state) {
    if (state == 1) return &colors.behind_wall;
    if (state == 2) return &colors.in_view;
    return &colors.normal;
}

void ReadColor(PickerState& picker) {
    ImGui::ColorConvertRGBtoHSV(
        picker.color->x, picker.color->y, picker.color->z,
        picker.hue, picker.saturation, picker.value);
    picker.alpha = picker.color->w;
}

void WriteColor(PickerState& picker) {
    ImGui::ColorConvertHSVtoRGB(
        picker.hue, picker.saturation, picker.value,
        picker.color->x, picker.color->y, picker.color->z);
    picker.color->w = picker.alpha;
}

void SelectState(PickerState& picker, int state) {
    picker.selected_state = std::clamp(state, 0, 2);
    picker.color = StateColorAt(*picker.colors, picker.selected_state);
    ReadColor(picker);
}

void DrawStateTabs(PickerState& picker) {
    constexpr const char* labels[]{"默认", "墙后", "视野内"};
    const float gap = 5.0f;
    const float width = (ImGui::GetContentRegionAvail().x - gap * 2.0f) / 3.0f;
    for (int state = 0; state < 3; ++state) {
        ImGui::PushID(state);
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("StateTab", ImVec2(width, 34.0f));
        const bool selected = picker.selected_state == state;
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) SelectState(picker, state);
        const float amount = animation::Clamp01(animation::Spring(
            ImGui::GetID("StateTabAmount"), selected ? 1.0f : (hovered ? 0.45f : 0.0f),
            270.0f, 21.0f));
        const ImVec2 maximum(minimum.x + width, minimum.y + 34.0f);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(
            minimum, maximum,
            ImGui::GetColorU32(ImVec4(0.10f + amount * 0.03f,
                                      0.11f + amount * 0.05f,
                                      0.15f + amount * 0.13f, 0.82f)), 7.0f);
        if (selected) {
            draw->AddRect(minimum, maximum,
                          ImGui::GetColorU32(ImVec4(0.13f, 0.36f, 0.95f, 0.86f)),
                          7.0f, 0, 1.0f);
        }
        const ImVec4& color = *StateColorAt(*picker.colors, state);
        draw->AddCircleFilled(ImVec2(minimum.x + 12.0f, minimum.y + 17.0f), 4.0f,
                              ImGui::GetColorU32(color));
        const bool enabled = state == 0 ||
            (state == 1 ? picker.colors->behind_wall_enabled : picker.colors->in_view_enabled);
        const ImVec4 text_color = enabled
            ? ImVec4(0.88f, 0.90f, 0.95f, 1.0f)
            : ImVec4(0.40f, 0.42f, 0.49f, 1.0f);
        draw->AddText(ImVec2(minimum.x + 21.0f, minimum.y + 9.0f),
                      ImGui::GetColorU32(text_color), labels[state]);
        ImGui::PopID();
        if (state != 2) ImGui::SameLine(0.0f, gap);
    }
}

void DrawStateToggle(const char* id, const char* label, bool* value, const ImVec4& color) {
    ImGui::PushID(id);
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("StateToggle", ImVec2(width, 36.0f));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) *value = !*value;
    const float amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("StateToggleAmount"), *value ? 1.0f : 0.0f, 280.0f, 19.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hovered) {
        draw->AddRectFilled(minimum, ImVec2(minimum.x + width, minimum.y + 36.0f),
                            ImGui::GetColorU32(ImVec4(0.12f, 0.13f, 0.17f, 0.42f)), 7.0f);
    }
    draw->AddCircleFilled(ImVec2(minimum.x + 9.0f, minimum.y + 18.0f), 5.0f,
                          ImGui::GetColorU32(color));
    draw->AddText(ImVec2(minimum.x + 21.0f, minimum.y + 10.0f),
                  ImGui::GetColorU32(ImVec4(0.84f, 0.86f, 0.91f, 1.0f)), label);
    const ImVec2 track_min(minimum.x + width - 38.0f, minimum.y + 9.0f);
    const ImVec2 track_max(track_min.x + 32.0f, track_min.y + 18.0f);
    draw->AddRectFilled(track_min, track_max,
                        ImGui::GetColorU32(ImVec4(0.08f + amount * 0.05f,
                                                  0.09f + amount * 0.27f,
                                                  0.12f + amount * 0.83f, 1.0f)), 9.0f);
    draw->AddCircleFilled(ImVec2(track_min.x + 9.0f + amount * 14.0f, track_min.y + 9.0f),
                          6.5f, ImGui::GetColorU32(
                              ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
    ImGui::PopID();
}

void Checkerboard(ImDrawList* draw, const ImVec2& minimum, const ImVec2& maximum, float cell) {
    for (float y = minimum.y; y < maximum.y; y += cell) {
        for (float x = minimum.x; x < maximum.x; x += cell) {
            const int column = static_cast<int>((x - minimum.x) / cell);
            const int row = static_cast<int>((y - minimum.y) / cell);
            const ImU32 color = ((column + row) & 1) == 0
                ? ImGui::GetColorU32(ImVec4(0.76f, 0.78f, 0.82f, 1.0f))
                : ImGui::GetColorU32(ImVec4(0.42f, 0.44f, 0.49f, 1.0f));
            draw->AddRectFilled(
                ImVec2(x, y),
                ImVec2(std::min(x + cell, maximum.x), std::min(y + cell, maximum.y)),
                color);
        }
    }
}

void DrawSvPicker(PickerState& picker, ImDrawList* draw, const ImVec2& minimum, float size) {
    const ImVec2 maximum(minimum.x + size, minimum.y + size);
    float hue_r = 1.0f;
    float hue_g = 1.0f;
    float hue_b = 1.0f;
    ImGui::ColorConvertHSVtoRGB(picker.hue, 1.0f, 1.0f, hue_r, hue_g, hue_b);
    draw->AddRectFilledMultiColor(
        minimum, maximum, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)),
        ImGui::GetColorU32(ImVec4(hue_r, hue_g, hue_b, 1.0f)),
        ImGui::GetColorU32(ImVec4(hue_r, hue_g, hue_b, 1.0f)),
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
    draw->AddRectFilledMultiColor(
        minimum, maximum, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)),
        ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 1.0f)));

    ImGui::SetCursorScreenPos(minimum);
    ImGui::InvisibleButton("SaturationValue", ImVec2(size, size));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        picker.saturation = std::clamp((mouse.x - minimum.x) / size, 0.0f, 1.0f);
        picker.value = 1.0f - std::clamp((mouse.y - minimum.y) / size, 0.0f, 1.0f);
        WriteColor(picker);
    }

    const ImVec2 marker(
        minimum.x + picker.saturation * size,
        minimum.y + (1.0f - picker.value) * size);
    const float marker_scale = 1.0f + animation::Clamp01(animation::Spring(
        ImGui::GetID("SvMarkerHover"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 250.0f, 20.0f)) * 0.18f;
    draw->AddCircle(marker, 8.0f * marker_scale,
                    ImGui::GetColorU32(ImVec4(0.05f, 0.06f, 0.09f, 0.86f)), 24, 4.0f);
    draw->AddCircle(marker, 7.0f * marker_scale,
                    ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 24, 2.0f);
    draw->AddRect(minimum, maximum, ImGui::GetColorU32(ImVec4(0.34f, 0.37f, 0.45f, 0.62f)), 9.0f);
}

void DrawHueBar(PickerState& picker, ImDrawList* draw, const ImVec2& minimum, const ImVec2& size) {
    constexpr int kSegments = 6;
    for (int index = 0; index < kSegments; ++index) {
        float first_r = 0.0f;
        float first_g = 0.0f;
        float first_b = 0.0f;
        float second_r = 0.0f;
        float second_g = 0.0f;
        float second_b = 0.0f;
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(index) / kSegments, 1.0f, 1.0f,
                                    first_r, first_g, first_b);
        ImGui::ColorConvertHSVtoRGB(static_cast<float>(index + 1) / kSegments, 1.0f, 1.0f,
                                    second_r, second_g, second_b);
        const float left = minimum.x + size.x * static_cast<float>(index) / kSegments;
        const float right = minimum.x + size.x * static_cast<float>(index + 1) / kSegments;
        draw->AddRectFilledMultiColor(
            ImVec2(left, minimum.y), ImVec2(right, minimum.y + size.y),
            ImGui::GetColorU32(ImVec4(first_r, first_g, first_b, 1.0f)),
            ImGui::GetColorU32(ImVec4(second_r, second_g, second_b, 1.0f)),
            ImGui::GetColorU32(ImVec4(second_r, second_g, second_b, 1.0f)),
            ImGui::GetColorU32(ImVec4(first_r, first_g, first_b, 1.0f)));
    }
    ImGui::SetCursorScreenPos(minimum);
    ImGui::InvisibleButton("Hue", size);
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        picker.hue = std::clamp((ImGui::GetIO().MousePos.x - minimum.x) / size.x, 0.0f, 0.9999f);
        WriteColor(picker);
    }
    const float marker_x = minimum.x + picker.hue * size.x;
    draw->AddRectFilled(ImVec2(marker_x - 5.0f, minimum.y - 4.0f),
                        ImVec2(marker_x + 5.0f, minimum.y + size.y + 4.0f),
                        ImGui::GetColorU32(ImVec4(0.96f, 0.97f, 0.99f, 1.0f)), 5.0f);
    draw->AddRect(ImVec2(marker_x - 6.0f, minimum.y - 5.0f),
                  ImVec2(marker_x + 6.0f, minimum.y + size.y + 5.0f),
                  ImGui::GetColorU32(ImVec4(0.07f, 0.09f, 0.14f, 0.82f)), 6.0f, 0, 2.0f);
}

void DrawAlphaBar(PickerState& picker, ImDrawList* draw, const ImVec2& minimum, const ImVec2& size) {
    const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
    Checkerboard(draw, minimum, maximum, 7.0f);
    ImVec4 opaque = *picker.color;
    opaque.w = 1.0f;
    draw->AddRectFilledMultiColor(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(opaque.x, opaque.y, opaque.z, 0.0f)),
        ImGui::GetColorU32(opaque), ImGui::GetColorU32(opaque),
        ImGui::GetColorU32(ImVec4(opaque.x, opaque.y, opaque.z, 0.0f)));
    ImGui::SetCursorScreenPos(minimum);
    ImGui::InvisibleButton("Alpha", size);
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        picker.alpha = std::clamp((ImGui::GetIO().MousePos.x - minimum.x) / size.x, 0.0f, 1.0f);
        WriteColor(picker);
    }
    const float marker_x = minimum.x + picker.alpha * size.x;
    draw->AddCircleFilled(ImVec2(marker_x, minimum.y + size.y * 0.5f), 8.0f,
                          ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
    draw->AddCircle(ImVec2(marker_x, minimum.y + size.y * 0.5f), 9.0f,
                    ImGui::GetColorU32(ImVec4(0.09f, 0.11f, 0.16f, 0.86f)), 24, 2.0f);
}

void DrawNumericValues(PickerState& picker) {
    int hue = static_cast<int>(std::round(picker.hue * 359.0f));
    int saturation = static_cast<int>(std::round(picker.saturation * 255.0f));
    int value = static_cast<int>(std::round(picker.value * 255.0f));
    int alpha = static_cast<int>(std::round(picker.alpha * 255.0f));
    ImGui::TextUnformatted("HSV");
    ImGui::SameLine(53.0f);
    ImGui::PushItemWidth(43.0f);
    bool changed = ImGui::DragInt("##HueValue", &hue, 1.0f, 0, 359, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    changed |= ImGui::DragInt("##SaturationValue", &saturation, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    changed |= ImGui::DragInt("##BrightnessValue", &value, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    changed |= ImGui::DragInt("##AlphaValue", &alpha, 1.0f, 0, 255, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopItemWidth();
    if (changed) {
        picker.hue = static_cast<float>(hue) / 359.0f;
        picker.saturation = static_cast<float>(saturation) / 255.0f;
        picker.value = static_cast<float>(value) / 255.0f;
        picker.alpha = static_cast<float>(alpha) / 255.0f;
        WriteColor(picker);
    }
}

}  // namespace

bool DrawColorSwatchInternal(const char* id, ImVec4* color,
                             features::visual::StateColor* colors, const ImVec2& size) {
    ImGui::PushID(id);
    const ImGuiID owner = ImGui::GetID("ColorSwatch");
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("ColorSwatch", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    if (clicked) {
        if (g_picker.owner == owner && g_picker.requested) {
            g_picker.requested = false;
        } else {
            g_picker.owner = owner;
            g_picker.colors = colors;
            g_picker.selected_state = 0;
            g_picker.color = color;
            g_picker.anchor = ImVec2(minimum.x, minimum.y + size.y);
            g_picker.requested = true;
            g_picker.just_opened = true;
            g_picker.amount = 0.0f;
            g_picker.velocity = 0.0f;
            ReadColor(g_picker);
        }
    }
    if (g_picker.owner == owner) {
        g_picker.anchor = ImVec2(minimum.x, minimum.y + size.y);
        g_picker.last_seen_frame = ImGui::GetFrameCount();
    }

    const float hover_amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("ColorSwatchHover"), hovered || (g_picker.owner == owner && g_picker.requested) ? 1.0f : 0.0f,
        300.0f, 21.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
    draw->AddRectFilled(ImVec2(minimum.x - 2.0f, minimum.y - 2.0f),
                        ImVec2(maximum.x + 2.0f, maximum.y + 2.0f),
                        ImGui::GetColorU32(ImVec4(0.16f, 0.18f, 0.23f, 0.55f + hover_amount * 0.30f)),
                        7.0f + hover_amount);
    draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(*color), 6.0f);
    if (colors != nullptr) {
        draw->AddRectFilled(ImVec2(minimum.x + 2.0f, maximum.y - 4.0f),
                            ImVec2(minimum.x + size.x * 0.5f, maximum.y - 2.0f),
                            ImGui::GetColorU32(colors->behind_wall), 1.0f);
        draw->AddRectFilled(ImVec2(minimum.x + size.x * 0.5f, maximum.y - 4.0f),
                            ImVec2(maximum.x - 2.0f, maximum.y - 2.0f),
                            ImGui::GetColorU32(colors->in_view), 1.0f);
    }
    draw->AddRect(minimum, maximum, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.16f + hover_amount * 0.24f)),
                  6.0f, 0, 1.0f);
    ImGui::PopID();
    return clicked;
}

bool DrawColorSwatch(const char* id, ImVec4* color, const ImVec2& size) {
    return DrawColorSwatchInternal(id, color, nullptr, size);
}

bool DrawColorSwatch(const char* id, features::visual::StateColor* colors, const ImVec2& size) {
    return DrawColorSwatchInternal(id, &colors->normal, colors, size);
}

void DrawColorPickerOverlay(bool host_visible, const ImVec2& host_minimum, const ImVec2& host_maximum) {
    if (g_picker.color == nullptr) return;
    if (!host_visible) g_picker.requested = false;

    animation::SpringValue(g_picker.amount, g_picker.velocity, g_picker.requested ? 1.0f : 0.0f, 230.0f, 20.0f);
    const float alpha = animation::Clamp01(g_picker.amount);
    if (!g_picker.requested && alpha < 0.002f) {
        g_picker = PickerState{};
        return;
    }

    const ImVec2 kPanelSize(292.0f, g_picker.colors == nullptr ? 410.0f : 526.0f);
    const float scale = 0.96f + alpha * 0.04f;
    ImVec2 position(g_picker.anchor.x - kPanelSize.x + 26.0f,
                    g_picker.anchor.y + 9.0f + (1.0f - alpha) * 12.0f);
    const float minimum_x = host_minimum.x + 12.0f;
    const float maximum_x = std::max(minimum_x, host_maximum.x - kPanelSize.x - 12.0f);
    const float minimum_y = host_minimum.y + 12.0f;
    const float maximum_y = std::max(minimum_y, host_maximum.y - kPanelSize.y - 12.0f);
    position.x = std::clamp(position.x, minimum_x, maximum_x);
    if (position.y > maximum_y) {
        position.y = g_picker.anchor.y - kPanelSize.y - 35.0f - (1.0f - alpha) * 12.0f;
    }
    position.y = std::clamp(position.y, minimum_y, maximum_y);

    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(ImVec2(kPanelSize.x * scale, kPanelSize.y * scale));
    if (g_picker.just_opened) ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 15.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.045f, 0.047f, 0.060f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.29f, 0.82f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoFocusOnAppearing |
                                   (g_picker.requested ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoInputs);
    if (ImGui::Begin("##AnimatedColorPicker", nullptr, flags)) {
        const ImVec2 panel_minimum = ImGui::GetWindowPos();
        const ImVec2 panel_maximum(
            panel_minimum.x + ImGui::GetWindowSize().x,
            panel_minimum.y + ImGui::GetWindowSize().y);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        DrawGlassPanel(draw, panel_minimum, panel_maximum, 14.0f);
        if (g_picker.colors != nullptr) {
            DrawStateTabs(g_picker);
            const ImVec2 tabs_min = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(tabs_min.x, tabs_min.y + 10.0f));
        }
        const ImVec2 square_min = ImGui::GetCursorScreenPos();
        const float square_size = ImGui::GetContentRegionAvail().x;
        DrawSvPicker(g_picker, draw, square_min, square_size);

        ImGui::SetCursorScreenPos(ImVec2(square_min.x, square_min.y + square_size + 15.0f));
        const ImVec2 hue_min = ImGui::GetCursorScreenPos();
        DrawHueBar(g_picker, draw, hue_min, ImVec2(square_size, 12.0f));

        ImGui::SetCursorScreenPos(ImVec2(square_min.x, hue_min.y + 25.0f));
        const ImVec2 alpha_min = ImGui::GetCursorScreenPos();
        DrawAlphaBar(g_picker, draw, alpha_min, ImVec2(square_size, 12.0f));

        ImGui::SetCursorScreenPos(ImVec2(square_min.x, alpha_min.y + 26.0f));
        DrawNumericValues(g_picker);

        if (g_picker.colors != nullptr) {
            ImGui::SetCursorScreenPos(ImVec2(square_min.x, alpha_min.y + 61.0f));
            draw->AddLine(ImGui::GetCursorScreenPos(),
                          ImVec2(square_min.x + square_size, ImGui::GetCursorScreenPos().y),
                          ImGui::GetColorU32(ImVec4(0.18f, 0.19f, 0.23f, 0.55f)), 1.0f);
            ImGui::SetCursorScreenPos(ImVec2(square_min.x, alpha_min.y + 68.0f));
            DrawStateToggle("BehindWall", "墙后颜色", &g_picker.colors->behind_wall_enabled,
                            g_picker.colors->behind_wall);
            DrawStateToggle("InView", "视野内颜色", &g_picker.colors->in_view_enabled,
                            g_picker.colors->in_view);
        }

        const bool mouse_inside = ImGui::IsMouseHoveringRect(
            panel_minimum, panel_maximum, false);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mouse_inside && !g_picker.just_opened) {
            g_picker.requested = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    g_picker.just_opened = false;
}

}  // namespace pztrainer::ui
