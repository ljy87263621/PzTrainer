#pragma once

#include <string>

namespace pztrainer::ui {

const char* ItemCategoryLabel(const std::string& category);
const char* ItemSpawnMethodDescription(const std::string& method);
float ItemSpawnMethodPopupWidth(float available_width);

}  // namespace pztrainer::ui
