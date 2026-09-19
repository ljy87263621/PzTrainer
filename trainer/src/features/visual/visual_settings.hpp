#pragma once

#include <imgui.h>

#include "features/visual/zombie_model_effect.hpp"

namespace pztrainer::features::visual {

enum class ZombieVisualState {
    Default,
    BehindWall,
    InView,
};

struct StateColor {
    ImVec4 normal;
    ImVec4 behind_wall;
    ImVec4 in_view;
    bool behind_wall_enabled = true;
    bool in_view_enabled = true;
};

const ImVec4& ResolveColor(const StateColor& colors, ZombieVisualState state);
const ImVec4& ModelCaptureMarker(ZombieVisualState state);

struct VisualSettings {
    bool zombie_esp = true;
    bool colored_marker = true;
    bool show_name = true;
    bool show_distance = true;
    bool show_skeleton = true;
    bool show_health_bar = true;
    StateColor box_colors{
        ImVec4(1.0f, 0.32f, 0.35f, 0.92f),
        ImVec4(0.55f, 0.35f, 1.0f, 0.88f),
        ImVec4(0.25f, 0.94f, 0.62f, 0.94f)};
    StateColor name_colors{
        ImVec4(1.0f, 0.88f, 0.89f, 1.0f),
        ImVec4(0.72f, 0.62f, 1.0f, 1.0f),
        ImVec4(0.72f, 1.0f, 0.84f, 1.0f)};
    StateColor distance_colors{
        ImVec4(1.0f, 0.74f, 0.35f, 1.0f),
        ImVec4(0.67f, 0.50f, 1.0f, 1.0f),
        ImVec4(0.35f, 0.94f, 0.65f, 1.0f)};
    StateColor skeleton_colors{
        ImVec4(0.87f, 0.93f, 1.0f, 0.93f),
        ImVec4(0.68f, 0.55f, 1.0f, 0.94f),
        ImVec4(0.55f, 1.0f, 0.75f, 0.96f)};
    StateColor model_colors{
        ImVec4(1.0f, 0.26f, 0.34f, 0.78f),
        ImVec4(0.48f, 0.24f, 1.0f, 0.74f),
        ImVec4(0.16f, 0.92f, 0.55f, 0.78f)};
    float max_distance = 40.0f;
    int preview_animation = 0;
    ZombieVisualState preview_state = ZombieVisualState::Default;
    ZombieModelEffect model_effect = ZombieModelEffect::Shaded;
    bool model_edge_glow = false;
    ImVec4 model_edge_glow_color{0.24f, 0.56f, 1.0f, 0.92f};
    bool force_model_visibility = false;
};

VisualSettings& GetVisualSettings();

}  // namespace pztrainer::features::visual
