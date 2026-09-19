#include "ui/experience_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>

#include "bridge/experience_bridge.hpp"
#include "settings/localization.hpp"
#include "settings/safety_mode.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

std::array<char, 96> g_search{};
std::string g_category = "全部";
std::string g_selected_id;
float g_amount = 100.0f;
std::uint64_t g_session_generation = 0;

float U(float value) {
    return value * settings::UiScale();
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool Matches(const bridge::SkillExperienceEntry& entry) {
    if (g_category != "全部" && entry.category != g_category) return false;
    const std::string query = LowerAscii(g_search.data());
    if (query.empty()) return true;
    return LowerAscii(entry.display_name).find(query) != std::string::npos ||
        LowerAscii(entry.id).find(query) != std::string::npos;
}

float Progress(const bridge::SkillExperienceEntry& entry) {
    if (entry.level >= 10) return 1.0f;
    const float span = entry.next_level_xp - entry.level_start_xp;
    if (span <= 0.0f) return 0.0f;
    return std::clamp(
        (entry.total_xp - entry.level_start_xp) / span, 0.0f, 1.0f);
}

bool DrawSkillRow(const bridge::SkillExperienceEntry& entry) {
    ImGui::PushID(entry.id.c_str());
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::InvisibleButton(
        "SkillRow", ImVec2(width, U(54.0f)));
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = g_selected_id == entry.id;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(minimum.x + width, minimum.y + U(50.0f));
    draw->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(selected
            ? ImVec4(0.11f, 0.16f, 0.30f, 0.94f)
            : hovered
                ? ImVec4(0.09f, 0.10f, 0.13f, 0.92f)
                : ImVec4(0.035f, 0.039f, 0.050f, 0.82f)),
        U(8.0f));
    if (selected) {
        draw->AddRect(minimum, maximum,
            ImGui::GetColorU32(ImVec4(
                components::kAccent.x, components::kAccent.y,
                components::kAccent.z, 0.78f)), U(8.0f), 0, U(1.0f));
    }
    draw->AddText(ImVec2(minimum.x + U(12.0f), minimum.y + U(7.0f)),
                  ImGui::GetColorU32(components::kText), entry.display_name.c_str());
    draw->AddText(ImVec2(minimum.x + U(12.0f), minimum.y + U(27.0f)),
                  ImGui::GetColorU32(components::kMuted), entry.id.c_str());

    char level[24]{};
    std::snprintf(level, sizeof(level), "%d / 10", entry.level);
    const ImVec2 level_size = ImGui::CalcTextSize(level);
    draw->AddText(ImVec2(
                      maximum.x - level_size.x - U(12.0f),
                      minimum.y + U(7.0f)),
                  ImGui::GetColorU32(components::kText), level);
    const float bar_left = minimum.x + width * 0.53f;
    const float bar_right = maximum.x - U(12.0f);
    draw->AddRectFilled(ImVec2(bar_left, minimum.y + U(32.0f)),
                        ImVec2(bar_right, minimum.y + U(36.0f)),
                        ImGui::GetColorU32(ImVec4(
                            33.0f / 255.0f, 36.0f / 255.0f,
                            47.0f / 255.0f, 1.0f)), U(2.0f));
    draw->AddRectFilled(ImVec2(bar_left, minimum.y + U(32.0f)),
                        ImVec2(bar_left + (bar_right - bar_left) * Progress(entry),
                               minimum.y + U(36.0f)),
                        ImGui::GetColorU32(components::kAccent), U(2.0f));
    if (clicked) g_selected_id = entry.id;
    ImGui::PopID();
    return clicked;
}

const bridge::SkillExperienceEntry* FindSelected(
        const bridge::ExperienceStatus& status) {
    const auto found = std::find_if(status.entries.begin(), status.entries.end(),
        [](const bridge::SkillExperienceEntry& entry) {
            return entry.id == g_selected_id;
        });
    return found == status.entries.end() ? nullptr : &*found;
}

