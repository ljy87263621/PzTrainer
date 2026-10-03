#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "bridge/game_snapshot.hpp"

namespace pztrainer::bridge {

inline constexpr float kViewpointHeightScale = 2.4494896f;

struct ViewpointCamera {
    bool valid = false;
    float width = 0.0f;
    float height = 0.0f;
    std::array<double, 3> origin{};
    std::array<float, 3> eye{};
    std::array<float, 16> view_projection{};
};

inline bool ProjectViewpointPoint(const ViewpointCamera& camera,
                                  const WorldPoint& point, ScreenPoint& screen) {
    screen = {-10000.0f, -10000.0f};
    if (!camera.valid || camera.width <= 0.0f || camera.height <= 0.0f) return false;
    const float x = static_cast<float>(-static_cast<double>(point.x) - camera.origin[0]);
    const float y = static_cast<float>(static_cast<double>(point.z) *
        kViewpointHeightScale - camera.origin[1]);
    const float z = static_cast<float>(-static_cast<double>(point.y) - camera.origin[2]);
    const auto& m = camera.view_projection;
    const float clip_x = m[0] * x + m[4] * y + m[8] * z + m[12];
    const float clip_y = m[1] * x + m[5] * y + m[9] * z + m[13];
    const float clip_z = m[2] * x + m[6] * y + m[10] * z + m[14];
    const float clip_w = m[3] * x + m[7] * y + m[11] * z + m[15];
    if (!std::isfinite(clip_x) || !std::isfinite(clip_y) ||
        !std::isfinite(clip_z) || !std::isfinite(clip_w)) return false;
    if (clip_w <= 0.001f || clip_z < -clip_w || clip_z > clip_w) {
        // Keep a direction outside the viewport for ESP arrows, never a visible target.
        const float length = std::hypot(clip_x, clip_y);
        if (length > 0.001f) {
            const float radius = std::max(camera.width, camera.height) * 2.0f;
            screen = {camera.width * 0.5f + clip_x / length * radius,
                      camera.height * 0.5f - clip_y / length * radius};
        }
        return false;
    }
    screen = {(clip_x / clip_w + 1.0f) * camera.width * 0.5f,
              (1.0f - clip_y / clip_w) * camera.height * 0.5f};
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

inline bool ViewpointTargetAngles(const ViewpointCamera& camera,
                                  const WorldPoint& point, float& yaw, float& pitch) {
    if (!camera.valid) return false;
    const double x = -static_cast<double>(point.x) - camera.origin[0] - camera.eye[0];
    const double y = static_cast<double>(point.z) * kViewpointHeightScale -
        camera.origin[1] - camera.eye[1];
    const double z = -static_cast<double>(point.y) - camera.origin[2] - camera.eye[2];
    const double horizontal = std::hypot(x, z);
    if (!std::isfinite(horizontal) || !std::isfinite(y) || horizontal <= 0.001) return false;
    yaw = static_cast<float>(std::atan2(-z, -x));
    pitch = std::clamp(static_cast<float>(std::atan2(y, horizontal)),
                       -1.4835298f, 1.4835298f);
    return true;
}

}  // namespace pztrainer::bridge
