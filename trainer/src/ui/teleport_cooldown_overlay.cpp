#include "ui/teleport_cooldown_overlay.hpp"

#include <imgui.h>

#include "bridge/player_teleport_bridge.hpp"
#include "settings/localization.hpp"

namespace pztrainer::ui {

void DrawTeleportCooldownOverlay() {
    const bridge::PlayerTeleportStatus& status =
        bridge::GetPlayerTeleportStatus();
    if (!status.cooldown_active) return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, 42.0f), ImGuiCond_Always,
        ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.86f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##TeleportCooldownOverlay", nullptr, flags)) {
        ImGui::Text(
            settings::Translate("下一次传送需要等待 %u 秒"),
            status.cooldown_remaining_seconds);
    }
    ImGui::End();
}

}  // namespace pztrainer::ui
