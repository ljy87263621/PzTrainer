#pragma once

#include <imgui.h>

#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

struct AnimalVisualSettings {
    bool animal_esp = true;
    bool colored_marker = true;
    bool show_name = true;
    bool show_distance = true;
    bool show_skeleton = true;
    bool show_health_bar = true;
    bool ray_esp = false;
    StateColor box_colors{
        ImVec4(0.96f, 0.68f, 0.20f, 0.94f),
        ImVec4(0.82f, 0.38f, 0.18f, 0.92f),
        ImVec4(0.32f, 0.92f, 0.46f, 0.96f)};
    StateColor name_colors{
        ImVec4(1.0f, 0.86f, 0.56f, 1.0f),
        ImVec4(1.0f, 0.62f, 0.40f, 1.0f),
        ImVec4(0.66f, 1.0f, 0.74f, 1.0f)};
    StateColor distance_colors{
        ImVec4(0.95f, 0.72f, 0.32f, 1.0f),
        ImVec4(0.92f, 0.48f, 0.26f, 1.0f),
        ImVec4(0.40f, 0.94f, 0.56f, 1.0f)};
    StateColor skeleton_colors{
        ImVec4(1.0f, 0.82f, 0.42f, 0.96f),
        ImVec4(0.96f, 0.52f, 0.30f, 0.96f),
        ImVec4(0.58f, 1.0f, 0.68f, 0.98f)};
    StateColor model_colors{
        ImVec4(0.96f, 0.58f, 0.14f, 0.82f),
        ImVec4(0.82f, 0.28f, 0.12f, 0.80f),
        ImVec4(0.20f, 0.86f, 0.38f, 0.82f)};
    ImVec4 ray_color{0.96f, 0.62f, 0.18f, 0.92f};
    float max_distance = 60.0f;
    int preview_animation = 0;
    ZombieVisualState preview_state = ZombieVisualState::Default;
    ZombieModelEffect model_effect = ZombieModelEffect::Shaded;
    bool model_edge_glow = false;
    ImVec4 model_edge_glow_color{1.0f, 0.66f, 0.20f, 0.94f};
    bool force_model_visibility = false;
};

AnimalVisualSettings& GetAnimalVisualSettings();
const ImVec4& AnimalModelCaptureMarker(ZombieVisualState state);

}  // namespace pztrainer::features::visual
