#include "ui/player_effect_editor.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>

#include "bridge/item_bridge.hpp"
#include "bridge/player_effect_bridge.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "bridge/server_player_effect_bridge.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

enum class EffectFilter {
    TimedMedication,
    CharacterStat,
};

constexpr float kRowHeight = 58.0f;
constexpr ImVec4 kRowFill{0.035f, 0.039f, 0.050f, 0.82f};
constexpr ImVec4 kRowHover{0.090f, 0.102f, 0.132f, 0.92f};
constexpr ImVec4 kRowSelected{0.110f, 0.160f, 0.300f, 0.92f};

std::array<char, 128> g_search{};
EffectFilter g_filter = EffectFilter::TimedMedication;
std::string g_selected_key;
float g_edit_duration = 0.0f;
float g_edit_strength = 0.0f;
float g_edit_value = 0.0f;
bool g_editor_dirty = false;

float U(float value) {
    return value * settings::UiScale();
}

ImU32 Color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

const char* KindLabel(bridge::PlayerEffectKind kind) {
    switch (kind) {
        case bridge::PlayerEffectKind::TimedMedication:
            return settings::Translate("药效");
        case bridge::PlayerEffectKind::CharacterStat:
            return settings::Translate("属性");
    }
    return settings::Translate("状态与效果");
}

