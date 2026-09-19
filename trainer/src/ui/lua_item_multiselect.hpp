#pragma once

#include <string>
#include <vector>

namespace pztrainer::ui {

bool DrawLuaItemMultiSelect(
    const char* label, std::vector<std::string>& selected_items,
    std::string& search);

}  // namespace pztrainer::ui
