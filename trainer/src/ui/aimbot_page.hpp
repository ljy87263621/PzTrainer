#pragma once

#include <imgui.h>

namespace pztrainer::ui {

enum class AimbotPage {
    Legit,
    Rage,
};

void DrawAimbotPage(AimbotPage page);
void DrawAimbotWeaponSelector(AimbotPage page);
void DrawAimbotTargetPreview(AimbotPage page, const ImVec2& size);

}  // namespace pztrainer::ui