std::string EntryKey(const bridge::PlayerEffectEntry& entry) {
    return std::to_string(static_cast<int>(entry.kind)) + ":" + entry.id;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool MatchesFilter(const bridge::PlayerEffectEntry& entry) {
    if (g_filter == EffectFilter::TimedMedication &&
        entry.kind != bridge::PlayerEffectKind::TimedMedication) return false;
    if (g_filter == EffectFilter::CharacterStat &&
        entry.kind != bridge::PlayerEffectKind::CharacterStat) return false;
    const std::string query = LowerAscii(g_search.data());
    if (query.empty()) return true;
    return LowerAscii(entry.display_name).find(query) != std::string::npos ||
        LowerAscii(entry.id).find(query) != std::string::npos;
}

void DrawFilterTabs() {
    if (components::TopTab(
            "药效", g_filter == EffectFilter::TimedMedication, U(112.0f))) {
        g_filter = EffectFilter::TimedMedication;
        g_selected_key.clear();
    }
    ImGui::SameLine(0.0f, U(7.0f));
    if (components::TopTab(
            "角色属性", g_filter == EffectFilter::CharacterStat, U(112.0f))) {
        g_filter = EffectFilter::CharacterStat;
        g_selected_key.clear();
    }
}

const bridge::ItemCatalogEntry* FindSourceItem(const std::string& full_type) {
    if (full_type.empty()) return nullptr;
    const auto& catalog = bridge::GetItemCatalog();
    const auto found = std::find_if(catalog.begin(), catalog.end(),
        [&full_type](const bridge::ItemCatalogEntry& item) {
            return item.full_type == full_type;
        });
    return found == catalog.end() ? nullptr : &*found;
}

void DrawFallbackIcon(ImDrawList* draw, const ImVec2& minimum,
                      bridge::PlayerEffectKind kind) {
    const ImVec2 maximum(minimum.x + U(38.0f), minimum.y + U(38.0f));
    const ImVec4 fill = kind == bridge::PlayerEffectKind::TimedMedication
        ? ImVec4(0.17f, 0.34f, 0.26f, 1.0f)
        : ImVec4(0.18f, 0.25f, 0.40f, 1.0f);
    draw->AddRectFilled(minimum, maximum, Color(fill), U(9.0f));
    const char* letters = kind == bridge::PlayerEffectKind::TimedMedication ? "RX" : "S";
    const ImVec2 text_size = ImGui::CalcTextSize(letters);
    draw->AddText(
        ImVec2(minimum.x + (U(38.0f) - text_size.x) * 0.5f,
               minimum.y + (U(38.0f) - text_size.y) * 0.5f),
        ImGui::GetColorU32(ImVec4(
            235.0f / 255.0f, 240.0f / 255.0f,
            250.0f / 255.0f, 1.0f)), letters);
}

void DrawEntryIcon(ImDrawList* draw, const ImVec2& minimum,
                   const bridge::PlayerEffectEntry& entry) {
    unsigned int texture_id = entry.texture_id;
    float uv_min_x = entry.uv_min_x;
    float uv_min_y = entry.uv_min_y;
    float uv_max_x = entry.uv_max_x;
    float uv_max_y = entry.uv_max_y;
    if (texture_id == 0) {
        const bridge::ItemCatalogEntry* item = FindSourceItem(entry.source_item_id);
        if (item != nullptr) {
            texture_id = item->texture_id;
            uv_min_x = item->uv_min_x;
            uv_min_y = item->uv_min_y;
            uv_max_x = item->uv_max_x;
            uv_max_y = item->uv_max_y;
        }
    }
    if (texture_id == 0) {
        DrawFallbackIcon(draw, minimum, entry.kind);
        return;
    }
    draw->AddImage(
        ImTextureRef(static_cast<ImTextureID>(texture_id)), minimum,
        ImVec2(minimum.x + U(38.0f), minimum.y + U(38.0f)),
        ImVec2(uv_min_x, uv_min_y), ImVec2(uv_max_x, uv_max_y),
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
}

std::string StatusText(const bridge::PlayerEffectEntry& entry) {
    char text[64]{};
    if (entry.kind == bridge::PlayerEffectKind::TimedMedication) {
        if (!entry.active) return "未激活";
        const int seconds = static_cast<int>(entry.duration_seconds + 0.5f);
        std::snprintf(text, sizeof(text), "%d:%02d  ·  %.2f",
                      seconds / 60, seconds % 60, entry.strength);
    } else {
        std::snprintf(text, sizeof(text), "%.3f", entry.value);
    }
    return text;
}

void SelectEntry(const bridge::PlayerEffectEntry& entry) {
    g_selected_key = EntryKey(entry);
    g_edit_duration = entry.duration_seconds;
    g_edit_strength = entry.strength > 0.0f ? entry.strength : 0.25f;
    g_edit_value = entry.value;
    g_editor_dirty = false;
}

bool DrawEntryRow(const bridge::PlayerEffectEntry& entry) {
    ImGui::PushID(EntryKey(entry).c_str());
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("EffectRow", ImVec2(width, U(kRowHeight)));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const bool selected = g_selected_key == EntryKey(entry);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(
        minimum.x + width, minimum.y + U(kRowHeight - 4.0f));
    draw->AddRectFilled(minimum, maximum,
        Color(selected ? kRowSelected : hovered ? kRowHover : kRowFill), U(9.0f));
    if (selected) {
        draw->AddRect(minimum, maximum,
            Color(ImVec4(components::kAccent.x, components::kAccent.y,
                         components::kAccent.z, 0.78f)), U(9.0f), 0, U(1.0f));
    }
    DrawEntryIcon(
        draw, ImVec2(minimum.x + U(9.0f), minimum.y + U(8.0f)), entry);
    draw->AddText(ImVec2(minimum.x + U(58.0f), minimum.y + U(8.0f)),
                  Color(components::kText), entry.display_name.c_str());
    draw->AddText(ImVec2(minimum.x + U(58.0f), minimum.y + U(31.0f)),
                  Color(components::kMuted), entry.id.c_str());

    const std::string status = StatusText(entry);
    const ImVec2 status_size = ImGui::CalcTextSize(status.c_str());
    const float status_x = maximum.x - status_size.x - U(14.0f);
    draw->AddRectFilled(
        ImVec2(status_x - U(9.0f), minimum.y + U(16.0f)),
        ImVec2(maximum.x - U(8.0f), minimum.y + U(40.0f)),
        Color(entry.active ? ImVec4(0.12f, 0.27f, 0.22f, 0.90f)
                           : ImVec4(0.10f, 0.11f, 0.14f, 0.90f)), U(12.0f));
    draw->AddText(ImVec2(status_x, minimum.y + U(20.0f)),
                  Color(entry.active ? ImVec4(0.57f, 0.91f, 0.72f, 1.0f)
                                     : components::kMuted), status.c_str());
    if (clicked) SelectEntry(entry);
    ImGui::PopID();
    return clicked;
}

bool DrawStatRow(const bridge::PlayerEffectEntry& entry) {
    ImGui::PushID(EntryKey(entry).c_str());
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float row_height = U(43.0f);
    ImGui::InvisibleButton("StatRow", ImVec2(width, row_height));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const bool selected = g_selected_key == EntryKey(entry);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(minimum.x + width, minimum.y + row_height - U(3.0f));
    draw->AddRectFilled(minimum, maximum,
        Color(selected ? kRowSelected : hovered ? kRowHover : kRowFill), U(7.0f));
    if (selected) {
        draw->AddRect(minimum, maximum,
            Color(ImVec4(components::kAccent.x, components::kAccent.y,
                         components::kAccent.z, 0.78f)), U(7.0f), 0, U(1.0f));
    }

    draw->AddText(ImVec2(minimum.x + U(12.0f), minimum.y + U(5.0f)),
                  Color(components::kText), entry.display_name.c_str());
    draw->AddText(ImVec2(minimum.x + U(12.0f), minimum.y + U(23.0f)),
                  Color(components::kMuted), entry.id.c_str());
    char current[32]{};
    char range[64]{};
    std::snprintf(current, sizeof(current), "%.3f", entry.value);
    std::snprintf(range, sizeof(range), "%.2f - %.2f", entry.minimum, entry.maximum);
    const ImVec4 value_color = entry.active
        ? ImVec4(0.96f, 0.48f, 0.22f, 1.0f) : components::kText;
    draw->AddText(ImVec2(minimum.x + width - U(244.0f), minimum.y + U(13.0f)),
                  Color(value_color), current);
    draw->AddText(ImVec2(minimum.x + width - U(125.0f), minimum.y + U(13.0f)),
                  Color(components::kMuted), range);
    if (clicked) SelectEntry(entry);
    ImGui::PopID();
    return clicked;
}

const bridge::PlayerEffectEntry* FindSelected(
        const bridge::PlayerEffectStatus& status) {
    const auto found = std::find_if(status.entries.begin(), status.entries.end(),
        [](const bridge::PlayerEffectEntry& entry) {
            return EntryKey(entry) == g_selected_key;
        });
    return found == status.entries.end() ? nullptr : &*found;
}

void DrawEditor(const bridge::PlayerEffectStatus& status,
                const bridge::PlayerEffectEntry* entry) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.030f, 0.034f, 0.044f, 0.72f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, U(8.0f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(12.0f), U(9.0f)));
    ImGui::BeginChild(
        "EffectEditorControls", ImVec2(0.0f, U(206.0f)), ImGuiChildFlags_Borders);
    if (entry == nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(settings::Translate("选择上方状态以编辑"));
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        return;
    }

    ImGui::Text("%s", entry->display_name.c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::Text("%s  ·  %s", entry->id.c_str(), KindLabel(entry->kind));
    ImGui::PopStyleColor();
    const bool multiplayer = status.session_mode ==
        bridge::PlayerEffectSessionMode::MultiplayerClient;
    const bool disabled = !entry->editable;
    ImGui::BeginDisabled(disabled);
    if (entry->kind == bridge::PlayerEffectKind::TimedMedication) {
        const char* apply_label = multiplayer ? "服务器服药" : "应用";
        const float apply_width = U(multiplayer ? 96.0f : 68.0f);
        ImGui::BeginDisabled(multiplayer);
        if (components::StepperRow("持续时间", &g_edit_duration,
                                   0.0f, 900.0f, 5.0f, "%.0f 秒")) {
            g_editor_dirty = true;
        }
        if (components::StepperRow("效果强度", &g_edit_strength,
                                   0.0f, 2.0f, 0.05f, "%.2f", false)) {
            g_editor_dirty = true;
        }
        ImGui::EndDisabled();
        const float button_width = U(64.0f + 7.0f) + apply_width;
        const float button_x = std::max(
            0.0f, ImGui::GetContentRegionAvail().x - button_width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + button_x);
        ImGui::BeginDisabled(multiplayer);
        if (ImGui::Button(
                settings::Translate("移除"), ImVec2(U(64.0f), U(30.0f)))) {
            bridge::SetTimedPlayerEffect(entry->id, 0.0f, 0.0f);
            g_edit_duration = 0.0f;
            g_editor_dirty = false;
        }
        ImGui::EndDisabled();
        if (multiplayer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            components::RoundedTooltip("普通玩家没有服务端移除现有药效的原版动作。");
        }
        ImGui::SameLine(0.0f, U(7.0f));
        ImGui::BeginDisabled(
            multiplayer && bridge::IsServerTimedMedicationBusy());
        if (ImGui::Button(
                settings::Translate(apply_label),
                 ImVec2(apply_width, U(30.0f)))) {
            if (multiplayer) {
                bridge::RequestServerTimedMedication(entry->id);
            } else {
                bridge::SetTimedPlayerEffect(
                    entry->id, g_edit_duration, g_edit_strength);
            }
            g_editor_dirty = false;
        }
        ImGui::EndDisabled();
        if (multiplayer && ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "使用服务端登记的原版药物并提交正常服药动作；时长和强度由游戏决定。"
            );
        } else if (multiplayer &&
                   ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            components::RoundedTooltip(
                bridge::GetServerPlayerEffectStatus().c_str());
        }
    } else if (entry->kind == bridge::PlayerEffectKind::CharacterStat) {
        if (components::StepperRow("当前值", &g_edit_value,
                                   entry->minimum, entry->maximum, 0.001f,
                                   "%.3f", false)) {
            g_editor_dirty = true;
        }
        const bool ordinary_server_route = multiplayer &&
            bridge::CanSetServerCharacterStat(entry->id);
        const char* apply_label = ordinary_server_route
            ? "服务器应用"
            : multiplayer
                ? (status.server_stat_sync_available ? "同步应用" : "本地应用")
                : "应用";
        const float apply_width = U(multiplayer ? 84.0f : 68.0f);
        const float button_width = U(64.0f + 7.0f) + apply_width;
        const float button_x = std::max(
            0.0f, ImGui::GetContentRegionAvail().x - button_width);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + button_x);
        if (ImGui::Button(
                settings::Translate("重置"), ImVec2(U(64.0f), U(30.0f)))) {
            if (ordinary_server_route) {
                bridge::ResetServerCharacterStat(
                    entry->id, entry->default_value);
            } else {
                bridge::ResetPlayerCharacterStat(entry->id);
            }
            g_edit_value = entry->default_value;
            g_editor_dirty = false;
        }
        ImGui::SameLine(0.0f, U(7.0f));
        if (ImGui::Button(
                settings::Translate(apply_label),
                ImVec2(apply_width, U(30.0f)))) {
            if (ordinary_server_route) {
                bridge::SetServerCharacterStat(entry->id, g_edit_value);
            } else {
                bridge::SetPlayerCharacterStat(entry->id, g_edit_value);
            }
            g_editor_dirty = false;
        }
        if (ordinary_server_route && ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "通过普通 Drink 包由服务端修改本人属性，不使用管理员指令。"
            );
        } else if (multiplayer && !status.server_stat_sync_available &&
            ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "只修改当前客户端的实际属性；服务器可能覆盖，退出重进后不保证保留。");
        }
    }
    ImGui::EndDisabled();
    if (disabled && ImGui::IsWindowHovered()) {
        const char* tooltip = entry->kind == bridge::PlayerEffectKind::TimedMedication
            ? "当前会话不允许编辑药效。"
            : "当前会话不允许编辑角色属性。";
        components::RoundedTooltip(tooltip);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

}  // namespace

