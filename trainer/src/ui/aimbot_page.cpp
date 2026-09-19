#include "ui/aimbot_page.hpp"

#include <imgui.h>

#include <array>
#include <cstddef>

#include "features/aim/aim_settings.hpp"
#include "settings/localization.hpp"
#include "ui/animation.hpp"
#include "ui/aim_target_selector.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

using features::aim::AimSettings;
using features::aim::LegitWeaponSettings;
using features::aim::RageWeaponSettings;
using features::aim::WeaponGroup;

constexpr std::array<WeaponGroup, 6> kWeaponGroups{
    WeaponGroup::Global,
    WeaponGroup::Pistol,
    WeaponGroup::Shotgun,
    WeaponGroup::Smg,
    WeaponGroup::Rifle,
    WeaponGroup::Sniper,
};

struct WeaponLibraryState {
    bool open = false;
    AimbotPage page = AimbotPage::Legit;
    float amount = 0.0f;
    float velocity = 0.0f;
};

WeaponLibraryState g_weapon_library;

enum class PresetRowAction {
    None,
    Select,
    Toggle,
};

ImU32 Color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

WeaponGroup& SelectedWeapon(AimSettings& settings, AimbotPage page) {
    return page == AimbotPage::Legit
        ? settings.selected_legit_weapon
        : settings.selected_rage_weapon;
}

bool& PresetEnabled(AimSettings& settings, AimbotPage page, WeaponGroup group) {
    const std::size_t index = static_cast<std::size_t>(group);
    return page == AimbotPage::Legit
        ? settings.legit_presets[index].enabled
        : settings.rage_presets[index].enabled;
}

