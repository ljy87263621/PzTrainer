#pragma once

#include <array>
#include <string>
#include <imgui.h>

namespace pztrainer::features::visual {

struct ViewpointModelChamsStatus {
    int painted_draws = 0;
    std::string message;
};

void DrawViewpointModelChams(const std::array<ImVec4, 12>& markers,
                            const std::array<ImVec4, 12>& colors);
const ViewpointModelChamsStatus& GetViewpointModelChamsStatus();

}  // namespace pztrainer::features::visual