void DrawCategoryFilter() {
    ImGui::SetNextItemWidth(U(128.0f));
    if (BeginAnimatedCombo(
            "##SkillCategory", g_category.c_str(), U(218.0f))) {
        constexpr std::array<const char*, 7> categories{
            "全部", "体能", "敏捷", "战斗", "枪械", "制作", "生存"};
        for (const char* category : categories) {
            const bool selected = g_category == category;
            if (ImGui::Selectable(category, selected)) {
                g_category = category;
                CloseAnimatedDropdown();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        EndAnimatedDropdown();
    }
}

void DrawSelectedEditor(const bridge::ExperienceStatus& status,
                        const bridge::SkillExperienceEntry* entry) {
    components::BeginCard("SelectedSkillEditor", nullptr, ImVec2(0.0f, 0.0f));
    if (entry == nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(settings::Translate("选择一项技能"));
        ImGui::PopStyleColor();
        components::EndCard();
        return;
    }

    ImGui::Text("%s", entry->display_name.c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::Text("%s  ·  %s", entry->id.c_str(), entry->category.c_str());
    ImGui::PopStyleColor();

    char details[160]{};
    if (entry->level >= 10) {
        std::snprintf(details, sizeof(details), "等级 10  ·  总经验 %.0f", entry->total_xp);
    } else {
        std::snprintf(details, sizeof(details),
            "等级 %d  ·  本级 %.0f / %.0f  ·  倍率 %.1fx",
            entry->level,
            std::max(0.0f, entry->total_xp - entry->level_start_xp),
            std::max(0.0f, entry->next_level_xp - entry->level_start_xp),
            entry->multiplier);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextUnformatted(details);
    ImGui::PopStyleColor();

    const bool multiplayer = status.session_mode ==
        bridge::ExperienceSessionMode::MultiplayerClient;
    const bool exercise_route = entry->online_route ==
            bridge::SkillOnlineRoute::FitnessExercise ||
        entry->online_route == bridge::SkillOnlineRoute::StrengthExercise;
    const float remaining_to_max =
        std::max(50.0f, entry->max_level_xp - entry->total_xp);
    if (exercise_route && multiplayer) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(settings::Translate(
            "联机添加经验目前会被检测，等待后续更新"));
        ImGui::PopStyleColor();
    } else {
        components::StepperRow(
            "增加经验", &g_amount, 50.0f,
            std::max(1000.0f, remaining_to_max), 50.0f, "%.0f", false);
    }
    const bool radio_route = entry->online_route ==
        bridge::SkillOnlineRoute::RadioTeaching;
    const bool maintenance_route = entry->online_route ==
        bridge::SkillOnlineRoute::MaintenanceTraining;
    const bool radio_cutoff_reached = multiplayer && radio_route &&
        status.media_level_cutoff > 0 && entry->level >= status.media_level_cutoff;
    const bool can_apply = status.player_ready &&
        !(multiplayer && exercise_route) && (!multiplayer ||
        (entry->online_supported && !radio_cutoff_reached &&
         (!radio_route || status.nearby_receiver_ready)));
    ImGui::BeginDisabled(!can_apply);
    const char* apply_label = "添加经验";
    if (multiplayer) {
        if (exercise_route) {
            apply_label = "等待更新";
        } else if (maintenance_route) {
            apply_label = status.maintenance_training_active
                ? "停止训练"
                : "低频训练";
        } else {
            apply_label = "广播训练";
        }
    }
    const bool show_level_button = !multiplayer || radio_route;
    const float button_gap = U(8.0f);
    const float standard_button_width = U(92.0f);
    const float api_button_width = U(132.0f);
    const float button_height = U(30.0f);
    const float action_width = standard_button_width + button_gap +
        api_button_width + (show_level_button
            ? button_gap + standard_button_width
            : 0.0f);
    const float action_x = std::max(
        0.0f, ImGui::GetContentRegionAvail().x - action_width);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + action_x);
    if (ImGui::Button(
            settings::Translate(apply_label),
            ImVec2(standard_button_width, button_height))) {
        bridge::AddSkillExperience(entry->id, g_amount);
    }
    ImGui::EndDisabled();
    if (multiplayer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!entry->online_supported) {
            components::RoundedTooltip("该技能没有原版广播教学代码，联机不可用。");
        } else if (radio_cutoff_reached) {
            components::RoundedTooltip(
                "当前技能已达到服务器媒体教学等级上限，广播不会再增加经验。"
            );
        } else if (exercise_route) {
            components::RoundedTooltip(
                "联机直接添加健身或力量经验目前会被服务器检测，等待后续更新。"
            );
        } else if (maintenance_route) {
            if (status.maintenance_training_active) {
                char tooltip[192]{};
                std::snprintf(
                    tooltip, sizeof(tooltip),
                    "固定每 2 秒最多提交一次，普通攻击时自动暂停；已提交 %d 次。再次点击可停止。",
                    status.maintenance_packets_sent);
                components::RoundedTooltip(tooltip);
            } else {
                components::RoundedTooltip(
                    "装备有耐久的近战武器后开始。为避开已确认的 HitWeapon 限速，固定低频提交；训练期间不要普通攻击。"
                );
            }
        } else if (!status.nearby_receiver_ready) {
            components::RoundedTooltip("站在开启且音量不为零的收音机或电视 5 格内。");
        } else {
            components::RoundedTooltip(
                "提交普通 WaveSignal 广播事件，由服务端教学逻辑增加本人经验；不使用管理员指令。"
            );
        }
    }
    if (!multiplayer) {
        ImGui::SameLine(0.0f, button_gap);
        ImGui::BeginDisabled(entry->level >= 10 || !status.player_ready);
        if (ImGui::Button(
                settings::Translate("提升 1 级"),
                ImVec2(standard_button_width, button_height))) {
            bridge::AddSkillLevel(entry->id);
        }
        ImGui::EndDisabled();
    } else if (radio_route) {
        ImGui::SameLine(0.0f, button_gap);
        ImGui::BeginDisabled(!can_apply || entry->level >= 10);
        if (ImGui::Button(
                settings::Translate("一次升满"),
                ImVec2(standard_button_width, button_height))) {
            constexpr float kServerXpReductionCompensation = 5.0f;
            bridge::AddSkillExperience(
                entry->id,
                std::max(1.0f,
                    remaining_to_max * kServerXpReductionCompensation + 1.0f));
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            components::RoundedTooltip(
                "必须在达到媒体教学等级上限前使用；请求量已补偿服务器的默认经验速率缩减，最终经验仍由服务器封顶。"
            );
        }
    }
    ImGui::SameLine(0.0f, button_gap);
    const bool vanilla_api_available =
        status.session_mode != bridge::ExperienceSessionMode::Unknown &&
        !settings::IsSafeModeEnabled() && status.player_ready;
    ImGui::BeginDisabled(!vanilla_api_available);
    if (ImGui::Button(
            settings::Translate("原版 API 添加经验"),
            ImVec2(api_button_width, button_height))) {
        bridge::AddSkillExperienceVanilla(entry->id, g_amount);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (settings::IsSafeModeEnabled()) {
            components::RoundedTooltip("安全模式已开启。请到设置中关闭安全模式后使用原版 API。");
        } else if (!status.player_ready) {
            components::RoundedTooltip("等待玩家对象准备完成。");
        }
    }
    components::EndCard();
}

}  // namespace

