#pragma once

#include <imgui.h>
#include "settings/ui_preferences.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui::cards {

inline bool BeginColumns(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6 * settings::UiScale(), 0));
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return false;
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    return true;
}

inline void EndColumns() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

inline void BeginSection(const char* id, const char* title) {
    components::SectionLabel(title);
    ImGui::Dummy(ImVec2(0, 3 * settings::UiScale()));
    components::BeginCompactCard(id, nullptr, ImVec2(0, 0));
}

}  // namespace pztrainer::ui::cards
