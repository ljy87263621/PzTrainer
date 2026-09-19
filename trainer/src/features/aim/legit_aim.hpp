#pragma once

#include <cstdint>
#include <string>

#include "bridge/game_snapshot.hpp"
#include "features/aim/aim_settings.hpp"

namespace pztrainer::features::aim {

struct LegitAimStatus {
    bool initialized = false;
    bool firearm_ready = false;
    bool aiming = false;
    bool target_locked = false;
    WeaponGroup weapon_group = WeaponGroup::Global;
    std::int32_t target_identity = 0;
    std::uint64_t adjustment_count = 0;
    std::string weapon_type;
    std::string message = "等待游戏对象";
};

void UpdateLegitAim(const bridge::FrameSnapshot& frame, bool menu_visible,
                    float viewport_width, float viewport_height,
                    float delta_time);
bool LegitAimNeedsZombieData();
bool LegitAimNeedsPlayerData();
float LegitAimCollectionRange();
const LegitAimStatus& GetLegitAimStatus();

}  // namespace pztrainer::features::aim
