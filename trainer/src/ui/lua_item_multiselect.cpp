#include "ui/lua_item_multiselect.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "bridge/item_bridge.hpp"
#include "settings/localization.hpp"
#include "ui/animated_dropdown.hpp"

namespace pztrainer::ui {
namespace {

bool ContainsInsensitive(const std::string& value, const std::string& search) {
    if (search.empty()) return true;
    return std::search(
               value.begin(), value.end(), search.begin(), search.end(),
               [](unsigned char left, unsigned char right) {
                   return std::tolower(left) == std::tolower(right);
               }) != value.end();
}

bool IsSelected(
    const std::vector<std::string>& selected_items,
    const std::string& full_type) {
    return std::find(
               selected_items.begin(), selected_items.end(), full_type) !=
        selected_items.end();
}

void ToggleSelection(
    std::vector<std::string>& selected_items, const std::string& full_type) {
    const auto found = std::find(
        selected_items.begin(), selected_items.end(), full_type);
    if (found == selected_items.end()) {
        selected_items.push_back(full_type);
        std::sort(selected_items.begin(), selected_items.end());
    } else {
        selected_items.erase(found);
    }
}

void DrawItemIcon(
    const bridge::ItemCatalogEntry& item, const ImVec2& minimum,
    const ImVec2& maximum) {
    if (item.texture_id == 0) return;
    float width = maximum.x - minimum.x;
    float height = maximum.y - minimum.y;
    if (item.texture_width > 0 && item.texture_height > 0) {
        const float aspect = static_cast<float>(item.texture_width) /
            static_cast<float>(item.texture_height);
        if (aspect > 1.0f) {
            height /= aspect;
        } else {
            width *= aspect;
        }
    }
    const ImVec2 center(
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddImage(
        ImTextureRef(static_cast<ImTextureID>(item.texture_id)),
        ImVec2(center.x - width * 0.5f, center.y - height * 0.5f),
        ImVec2(center.x + width * 0.5f, center.y + height * 0.5f),
        ImVec2(item.uv_min_x, item.uv_min_y),
        ImVec2(item.uv_max_x, item.uv_max_y),
        ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
}

}  // namespace

bool DrawLuaItemMultiSelect(
    const char* label, std::vector<std::string>& selected_items,
    std::string& search) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);

    char summary[96]{};
    std::snprintf(
        summary, sizeof(summary), settings::Translate("已选择 %zu 项"),
        selected_items.size());
    const ImGuiID dropdown_id = AnimatedDropdownId("##LuaItemMultiSelect");
    if (ImGui::Button(summary, ImVec2(ImGui::GetContentRegionAvail().x, 34.0f))) {
        ToggleAnimatedDropdown(dropdown_id);
    }
    const bool control_hovered = ImGui::IsItemHovered();
    const ImVec2 control_minimum = ImGui::GetItemRectMin();
    const ImVec2 control_maximum = ImGui::GetItemRectMax();
    if (BeginAnimatedDropdownPopup(
            dropdown_id, control_minimum, control_maximum,
            ImVec2(560.0f, 560.0f), control_hovered)) {
        if (bridge::GetItemCatalog().empty()) bridge::RefreshItemCatalog();
        const std::vector<bridge::ItemCatalogEntry>& catalog =
            bridge::GetItemCatalog();

        std::vector<char> search_buffer(
            std::max<std::size_t>(search.size() + 64, 512), '\0');
        std::copy(search.begin(), search.end(), search_buffer.begin());
        const char* clear_label = settings::Translate("清空选择");
        const float clear_width = std::max(
            110.0f, ImGui::CalcTextSize(clear_label).x + 28.0f);
        ImGui::SetNextItemWidth(
            std::max(160.0f, ImGui::GetContentRegionAvail().x - clear_width - 8.0f));
        if (ImGui::InputTextWithHint(
                "##LuaItemSearch",
                settings::Translate("搜索物品名称或完整类型"),
                search_buffer.data(), search_buffer.size())) {
            search = search_buffer.data();
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ImGui::Button(clear_label, ImVec2(clear_width, 0.0f)) &&
            !selected_items.empty()) {
            selected_items.clear();
            changed = true;
        }

        std::vector<const bridge::ItemCatalogEntry*> filtered;
        filtered.reserve(catalog.size());
        for (const bridge::ItemCatalogEntry& item : catalog) {
            if (ContainsInsensitive(item.display_name, search) ||
                ContainsInsensitive(item.full_type, search) ||
                ContainsInsensitive(item.category, search)) {
                filtered.push_back(&item);
            }
        }

        char count[96]{};
        std::snprintf(
            count, sizeof(count), settings::Translate("可用物品 · %zu"),
            filtered.size());
        ImGui::TextDisabled("%s", count);
        ImGui::Separator();
        ImGui::BeginChild("##LuaItemCatalog", ImVec2(0.0f, 0.0f));
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filtered.size()), 46.0f);
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart;
                 index < clipper.DisplayEnd; ++index) {
                const bridge::ItemCatalogEntry& item = *filtered[index];
                ImGui::PushID(item.full_type.c_str());
                const bool selected = IsSelected(selected_items, item.full_type);
                const ImVec2 row_minimum = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable(
                        "##LuaItem", selected,
                        ImGuiSelectableFlags_NoAutoClosePopups,
                        ImVec2(0.0f, 42.0f))) {
                    ToggleSelection(selected_items, item.full_type);
                    changed = true;
                }
                const ImVec2 icon_minimum(
                    row_minimum.x + 5.0f, row_minimum.y + 4.0f);
                DrawItemIcon(
                    item, icon_minimum,
                    ImVec2(icon_minimum.x + 34.0f, icon_minimum.y + 34.0f));
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->PushClipRect(
                    ImVec2(row_minimum.x + 46.0f, row_minimum.y),
                    ImVec2(ImGui::GetItemRectMax().x - 30.0f,
                           row_minimum.y + 42.0f),
                    true);
                draw->AddText(
                    ImVec2(row_minimum.x + 46.0f, row_minimum.y + 3.0f),
                    ImGui::GetColorU32(ImGuiCol_Text),
                    item.display_name.c_str());
                draw->AddText(
                    ImVec2(row_minimum.x + 46.0f, row_minimum.y + 22.0f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled),
                    item.full_type.c_str());
                draw->PopClipRect();
                if (selected) {
                    draw->AddText(
                        ImVec2(ImGui::GetItemRectMax().x - 22.0f,
                               row_minimum.y + 11.0f),
                        ImGui::GetColorU32(ImVec4(0.30f, 0.78f, 1.0f, 1.0f)),
                        "\xE2\x9C\x93");
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        EndAnimatedDropdown();
    }
    ImGui::PopID();
    return changed;
}

}  // namespace pztrainer::ui
