#include "features/visual/player_esp.hpp"
#include "features/visual/direction_indicator.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace pztrainer::features::visual {
namespace {

ImU32 ColorWithAlpha(const ImVec4& color, float alpha_scale) {
    return ImGui::GetColorU32(
        ImVec4(color.x, color.y, color.z, color.w * alpha_scale));
}

ZombieVisualState StateFor(const bridge::PlayerSnapshot& player) {
    if (player.behind_wall) return ZombieVisualState::BehindWall;
    if (player.in_view) return ZombieVisualState::InView;
    return ZombieVisualState::Default;
}

ImVec4 HealthBarColor(float health_fraction) {
    if (health_fraction >= 0.60f) return ImVec4(0.22f, 0.88f, 0.42f, 0.96f);
    if (health_fraction >= 0.30f) return ImVec4(1.00f, 0.67f, 0.16f, 0.96f);
    return ImVec4(1.00f, 0.24f, 0.28f, 0.96f);
}

const ImVec4& AdminColor() {
    static const ImVec4 color(1.00f, 0.72f, 0.18f, 1.00f);
    return color;
}

std::string DisplayName(const bridge::PlayerSnapshot& player,
                        bool show_admin_marker) {
    std::string name = player.name.empty() ? "玩家" : player.name;
    if (show_admin_marker && player.admin) name = "[ADMIN] " + name;
    if (player.invisible) name += " [隐身]";
    if (player.pvp_enabled) name += " [PVP]";
    return name;
}

void DrawHealthBar(ImDrawList* draw, const ImVec2& top_left,
                   const ImVec2& bottom_right, float health_fraction) {
    constexpr float width = 5.0f;
    constexpr float gap = 5.0f;
    constexpr float inset = 1.0f;
    const float health = std::clamp(health_fraction, 0.0f, 1.0f);
    const ImVec2 bar_min(top_left.x - gap - width, top_left.y);
    const ImVec2 bar_max(top_left.x - gap, bottom_right.y);
    draw->AddRectFilled(bar_min, bar_max, IM_COL32(4, 6, 9, 225), 2.0f);
    const float inner_height = std::max(0.0f, bar_max.y - bar_min.y - inset * 2.0f);
    const ImVec2 fill_min(bar_min.x + inset,
                          bar_max.y - inset - inner_height * health);
    const ImVec2 fill_max(bar_max.x - inset, bar_max.y - inset);
    if (fill_max.y > fill_min.y) {
        draw->AddRectFilled(fill_min, fill_max,
                            ImGui::GetColorU32(HealthBarColor(health)), 1.0f);
    }
}

}  // namespace

