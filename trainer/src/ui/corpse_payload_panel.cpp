#include "ui/corpse_payload_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animation.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

using namespace components;

struct PayloadSelection {
    bridge::ItemCatalogEntry item;
    int quantity = 1;
    bool removing = false;
    float reveal = 0.0f;
    float reveal_velocity = 0.0f;
};

enum class DockSide {
    Detached,
    Left,
    Right,
};

std::vector<PayloadSelection> g_selections;
DockSide g_dock_side = DockSide::Right;
bool g_hidden = false;
bool g_position_initialized = false;
ImVec2 g_position{};
ImVec2 g_position_velocity{};
ImVec2 g_target_position{};
float g_panel_reveal = 0.0f;
float g_panel_reveal_velocity = 0.0f;
float g_handle_reveal = 0.0f;
float g_handle_reveal_velocity = 0.0f;

const char* T(const char* text) {
    return settings::Translate(text);
}

float U(float value) {
    return value * settings::UiScale();
}

std::size_t ActiveCount() {
    return static_cast<std::size_t>(std::count_if(
        g_selections.begin(), g_selections.end(),
        [](const PayloadSelection& selection) { return !selection.removing; }));
}

void AdvanceSelectionAnimations() {
    for (PayloadSelection& selection : g_selections) {
        animation::SpringValue(
            selection.reveal, selection.reveal_velocity,
            selection.removing ? 0.0f : 1.0f, 280.0f, 22.0f);
    }
    g_selections.erase(
        std::remove_if(
            g_selections.begin(), g_selections.end(),
            [](const PayloadSelection& selection) {
                return selection.removing && selection.reveal < 0.002f;
            }),
        g_selections.end());
}

ImVec2 PanelSize() {
    const ImGuiIO& io = ImGui::GetIO();
    const float width = std::min(U(360.0f), io.DisplaySize.x * 0.46f);
    const float desired_height = U(142.0f) +
        static_cast<float>(g_selections.size()) * U(88.0f);
    const float height = std::clamp(
        desired_height, U(230.0f),
        std::max(U(230.0f), io.DisplaySize.y - U(32.0f)));
    return ImVec2(width, std::min(height, U(620.0f)));
}

ImVec2 DockPosition(DockSide side, const ImVec2& host_minimum,
                    const ImVec2& host_maximum, const ImVec2& panel_size) {
    const float gap = U(12.0f);
    const float y = host_minimum.y + U(74.0f);
    if (side == DockSide::Left) {
        return ImVec2(host_minimum.x - panel_size.x - gap, y);
    }
    return ImVec2(host_maximum.x + gap, y);
}

void ClampDetachedTarget(const ImVec2& panel_size) {
    const ImGuiIO& io = ImGui::GetIO();
    const float margin = U(8.0f);
    g_target_position.x = std::clamp(
        g_target_position.x, margin,
        std::max(margin, io.DisplaySize.x - panel_size.x - margin));
    g_target_position.y = std::clamp(
        g_target_position.y, margin,
        std::max(margin, io.DisplaySize.y - panel_size.y - margin));
}

void UpdatePanelTarget(const ImVec2& host_minimum,
                       const ImVec2& host_maximum,
                       const ImVec2& panel_size) {
    const ImGuiIO& io = ImGui::GetIO();
    if (!g_position_initialized) {
        const ImVec2 right = DockPosition(
            DockSide::Right, host_minimum, host_maximum, panel_size);
        g_dock_side = right.x + panel_size.x <= io.DisplaySize.x - U(8.0f)
            ? DockSide::Right : DockSide::Left;
        g_target_position = DockPosition(
            g_dock_side, host_minimum, host_maximum, panel_size);
        ClampDetachedTarget(panel_size);
        g_position = g_target_position;
        g_position_initialized = true;
    } else if (g_dock_side != DockSide::Detached) {
        g_target_position = DockPosition(
            g_dock_side, host_minimum, host_maximum, panel_size);
        ClampDetachedTarget(panel_size);
    } else {
        ClampDetachedTarget(panel_size);
    }

    animation::SpringValue(
        g_position.x, g_position_velocity.x, g_target_position.x,
        250.0f, 23.0f);
    animation::SpringValue(
        g_position.y, g_position_velocity.y, g_target_position.y,
        250.0f, 23.0f);
}

