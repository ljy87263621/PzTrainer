#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pztrainer::features::aim {

enum class WeaponGroup : std::size_t {
    Global,
    Pistol,
    Shotgun,
    Smg,
    Rifle,
    Sniper,
    Count,
};

enum class TargetPoint : std::uint32_t {
    Head = 1u << 0,
    Neck = 1u << 1,
    Chest = 1u << 2,
    Abdomen = 1u << 3,
    Pelvis = 1u << 4,
    LeftArm = 1u << 5,
    RightArm = 1u << 6,
    LeftHand = 1u << 7,
    RightHand = 1u << 8,
    LeftThigh = 1u << 9,
    RightThigh = 1u << 10,
    LeftCalf = 1u << 11,
    RightCalf = 1u << 12,
    LeftFoot = 1u << 13,
    RightFoot = 1u << 14,
};

struct LegitWeaponSettings {
    bool enabled = false;
    bool automatic_aim = true;
    bool target_zombies = true;
    bool target_players = false;
    bool wall_check = true;
    bool prioritize_upright = true;
    bool remove_visual_recoil = false;
    float range = 20.0f;
    float smoothing = 45.0f;
    float accuracy = 85.0f;
    float minimum_damage = 25.0f;
    std::uint32_t target_points = static_cast<std::uint32_t>(TargetPoint::Head);
};

struct RageWeaponSettings {
    bool enabled = false;
    bool automatic_aim = true;
    bool target_zombies = true;
    bool target_players = false;
    bool automatic_stop = true;
    bool automatic_fire = true;
    bool silent_aim = true;
    bool wall_check = true;
    bool no_spread = true;
    bool no_recoil = true;
    bool double_tap = false;
    bool magic_bullet = false;
    float range = 30.0f;
    float accuracy = 100.0f;
    float minimum_damage = 100.0f;
    float maximum_damage = 100.0f;
    std::uint32_t target_points = static_cast<std::uint32_t>(TargetPoint::Head);
};

struct AimSettings {
    bool legit_enabled = false;
    bool rage_enabled = false;
    std::array<LegitWeaponSettings,
               static_cast<std::size_t>(WeaponGroup::Count)> legit_presets{};
    std::array<RageWeaponSettings,
               static_cast<std::size_t>(WeaponGroup::Count)> rage_presets{};
    WeaponGroup selected_legit_weapon = WeaponGroup::Global;
    WeaponGroup selected_rage_weapon = WeaponGroup::Global;
};

AimSettings& GetAimSettings();
const char* WeaponGroupName(WeaponGroup group);

}  // namespace pztrainer::features::aim