void DrawPlayerEsp(const bridge::FrameSnapshot& frame,
                   const PlayerVisualSettings& settings) {
    if (!settings.player_esp ||
        frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (settings.ray_esp && frame.has_local_screen_position) {
        const ImVec2 source(frame.local_screen_x, frame.local_screen_y);
        for (const bridge::PlayerSnapshot& player : frame.players) {
            const ImVec2 target(player.screen_x, player.screen_y);
            const bool highlighted_admin =
                settings.show_admin_marker && player.admin;
            const ImVec4& arrow_color = highlighted_admin
                ? AdminColor() : settings.ray_color;
            const std::string name = DisplayName(
                player, settings.show_admin_marker);
            DrawNearbyDirectionIndicator(draw, source, target, arrow_color,
                                         name.c_str());
        }
    }

    for (const bridge::PlayerSnapshot& player : frame.players) {
        if (player.screen_x < -80.0f || player.screen_y < -120.0f ||
            player.screen_x > display.x + 80.0f || player.screen_y > display.y + 80.0f) {
            continue;
        }
        const float height = std::max(12.0f, player.screen_y - player.screen_top_y);
        const float width = height * (player.prone ? 1.65f : 0.42f);
        const ImVec2 top_left(player.screen_x - width * 0.5f, player.screen_top_y);
        const ImVec2 bottom_right(player.screen_x + width * 0.5f, player.screen_y);
        const ZombieVisualState state = StateFor(player);
        const ImVec4& box_color = ResolveColor(settings.box_colors, state);
        const bool highlighted_admin = settings.show_admin_marker && player.admin;
        const ImVec4& name_color = highlighted_admin
            ? AdminColor() : ResolveColor(settings.name_colors, state);
        const ImVec4& distance_color = ResolveColor(settings.distance_colors, state);
        const ImVec4& skeleton_color = ResolveColor(settings.skeleton_colors, state);

        if (settings.colored_marker) {
            draw->AddRectFilled(top_left, bottom_right,
                                ColorWithAlpha(box_color, 0.16f), 3.0f);
            draw->AddRect(top_left, bottom_right, ImGui::GetColorU32(box_color),
                          3.0f, 0, 1.6f);
            draw->AddCircleFilled(ImVec2(player.screen_x, player.screen_y), 3.2f,
                                  ImGui::GetColorU32(box_color));
        }
        if (highlighted_admin) {
            const ImVec4& admin_color = AdminColor();
            draw->AddRect(
                ImVec2(top_left.x - 2.0f, top_left.y - 2.0f),
                ImVec2(bottom_right.x + 2.0f, bottom_right.y + 2.0f),
                ImGui::GetColorU32(admin_color), 4.0f, 0, 2.2f);
            const ImVec2 badge(player.screen_x, top_left.y - 7.0f);
            draw->AddCircleFilled(badge, 4.5f,
                                  ImGui::GetColorU32(admin_color), 12);
            draw->AddCircle(badge, 4.5f, IM_COL32(255, 250, 220, 235), 12, 1.0f);
        }
        if (settings.show_health_bar) {
            DrawHealthBar(draw, top_left, bottom_right, player.health_fraction);
        }
        if (settings.show_skeleton && player.has_bones) {
            constexpr std::array<std::array<std::size_t, 2>, 18> links{{
                {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 4}},
                {{1, 5}}, {{5, 6}}, {{6, 7}}, {{7, 8}},
                {{1, 9}}, {{9, 10}}, {{10, 11}}, {{11, 12}},
                {{4, 13}}, {{13, 14}}, {{14, 15}},
                {{4, 16}}, {{16, 17}}, {{17, 18}},
            }};
            const ImU32 color = ImGui::GetColorU32(skeleton_color);
            for (const auto& link : links) {
                const bridge::ScreenPoint& from = player.bones[link[0]];
                const bridge::ScreenPoint& to = player.bones[link[1]];
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y),
                              IM_COL32(5, 8, 12, 220), 3.5f);
                draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), color, 1.3f);
            }
        }
        if (settings.show_name || settings.show_distance) {
            const std::string display_name = DisplayName(
                player, settings.show_admin_marker);
            const char* name = display_name.c_str();
            char distance[48]{};
            std::snprintf(distance, sizeof(distance), "%.1f 格", player.distance);
            const ImVec2 name_size = settings.show_name
                ? ImGui::CalcTextSize(name) : ImVec2{};
            const ImVec2 distance_size = settings.show_distance
                ? ImGui::CalcTextSize(distance) : ImVec2{};
            const float gap = settings.show_name && settings.show_distance ? 6.0f : 0.0f;
            const ImVec2 text_size(name_size.x + gap + distance_size.x,
                                   std::max(name_size.y, distance_size.y));
            const ImVec2 text_pos(player.screen_x - text_size.x * 0.5f,
                                  top_left.y - text_size.y - 4.0f);
            draw->AddRectFilled(ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
                ImVec2(text_pos.x + text_size.x + 4.0f,
                       text_pos.y + text_size.y + 2.0f),
                IM_COL32(5, 7, 11, 205), 3.0f);
            float text_x = text_pos.x;
            if (settings.show_name) {
                draw->AddText(ImVec2(text_x, text_pos.y),
                              ImGui::GetColorU32(name_color), name);
                text_x += name_size.x + gap;
            }
            if (settings.show_distance) {
                draw->AddText(ImVec2(text_x, text_pos.y),
                              ImGui::GetColorU32(distance_color), distance);
            }
        }
    }
}

}  // namespace pztrainer::features::visual