void DrawExperienceEditor() {
    const bridge::ExperienceStatus& status = bridge::GetExperienceStatus();
    if (g_session_generation != status.session_generation) {
        g_session_generation = status.session_generation;
        g_selected_id.clear();
        g_amount = 100.0f;
    }
    components::SectionLabel("技能与熟练度");
    ImGui::Dummy(ImVec2(0.0f, U(4.0f)));

    ImGui::SetNextItemWidth(std::max(
        U(180.0f), ImGui::GetContentRegionAvail().x - U(136.0f)));
    ImGui::InputTextWithHint(
        "##SkillSearch", settings::Translate("搜索技能名称或 ID"),
        g_search.data(), g_search.size());
    ImGui::SameLine(0.0f, U(8.0f));
    DrawCategoryFilter();
    ImGui::Dummy(ImVec2(0.0f, U(8.0f)));

    components::BeginCard("SkillList", nullptr, ImVec2(0.0f, 365.0f));
    ImGui::BeginChild("SkillRows", ImVec2(0.0f, 0.0f));
    bool any_visible = false;
    for (const bridge::SkillExperienceEntry& entry : status.entries) {
        if (!Matches(entry)) continue;
        any_visible = true;
        DrawSkillRow(entry);
    }
    if (!any_visible) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(status.player_ready
            ? settings::Translate("没有匹配的技能")
            : settings::Translate(status.message.c_str()));
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    components::EndCard();

    if (g_selected_id.empty() && !status.entries.empty()) {
        g_selected_id = status.entries.front().id;
    }
    ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
    DrawSelectedEditor(status, FindSelected(status));
}

}  // namespace pztrainer::ui
