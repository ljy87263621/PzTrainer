#include "ui/controls/action_controls.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>

#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animation.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui::controls {
namespace {
float U(float value) { return value * settings::UiScale(); }

bool DrawAction(const char* label, bool selected, bool choice) {
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x, U(choice ? 32.0f : 36.0f));
    const bool clicked = ImGui::InvisibleButton("action", size);
    const float hover = animation::Clamp01(animation::Spring(
        ImGui::GetID("hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 260.0f, 24.0f));
    const float press = animation::Clamp01(animation::Spring(
        ImGui::GetID("press"), ImGui::IsItemActive() ? 1.0f : 0.0f, 300.0f, 22.0f));
    const float selection = animation::Clamp01(animation::Spring(
        ImGui::GetID("selection"), selected ? 1.0f : 0.0f));
    const ImVec2 minimum(start.x + U(press), start.y + U(press));
    const ImVec2 maximum(start.x + size.x - U(press), start.y + size.y - U(press));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec4 fill = animation::Lerp(ImVec4(0.045f, 0.054f, 0.074f, choice ? 0.0f : 0.8f),
        ImVec4(0.12f, 0.26f, 0.46f, 0.7f), std::max(selection, hover * 0.65f));
    draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(fill), U(7.0f));
    if (!choice) draw->AddRect(minimum, maximum,
        ImGui::GetColorU32(ImVec4(0.24f, 0.49f, 0.92f, 0.25f + hover * 0.35f + selection * 0.25f)),
        U(7.0f), 0, U(1.0f));
    draw->PushClipRect(minimum, ImVec2(maximum.x - U(9.0f), maximum.y), true);
    draw->AddText(ImVec2(minimum.x + U(12.0f) + hover * U(2.0f),
        start.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f),
        ImGui::GetColorU32(animation::Lerp(components::kText, ImVec4(1, 1, 1, 1), hover)),
        settings::Translate(label));
    draw->PopClipRect();
    if (ImGui::IsItemHovered() && ImGui::CalcTextSize(settings::Translate(label)).x > size.x - U(24.0f))
        components::RoundedTooltip(label);
    ImGui::PopID();
    return clicked;
}
}  // namespace

bool ActionButton(const char* label, bool selected) { return DrawAction(label, selected, false); }
bool ChoiceRow(const char* label) { return DrawAction(label, false, true); }

bool TextField(const char* label, char* value, std::size_t capacity, const char* hint) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(settings::Translate(label));
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetTextLineHeight() + U(16.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(0.035f, 0.042f, 0.06f, 0.9f)), U(7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(U(11.0f), U(8.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(7.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::SetNextItemWidth(width);
    // ImGui supplies text editing/IME only; the field chrome is drawn here.
    const bool changed = ImGui::InputTextWithHint("##text", settings::Translate(hint), value, capacity);
    const float amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("focus"), ImGui::IsItemActive() || ImGui::IsItemHovered() ? 1.0f : 0.0f));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    draw->AddRect(start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(0.24f, 0.49f, 0.92f, 0.22f + amount * 0.55f)), U(7.0f), 0, U(1.0f));
    ImGui::PopID();
    return changed;
}

bool IntegerRow(const char* label, int* value, int minimum, int maximum) {
    float next = static_cast<float>(*value);
    if (!components::StepperRow(label, &next, static_cast<float>(minimum),
            static_cast<float>(maximum), 1.0f, "%.0f", false)) return false;
    *value = std::clamp(static_cast<int>(std::round(next)), minimum, maximum);
    return true;
}

bool IntegerField(const char* id, int* value, int minimum, int maximum, float width) {
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(0.035f, 0.042f, 0.06f, 0.9f)), U(7));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::SetNextItemWidth(width);
    const bool changed = ImGui::InputInt("##value", value, 0, 0);
    const float amount = animation::Clamp01(animation::Spring(ImGui::GetID("focus"),
        ImGui::IsItemActive() || ImGui::IsItemHovered() ? 1.0f : 0.0f));
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    draw->AddRect(start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(0.24f, 0.49f, 0.92f, 0.22f + amount * 0.55f)), U(7), 0, U(1));
    if (changed) *value = std::clamp(*value, minimum, maximum);
    ImGui::PopID();
    return changed;
}

void Hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextWrapped("%s", settings::Translate(text));
    ImGui::PopStyleColor();
}

void ScrollRail() {
    const float maximum_scroll = ImGui::GetScrollMaxY();
    if (maximum_scroll <= 0) return;
    const ImVec2 window = ImGui::GetWindowPos();
    const float height = ImGui::GetWindowHeight() - U(16);
    const ImVec2 start(window.x + ImGui::GetWindowWidth() - U(7), window.y + U(8));
    const float thumb_height = std::min(height, std::max(U(24), height * height / (height + maximum_scroll)));
    const float travel = height - thumb_height;
    if (travel <= 0) return;
    float top = start.y + travel * ImGui::GetScrollY() / maximum_scroll;
    const ImRect bounds(start, ImVec2(start.x + U(6), start.y + height));
    const ImGuiID id = ImGui::GetID("##scroll_rail");
    if (!ImGui::ItemAdd(bounds, id)) return;
    bool hovered = false, held = false;
    ImGui::ButtonBehavior(bounds, id, &hovered, &held);
    const ImGuiID offset_id = ImGui::GetID("scroll_grab");
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const float mouse = ImGui::GetIO().MousePos.y;
        ImGui::GetStateStorage()->SetFloat(offset_id,
            mouse >= top && mouse <= top + thumb_height ? mouse - top : thumb_height * 0.5f);
    }
    if (held) {
        const float amount = std::clamp((ImGui::GetIO().MousePos.y - start.y -
            ImGui::GetStateStorage()->GetFloat(offset_id)) / travel, 0.0f, 1.0f);
        ImGui::SetScrollY(amount * maximum_scroll);
        top = start.y + amount * travel;
    }
    const float hover = animation::Clamp01(animation::Spring(ImGui::GetID("scroll_hover"),
        hovered || held ? 1.0f : 0.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(start.x, top),
        ImVec2(start.x + U(3 + 2 * hover), top + thumb_height),
        ImGui::GetColorU32(ImVec4(0.24f, 0.49f, 0.92f, 0.28f + hover * 0.5f)), U(3));
}
}  // namespace pztrainer::ui::controls
