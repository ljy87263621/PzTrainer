#pragma once

#include <imgui.h>

#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

struct PlayerVisualSettings {
    bool player_esp = true;
    bool colored_marker = true;
    bool show_name = true;
    bool show_distance = true;
    bool show_skeleton = true;
    bool show_health_bar = true;
    bool ray_esp = true;
    bool show_admin_marker = false;
    StateColor box_colors{
        ImVec4(0.24f, 0.58f, 1.0f, 0.94f),
        ImVec4(0.72f, 0.38f, 1.0f, 0.90f),
        ImVec4(0.22f, 0.96f, 0.78f, 0.96f)};
    StateColor name_colors{
        ImVec4(0.82f, 0.90f, 1.0f, 1.0f),
        ImVec4(0.86f, 0.72f, 1.0f, 1.0f),
        ImVec4(0.72f, 1.0f, 0.90f, 1.0f)};
    StateColor distance_colors{
        ImVec4(0.42f, 0.76f, 1.0f, 1.0f),
        ImVec4(0.78f, 0.58f, 1.0f, 1.0f),
        ImVec4(0.36f, 0.96f, 0.78f, 1.0f)};
    StateColor skeleton_colors{
        ImVec4(0.72f, 0.88f, 1.0f, 0.96f),
        ImVec4(0.82f, 0.65f, 1.0f, 0.96f),
        ImVec4(0.60f, 1.0f, 0.84f, 0.98f)};
    StateColor model_colors{
        ImVec4(0.20f, 0.54f, 1.0f, 0.80f),
        ImVec4(0.66f, 0.30f, 1.0f, 0.78f),
        ImVec4(0.12f, 0.90f, 0.66f, 0.80f)};
    ImVec4 ray_color{0.24f, 0.64f, 1.0f, 0.92f};
    float max_distance = 60.0f;
    int preview_animation = 0;
    ZombieVisualState preview_state = ZombieVisualState::Default;
    ZombieModelEffect model_effect = ZombieModelEffect::Shaded;
    bool model_edge_glow = false;
    ImVec4 model_edge_glow_color{0.22f, 0.62f, 1.0f, 0.94f};
    bool force_model_visibility = false;
};

PlayerVisualSettings& GetPlayerVisualSettings();
const ImVec4& PlayerModelCaptureMarker(ZombieVisualState state);

}  // namespace pztrainer::features::visual
