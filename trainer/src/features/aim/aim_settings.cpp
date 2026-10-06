#include "features/aim/aim_settings.hpp"

#include <algorithm>
#include <cmath>

namespace pztrainer::features::aim {

AimSettings& GetAimSettings() {
    static AimSettings settings = [] {
        AimSettings defaults;
        const std::size_t global =
            static_cast<std::size_t>(WeaponGroup::Global);
        defaults.legit_presets[global].enabled = true;
        defaults.rage_presets[global].enabled = true;
        return defaults;
    }();
    return settings;
}

std::array<RageWeaponModifierToggles,
           static_cast<std::size_t>(WeaponGroup::Count)>&
GetRageWeaponModifierToggles() {
    static std::array<RageWeaponModifierToggles,
                      static_cast<std::size_t>(WeaponGroup::Count)> toggles{};
    return toggles;
}

float EffectiveRageAccuracy(const RageWeaponSettings& settings,
                            const RageWeaponModifierToggles& toggles) {
    return toggles.accuracy_enabled
        ? std::clamp(settings.accuracy, 0.0f, 100.0f)
        : 100.0f;
}

RageWeaponAppliedValues ResolveRageWeaponModifiers(
    const RageWeaponSettings& settings,
    const RageWeaponModifierToggles& toggles,
    const RageWeaponOriginalValues& original,
    float target_current_health) {
    RageWeaponAppliedValues values{};
    values.projectile_spread = settings.no_spread
        ? 0.0f : original.projectile_spread;
    const float accuracy = EffectiveRageAccuracy(settings, toggles);
    values.hit_chance = toggles.accuracy_enabled
        ? static_cast<int>(std::lround(accuracy))
        : original.hit_chance;
    values.aiming_time = settings.no_spread
        ? 0
        : toggles.accuracy_enabled
            ? static_cast<int>(std::lround(
                  original.aiming_time * (1.0f - accuracy / 100.0f)))
            : original.aiming_time;

    const bool has_target_health = std::isfinite(target_current_health) &&
        target_current_health > 0.0f;
    const float minimum_percent = std::clamp(
        settings.minimum_damage, 0.0f, 100.0f);
    const float maximum_percent = std::clamp(
        std::max(settings.maximum_damage, minimum_percent), 0.0f, 100.0f);
    values.minimum_damage = toggles.minimum_damage_enabled && has_target_health
        ? target_current_health * minimum_percent / 100.0f
        : original.minimum_damage;
    values.maximum_damage = toggles.maximum_damage_enabled && has_target_health
        ? target_current_health * maximum_percent / 100.0f
        : original.maximum_damage;

    // Keep the game's min/max invariant when only one damage override is on.
    if (values.minimum_damage > values.maximum_damage) {
        if (toggles.minimum_damage_enabled &&
            !toggles.maximum_damage_enabled) {
            values.minimum_damage = values.maximum_damage;
        } else {
            values.maximum_damage = values.minimum_damage;
        }
    }
    return values;
}

const char* WeaponGroupName(WeaponGroup group) {
    switch (group) {
        case WeaponGroup::Global: return "全局";
        case WeaponGroup::Pistol: return "手枪";
        case WeaponGroup::Shotgun: return "霰弹枪";
        case WeaponGroup::Smg: return "冲锋枪";
        case WeaponGroup::Rifle: return "步枪";
        case WeaponGroup::Sniper: return "狙击枪";
        case WeaponGroup::Count: break;
    }
    return "未知";
}

}  // namespace pztrainer::features::aim
