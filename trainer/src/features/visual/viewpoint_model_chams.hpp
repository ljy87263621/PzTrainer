#pragma once

#include <array>
#include <string>
#include <imgui.h>
#include "bridge/game_snapshot.hpp"

namespace pztrainer::features::visual {

struct ViewpointModelChamsStatus {
    int painted_draws = 0;
    std::string message;
};

void DrawViewpointModelChams(const std::array<ImVec4, 12>& markers,
                            const std::array<ImVec4, 12>& colors,
                            const std::array<ImVec4, 12>& edges,
                            const std::array<int, 12>& effects,
                            const bridge::FrameSnapshot& frame);
void ResetViewpointModelChamsStatus(const std::string& message);
const ViewpointModelChamsStatus& GetViewpointModelChamsStatus();

}  // namespace pztrainer::features::visual