void TrySnapToHost(const ImVec2& host_minimum,
                   const ImVec2& host_maximum,
                   const ImVec2& panel_size) {
    const ImVec2 right = DockPosition(
        DockSide::Right, host_minimum, host_maximum, panel_size);
    const ImVec2 left = DockPosition(
        DockSide::Left, host_minimum, host_maximum, panel_size);
    const float threshold = U(52.0f);
    if (std::abs(g_target_position.x - right.x) <= threshold) {
        g_dock_side = DockSide::Right;
        g_target_position = right;
    } else if (std::abs(g_target_position.x - left.x) <= threshold) {
        g_dock_side = DockSide::Left;
        g_target_position = left;
    }
}

void DrawPayloadRow(PayloadSelection& selection) {
    const float reveal = animation::Clamp01(selection.reveal);
    const float full_height = U(80.0f);
    const float row_height = std::max(U(1.0f), full_height * reveal);
    const ImVec2 row_minimum = ImGui::GetCursorScreenPos();
    const ImVec2 row_maximum(
        row_minimum.x + ImGui::GetContentRegionAvail().x,
        row_minimum.y + row_height);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        row_minimum, row_maximum,
        ImGui::GetColorU32(ImVec4(0.055f, 0.061f, 0.080f, 0.82f * reveal)),
        U(8.0f));
    draw->AddRect(
        row_minimum, row_maximum,
        ImGui::GetColorU32(ImVec4(
            kAccent.x, kAccent.y, kAccent.z, 0.16f * reveal)),
        U(8.0f));

    if (reveal > 0.04f) {
        const float image_size = U(48.0f);
        const ImVec2 image_minimum(
            row_minimum.x + U(9.0f),
            row_minimum.y + (full_height - image_size) * 0.5f);
        if (selection.item.texture_id != 0) {
            draw->AddImage(
                ImTextureRef(static_cast<ImTextureID>(selection.item.texture_id)),
                image_minimum,
                ImVec2(image_minimum.x + image_size,
                       image_minimum.y + image_size),
                ImVec2(selection.item.uv_min_x, selection.item.uv_min_y),
                ImVec2(selection.item.uv_max_x, selection.item.uv_max_y),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, reveal)));
        }

        const float controls_width = U(176.0f);
        const float text_left = image_minimum.x + image_size + U(9.0f);
        const float text_right = row_maximum.x - controls_width - U(8.0f);
        draw->PushClipRect(
            ImVec2(text_left, row_minimum.y),
            ImVec2(text_right, row_maximum.y), true);
        draw->AddText(
            ImVec2(text_left, row_minimum.y + U(18.0f)),
            ImGui::GetColorU32(ImVec4(kText.x, kText.y, kText.z, reveal)),
            selection.item.display_name.c_str());
        draw->AddText(
            ImVec2(text_left, row_minimum.y + U(43.0f)),
            ImGui::GetColorU32(ImVec4(kMuted.x, kMuted.y, kMuted.z, reveal)),
            selection.item.full_type.c_str());
        draw->PopClipRect();

        const float controls_left = row_maximum.x - controls_width;
        const float quantity_width = U(104.0f);
        const char* quantity_label = T("数量");
        const ImVec2 quantity_label_size = ImGui::CalcTextSize(quantity_label);
        draw->AddText(
            ImVec2(
                controls_left + (quantity_width - quantity_label_size.x) * 0.5f,
                row_minimum.y + U(8.0f)),
            ImGui::GetColorU32(ImVec4(kMuted.x, kMuted.y, kMuted.z, reveal)),
            quantity_label);

        const float original_alpha = ImGui::GetStyle().Alpha;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, original_alpha * reveal);
        ImGui::SetCursorScreenPos(ImVec2(
            controls_left, row_minimum.y + U(32.0f)));
        ImGui::PushID(selection.item.full_type.c_str());
        if (ImGui::Button("-", ImVec2(U(26.0f), 0.0f))) {
            selection.quantity = std::max(1, selection.quantity - 1);
        }
        ImGui::SameLine(0.0f, U(4.0f));
        ImGui::SetNextItemWidth(U(44.0f));
        if (ImGui::InputInt("##PayloadQuantity", &selection.quantity, 0, 0)) {
            selection.quantity = std::clamp(selection.quantity, 1, 100);
        }
        ImGui::SameLine(0.0f, U(4.0f));
        if (ImGui::Button("+", ImVec2(U(26.0f), 0.0f))) {
            selection.quantity = std::min(100, selection.quantity + 1);
        }
        ImGui::SameLine(0.0f, U(6.0f));
        if (ImGui::Button(T("取消"), ImVec2(U(58.0f), 0.0f))) {
            selection.removing = true;
        }
        ImGui::PopID();
        ImGui::PopStyleVar();
    }

    ImGui::SetCursorScreenPos(row_minimum);
    ImGui::Dummy(ImVec2(
        row_maximum.x - row_minimum.x,
        row_height + U(7.0f) * reveal));
}

