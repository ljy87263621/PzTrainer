#include "ui/model_effect_controls.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <iterator>

#include "ui/animation.hpp"
#include "ui/color_picker.hpp"
#include "ui/components.hpp"
#include "settings/localization.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/glass_blur.hpp"

namespace pztrainer::ui {
namespace {

using features::visual::ZombieModelEffect;
using features::visual::ZombieVisualState;

constexpr ZombieModelEffect kEffects[]{
    ZombieModelEffect::Disabled,
    ZombieModelEffect::Shaded,
    ZombieModelEffect::Solid,
    ZombieModelEffect::Glow,
    ZombieModelEffect::GlowOutline,
    ZombieModelEffect::Iridescent,
    ZombieModelEffect::WaterFlow,
    ZombieModelEffect::Glossy,
};

template <typename Settings>
struct AdvancedPopupState {
    bool requested = false;
    bool just_opened = false;
    float amount = 0.0f;
    float velocity = 0.0f;
};

template <typename Settings>
AdvancedPopupState<Settings>& GetAdvancedPopupState() {
    static AdvancedPopupState<Settings> state;
    return state;
}

void DrawChevron(ImDrawList* draw, const ImVec2& center, ImU32 color, bool open) {
    const float direction = open ? -1.0f : 1.0f;
    draw->AddLine(ImVec2(center.x - 5.0f, center.y - 2.5f * direction),
                  ImVec2(center.x, center.y + 2.5f * direction), color, 1.7f);
    draw->AddLine(ImVec2(center.x, center.y + 2.5f * direction),
                  ImVec2(center.x + 5.0f, center.y - 2.5f * direction), color, 1.7f);
}

template <typename Settings>
void DrawModelAdvancedPopup(Settings* settings, const ImVec2& anchor,
                            const char* popup_id) {
    AdvancedPopupState<Settings>& state = GetAdvancedPopupState<Settings>();
    animation::SpringValue(
        state.amount, state.velocity, state.requested ? 1.0f : 0.0f,
        245.0f, 19.0f);
    const float alpha = animation::Clamp01(state.amount);
    if (!state.requested && alpha < 0.002f) return;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float font_size = ImGui::GetFontSize();
    const float title_size = font_size * 1.32f;
    const float title_width = ImGui::CalcTextSize(
        settings::Translate("模型高级设置")).x * 1.32f;
    const float longest_label = std::max(
        ImGui::CalcTextSize(settings::Translate("边缘发光")).x,
        ImGui::CalcTextSize(settings::Translate("强制显示上色模型")).x);
    const ImVec2 outer_padding(
        style.FramePadding.x + style.ItemSpacing.x,
        style.FramePadding.y + style.ItemSpacing.y);
    const ImVec2 inner_padding = style.FramePadding;
    const float right_controls_width = font_size * 4.8f;
    const float inner_width = std::ceil(std::max(
        title_width,
        longest_label + right_controls_width + style.ItemInnerSpacing.x * 3.0f +
            inner_padding.x * 2.0f));
    const float panel_width = inner_width + outer_padding.x * 2.0f;
    const ImVec2 position(
        anchor.x - panel_width,
        anchor.y + (1.0f - alpha) * style.ItemSpacing.y);
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowContentSize(ImVec2(inner_width, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, font_size * 0.9f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, outer_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.020f, 0.028f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.20f, 0.27f, 0.66f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
        (state.requested ? ImGuiWindowFlags_None
                                    : ImGuiWindowFlags_NoInputs);
    ImGui::PushID(popup_id);
    if (ImGui::Begin("##ModelAdvancedPopup", nullptr, flags)) {
        const ImVec2 panel_minimum = ImGui::GetWindowPos();
        const ImVec2 panel_maximum(
            panel_minimum.x + ImGui::GetWindowSize().x,
            panel_minimum.y + ImGui::GetWindowSize().y);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        DrawGlassPanel(draw, panel_minimum, panel_maximum, font_size * 0.9f);
        const ImVec2 title_position = ImGui::GetCursorScreenPos();
        draw->AddText(ImGui::GetFont(), title_size, title_position,
                      ImGui::GetColorU32(components::kText),
                      settings::Translate("模型高级设置"));
        ImGui::Dummy(ImVec2(inner_width, title_size + style.ItemSpacing.y));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.14f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, font_size * 0.7f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, inner_padding);
        ImGui::BeginChild(
            "AdvancedSettingsGroup", ImVec2(inner_width, 0.0f),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding |
                ImGuiChildFlags_AutoResizeY,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        components::ToggleRow(
            "边缘发光", &settings->model_edge_glow,
            &settings->model_edge_glow_color, false);
        components::ToggleRow(
            "强制显示上色模型", &settings->force_model_visibility,
            static_cast<features::visual::StateColor*>(nullptr), false);
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseHoveringRect(panel_minimum, panel_maximum, false) &&
            !state.just_opened) {
            state.requested = false;
        }
    }
    ImGui::End();
    ImGui::PopID();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    state.just_opened = false;
}

}  // namespace

template <typename Settings>
bool DrawModelEffectDropdownImpl(Settings* settings, const char* popup_id) {
    ZombieModelEffect* effect = &settings->model_effect;
    features::visual::StateColor* colors = &settings->model_colors;
    ImGui::PushID(popup_id);
    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    const float row_width = ImGui::GetContentRegionAvail().x;
    const float control_width = 142.0f;
    const ImVec2 control_min(
        row_start.x + row_width - control_width, row_start.y + 4.0f);
    const ImVec2 control_max(
        row_start.x + row_width, control_min.y + 34.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const char* effect_label = settings::Translate("上色效果");
    const ImVec2 label_size = ImGui::CalcTextSize(effect_label);
    draw->AddText(ImVec2(
                      row_start.x + 13.0f,
                      row_start.y + (42.0f - label_size.y) * 0.5f),
                  ImGui::GetColorU32(components::kText), effect_label);

    ImGui::SetCursorScreenPos(ImVec2(control_min.x - 68.0f, row_start.y + 6.0f));
    if (components::MoreOptionsButton("ModelAdvanced", ImVec2(30.0f, 30.0f))) {
        AdvancedPopupState<Settings>& state = GetAdvancedPopupState<Settings>();
        state.requested = !state.requested;
        if (state.requested) {
            state.just_opened = true;
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(control_min.x - 34.0f, row_start.y + 8.0f));
    DrawColorSwatch("ModelEffectColor", colors, ImVec2(26.0f, 26.0f));
    ImGui::SetCursorScreenPos(control_min);
    ImGui::InvisibleButton("EffectSelector", ImVec2(control_width, 34.0f));
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiID dropdown_id = AnimatedDropdownId("EffectPopup");
    if (ImGui::IsItemClicked()) ToggleAnimatedDropdown(dropdown_id);
    const bool open = IsAnimatedDropdownOpen(dropdown_id);
    const float hover = animation::Clamp01(animation::Spring(
        ImGui::GetID("EffectSelectorHover"), hovered || open ? 1.0f : 0.0f,
        250.0f, 22.0f));
    draw->AddRectFilled(control_min, control_max,
                        ImGui::GetColorU32(ImVec4(0.055f + hover * 0.018f,
                                                  0.058f + hover * 0.020f,
                                                  0.075f + hover * 0.030f, 1.0f)), 8.0f);
    draw->AddRect(control_min, control_max,
                  ImGui::GetColorU32(ImVec4(0.18f, 0.20f, 0.27f, 0.55f + hover * 0.30f)),
                  8.0f, 0, 1.0f);
    const ZombieModelEffect displayed = *effect;
    const ImVec2 effect_label_size = ImGui::CalcTextSize(
        features::visual::ZombieModelEffectLabel(displayed));
    draw->AddText(ImVec2(
                      control_min.x + 13.0f,
                      control_min.y + (34.0f - effect_label_size.y) * 0.5f),
                  ImGui::GetColorU32(components::kText),
                  features::visual::ZombieModelEffectLabel(displayed));
    DrawChevron(draw, ImVec2(control_max.x - 17.0f, control_min.y + 17.0f),
                ImGui::GetColorU32(components::kMuted), open);

    bool changed = false;
    if (BeginAnimatedDropdownPopup(
            dropdown_id, control_min, control_max,
            ImVec2(control_width, 292.0f), hovered)) {
        const ImVec2 popup_minimum = ImGui::GetWindowPos();
        const ImVec2 popup_size = ImGui::GetWindowSize();
        const ImVec2 popup_maximum(
            popup_minimum.x + popup_size.x, popup_minimum.y + popup_size.y);
        DrawGlassPanel(ImGui::GetWindowDrawList(), popup_minimum, popup_maximum, 12.0f);
        for (int index = 0; index < static_cast<int>(std::size(kEffects)); ++index) {
            ImGui::PushID(index);
            const ZombieModelEffect option = kEffects[index];
            const bool selected = option == displayed;
            const ImVec2 item_start = ImGui::GetCursorScreenPos();
            const float item_width = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("EffectItem", ImVec2(item_width, 34.0f));
            const bool item_hovered = ImGui::IsItemHovered();
            const float item_amount = animation::Clamp01(animation::Spring(
                ImGui::GetID("EffectItemAmount"), selected ? 1.0f : (item_hovered ? 0.45f : 0.0f),
                260.0f, 21.0f));
            if (item_amount > 0.001f) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    item_start, ImVec2(item_start.x + item_width, item_start.y + 34.0f),
                    ImGui::GetColorU32(ImVec4(0.13f, 0.36f, 0.95f, 0.16f * item_amount)), 7.0f);
            }
            if (selected) {
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(item_start.x + 11.0f, item_start.y + 17.0f), 3.2f,
                    ImGui::GetColorU32(components::kAccent));
            }
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(item_start.x + 22.0f, item_start.y + 8.0f),
                ImGui::GetColorU32(selected ? components::kText : components::kMuted),
                features::visual::ZombieModelEffectLabel(option));
            if (ImGui::IsItemClicked()) {
                *effect = option;
                changed = true;
                CloseAnimatedDropdown();
            }
            ImGui::PopID();
        }
        EndAnimatedDropdown();
    }
    DrawModelAdvancedPopup(
        settings, ImVec2(control_max.x, row_start.y + 47.0f), popup_id);
    ImGui::SetCursorScreenPos(ImVec2(row_start.x, row_start.y + 48.0f));
    ImGui::Dummy(ImVec2(row_width, 1.0f));
    ImGui::PopID();
    return changed;
}

