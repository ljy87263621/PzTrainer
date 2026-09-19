#include "features/visual/animal_visual_settings.hpp"

namespace pztrainer::features::visual {

AnimalVisualSettings& GetAnimalVisualSettings() {
    static AnimalVisualSettings settings;
    return settings;
}

const ImVec4& AnimalModelCaptureMarker(ZombieVisualState state) {
    static const ImVec4 normal(0.953f, 0.322f, 0.612f, 0.0f);
    static const ImVec4 behind_wall(0.576f, 0.953f, 0.063f, 0.0f);
    static const ImVec4 in_view(0.953f, 0.812f, 0.063f, 0.0f);
    if (state == ZombieVisualState::BehindWall) return behind_wall;
    if (state == ZombieVisualState::InView) return in_view;
    return normal;
}

}  // namespace pztrainer::features::visual