void DrawHiddenHandle(const ImVec2& host_minimum,
                      const ImVec2& host_maximum, bool enabled) {
    const bool target_visible = enabled && g_hidden && ActiveCount() > 0;
    animation::SpringValue(
        g_handle_reveal, g_handle_reveal_velocity,
        target_visible ? 1.0f : 0.0f, 300.0f, 23.0f);
    const float reveal = animation::Clamp01(g_handle_reveal);
    if (reveal < 0.003f) return;

    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 size(U(38.0f), U(74.0f));
    float x = host_maximum.x + U(7.0f);
    if (x + size.x > io.DisplaySize.x - U(5.0f)) {
        x = host_minimum.x - size.x - U(7.0f);
    }
    const ImVec2 position(
        x + (1.0f - reveal) * U(18.0f),
        host_minimum.y + U(112.0f));
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(U(4.0f), U(4.0f)));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.025f, 0.029f, 0.040f, 0.94f));
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##CorpsePayloadHandle", nullptr, flags)) {
        ImGui::PushStyleColor(
            ImGuiCol_Button, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.30f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
        if (ImGui::Button(">", ImGui::GetContentRegionAvail())) {
            g_hidden = false;
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

}  // namespace

bool IsCorpsePayloadItemSelected(const std::string& full_type) {
    const auto found = std::find_if(
        g_selections.begin(), g_selections.end(),
        [&full_type](const PayloadSelection& selection) {
            return selection.item.full_type == full_type;
        });
    return found != g_selections.end() && !found->removing;
}

void ToggleCorpsePayloadItem(const bridge::ItemCatalogEntry& item) {
    const auto found = std::find_if(
        g_selections.begin(), g_selections.end(),
        [&item](const PayloadSelection& selection) {
            return selection.item.full_type == item.full_type;
        });
    if (found != g_selections.end()) {
        found->removing = !found->removing;
        return;
    }
    const bool was_empty = ActiveCount() == 0;
    PayloadSelection selection{};
    selection.item = item;
    selection.quantity = 1;
    g_selections.push_back(std::move(selection));
    if (was_empty) g_hidden = false;
}

bool HasCorpsePayloadSelection() {
    return ActiveCount() > 0;
}

std::size_t CorpsePayloadSelectionCount() {
    return ActiveCount();
}

std::vector<bridge::ItemSpawnBatchEntry> BuildCorpsePayloadBatch() {
    std::vector<bridge::ItemSpawnBatchEntry> result;
    result.reserve(ActiveCount());
    for (const PayloadSelection& selection : g_selections) {
        if (selection.removing) continue;
        result.push_back(bridge::ItemSpawnBatchEntry{
            selection.item.full_type,
            std::clamp(selection.quantity, 1, 100)});
    }
    return result;
}

void ClearCorpsePayloadSelection() {
    for (PayloadSelection& selection : g_selections) {
        selection.removing = true;
    }
}

void DrawCorpsePayloadPanel(const ImVec2& host_minimum,
                            const ImVec2& host_maximum, bool enabled) {
    AdvanceSelectionAnimations();
    const bool has_items = ActiveCount() > 0;
    const bool target_visible = enabled && has_items && !g_hidden;
    animation::SpringValue(
        g_panel_reveal, g_panel_reveal_velocity,
        target_visible ? 1.0f : 0.0f, 250.0f, 21.0f);
    DrawHiddenHandle(host_minimum, host_maximum, enabled);

    const float reveal = animation::Clamp01(g_panel_reveal);
    if (reveal < 0.003f && !target_visible) return;

    const ImVec2 panel_size = PanelSize();
    UpdatePanelTarget(host_minimum, host_maximum, panel_size);
    float slide_direction = 1.0f;
    if (g_dock_side == DockSide::Left) slide_direction = -1.0f;
    const ImVec2 animated_position(
        g_position.x + slide_direction * (1.0f - reveal) * U(28.0f),
        g_position.y + (1.0f - reveal) * U(8.0f));

    ImGui::SetNextWindowPos(animated_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panel_size, ImGuiCond_Always);
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(11.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.0f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(12.0f), U(11.0f)));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.018f, 0.021f, 0.030f, 0.94f));
    ImGui::PushStyleColor(
        ImGuiCol_Border, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.42f));
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##CorpsePayloadSelectionPanel", nullptr, flags)) {
        const float header_height = U(34.0f);
        const float button_width = U(57.0f);
        const float drag_width = std::max(
            U(96.0f), ImGui::GetContentRegionAvail().x - button_width * 2.0f - U(12.0f));
        const ImVec2 drag_minimum = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##PayloadPanelDrag", ImVec2(drag_width, header_height));
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(drag_minimum.x + U(3.0f), drag_minimum.y + U(7.0f)),
            ImGui::GetColorU32(kText), T("尸体载荷清单"));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const ImVec2 delta = ImGui::GetIO().MouseDelta;
            g_dock_side = DockSide::Detached;
            g_target_position.x += delta.x;
            g_target_position.y += delta.y;
            g_position.x += delta.x;
            g_position.y += delta.y;
            ClampDetachedTarget(panel_size);
        }
        if (ImGui::IsItemDeactivated()) {
            TrySnapToHost(host_minimum, host_maximum, panel_size);
        }

        ImGui::SameLine(0.0f, U(6.0f));
        const bool docked = g_dock_side != DockSide::Detached;
        if (ImGui::Button(
                T(docked ? "已吸附" : "吸附"),
                ImVec2(button_width, header_height))) {
            g_dock_side = DockSide::Right;
        }
        ImGui::SameLine(0.0f, U(6.0f));
        if (ImGui::Button(T("隐藏"), ImVec2(button_width, header_height))) {
            g_hidden = true;
        }

        ImGui::Separator();
        char summary[96]{};
        std::snprintf(
            summary, sizeof(summary), T("已选择 %zu 种"), ActiveCount());
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(summary);
        const float clear_width = U(76.0f);
        ImGui::SameLine(std::max(
            ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - clear_width - U(13.0f)));
        if (ImGui::Button(T("全部取消"), ImVec2(clear_width, 0.0f))) {
            ClearCorpsePayloadSelection();
        }
        ImGui::TextDisabled("%s", T("拖动顶部可移动，靠近主窗口会自动吸附"));
        ImGui::Dummy(ImVec2(0.0f, U(4.0f)));

        ImGui::BeginChild(
            "PayloadItems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoBackground);
        for (PayloadSelection& selection : g_selections) {
            DrawPayloadRow(selection);
        }
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

}  // namespace pztrainer::ui
