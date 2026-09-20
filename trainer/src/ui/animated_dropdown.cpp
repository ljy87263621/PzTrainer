#include "ui/animated_dropdown.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

#include "settings/ui_preferences.hpp"
#include "ui/animation.hpp"
#include "ui/components.hpp"
#include "ui/controls/action_controls.hpp"

namespace pztrainer::ui {
namespace {

struct DropdownState {
    bool requested = false;
    bool just_opened = false;
    float amount = 0.0f;
    float velocity = 0.0f;
    int last_frame = -1;
};

std::unordered_map<ImGuiID, DropdownState> g_states;
DropdownState* g_current_state = nullptr;
bool g_current_disabled = false;

float U(float value) {
    return value * settings::UiScale();
}

DropdownState& State(ImGuiID id) {
    return g_states[id];
}

void DrawChevron(ImDrawList* draw, const ImVec2& center, float amount) {
    const float direction = 1.0f - animation::Clamp01(amount) * 2.0f;
    const ImU32 color = ImGui::GetColorU32(components::kMuted);
    draw->AddLine(
        ImVec2(center.x - U(4.0f), center.y - U(2.0f) * direction),
        ImVec2(center.x, center.y + U(2.0f) * direction),
        color, U(1.5f));
    draw->AddLine(
        ImVec2(center.x, center.y + U(2.0f) * direction),
        ImVec2(center.x + U(4.0f), center.y - U(2.0f) * direction),
        color, U(1.5f));
}

}  // namespace

ImGuiID AnimatedDropdownId(const char* id) {
    return ImGui::GetID(id);
}

void ToggleAnimatedDropdown(ImGuiID id) {
    DropdownState& state = State(id);
    if (state.requested) {
        state.requested = false;
        return;
    }
    for (auto& entry : g_states) entry.second.requested = false;
    state.requested = true;
    state.just_opened = true;
}

bool IsAnimatedDropdownOpen(ImGuiID id) {
    return State(id).requested;
}

float AnimatedDropdownAmount(ImGuiID id) {
    return animation::Clamp01(State(id).amount);
}

bool BeginAnimatedDropdownPopup(ImGuiID id, const ImVec2& anchor_minimum,
                                const ImVec2& anchor_maximum,
                                const ImVec2& popup_size,
                                bool control_hovered) {
    DropdownState& state = State(id);
    const int frame = ImGui::GetFrameCount();
    if (state.last_frame >= 0 && frame - state.last_frame > 1) {
        state.requested = false;
        state.just_opened = false;
        state.amount = 0.0f;
        state.velocity = 0.0f;
    }
    state.last_frame = frame;
    animation::SpringValue(
        state.amount, state.velocity, state.requested ? 1.0f : 0.0f,
        state.requested ? 230.0f : 165.0f,
        state.requested ? 21.0f : 19.0f);
    const float amount = animation::Clamp01(state.amount);
    if (!state.requested && amount < 0.002f) {
        state.amount = 0.0f;
        state.velocity = 0.0f;
        state.just_opened = false;
        return false;
    }

    const float full_width = popup_size.x > 0.0f
        ? popup_size.x : anchor_maximum.x - anchor_minimum.x;
    const float full_height = std::max(U(1.0f), popup_size.y);
    const float animated_height = std::max(U(1.0f), full_height * amount);
    ImVec2 position(
        anchor_minimum.x,
        anchor_maximum.y + U(5.0f) - (1.0f - amount) * U(7.0f));
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 work_minimum = viewport->WorkPos;
    const ImVec2 work_maximum(
        viewport->WorkPos.x + viewport->WorkSize.x,
        viewport->WorkPos.y + viewport->WorkSize.y);
    position.x = std::clamp(
        position.x, work_minimum.x + U(4.0f),
        std::max(work_minimum.x + U(4.0f),
                 work_maximum.x - full_width - U(4.0f)));
    if (anchor_maximum.y + U(5.0f) + full_height > work_maximum.y &&
        anchor_minimum.y - U(5.0f) - full_height >= work_minimum.y) {
        position.y = anchor_minimum.y - U(5.0f) - animated_height;
    }
    position.y = std::clamp(
        position.y, work_minimum.y + U(4.0f),
        std::max(work_minimum.y + U(4.0f),
                 work_maximum.y - animated_height - U(4.0f)));

    char window_name[64]{};
    std::snprintf(window_name, sizeof(window_name),
                  "##AnimatedDropdown_%08X", id);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(full_width, animated_height), ImGuiCond_Always);
    if (state.just_opened) ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * amount);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(8.0f), U(8.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.2f));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.035f, 0.038f, 0.050f, 0.88f));
    ImGui::PushStyleColor(
        ImGuiCol_Border, ImVec4(0.24f, 0.49f, 0.92f, 0.74f));
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin(window_name, nullptr, flags);

    g_current_state = &state;
    g_current_disabled = !state.requested;
    if (g_current_disabled) ImGui::BeginDisabled();

    const bool window_hovered = ImGui::IsWindowHovered(
        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !control_hovered && !window_hovered && !state.just_opened) {
        state.requested = false;
    }
    if (state.requested && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state.requested = false;
    }
    state.just_opened = false;
    return true;
}

bool BeginAnimatedCombo(const char* id, const char* preview,
                        float popup_height, float popup_width) {
    const ImGuiID dropdown_id = AnimatedDropdownId(id);
    const float width = ImGui::CalcItemWidth();
    const float height = ImGui::GetFrameHeight();
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const ImVec2 maximum(minimum.x + width, minimum.y + height);
    ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) ToggleAnimatedDropdown(dropdown_id);

    const bool requested = IsAnimatedDropdownOpen(dropdown_id);
    const float hover = animation::Clamp01(animation::Spring(
        dropdown_id ^ static_cast<ImGuiID>(0xA7C31D5Bu),
        hovered || requested ? 1.0f : 0.0f, 250.0f, 22.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(
            0.055f + hover * 0.018f,
            0.058f + hover * 0.020f,
            0.075f + hover * 0.030f, 1.0f)), U(7.0f));
    draw->AddRect(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(
            0.24f, 0.49f, 0.92f, 0.38f + hover * 0.42f)),
        U(7.0f), 0, U(requested ? 1.4f : 1.0f));
    draw->AddText(
        ImVec2(minimum.x + U(11.0f),
               minimum.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
        ImGui::GetColorU32(components::kText), preview);
    DrawChevron(
        draw, ImVec2(maximum.x - U(15.0f), minimum.y + height * 0.5f),
        requested ? std::max(AnimatedDropdownAmount(dropdown_id), 0.01f)
                  : AnimatedDropdownAmount(dropdown_id));

    return BeginAnimatedDropdownPopup(
        dropdown_id, minimum, maximum,
        ImVec2(popup_width > 0.0f ? popup_width : width, popup_height),
        hovered);
}

void CloseAnimatedDropdown() {
    if (g_current_state != nullptr) g_current_state->requested = false;
}

void EndAnimatedDropdown() {
    if (g_current_state == nullptr) return;
    controls::ScrollRail();
    if (g_current_disabled) ImGui::EndDisabled();
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    g_current_state = nullptr;
    g_current_disabled = false;
}

}  // namespace pztrainer::ui
