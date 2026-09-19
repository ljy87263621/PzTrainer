#include "features/visual/player_visual_settings.hpp"

namespace pztrainer::features::visual {

PlayerVisualSettings& GetPlayerVisualSettings() {
    static PlayerVisualSettings settings;
    return settings;
}

const ImVec4& PlayerModelCaptureMarker(ZombieVisualState state) {
    static const ImVec4 normal(0.929f, 0.514f, 0.071f, 0.0f);
    static const ImVec4 behind_wall(0.322f, 0.071f, 0.929f, 0.0f);
    static const ImVec4 in_view(0.071f, 0.706f, 0.929f, 0.0f);
    if (state == ZombieVisualState::BehindWall) return behind_wall;
    if (state == ZombieVisualState::InView) return in_view;
    return normal;
}

}  // namespace pztrainer::features::visual
