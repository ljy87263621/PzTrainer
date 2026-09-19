#pragma once

#include <imgui.h>

namespace pztrainer::features::visual {

struct WeaponRaySettings {
    bool enabled = false;
    ImVec4 color{0.18f, 0.62f, 1.0f, 0.95f};
};

struct WeaponRayStatus {
    bool initialized = false;
    bool visible = false;
    bool hit = false;
    ImVec2 muzzle{};
    ImVec2 endpoint{};
};

WeaponRaySettings& GetWeaponRaySettings();
const WeaponRayStatus& GetWeaponRayStatus();
void UpdateWeaponRay(bool world_ready);
void DrawWeaponRay();

}  // namespace pztrainer::features::visual