PresetRowAction PresetMenuRow(WeaponGroup group, bool selected, bool enabled) {
    ImGui::PushID(static_cast<int>(group));
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("preset_row", ImVec2(width, 44.0f));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 end(start.x + width, start.y + 44.0f);
    const ImVec4 background = selected
        ? ImVec4(components::kAccent.x, components::kAccent.y,
                 components::kAccent.z, 0.15f)
        : hovered ? ImVec4(0.11f, 0.12f, 0.15f, 0.72f)
                  : ImVec4(0.045f, 0.049f, 0.062f, 0.72f);
    const ImVec4 border = selected
        ? components::kAccent
        : ImVec4(0.48f, 0.58f, 0.72f, hovered ? 0.46f : 0.27f);
    draw->AddRectFilled(start, end, Color(background), 8.0f);
    draw->AddRect(start, end, Color(border), 8.0f, 0, selected ? 1.7f : 1.1f);
    draw->AddText(ImVec2(start.x + 15.0f, start.y + 14.0f),
                  Color(selected ? ImVec4(0.88f, 0.92f, 1.0f, 1.0f)
                                  : components::kText),
                  settings::Translate(features::aim::WeaponGroupName(group)));
    const ImVec2 check_min(end.x - 31.0f, start.y + 12.0f);
    const ImVec2 check_max(check_min.x + 20.0f, check_min.y + 20.0f);
    draw->AddRectFilled(check_min, check_max,
                        Color(enabled ? components::kAccent
                                      : ImVec4(0.08f, 0.09f, 0.12f, 1.0f)),
                        5.0f);
    draw->AddRect(check_min, check_max,
                  Color(enabled ? ImVec4(0.48f, 0.68f, 1.0f, 0.90f)
                                : ImVec4(0.48f, 0.58f, 0.72f, 0.42f)),
                  5.0f, 0, 1.0f);
    if (enabled) {
        draw->AddLine(ImVec2(check_min.x + 5.0f, check_min.y + 10.0f),
                      ImVec2(check_min.x + 8.5f, check_min.y + 13.5f),
                      Color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 2.0f);
        draw->AddLine(ImVec2(check_min.x + 8.5f, check_min.y + 13.5f),
                      ImVec2(check_min.x + 15.5f, check_min.y + 6.5f),
                      Color(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 2.0f);
    }
    PresetRowAction action = PresetRowAction::None;
    if (clicked) {
        action = ImGui::GetIO().MousePos.x >= check_min.x - 7.0f
            ? PresetRowAction::Toggle
            : PresetRowAction::Select;
    }
    ImGui::PopID();
    return action;
}

bool DrawWeaponLibraryPanel(AimSettings& settings, AimbotPage page,
                            const ImVec2& selector_position,
                            const ImVec2& selector_size) {
    animation::SpringValue(g_weapon_library.amount, g_weapon_library.velocity,
                           g_weapon_library.open ? 1.0f : 0.0f,
                           g_weapon_library.open ? 250.0f : 135.0f,
                           g_weapon_library.open ? 22.0f : 17.0f);
    const float amount = animation::Clamp01(g_weapon_library.amount);
    if (!g_weapon_library.open && amount < 0.002f) {
        g_weapon_library.amount = 0.0f;
        g_weapon_library.velocity = 0.0f;
        return false;
    }

    constexpr float kPanelWidth = 286.0f;
    constexpr float kPanelHeight = 350.0f;
    const ImVec2 position(
        selector_position.x,
        selector_position.y + selector_size.y + 7.0f - (1.0f - amount) * 9.0f);
    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(kPanelWidth, kPanelHeight * amount), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * amount);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.35f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.038f, 0.050f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.49f, 0.92f, 0.74f));
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav;
    const char* window_name = page == AimbotPage::Legit
        ? "##LegitWeaponLibraryPanel"
        : "##RageWeaponLibraryPanel";
    bool panel_hovered = false;
    if (ImGui::Begin(window_name, nullptr, flags)) {
        panel_hovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(settings::Translate("武器预设"));
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        WeaponGroup& selected = SelectedWeapon(settings, page);
        for (const WeaponGroup group : kWeaponGroups) {
            bool& enabled = PresetEnabled(settings, page, group);
            const PresetRowAction action = PresetMenuRow(
                group, selected == group, enabled);
            if (action == PresetRowAction::Select) {
                selected = group;
                g_weapon_library.open = false;
            } else if (action == PresetRowAction::Toggle) {
                enabled = !enabled;
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);
    return panel_hovered;
}

void DrawLegitSettings(AimSettings& settings, LegitWeaponSettings* preset) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 0.0f));
    if (!ImGui::BeginTable(
            "LegitSettingsGrid", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("辅助瞄准");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "LegitMainCard", nullptr, ImVec2(0.0f, 0.0f));
    components::CompactToggleRow("启用辅助瞄准", &settings.legit_enabled);
    components::CompactToggleRow("自动辅助瞄准", &preset->automatic_aim);
    components::CompactToggleRow("瞄准僵尸", &preset->target_zombies);
    components::CompactToggleRow("瞄准 PVP 玩家", &preset->target_players);
    components::CompactToggleRow("检测墙壁", &preset->wall_check);
    components::CompactToggleRow("优先站立僵尸", &preset->prioritize_upright);
    components::StepperRow("瞄准范围", &preset->range, 1.0f, 100.0f, 1.0f,
                           "%.0f 格", false);
    components::EndCard();

    ImGui::TableSetColumnIndex(1);
    components::SectionLabel("瞄准修正");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "LegitTuningCard", nullptr, ImVec2(0.0f, 0.0f));
    components::CompactToggleRow(
        "移除视觉后坐力", &preset->remove_visual_recoil);
    components::StepperRow("自瞄平滑", &preset->smoothing, 1.0f, 100.0f, 1.0f,
                           "%.0f%%");
    components::StepperRow("命中精度", &preset->accuracy, 0.0f, 100.0f, 1.0f,
                           "%.0f%%");
    components::StepperRow("伤害偏好阈值", &preset->minimum_damage, 0.0f, 100.0f, 1.0f,
                           "%.0f%%", false);
    components::EndCard();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void DrawRageSettings(AimSettings& settings, RageWeaponSettings* preset) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f));
    if (!ImGui::BeginTable(
            "RageSettingsGrid", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("暴力瞄准");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "RageMainCard", nullptr, ImVec2(0.0f, 0.0f));
    components::CompactToggleRow("启用 Rage", &settings.rage_enabled);
    components::CompactToggleRow("自动瞄准", &preset->automatic_aim);
    components::CompactToggleRow("瞄准僵尸", &preset->target_zombies);
    components::CompactToggleRow("瞄准 PVP 玩家", &preset->target_players);
    components::CompactToggleRow("自动急停", &preset->automatic_stop);
    components::CompactToggleRow("自动开枪", &preset->automatic_fire);
    components::CompactToggleRow(
        "静默瞄准（不移动鼠标）", &preset->silent_aim);
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "不移动鼠标或原版准星；人物仍会在射击时正确转向 Rage 目标。关闭后会移动鼠标到目标位置。魔法子弹开启时始终保持静默。");
    }
    components::CompactToggleRow("魔法子弹（实验）", &preset->magic_bullet);
    components::CompactToggleRow("检测墙壁", &preset->wall_check);
    components::StepperRow("攻击范围", &preset->range, 1.0f, 100.0f, 1.0f,
                           "%.0f 格", false);
    components::EndCard();

    ImGui::TableSetColumnIndex(1);
    components::SectionLabel("武器修正");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "RageWeaponCard", nullptr, ImVec2(0.0f, 0.0f));
    components::CompactToggleRow("无扩散", &preset->no_spread);
    components::CompactToggleRow("无视觉后坐力", &preset->no_recoil);
    components::StepperRow("命中精度", &preset->accuracy, 0.0f, 100.0f, 1.0f,
                           "%.0f%%");
    components::StepperRow("最低伤害", &preset->minimum_damage,
                           0.0f, 100.0f, 1.0f, "%.0f%%");
    components::StepperRow("最高伤害", &preset->maximum_damage,
                           0.0f, 100.0f, 1.0f, "%.0f%%", false);
    if (preset->maximum_damage < preset->minimum_damage) {
        preset->maximum_damage = preset->minimum_damage;
    }
    components::EndCard();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("射击模式");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "RageFireModeCard", nullptr, ImVec2(0.0f, 0.0f));
    components::CompactToggleRow("DT 双发模式", &preset->double_tap, false);
    components::EndCard();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

}  // namespace

