#pragma once

#include <cstdint>
#include <string>

#include "bridge/game_snapshot.hpp"
#include "features/aim/aim_settings.hpp"

namespace pztrainer::features::aim {

struct RageAimStatus {
    bool initialized = false;
    bool firearm_ready = false;
    bool target_locked = false;
    bool double_tap_charging = false;
    WeaponGroup weapon_group = WeaponGroup::Global;
    std::int32_t target_identity = 0;
    std::uint64_t fired_count = 0;
    std::uint64_t java_direction_override_calls = 0;
    std::uint64_t java_hit_list_calls = 0;
    std::uint64_t java_body_part_calls = 0;
    bool ballistics_hook_ready = false;
    std::uint64_t ballistics_hook_calls = 0;
    std::uint64_t ballistics_override_calls = 0;
    std::uint64_t ballistics_reticle_override_calls = 0;
    std::uint64_t ballistics_target_override_calls = 0;
    std::uint64_t ballistics_spread_override_calls = 0;
    bool waiting_for_aim = false;
    float aiming_delay = 0.0f;
    bool silent_aim = false;
    bool no_spread = false;
    bool viewpoint_3d = false;
    int ballistics_hook_character_id = -1;
    int ballistics_published_character_id = -1;
    std::string weapon_type;
    std::string message = "等待游戏对象";
};

void UpdateRageAim(const bridge::FrameSnapshot& frame, bool menu_visible,
                   float viewport_width, float viewport_height);
bool RageAimNeedsZombieData();
bool RageAimNeedsPlayerData();
float RageAimCollectionRange();
const RageAimStatus& GetRageAimStatus();

}  // namespace pztrainer::features::aim
