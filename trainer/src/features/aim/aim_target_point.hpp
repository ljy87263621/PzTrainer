#pragma once

#include "bridge/game_snapshot.hpp"

namespace pztrainer::features::aim {

// A captured skeleton remains authoritative for prone/crawling targets.
bridge::ScreenPoint ResolveZombieAimPoint(
    bool has_bones, bool unstable_pose,
    const bridge::ScreenPoint& bone_point,
    const bridge::ScreenPoint& fallback_point);

}  // namespace pztrainer::features::aim
