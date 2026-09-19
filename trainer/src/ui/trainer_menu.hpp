#pragma once

#include <atomic>

#include <imgui.h>

#include "bridge/game_snapshot.hpp"

namespace pztrainer::ui {

struct TrainerMenuLayout {
    ImVec2 host_position;
    ImVec2 host_size;
    bool detached_preview_visible = false;
    ImVec2 detached_preview_position;
    ImVec2 detached_preview_size;
};

TrainerMenuLayout GetTrainerMenuLayout();
void PrepareTrainerMenuStartupReveal();
void DrawTrainerMenu(std::atomic_bool& visible, const bridge::FrameSnapshot& frame);

}  // namespace pztrainer::ui
