#include <cassert>
#include <cmath>

#include "features/aim/aim_settings.hpp"

int main() {
    using namespace pztrainer::features::aim;

    const RageWeaponModifierToggles defaults{};
    assert(defaults.accuracy_enabled);
    assert(defaults.minimum_damage_enabled);
    assert(defaults.maximum_damage_enabled);
    for (const RageWeaponModifierToggles& toggles :
         GetRageWeaponModifierToggles()) {
        assert(toggles.accuracy_enabled);
        assert(toggles.minimum_damage_enabled);
        assert(toggles.maximum_damage_enabled);
    }

    const RageWeaponOriginalValues original{
        0.35f, 62, 4.0f, 8.0f, 120};
    RageWeaponSettings settings{};
    settings.no_spread = false;
    settings.no_recoil = false;
    settings.accuracy = 15.0f;
    settings.minimum_damage = 90.0f;
    settings.maximum_damage = 95.0f;
    RageWeaponModifierToggles disabled = defaults;
    disabled.accuracy_enabled = false;
    disabled.minimum_damage_enabled = false;
    disabled.maximum_damage_enabled = false;

    settings.no_spread = true;
    const RageWeaponAppliedValues no_spread = ResolveRageWeaponModifiers(
        settings, disabled, original, 20.0f);
    assert(no_spread.aiming_time == 0);
    settings.no_spread = false;

    const RageWeaponAppliedValues restored = ResolveRageWeaponModifiers(
        settings, disabled, original, 20.0f);
    assert(std::abs(restored.projectile_spread - original.projectile_spread) < 0.001f);
    assert(restored.hit_chance == original.hit_chance);
    assert(std::abs(restored.minimum_damage - original.minimum_damage) < 0.001f);
    assert(std::abs(restored.maximum_damage - original.maximum_damage) < 0.001f);
    assert(restored.aiming_time == original.aiming_time);

    const float effective_accuracy = EffectiveRageAccuracy(settings, disabled);
    assert(std::abs(effective_accuracy - 100.0f) < 0.001f);

    disabled.accuracy_enabled = true;
    disabled.minimum_damage_enabled = true;
    disabled.maximum_damage_enabled = true;
    const RageWeaponAppliedValues modified = ResolveRageWeaponModifiers(
        settings, disabled, original, 20.0f);
    assert(modified.hit_chance == 15);
    assert(modified.aiming_time < original.aiming_time);
    assert(modified.minimum_damage > original.minimum_damage);
    assert(modified.maximum_damage > modified.minimum_damage);

    disabled.minimum_damage_enabled = true;
    disabled.maximum_damage_enabled = false;
    RageWeaponSettings bounded_settings = settings;
    bounded_settings.minimum_damage = 100.0f;
    const RageWeaponAppliedValues bounded = ResolveRageWeaponModifiers(
        bounded_settings, disabled, original, 20.0f);
    assert(bounded.minimum_damage <= bounded.maximum_damage);
    return 0;
}
