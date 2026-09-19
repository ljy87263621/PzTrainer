#include "features/visual/vehicle_visual_settings.hpp"

namespace pztrainer::features::visual {

VehicleVisualSettings& GetVehicleVisualSettings() {
    static VehicleVisualSettings settings;
    return settings;
}

const ImVec4& VehicleModelCaptureMarker(ZombieVisualState state) {
    static const ImVec4 normal(0.063f, 0.941f, 0.776f, 0.0f);
    static const ImVec4 behind_wall(0.953f, 0.063f, 0.306f, 0.0f);
    static const ImVec4 in_view(0.478f, 0.953f, 0.063f, 0.0f);
    if (state == ZombieVisualState::BehindWall) return behind_wall;
    if (state == ZombieVisualState::InView) return in_view;
    return normal;
}

}  // namespace pztrainer::features::visual
