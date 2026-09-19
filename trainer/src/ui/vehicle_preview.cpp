#include "ui/vehicle_preview.hpp"

#include "settings/localization.hpp"
#include "ui/game_asset_mesh.hpp"

namespace pztrainer::ui {
namespace {

GameAssetMesh g_vehicle_mesh(GameAssetMesh::Asset::Vehicle);
float g_vehicle_yaw = -0.68f;

}  // namespace

void DrawVehiclePreview(const ImVec2& size,
                        const features::visual::VehicleVisualSettings& settings) {
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("VehiclePreview", size);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        g_vehicle_yaw += ImGui::GetIO().MouseDelta.x * 0.012f;
    } else if (!ImGui::IsItemHovered()) {
        g_vehicle_yaw += ImGui::GetIO().DeltaTime * 0.14f;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
    draw->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(
            8.0f / 255.0f, 10.0f / 255.0f,
            15.0f / 255.0f, 235.0f / 255.0f)), 9.0f);
    draw->PushClipRect(minimum, maximum, true);
    const ImVec4& color = features::visual::ResolveColor(
        settings.model_colors, settings.preview_state);
    ImVec2 bounds_min{}, bounds_max{};
    const bool rendered = g_vehicle_mesh.EnsureLoaded() && g_vehicle_mesh.Draw(
        draw, ImVec2(minimum.x + size.x * 0.50f, minimum.y + size.y * 0.57f),
        size, g_vehicle_yaw, 0, settings.model_effect, color, false,
        settings.model_edge_glow, settings.model_edge_glow_color,
        bounds_min, bounds_max);
    if (rendered && settings.colored_marker) {
        ImVec4 fill = features::visual::ResolveColor(
            settings.box_colors, settings.preview_state);
        fill.w *= 0.09f;
        draw->AddRectFilled(bounds_min, bounds_max, ImGui::GetColorU32(fill), 4.0f);
        draw->AddRect(bounds_min, bounds_max, ImGui::GetColorU32(
            features::visual::ResolveColor(settings.box_colors, settings.preview_state)),
            4.0f, 0, 1.2f);
    }
    if (!rendered) {
        const char* text = settings::Translate("游戏载具模型加载失败");
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        draw->AddText(ImVec2(minimum.x + (size.x - text_size.x) * 0.5f,
                             minimum.y + (size.y - text_size.y) * 0.5f),
                      ImGui::GetColorU32(ImVec4(
                          238.0f / 255.0f, 104.0f / 255.0f,
                          104.0f / 255.0f, 1.0f)), text);
    }
    draw->AddText(ImVec2(minimum.x + 12.0f, minimum.y + 10.0f),
                  ImGui::GetColorU32(ImVec4(
                      154.0f / 255.0f, 164.0f / 255.0f,
                      184.0f / 255.0f, 220.0f / 255.0f)),
                   settings::Translate("游戏模型：Vehicles_CarNormal.fbx"));
    draw->PopClipRect();
}

}  // namespace pztrainer::ui
