#include "ui/zombie_preview.hpp"

#include <algorithm>
#include <cstdio>

#include "features/visual/visual_settings.hpp"
#include "settings/localization.hpp"
#include "ui/game_zombie_mesh.hpp"

namespace pztrainer::ui {
namespace {

float g_preview_yaw = -0.42f;

ImU32 ColorWithAlpha(const ImVec4& color, float alpha_scale) {
    return ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w * alpha_scale));
}

}  // namespace

void DrawZombiePreview(const ImVec2& size, const features::visual::VisualSettings& settings) {
    const features::visual::ZombieVisualState state = settings.preview_state;
    const ImVec4& box_color = features::visual::ResolveColor(settings.box_colors, state);
    const ImVec4& name_color = features::visual::ResolveColor(settings.name_colors, state);
    const ImVec4& distance_color = features::visual::ResolveColor(settings.distance_colors, state);
    const ImVec4& skeleton_color = features::visual::ResolveColor(settings.skeleton_colors, state);
    const ImVec4& model_color = features::visual::ResolveColor(settings.model_colors, state);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("Zombie3DPreview", size);
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        g_preview_yaw += ImGui::GetIO().MouseDelta.x * 0.012f;
    } else if (!hovered) {
        g_preview_yaw += ImGui::GetIO().DeltaTime * 0.24f;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, maximum,
                        ImGui::GetColorU32(ImVec4(0.016f, 0.027f, 0.047f, 0.96f)), 9.0f);
    draw->AddRect(origin, maximum,
                  ImGui::GetColorU32(ImVec4(0.13f, 0.36f, 0.95f, 0.62f)), 9.0f, 0, 1.0f);
    draw->PushClipRect(origin, maximum, true);

    const float base_scale = std::min(size.x * 0.30f, size.y * 0.25f);
    const ImVec2 model_center(origin.x + size.x * 0.5f,
                              origin.y + size.y * 0.72f - base_scale * 1.13f);
    ImVec2 box_min{};
    ImVec2 box_max{};
    GameZombieMesh& model = GameZombieMesh::Instance();
    ZombieMeshStyle style{};
    style.effect = settings.model_effect;
    style.color = model_color;
    style.edge_glow = settings.model_edge_glow;
    style.edge_color = settings.model_edge_glow_color;
    style.draw_base_if_disabled = true;
    const bool rendered = model.EnsureLoaded() && model.Draw(
        draw, model_center, base_scale * 3.25f, g_preview_yaw,
        settings.preview_animation, style, settings.show_skeleton, skeleton_color, box_min, box_max);

    if (rendered) {
        box_min.x -= 8.0f;
        box_min.y -= 7.0f;
        box_max.x += 8.0f;
        box_max.y += 7.0f;
        box_min.x = std::max(box_min.x, origin.x + 8.0f);
        box_min.y = std::max(box_min.y, origin.y + 18.0f);
        box_max.x = std::min(box_max.x, maximum.x - 8.0f);
        box_max.y = std::min(box_max.y, maximum.y - 31.0f);
        if (settings.colored_marker) {
            draw->AddRectFilled(box_min, box_max,
                                ColorWithAlpha(box_color, 0.16f), 4.0f);
            draw->AddRect(box_min, box_max,
                          ImGui::GetColorU32(box_color), 4.0f, 0, 1.4f);
        }
        if (settings.show_name || settings.show_distance) {
        const char* name = settings::Translate("僵尸");
        char distance[32]{};
        std::snprintf(
            distance, sizeof(distance), settings::Translate("%.1f 格"), 12.4f);
            const ImVec2 name_size = settings.show_name ? ImGui::CalcTextSize(name) : ImVec2{};
            const ImVec2 distance_size = settings.show_distance ? ImGui::CalcTextSize(distance) : ImVec2{};
            const float gap = settings.show_name && settings.show_distance ? 6.0f : 0.0f;
            const ImVec2 text_size(name_size.x + gap + distance_size.x,
                                   std::max(name_size.y, distance_size.y));
            const ImVec2 text_position(
                box_min.x + (box_max.x - box_min.x - text_size.x) * 0.5f,
                box_min.y - text_size.y - 5.0f);
            draw->AddRectFilled(ImVec2(text_position.x - 4.0f, text_position.y - 2.0f),
                                ImVec2(text_position.x + text_size.x + 4.0f,
                                       text_position.y + text_size.y + 2.0f),
                                ImGui::GetColorU32(ImVec4(0.012f, 0.020f, 0.035f, 0.90f)), 3.0f);
            float text_x = text_position.x;
            if (settings.show_name) {
                draw->AddText(ImVec2(text_x, text_position.y), ImGui::GetColorU32(name_color), name);
                text_x += name_size.x + gap;
            }
            if (settings.show_distance) {
                draw->AddText(ImVec2(text_x, text_position.y), ImGui::GetColorU32(distance_color), distance);
            }
        }
    } else {
        draw->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f),
                      ImGui::GetColorU32(ImVec4(1.0f, 0.74f, 0.35f, 1.0f)),
                       settings::Translate("游戏模型或动画资源加载失败"));
    }

    draw->AddText(ImVec2(origin.x + 12.0f, maximum.y - 24.0f),
                  ImGui::GetColorU32(ImVec4(0.40f, 0.44f, 0.52f, 1.0f)),
                   settings::Translate("拖动旋转 · 实时动画与 ESP 预览"));
    draw->PopClipRect();
}

}  // namespace pztrainer::ui
