#include "ui/aim_target_selector.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "features/aim/aim_settings.hpp"
#include "features/visual/zombie_model_effect.hpp"
#include "settings/localization.hpp"
#include "ui/components.hpp"
#include "ui/game_zombie_mesh.hpp"

namespace pztrainer::ui {
namespace {

using features::aim::TargetPoint;

struct TargetHotspot {
    TargetPoint point;
    const char* bone;
    const char* label;
};

constexpr std::array<TargetHotspot, 15> kHotspots{{
    {TargetPoint::Head, "Bip01_Head", "头部"},
    {TargetPoint::Neck, "Bip01_Neck", "颈部"},
    {TargetPoint::Chest, "Bip01_Spine1", "胸部"},
    {TargetPoint::Abdomen, "Bip01_Spine", "腹部"},
    {TargetPoint::Pelvis, "Bip01_Pelvis", "骨盆"},
    {TargetPoint::LeftArm, "Bip01_L_UpperArm", "左臂"},
    {TargetPoint::RightArm, "Bip01_R_UpperArm", "右臂"},
    {TargetPoint::LeftHand, "Bip01_L_Hand", "左手"},
    {TargetPoint::RightHand, "Bip01_R_Hand", "右手"},
    {TargetPoint::LeftThigh, "Bip01_L_Thigh", "左大腿"},
    {TargetPoint::RightThigh, "Bip01_R_Thigh", "右大腿"},
    {TargetPoint::LeftCalf, "Bip01_L_Calf", "左小腿"},
    {TargetPoint::RightCalf, "Bip01_R_Calf", "右小腿"},
    {TargetPoint::LeftFoot, "Bip01_L_Foot", "左脚"},
    {TargetPoint::RightFoot, "Bip01_R_Foot", "右脚"},
}};

float g_target_preview_yaw = -0.18f;

ImU32 Color(const ImVec4& value) {
    return ImGui::GetColorU32(value);
}

void DrawHotspot(ImDrawList* draw, const ImVec2& position, bool enabled,
                 bool hovered) {
    if (enabled) {
        draw->AddCircleFilled(position, hovered ? 14.0f : 12.0f,
                              Color(ImVec4(components::kAccent.x,
                                           components::kAccent.y,
                                           components::kAccent.z, 0.14f)));
        draw->AddCircleFilled(position, hovered ? 9.0f : 8.0f,
                              Color(ImVec4(0.10f, 0.68f, 1.0f, 0.98f)));
    } else {
        draw->AddCircleFilled(position, hovered ? 8.0f : 7.0f,
                              Color(ImVec4(0.055f, 0.070f, 0.090f, 0.96f)));
        draw->AddCircle(position, hovered ? 8.0f : 7.0f,
                        Color(ImVec4(0.58f, 0.66f, 0.76f, 0.86f)), 0, 1.2f);
    }
    const ImU32 plus = Color(enabled ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                                     : ImVec4(0.76f, 0.82f, 0.90f, 1.0f));
    draw->AddLine(ImVec2(position.x - 3.0f, position.y),
                  ImVec2(position.x + 3.0f, position.y), plus, 1.5f);
    draw->AddLine(ImVec2(position.x, position.y - 3.0f),
                  ImVec2(position.x, position.y + 3.0f), plus, 1.5f);
}

}  // namespace

void DrawAimTargetSelector(const ImVec2& requested_size,
                           std::uint32_t* target_points) {
    const ImVec2 size(std::max(requested_size.x, 220.0f),
                      std::max(requested_size.y, 320.0f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("AimTarget3DBackground", size);
    const ImVec2 cursor_after_preview = ImGui::GetCursorScreenPos();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        g_target_preview_yaw += ImGui::GetIO().MouseDelta.x * 0.012f;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(origin.x + size.x, origin.y + size.y);
    draw->AddRectFilled(origin, maximum,
                        Color(ImVec4(0.016f, 0.020f, 0.030f, 0.94f)), 8.0f);
    draw->PushClipRect(origin, maximum, true);

    const float base_scale = std::min(size.x * 0.30f, size.y * 0.21f);
    const ImVec2 model_center(origin.x + size.x * 0.5f,
                              origin.y + size.y * 0.70f - base_scale * 1.02f);
    GameZombieMesh& model = GameZombieMesh::Instance();
    ZombieMeshStyle style{};
    style.effect = features::visual::ZombieModelEffect::Disabled;
    style.draw_base_if_disabled = true;
    ImVec2 bounds_min{};
    ImVec2 bounds_max{};
    const bool rendered = model.EnsureLoaded() && model.Draw(
        draw, model_center, base_scale * 3.10f, g_target_preview_yaw, 0,
        style, false, ImVec4{}, bounds_min, bounds_max);

    if (rendered && target_points != nullptr) {
        for (std::size_t index = 0; index < kHotspots.size(); ++index) {
            const TargetHotspot& hotspot = kHotspots[index];
            ImVec2 position{};
            if (!model.ProjectBone(hotspot.bone, model_center, base_scale * 3.10f,
                                   g_target_preview_yaw, position)) {
                continue;
            }
            if (position.x < origin.x + 8.0f || position.x > maximum.x - 8.0f ||
                position.y < origin.y + 8.0f || position.y > maximum.y - 8.0f) {
                continue;
            }
            ImGui::PushID(static_cast<int>(index));
            ImGui::SetCursorScreenPos(ImVec2(position.x - 11.0f, position.y - 11.0f));
            ImGui::InvisibleButton("target_point", ImVec2(22.0f, 22.0f));
            const bool hovered = ImGui::IsItemHovered();
            const std::uint32_t bit = static_cast<std::uint32_t>(hotspot.point);
            if (ImGui::IsItemClicked()) *target_points ^= bit;
            if (hovered) components::RoundedTooltip(hotspot.label);
            DrawHotspot(draw, position, (*target_points & bit) != 0, hovered);
            ImGui::PopID();
        }
    } else if (!rendered) {
        draw->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f),
                      Color(ImVec4(1.0f, 0.72f, 0.34f, 1.0f)),
                       settings::Translate("3D 模型资源加载失败"));
    }
    draw->PopClipRect();
    ImGui::SetCursorScreenPos(cursor_after_preview);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

}  // namespace pztrainer::ui
