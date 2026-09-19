#pragma once

#include <imgui.h>

namespace pztrainer::ui {

void ApplyTheme();
void SetInterfaceFonts(ImFont* regular, ImFont* semibold);
ImFont* RegularFont();
ImFont* SemiboldFont();

}  // namespace pztrainer::ui
