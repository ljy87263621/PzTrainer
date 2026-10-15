#pragma once
#include "features/aim/aim_settings.hpp"

namespace pztrainer::features::aim {
inline bool HasRageTargetingMode(const RageWeaponSettings& preset) {
    return preset.automatic_aim || preset.automatic_fire ||
        preset.silent_aim || preset.magic_bullet;
}
inline bool ShouldAdjustRageAim(const RageWeaponSettings& preset, bool aiming) {
    return aiming && (preset.automatic_aim || preset.silent_aim || preset.magic_bullet);
}
}
