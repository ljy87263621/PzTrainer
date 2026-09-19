#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

const ImVec4& ResolveColor(const StateColor& colors, ZombieVisualState state) {
    if (state == ZombieVisualState::BehindWall && colors.behind_wall_enabled) {
        return colors.behind_wall;
    }
    if (state == ZombieVisualState::InView && colors.in_view_enabled) {
        return colors.in_view;
    }
    return colors.normal;
}

const ImVec4& ModelCaptureMarker(ZombieVisualState state) {
    static const ImVec4 normal(0.071f, 0.149f, 0.929f, 0.0f);
    static const ImVec4 behind_wall(0.839f, 0.071f, 0.639f, 0.0f);
    static const ImVec4 in_view(0.063f, 0.827f, 0.251f, 0.0f);
    if (state == ZombieVisualState::BehindWall) return behind_wall;
    if (state == ZombieVisualState::InView) return in_view;
    return normal;
}

VisualSettings& GetVisualSettings() {
    static VisualSettings settings;
    return settings;
}

}  // namespace pztrainer::features::visual