void DrawAimbotWeaponSelector(AimbotPage page) {
    AimSettings& settings = features::aim::GetAimSettings();
    if (g_weapon_library.page != page) {
        g_weapon_library = WeaponLibraryState{};
        g_weapon_library.page = page;
    }
    ImGui::PushID(page == AimbotPage::Legit ? "LegitWeaponSelector" :
                                             "RageWeaponSelector");
    const char* label = settings::Translate(
        features::aim::WeaponGroupName(SelectedWeapon(settings, page)));
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 size(174.0f, 30.0f);
    ImGui::InvisibleButton("selector", size);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) g_weapon_library.open = !g_weapon_library.open;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hovered || g_weapon_library.open || g_weapon_library.amount > 0.02f) {
        draw->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y),
                            Color(ImVec4(0.11f, 0.12f, 0.15f, 0.68f)), 6.0f);
    }
    const float selector_border_alpha = g_weapon_library.open
        ? 0.92f : hovered ? 0.62f : 0.34f;
    draw->AddRect(
        start, ImVec2(start.x + size.x, start.y + size.y),
        Color(ImVec4(
            components::kAccent.x, components::kAccent.y,
            components::kAccent.z, selector_border_alpha)),
        6.0f, 0, g_weapon_library.open ? 1.5f : 1.0f);
    draw->AddText(ImVec2(start.x + 11.0f, start.y + 7.0f),
                  Color(components::kText), label);
    const ImVec2 arrow(start.x + size.x - 15.0f, start.y + 15.0f);
    draw->AddTriangleFilled(ImVec2(arrow.x - 4.0f, arrow.y - 2.0f),
                            ImVec2(arrow.x + 4.0f, arrow.y - 2.0f),
                            ImVec2(arrow.x, arrow.y + 3.0f),
                            Color(components::kMuted));
    const bool panel_hovered = DrawWeaponLibraryPanel(settings, page, start, size);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !hovered && !panel_hovered) {
        g_weapon_library.open = false;
    }
    ImGui::PopID();
}

void DrawAimbotPage(AimbotPage page) {
    AimSettings& settings = features::aim::GetAimSettings();
    if (page == AimbotPage::Rage) {
        RageWeaponSettings& preset = settings.rage_presets[
            static_cast<std::size_t>(settings.selected_rage_weapon)];
        DrawRageSettings(settings, &preset);
        return;
    }
    LegitWeaponSettings& preset = settings.legit_presets[
        static_cast<std::size_t>(settings.selected_legit_weapon)];
    DrawLegitSettings(settings, &preset);
}

void DrawAimbotTargetPreview(AimbotPage page, const ImVec2& size) {
    AimSettings& settings = features::aim::GetAimSettings();
    ImGui::PushID(page == AimbotPage::Legit
        ? "LegitDetachedTargetPreview"
        : "RageDetachedTargetPreview");
    if (page == AimbotPage::Rage) {
        RageWeaponSettings& preset = settings.rage_presets[
            static_cast<std::size_t>(settings.selected_rage_weapon)];
        DrawAimTargetSelector(size, &preset.target_points);
    } else {
        LegitWeaponSettings& preset = settings.legit_presets[
            static_cast<std::size_t>(settings.selected_legit_weapon)];
        DrawAimTargetSelector(size, &preset.target_points);
    }
    ImGui::PopID();
}

}  // namespace pztrainer::ui
