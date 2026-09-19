#include <cassert>

#include "bridge/game_snapshot.hpp"
#include "features/aim/aim_target_point.hpp"

int main() {
    const pztrainer::bridge::ScreenPoint bone_point{120.0f, 80.0f};
    const pztrainer::bridge::ScreenPoint fallback_point{120.0f, 200.0f};

    const auto resolved = pztrainer::features::aim::ResolveZombieAimPoint(
        true, true, bone_point, fallback_point);
    assert(resolved.x == bone_point.x);
    assert(resolved.y == bone_point.y);

    const auto fallback = pztrainer::features::aim::ResolveZombieAimPoint(
        false, true, bone_point, fallback_point);
    assert(fallback.x == fallback_point.x);
    assert(fallback.y == fallback_point.y);
    return 0;
}