void DrawPlayerEffectEditor() {
    const bridge::PlayerEffectStatus& status = bridge::GetPlayerEffectStatus();
    components::SectionLabel("状态与效果");
    ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
    components::BeginCard("PlayerEffectEditor", nullptr, ImVec2(0.0f, 0.0f));

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint(
        "##EffectSearch", settings::Translate("搜索名称或 ID"),
        g_search.data(), g_search.size());
    ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
    DrawFilterTabs();

    ImGui::Dummy(ImVec2(0.0f, U(7.0f)));
    if (g_filter == EffectFilter::CharacterStat) {
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(settings::Translate("属性"));
        ImGui::SameLine(std::max(0.0f, width - U(244.0f)));
        ImGui::TextUnformatted(settings::Translate("当前"));
        ImGui::SameLine(std::max(0.0f, width - U(125.0f)));
        ImGui::TextUnformatted(settings::Translate("范围"));
        ImGui::PopStyleColor();
    }
    ImGui::BeginChild(
        "EffectRows", ImVec2(0.0f, U(260.0f)), ImGuiChildFlags_None);
    bool any_visible = false;
    for (const bridge::PlayerEffectEntry& entry : status.entries) {
        if (!MatchesFilter(entry)) continue;
        any_visible = true;
        if (g_filter == EffectFilter::CharacterStat) {
            DrawStatRow(entry);
        } else {
            DrawEntryRow(entry);
        }
    }
    if (!any_visible) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextUnformatted(status.player_ready
            ? settings::Translate("没有匹配的状态")
            : settings::Translate(status.message.c_str()));
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0.0f, U(7.0f)));
    const bridge::PlayerEffectEntry* selected = FindSelected(status);
    if (selected != nullptr && !g_editor_dirty && !ImGui::IsAnyItemActive()) {
        g_edit_duration = selected->duration_seconds;
        g_edit_strength = selected->strength > 0.0f ? selected->strength : 0.25f;
        g_edit_value = selected->value;
    }
    DrawEditor(status, selected);
    components::EndCard();
}

}  // namespace pztrainer::ui
