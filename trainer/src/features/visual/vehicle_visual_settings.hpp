#pragma once

#include <imgui.h>

#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

struct VehicleVisualSettings {
    bool vehicle_esp = true;
    bool colored_marker = true;
    bool show_name = true;
    bool show_distance = true;
    bool show_details = true;
    bool show_engine_bar = true;
    bool ray_esp = false;
    StateColor box_colors{
        ImVec4(0.20f, 0.76f, 1.0f, 0.94f),
        ImVec4(0.64f, 0.34f, 1.0f, 0.92f),
        ImVec4(0.18f, 0.94f, 0.68f, 0.96f)};
    StateColor name_colors{
        ImVec4(0.76f, 0.92f, 1.0f, 1.0f),
        ImVec4(0.82f, 0.70f, 1.0f, 1.0f),
        ImVec4(0.66f, 1.0f, 0.86f, 1.0f)};
    StateColor distance_colors{
        ImVec4(0.36f, 0.78f, 1.0f, 1.0f),
        ImVec4(0.72f, 0.54f, 1.0f, 1.0f),
        ImVec4(0.34f, 0.96f, 0.72f, 1.0f)};
    StateColor model_colors{
        ImVec4(0.14f, 0.66f, 1.0f, 0.82f),
        ImVec4(0.58f, 0.24f, 1.0f, 0.80f),
        ImVec4(0.12f, 0.88f, 0.58f, 0.82f)};
    ImVec4 ray_color{0.18f, 0.68f, 1.0f, 0.92f};
    float max_distance = 100.0f;
    ZombieVisualState preview_state = ZombieVisualState::Default;
    ZombieModelEffect model_effect = ZombieModelEffect::Shaded;
    bool model_edge_glow = false;
    ImVec4 model_edge_glow_color{0.16f, 0.72f, 1.0f, 0.94f};
    bool force_model_visibility = false;
};

VehicleVisualSettings& GetVehicleVisualSettings();
const ImVec4& VehicleModelCaptureMarker(ZombieVisualState state);

}  // namespace pztrainer::features::visual
