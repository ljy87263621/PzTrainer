#pragma once

#include <imgui.h>

#include "features/visual/zombie_model_effect.hpp"

namespace pztrainer::ui {

class GameAssetMesh {
public:
    enum class Asset { Animal, Vehicle };

    explicit GameAssetMesh(Asset asset) : asset_(asset) {}
    ~GameAssetMesh();
    bool EnsureLoaded();
    bool Draw(ImDrawList* draw, const ImVec2& center, const ImVec2& size,
              float yaw, int animation,
              features::visual::ZombieModelEffect effect,
              const ImVec4& color, bool wireframe,
              bool edge_glow, const ImVec4& edge_color,
              ImVec2& bounds_min, ImVec2& bounds_max) const;

private:
    struct Impl;
    Asset asset_;
    Impl* impl_ = nullptr;
    bool attempted_ = false;
};

}  // namespace pztrainer::ui
