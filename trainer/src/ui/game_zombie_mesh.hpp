#pragma once

#include <imgui.h>

#include "features/visual/zombie_model_effect.hpp"

namespace pztrainer::ui {

struct ZombieMeshStyle {
    features::visual::ZombieModelEffect effect = features::visual::ZombieModelEffect::Disabled;
    ImVec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    bool edge_glow = false;
    ImVec4 edge_color{0.24f, 0.56f, 1.0f, 0.92f};
    bool draw_base_if_disabled = false;
};

class GameZombieMesh {
public:
    static GameZombieMesh& Instance();
    static GameZombieMesh& PlayerInstance();
    bool EnsureLoaded();
    bool Draw(ImDrawList* draw, const ImVec2& center, float scale, float yaw,
              int animation, const ZombieMeshStyle& style, bool show_skeleton,
              const ImVec4& skeleton_color, ImVec2& bounds_min, ImVec2& bounds_max) const;
    bool ProjectBone(const char* bone_name, const ImVec2& center, float scale,
                     float yaw, ImVec2& screen_position) const;
private:
    explicit GameZombieMesh(bool player_variant = false)
        : player_variant_(player_variant) {}
    ~GameZombieMesh();
    GameZombieMesh(const GameZombieMesh&) = delete;
    GameZombieMesh& operator=(const GameZombieMesh&) = delete;

    struct Impl;
    Impl* impl_ = nullptr;
    bool attempted_ = false;
    bool player_variant_ = false;
};

}  // namespace pztrainer::ui
