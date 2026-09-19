#include "ui/player_preview.hpp"

#include <algorithm>
#include <cstdio>

#include "settings/localization.hpp"
#include "ui/game_zombie_mesh.hpp"

namespace pztrainer::ui {
namespace {

float g_player_preview_yaw = -0.42f;

ImU32 ColorWithAlpha(const ImVec4& color, float alpha_scale) {
    return ImGui::GetColorU32(
        ImVec4(color.x, color.y, color.z, color.w * alpha_scale));
}

}  // namespace

void DrawPlayerPreview(
        const ImVec2& size,
        const features::visual::PlayerVisualSettings& settings) {
    const features::visual::ZombieVisualState state = settings.preview_state;
    const ImVec4& box_color = features::visual::ResolveColor(settings.box_colors, state);
    const ImVec4& name_color = features::visual::ResolveColor(settings.name_colors, state);
    const ImVec4& distance_color = features::visual::ResolveColor(
        settings.distance_colors, state);
    const ImVec4& skeleton_color = features::visual::ResolveColor(
        settings.skeleton_colors, state);
    const ImVec4& model_color = features::visual::ResolveColor(
        settings.model_colors, state);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Player3DPreview", size);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        g_player_preview_yaw += ImGui::GetIO().MouseDelta.x * 0.012f;
    } else if (!hovered) {
        g_player_preview_yaw += ImGui::GetIO().DeltaTime * 0.24f;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, maximum,
                        ImGui::GetColorU32(ImVec4(0.016f, 0.027f, 0.047f, 0.96f)), 9.0f);
    draw->AddRect(origin, maximum,
                  ImGui::GetColorU32(ImVec4(0.13f, 0.36f, 0.95f, 0.62f)),
                  9.0f, 0, 1.0f);
    draw->PushClipRect(origin, maximum, true);

    const float base_scale = std::min(size.x * 0.30f, size.y * 0.25f);
    const ImVec2 model_center(origin.x + size.x * 0.5f,
                              origin.y + size.y * 0.72f - base_scale * 1.13f);
    ImVec2 box_min{};
    ImVec2 box_max{};
    GameZombieMesh& model = GameZombieMesh::PlayerInstance();
    ZombieMeshStyle style{};
    style.effect = settings.model_effect;
    style.color = model_color;
    style.edge_glow = settings.model_edge_glow;
    style.edge_color = settings.model_edge_glow_color;
    style.draw_base_if_disabled = true;
    const bool rendered = model.EnsureLoaded() && model.Draw(
        draw, model_center, base_scale * 3.25f, g_player_preview_yaw,
        settings.preview_animation, style, settings.show_skeleton,
        skeleton_color, box_min, box_max);

    if (rendered) {
        box_min.x = std::max(box_min.x - 8.0f, origin.x + 8.0f);
        box_min.y = std::max(box_min.y - 7.0f, origin.y + 18.0f);
        box_max.x = std::min(box_max.x + 8.0f, maximum.x - 8.0f);
        box_max.y = std::min(box_max.y + 7.0f, maximum.y - 31.0f);
        if (settings.ray_esp) {
            const ImVec2 arrow(origin.x + 27.0f, origin.y + size.y * 0.46f);
            ImVec4 glow = settings.ray_color;
            glow.w *= 0.18f;
            draw->AddCircleFilled(arrow, 18.0f, ImGui::GetColorU32(glow), 20);
            draw->AddTriangleFilled(
                ImVec2(arrow.x + 10.0f, arrow.y),
                ImVec2(arrow.x - 7.0f, arrow.y - 7.0f),
                ImVec2(arrow.x - 7.0f, arrow.y + 7.0f),
                ImGui::GetColorU32(settings.ray_color));
            draw->AddText(
                ImVec2(arrow.x + 17.0f, arrow.y - 8.0f),
                ImGui::GetColorU32(settings.ray_color),
                settings::Translate("远程玩家"));
        }
        if (settings.colored_marker) {
            draw->AddRectFilled(box_min, box_max,
                                ColorWithAlpha(box_color, 0.16f), 4.0f);
            draw->AddRect(box_min, box_max, ImGui::GetColorU32(box_color),
                          4.0f, 0, 1.4f);
        }
        if (settings.show_health_bar) {
            const ImVec2 bar_min(box_min.x - 10.0f, box_min.y);
            const ImVec2 bar_max(box_min.x - 5.0f, box_max.y);
            draw->AddRectFilled(
                bar_min, bar_max,
                ImGui::GetColorU32(ImVec4(
                    4.0f / 255.0f, 6.0f / 255.0f,
                    9.0f / 255.0f, 225.0f / 255.0f)), 2.0f);
            draw->AddRectFilled(ImVec2(bar_min.x + 1.0f, bar_min.y + 1.0f),
                                ImVec2(bar_max.x - 1.0f, bar_max.y - 1.0f),
                                ImGui::GetColorU32(ImVec4(
                                    56.0f / 255.0f, 224.0f / 255.0f,
                                    107.0f / 255.0f, 245.0f / 255.0f)), 1.0f);
        }
        if (settings.show_name || settings.show_distance) {
            const char* name = settings::Translate("远程玩家");
            char distance[32]{};
            std::snprintf(
                distance, sizeof(distance), settings::Translate("%.1f 格"),
                18.6f);
            const ImVec2 name_size = settings.show_name
                ? ImGui::CalcTextSize(name) : ImVec2{};
            const ImVec2 distance_size = settings.show_distance
                ? ImGui::CalcTextSize(distance) : ImVec2{};
            const float gap = settings.show_name && settings.show_distance ? 6.0f : 0.0f;
            const ImVec2 text_size(name_size.x + gap + distance_size.x,
                                   std::max(name_size.y, distance_size.y));
            const ImVec2 text_pos(
                box_min.x + (box_max.x - box_min.x - text_size.x) * 0.5f,
                box_min.y - text_size.y - 5.0f);
            draw->AddRectFilled(ImVec2(text_pos.x - 4.0f, text_pos.y - 2.0f),
                ImVec2(text_pos.x + text_size.x + 4.0f,
                       text_pos.y + text_size.y + 2.0f),
                ImGui::GetColorU32(ImVec4(
                    3.0f / 255.0f, 6.0f / 255.0f,
                    11.0f / 255.0f, 225.0f / 255.0f)), 3.0f);
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
    } else {
        draw->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f),
                      ImGui::GetColorU32(ImVec4(
                          1.0f, 189.0f / 255.0f,
                          89.0f / 255.0f, 1.0f)),
                      settings::Translate("玩家模型或动画资源加载失败"));
    }
    draw->AddText(ImVec2(origin.x + 12.0f, maximum.y - 24.0f),
                  ImGui::GetColorU32(ImVec4(
                      102.0f / 255.0f, 112.0f / 255.0f,
                      133.0f / 255.0f, 1.0f)),
                  settings::Translate("拖动旋转 · 玩家动画与 ESP 预览"));
    draw->PopClipRect();
}

}  // namespace pztrainer::ui
