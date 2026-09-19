#include "features/visual/animal_esp.hpp"
#include "features/visual/direction_indicator.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace pztrainer::features::visual {
namespace {

ZombieVisualState StateFor(const bridge::AnimalSnapshot& animal) {
    if (animal.behind_wall) return ZombieVisualState::BehindWall;
    if (animal.in_view) return ZombieVisualState::InView;
    return ZombieVisualState::Default;
}

void DrawHealthBar(ImDrawList* draw, const ImVec2& minimum,
                   const ImVec2& maximum, float fraction) {
    const float health = std::clamp(fraction, 0.0f, 1.0f);
    const ImVec2 bar_min(minimum.x - 10.0f, minimum.y);
    const ImVec2 bar_max(minimum.x - 5.0f, maximum.y);
    draw->AddRectFilled(bar_min, bar_max, IM_COL32(4, 6, 9, 225), 2.0f);
    const ImVec4 color = health >= 0.60f
        ? ImVec4(0.22f, 0.88f, 0.42f, 0.96f)
        : health >= 0.30f ? ImVec4(1.0f, 0.67f, 0.16f, 0.96f)
                          : ImVec4(1.0f, 0.24f, 0.28f, 0.96f);
    draw->AddRectFilled(
        ImVec2(bar_min.x + 1.0f, bar_max.y - 1.0f -
            (bar_max.y - bar_min.y - 2.0f) * health),
        ImVec2(bar_max.x - 1.0f, bar_max.y - 1.0f),
        ImGui::GetColorU32(color), 1.0f);
}

}  // namespace

void DrawAnimalEsp(const bridge::FrameSnapshot& frame,
                   const AnimalVisualSettings& settings) {
    if (!settings.animal_esp ||
        frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (settings.ray_esp && frame.has_local_screen_position) {
        const ImVec2 source(frame.local_screen_x, frame.local_screen_y);
        for (const bridge::AnimalSnapshot& animal : frame.animals) {
            const ImVec2 target(animal.screen_x, animal.screen_y);
            DrawDirectionIndicator(draw, source, target, settings.ray_color);
        }
    }

    constexpr std::array<std::array<std::size_t, 2>, 18> links{{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 4}},
        {{1, 5}}, {{5, 6}}, {{6, 7}}, {{7, 8}},
        {{1, 9}}, {{9, 10}}, {{10, 11}}, {{11, 12}},
        {{4, 13}}, {{13, 14}}, {{14, 15}},
        {{4, 16}}, {{16, 17}}, {{17, 18}},
    }};
    for (const bridge::AnimalSnapshot& animal : frame.animals) {
        if (animal.screen_x < -100.0f || animal.screen_y < -140.0f ||
            animal.screen_x > display.x + 100.0f ||
            animal.screen_y > display.y + 100.0f) continue;
        const float height = std::max(14.0f, animal.screen_y - animal.screen_top_y);
        const float width = height * 1.05f;
        const ImVec2 minimum(animal.screen_x - width * 0.5f, animal.screen_top_y);
        const ImVec2 maximum(animal.screen_x + width * 0.5f, animal.screen_y);
        const ZombieVisualState state = StateFor(animal);
        const ImVec4& box = ResolveColor(settings.box_colors, state);
        if (settings.colored_marker) {
            ImVec4 fill = box;
            fill.w *= 0.15f;
            draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(fill), 4.0f);
            draw->AddRect(minimum, maximum, ImGui::GetColorU32(box), 4.0f, 0, 1.6f);
        }
        if (settings.show_health_bar) {
            DrawHealthBar(draw, minimum, maximum, animal.health_fraction);
        }
        if (settings.show_skeleton && animal.has_bones) {
            const ImU32 color = ImGui::GetColorU32(
                ResolveColor(settings.skeleton_colors, state));
            for (const auto& link : links) {
                const bridge::ScreenPoint& from = animal.bones[link[0]];
                const bridge::ScreenPoint& to = animal.bones[link[1]];
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y),
                              IM_COL32(5, 8, 12, 220), 3.5f);
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y),
                              color, 1.3f);
            }
        }
        std::string label = animal.name.empty() ? "动物" : animal.name;
        if (animal.baby) label += "（幼年）";
        char distance[32]{};
        std::snprintf(distance, sizeof(distance), "%.1f 格", animal.distance);
        if (settings.show_name || settings.show_distance) {
            const std::string text = settings.show_name && settings.show_distance
                ? label + "  " + distance
                : settings.show_name ? label : distance;
            const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
            const ImVec2 text_pos(animal.screen_x - text_size.x * 0.5f,
                                  minimum.y - text_size.y - 5.0f);
            draw->AddRectFilled(ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
                ImVec2(text_pos.x + text_size.x + 4.0f,
                       text_pos.y + text_size.y + 2.0f),
                IM_COL32(5, 7, 11, 205), 3.0f);
            draw->AddText(text_pos, ImGui::GetColorU32(
                ResolveColor(settings.name_colors, state)), text.c_str());
        }
    }
}

}  // namespace pztrainer::features::visual
