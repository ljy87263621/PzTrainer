#pragma once

#include <imgui.h>

namespace pztrainer::ui::navigation {

enum class Icon {
    Crosshair,
    Mouse,
    Eye,
    World,
    User,
    Package,
    Spark,
    Lua,
    Clock,
    Settings,
};

using IconDrawCallback = void (*)(
    ImDrawList* draw, const ImVec2& center, ImU32 color,
    const void* context);

void BeginGroup(const char* id, int item_count);
void EndGroup();
bool Item(const char* label, Icon icon, bool selected);
bool Item(const char* label, bool selected, IconDrawCallback draw_icon,
          const void* context);

}  // namespace pztrainer::ui::navigation