bool DrawModelEffectDropdown(features::visual::VisualSettings* settings) {
    return DrawModelEffectDropdownImpl(settings, "ZombieModelEffect");
}

bool DrawPlayerModelEffectDropdown(
        features::visual::PlayerVisualSettings* settings) {
    return DrawModelEffectDropdownImpl(settings, "PlayerModelEffect");
}

bool DrawAnimalModelEffectDropdown(
        features::visual::AnimalVisualSettings* settings) {
    return DrawModelEffectDropdownImpl(settings, "AnimalModelEffect");
}

bool DrawVehicleModelEffectDropdown(
        features::visual::VehicleVisualSettings* settings) {
    return DrawModelEffectDropdownImpl(settings, "VehicleModelEffect");
}

bool DrawPreviewStateSelector(ZombieVisualState* state) {
    ImGui::PushID("PreviewStateSelector");
    constexpr const char* labels[]{"默认", "墙后", "视野内"};
    constexpr ZombieVisualState states[]{
        ZombieVisualState::Default, ZombieVisualState::BehindWall, ZombieVisualState::InView};
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float available = ImGui::GetContentRegionAvail().x;
    const float gap = 5.0f;
    constexpr float horizontal_padding = 14.0f;
    const char* translated_labels[3]{};
    float text_widths[3]{};
    float total_text_width = 0.0f;
    for (int index = 0; index < 3; ++index) {
        translated_labels[index] = settings::Translate(labels[index]);
        text_widths[index] = ImGui::CalcTextSize(translated_labels[index]).x;
        total_text_width += text_widths[index];
    }
    const float available_text_width = std::max(
        1.0f, available - gap * 2.0f - horizontal_padding * 3.0f);
    const float text_scale = total_text_width > available_text_width
        ? available_text_width / total_text_width
        : 1.0f;
    const float used_width = total_text_width * text_scale +
        horizontal_padding * 3.0f + gap * 2.0f;
    const float extra_width = std::max(0.0f, available - used_width) / 3.0f;
    bool changed = false;
    for (int index = 0; index < 3; ++index) {
        ImGui::PushID(index);
        const float width = text_widths[index] * text_scale +
            horizontal_padding + extra_width;
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("PreviewState", ImVec2(width, 32.0f));
        const bool selected = *state == states[index];
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            *state = states[index];
            changed = true;
        }
        const float amount = animation::Clamp01(animation::Spring(
            ImGui::GetID("PreviewStateAmount"), selected ? 1.0f : (hovered ? 0.4f : 0.0f),
            270.0f, 21.0f));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 maximum(minimum.x + width, minimum.y + 32.0f);
        draw->AddRectFilled(minimum, maximum,
                            ImGui::GetColorU32(ImVec4(0.06f + amount * 0.03f,
                                                      0.065f + amount * 0.07f,
                                                      0.085f + amount * 0.20f, 1.0f)), 7.0f);
        if (selected) {
            draw->AddRect(minimum, maximum, ImGui::GetColorU32(components::kAccent),
                          7.0f, 0, 1.0f);
        }
        const float font_size = ImGui::GetFontSize() * text_scale;
        const ImVec2 text_size(
            text_widths[index] * text_scale, font_size);
        draw->AddText(
            ImGui::GetFont(), font_size,
            ImVec2(minimum.x + (width - text_size.x) * 0.5f,
                   minimum.y + (32.0f - text_size.y) * 0.5f),
            ImGui::GetColorU32(
                selected ? components::kText : components::kMuted),
            translated_labels[index]);
        ImGui::PopID();
        if (index != 2) ImGui::SameLine(0.0f, gap);
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + 38.0f));
    ImGui::Dummy(ImVec2(available, 1.0f));
    ImGui::PopID();
    return changed;
}

}  // namespace pztrainer::ui
