#include "features/visual/direction_indicator.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace pztrainer::features::visual {
namespace {

struct ArrowGeometry {
    ImVec2 center{};
    ImVec2 direction{};
    ImVec2 side{};
};

void DrawArrowTriangle(ImDrawList* draw, const ArrowGeometry& arrow,
                       const ImVec4& color) {
    const auto vertices = [&arrow](float scale) {
        const ImVec2 tip(
            arrow.center.x + arrow.direction.x * 11.0f * scale,
            arrow.center.y + arrow.direction.y * 11.0f * scale);
        const ImVec2 back(
            arrow.center.x - arrow.direction.x * 8.0f * scale,
            arrow.center.y - arrow.direction.y * 8.0f * scale);
        return std::array<ImVec2, 3>{
            tip,
            ImVec2(back.x + arrow.side.x * 7.0f * scale,
                   back.y + arrow.side.y * 7.0f * scale),
            ImVec2(back.x - arrow.side.x * 7.0f * scale,
                   back.y - arrow.side.y * 7.0f * scale),
        };
    };

    constexpr std::array<float, 4> glow_scales{{1.70f, 1.48f, 1.30f, 1.15f}};
    constexpr std::array<float, 4> glow_alpha{{0.05f, 0.08f, 0.13f, 0.22f}};
    constexpr std::array<float, 4> glow_thickness{{3.2f, 2.8f, 2.3f, 1.8f}};
    for (std::size_t index = 0; index < glow_scales.size(); ++index) {
        const auto points = vertices(glow_scales[index]);
        ImVec4 glow = color;
        glow.w *= glow_alpha[index];
        draw->AddTriangle(
            points[0], points[1], points[2], ImGui::GetColorU32(glow),
            glow_thickness[index]);
    }

    const auto points = vertices(1.0f);
    draw->AddTriangleFilled(
        points[0], points[1], points[2], ImGui::GetColorU32(color));
    draw->AddTriangle(
        points[0], points[1], points[2], IM_COL32(245, 248, 255, 225), 1.1f);
}

void DrawTrailingLabel(ImDrawList* draw, const ArrowGeometry& arrow,
                       const ImVec4& color, const char* trailing_label) {
    if (trailing_label == nullptr || trailing_label[0] == '\0') return;
    const ImVec2 text_size = ImGui::CalcTextSize(trailing_label);
    const ImVec2 text_center(
        arrow.center.x - arrow.direction.x * (22.0f + text_size.x * 0.5f),
        arrow.center.y - arrow.direction.y * (22.0f + text_size.y * 0.5f));
    const ImVec2 text_pos(text_center.x - text_size.x * 0.5f,
                          text_center.y - text_size.y * 0.5f);
    draw->AddRectFilled(ImVec2(text_pos.x - 5.0f, text_pos.y - 3.0f),
        ImVec2(text_pos.x + text_size.x + 5.0f,
               text_pos.y + text_size.y + 3.0f),
        IM_COL32(5, 7, 11, 210), 5.0f);
    draw->AddText(text_pos, ImGui::GetColorU32(color), trailing_label);
}

bool MakeDirection(const ImVec2& origin, const ImVec2& target,
                   ImVec2& direction) {
    const float dx = target.x - origin.x;
    const float dy = target.y - origin.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) return false;
    direction = ImVec2(dx / length, dy / length);
    return true;
}

}  // namespace

void DrawDirectionIndicator(ImDrawList* draw, const ImVec2& origin,
                            const ImVec2& target, const ImVec4& color,
                            const char* trailing_label) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 direction{};
    if (!MakeDirection(origin, target, direction)) return;
    const float margin_x = 54.0f;
    const float margin_y = 46.0f;
    const float horizontal = std::max(1.0f,
        direction.x > 0.0f ? display.x - margin_x - origin.x
                           : origin.x - margin_x);
    const float vertical = std::max(1.0f,
        direction.y > 0.0f ? display.y - margin_y - origin.y
                           : origin.y - margin_y);
    const float scale_x = std::abs(direction.x) > 0.001f
        ? horizontal / std::abs(direction.x) : 100000.0f;
    const float scale_y = std::abs(direction.y) > 0.001f
        ? vertical / std::abs(direction.y) : 100000.0f;
    const float edge_scale = std::min(scale_x, scale_y);
    const ArrowGeometry arrow{
        ImVec2(origin.x + direction.x * edge_scale,
               origin.y + direction.y * edge_scale),
        direction,
        ImVec2(-direction.y, direction.x),
    };
    DrawArrowTriangle(draw, arrow, color);
    DrawTrailingLabel(draw, arrow, color, trailing_label);
}

void DrawNearbyDirectionIndicator(ImDrawList* draw, const ImVec2& origin,
                                  const ImVec2& target, const ImVec4& color,
                                  const char* trailing_label) {
    ImVec2 direction{};
    if (!MakeDirection(origin, target, direction)) return;
    constexpr float radius = 68.0f;
    constexpr float margin = 28.0f;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ArrowGeometry arrow{
        ImVec2(
            std::clamp(origin.x + direction.x * radius, margin, display.x - margin),
            std::clamp(origin.y + direction.y * radius, margin, display.y - margin)),
        direction,
        ImVec2(-direction.y, direction.x),
    };
    DrawArrowTriangle(draw, arrow, color);
    DrawTrailingLabel(draw, arrow, color, trailing_label);
}

}  // namespace pztrainer::features::visual
