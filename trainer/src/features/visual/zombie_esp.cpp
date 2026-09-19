#include "features/visual/zombie_esp.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace pztrainer::features::visual {
namespace {

ImU32 ColorWithAlpha(const ImVec4& color, float alpha_scale) {
    return ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w * alpha_scale));
}

ZombieVisualState StateFor(const bridge::ZombieSnapshot& zombie) {
    if (zombie.behind_wall) return ZombieVisualState::BehindWall;
    if (zombie.in_view) return ZombieVisualState::InView;
    return ZombieVisualState::Default;
}

ImVec4 HealthBarColor(float health_fraction) {
    if (health_fraction >= 0.60f) return ImVec4(0.22f, 0.88f, 0.42f, 0.96f);
    if (health_fraction >= 0.30f) return ImVec4(1.00f, 0.67f, 0.16f, 0.96f);
    return ImVec4(1.00f, 0.24f, 0.28f, 0.96f);
}

void DrawHealthBar(ImDrawList* draw_list, const ImVec2& top_left,
                   const ImVec2& bottom_right, float health_fraction) {
    constexpr float width = 5.0f;
    constexpr float gap = 5.0f;
    constexpr float inset = 1.0f;
    const float clamped_health = std::clamp(health_fraction, 0.0f, 1.0f);
    const ImVec2 bar_minimum(top_left.x - gap - width, top_left.y);
    const ImVec2 bar_maximum(top_left.x - gap, bottom_right.y);
    draw_list->AddRectFilled(bar_minimum, bar_maximum, IM_COL32(4, 6, 9, 225), 2.0f);
    const float inner_height = std::max(0.0f, bar_maximum.y - bar_minimum.y - inset * 2.0f);
    const ImVec2 fill_minimum(
        bar_minimum.x + inset,
        bar_maximum.y - inset - inner_height * clamped_health);
    const ImVec2 fill_maximum(bar_maximum.x - inset, bar_maximum.y - inset);
    if (fill_maximum.y > fill_minimum.y) {
        draw_list->AddRectFilled(
            fill_minimum, fill_maximum,
            ImGui::GetColorU32(HealthBarColor(clamped_health)), 1.0f);
    }
}

}  // namespace

void DrawZombieEsp(const bridge::FrameSnapshot& frame, const VisualSettings& settings) {
    if (!settings.zombie_esp ||
        frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        return;
    }

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    for (const bridge::ZombieSnapshot& zombie : frame.zombies) {
        if (zombie.screen_x < -80.0f || zombie.screen_y < -120.0f ||
            zombie.screen_x > display_size.x + 80.0f ||
            zombie.screen_y > display_size.y + 80.0f) {
            continue;
        }

        const float projected_height = std::max(12.0f, zombie.screen_y - zombie.screen_top_y);
        const float width = projected_height * (zombie.prone ? 1.65f : 0.42f);
        const ImVec2 top_left(zombie.screen_x - width * 0.5f, zombie.screen_top_y);
        const ImVec2 bottom_right(zombie.screen_x + width * 0.5f, zombie.screen_y);
        const ZombieVisualState state = StateFor(zombie);
        const ImVec4& box_color = ResolveColor(settings.box_colors, state);
        const ImVec4& name_color = ResolveColor(settings.name_colors, state);
        const ImVec4& distance_color = ResolveColor(settings.distance_colors, state);
        const ImVec4& skeleton_color = ResolveColor(settings.skeleton_colors, state);

        if (settings.colored_marker) {
            draw_list->AddRectFilled(top_left, bottom_right, ColorWithAlpha(box_color, 0.16f), 3.0f);
            draw_list->AddRect(top_left, bottom_right, ImGui::GetColorU32(box_color), 3.0f, 0, 1.6f);
            draw_list->AddCircleFilled(
                ImVec2(zombie.screen_x, zombie.screen_y), 3.2f, ImGui::GetColorU32(box_color));
        }

        if (settings.show_health_bar) {
            DrawHealthBar(draw_list, top_left, bottom_right, zombie.health_fraction);
        }

        if (settings.show_skeleton && zombie.has_bones) {
            constexpr std::array<std::array<std::size_t, 2>, 18> links{{
                {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 4}},
                {{1, 5}}, {{5, 6}}, {{6, 7}}, {{7, 8}},
                {{1, 9}}, {{9, 10}}, {{10, 11}}, {{11, 12}},
                {{4, 13}}, {{13, 14}}, {{14, 15}},
                {{4, 16}}, {{16, 17}}, {{17, 18}},
            }};
            const ImU32 bone_color = ImGui::GetColorU32(skeleton_color);
            const auto bone = [draw_list, bone_color](const ImVec2& from, const ImVec2& to) {
                draw_list->AddLine(from, to, IM_COL32(5, 8, 12, 220), 3.5f);
                draw_list->AddLine(from, to, bone_color, 1.3f);
            };
            for (const auto& link : links) {
                const bridge::ScreenPoint& from = zombie.bones[link[0]];
                const bridge::ScreenPoint& to = zombie.bones[link[1]];
                bone(ImVec2(from.x, from.y), ImVec2(to.x, to.y));
            }
        }

        if (settings.show_name || settings.show_distance) {
            constexpr const char* name = "僵尸";
            char distance[48]{};
            std::snprintf(distance, sizeof(distance), "%.1f 格", zombie.distance);
            const ImVec2 name_size = settings.show_name ? ImGui::CalcTextSize(name) : ImVec2{};
            const ImVec2 distance_size = settings.show_distance ? ImGui::CalcTextSize(distance) : ImVec2{};
            const float gap = settings.show_name && settings.show_distance ? 6.0f : 0.0f;
            const ImVec2 text_size(name_size.x + gap + distance_size.x,
                                   std::max(name_size.y, distance_size.y));
            const ImVec2 text_pos(
                zombie.screen_x - text_size.x * 0.5f,
                top_left.y - text_size.y - 4.0f);
            draw_list->AddRectFilled(
                ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
                ImVec2(text_pos.x + text_size.x + 4.0f, text_pos.y + text_size.y + 2.0f),
                IM_COL32(5, 7, 11, 205),
                3.0f);
            float text_x = text_pos.x;
            if (settings.show_name) {
                draw_list->AddText(ImVec2(text_x, text_pos.y), ImGui::GetColorU32(name_color), name);
                text_x += name_size.x + gap;
            }
            if (settings.show_distance) {
                draw_list->AddText(ImVec2(text_x, text_pos.y), ImGui::GetColorU32(distance_color), distance);
            }
        }
    }
}

}  // namespace pztrainer::features::visual
