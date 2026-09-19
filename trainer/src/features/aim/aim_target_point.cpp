#include "features/aim/aim_target_point.hpp"

namespace pztrainer::features::aim {

bridge::ScreenPoint ResolveZombieAimPoint(
    bool has_bones, bool unstable_pose,
    const bridge::ScreenPoint& bone_point,
    const bridge::ScreenPoint& fallback_point) {
    (void)unstable_pose;
    return has_bones ? bone_point : fallback_point;
}

}  // namespace pztrainer::features::aim
