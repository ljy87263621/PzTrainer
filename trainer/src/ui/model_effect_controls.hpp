#pragma once

#include "features/visual/visual_settings.hpp"
#include "features/visual/player_visual_settings.hpp"
#include "features/visual/animal_visual_settings.hpp"
#include "features/visual/vehicle_visual_settings.hpp"

namespace pztrainer::ui {

bool DrawModelEffectDropdown(features::visual::VisualSettings* settings);
bool DrawPlayerModelEffectDropdown(
    features::visual::PlayerVisualSettings* settings);
bool DrawAnimalModelEffectDropdown(
    features::visual::AnimalVisualSettings* settings);
bool DrawVehicleModelEffectDropdown(
    features::visual::VehicleVisualSettings* settings);
bool DrawPreviewStateSelector(features::visual::ZombieVisualState* state);

}  // namespace pztrainer::ui
