#include "ui/world_visual.hpp"

#include <imgui.h>

#include "bridge/world_visual_bridge.hpp"
#include "bridge/world_map_reveal_bridge.hpp"
#include "bridge/world_map_player_bridge.hpp"
#include "features/visual/weapon_ray.hpp"
#include "settings/localization.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/color_picker.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

const char* FogModeLabel(bridge::WeatherVisualMode mode) {
    switch (mode) {
        case bridge::WeatherVisualMode::ForceOff: return settings::Translate("强制关闭");
        case bridge::WeatherVisualMode::ForceOn: return settings::Translate("强制开启");
        default: return settings::Translate("跟随游戏");
    }
}

const char* PrecipitationModeLabel(bridge::PrecipitationVisualMode mode) {
    switch (mode) {
        case bridge::PrecipitationVisualMode::ForceOff: return settings::Translate("强制关闭");
        case bridge::PrecipitationVisualMode::Rain: return settings::Translate("强制下雨");
        case bridge::PrecipitationVisualMode::Snow: return settings::Translate("强制下雪");
        default: return settings::Translate("跟随游戏");
    }
}

void DrawWeatherOptions(const bridge::WorldVisualStatus& status) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(settings::Translate("雾效果"));
    ImGui::SameLine(116.0f);
    ImGui::SetNextItemWidth(-1.0f);
    bridge::WeatherVisualMode fog_mode = status.fog_mode;
    if (BeginAnimatedCombo("##FogMode", FogModeLabel(fog_mode), 98.0f)) {
        constexpr bridge::WeatherVisualMode modes[]{
            bridge::WeatherVisualMode::FollowGame,
            bridge::WeatherVisualMode::ForceOff,
            bridge::WeatherVisualMode::ForceOn,
        };
        for (const bridge::WeatherVisualMode mode : modes) {
            if (ImGui::Selectable(FogModeLabel(mode), mode == fog_mode)) {
                bridge::SetFogVisualMode(mode);
                CloseAnimatedDropdown();
            }
        }
        EndAnimatedDropdown();
    }

    if (fog_mode == bridge::WeatherVisualMode::ForceOn) {
        float intensity = status.fog_intensity;
        if (components::StepperRow("雾强度", &intensity, 0.05f, 1.0f, 0.05f,
                                   "%.2f")) {
            bridge::SetFogVisualIntensity(intensity);
        }
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(settings::Translate("降水效果"));
    ImGui::SameLine(116.0f);
    ImGui::SetNextItemWidth(-1.0f);
    bridge::PrecipitationVisualMode precipitation_mode =
        status.precipitation_mode;
    if (BeginAnimatedCombo(
            "##PrecipitationMode",
            PrecipitationModeLabel(precipitation_mode), 126.0f)) {
        constexpr bridge::PrecipitationVisualMode modes[]{
            bridge::PrecipitationVisualMode::FollowGame,
            bridge::PrecipitationVisualMode::ForceOff,
            bridge::PrecipitationVisualMode::Rain,
            bridge::PrecipitationVisualMode::Snow,
        };
        for (const bridge::PrecipitationVisualMode mode : modes) {
            if (ImGui::Selectable(
                    PrecipitationModeLabel(mode), mode == precipitation_mode)) {
                bridge::SetPrecipitationVisualMode(mode);
                CloseAnimatedDropdown();
            }
        }
        EndAnimatedDropdown();
    }

    if (precipitation_mode == bridge::PrecipitationVisualMode::Rain ||
        precipitation_mode == bridge::PrecipitationVisualMode::Snow) {
        float intensity = status.precipitation_intensity;
        if (components::StepperRow("降水强度", &intensity, 0.05f, 1.0f,
                                   0.05f, "%.2f", false)) {
            bridge::SetPrecipitationVisualIntensity(intensity);
        }
    }
}

}  // namespace

void DrawWorldVisual() {
    const bridge::WorldVisualStatus& status = bridge::GetWorldVisualStatus();
    const bridge::WorldMapRevealStatus& map_status =
        bridge::GetWorldMapRevealStatus();
    const bridge::WorldMapPlayerStatus& map_player_status =
        bridge::GetWorldMapPlayerStatus();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f));
    if (!ImGui::BeginTable(
            "WorldVisualCards", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("效果");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

    components::BeginCompactCard(
        "WorldVisualEffects", nullptr, ImVec2(0.0f, 0.0f));
    bool fake_daylight = status.fake_daylight_enabled;
    if (components::CompactToggleRow("伪白天", &fake_daylight)) {
        bridge::SetFakeDaylightEnabled(fake_daylight);
    }

    bool dark_area = status.dark_area_brightness_enabled;
    if (components::CompactToggleRow("暗区增亮", &dark_area)) {
        bridge::SetDarkAreaBrightnessEnabled(dark_area);
    }

    bool force_360_vision = status.force_360_vision_enabled;
    if (components::CompactToggleRow(
            "360°全向视野", &force_360_vision)) {
        bridge::SetForce360VisionEnabled(force_360_vision);
    }

    bool reveal_unexplored = status.reveal_unexplored_enabled;
    if (components::CompactToggleRow(
            "移除未探索黑幕", &reveal_unexplored, false)) {
        bridge::SetRevealUnexploredEnabled(reveal_unexplored);
    }

    bool reveal_world_map = map_status.enabled;
    if (components::CompactToggleRow(
            "解锁世界地图迷雾", &reveal_world_map, false)) {
        bridge::SetWorldMapRevealEnabled(reveal_world_map);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "只解除本地世界地图的未探索遮罩，单机和联机均可使用。关闭后恢复原来的地图迷雾设置。");
    }
    if (map_status.enabled) {
        ImGui::TextDisabled(
            "%s", settings::Translate(map_status.message.c_str()));
    }

    bool show_map_players = map_player_status.enabled;
    if (components::CompactToggleRow(
            "地图显示其他玩家", &show_map_players, false)) {
        bridge::SetWorldMapPlayerEnabled(show_map_players);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "调用原版大地图和小地图玩家标记。可显示客户端已经同步到的其他玩家；服务器未下发的位置无法显示。大地图或小地图内的“控制玩家”选项需要保持开启。");
    }
    if (map_player_status.enabled) {
        ImGui::TextDisabled(
            "%s", settings::Translate(map_player_status.message.c_str()));
    }
    components::EndCard();

    ImGui::TableSetColumnIndex(1);
    components::SectionLabel("天气选项");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "WorldWeatherEffects", nullptr, ImVec2(0.0f, 0.0f));
    DrawWeatherOptions(status);
    components::EndCard();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("枪口射线");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "WeaponRayEffects", nullptr, ImVec2(0.0f, 0.0f));
    features::visual::WeaponRaySettings& ray =
        features::visual::GetWeaponRaySettings();
    components::CompactToggleRow("显示枪口射线", &ray.enabled);
    const float color_button_x = ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x - 22.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(settings::Translate("射线颜色"));
    ImGui::SameLine();
    ImGui::SetCursorPosX(color_button_x);
    DrawColorSwatch("WeaponRayColor", &ray.color, ImVec2(22.0f, 22.0f));
    components::EndCard();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

}  // namespace pztrainer::ui
