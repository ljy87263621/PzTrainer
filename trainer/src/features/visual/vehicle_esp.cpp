#include "features/visual/vehicle_esp.hpp"
#include "features/visual/direction_indicator.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace pztrainer::features::visual {
namespace {

ZombieVisualState StateFor(const bridge::VehicleSnapshot& vehicle) {
    if (vehicle.behind_wall) return ZombieVisualState::BehindWall;
    if (vehicle.in_view) return ZombieVisualState::InView;
    return ZombieVisualState::Default;
}

const char* DoorStatus(const bridge::VehicleSnapshot& vehicle) {
    if (vehicle.all_doors_locked) return "车门：全部上锁";
    if (vehicle.any_door_locked) return "车门：部分上锁";
    return "车门：未上锁";
}

const char* StartStatus(const bridge::VehicleSnapshot& vehicle) {
    if (vehicle.engine_running) return "启动：发动机运行中";
    if (vehicle.keys_in_ignition) return "启动：钥匙在点火器";
    if (vehicle.has_key) return "启动：持有匹配钥匙";
    if (vehicle.hotwired && !vehicle.hotwire_broken) return "启动：已完成热接线";
    if (vehicle.hotwire_broken) return "启动：热接线已损坏";
    return "启动：无可用钥匙或热接线";
}

}  // namespace

void DrawVehicleEsp(const bridge::FrameSnapshot& frame,
                    const VehicleVisualSettings& settings) {
    if (!settings.vehicle_esp ||
        frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (settings.ray_esp && frame.has_local_screen_position) {
        const ImVec2 source(frame.local_screen_x, frame.local_screen_y);
        for (const bridge::VehicleSnapshot& vehicle : frame.vehicles) {
            DrawDirectionIndicator(draw, source,
                ImVec2(vehicle.screen_x, vehicle.screen_y), settings.ray_color);
        }
    }
    for (const bridge::VehicleSnapshot& vehicle : frame.vehicles) {
        if (vehicle.screen_x < -160.0f || vehicle.screen_y < -160.0f ||
            vehicle.screen_x > display.x + 160.0f ||
            vehicle.screen_y > display.y + 160.0f) continue;
        const float height = std::max(20.0f, vehicle.screen_y - vehicle.screen_top_y);
        const float width = height * 2.15f;
        const ImVec2 minimum(vehicle.screen_x - width * 0.5f, vehicle.screen_top_y);
        const ImVec2 maximum(vehicle.screen_x + width * 0.5f, vehicle.screen_y);
        const ZombieVisualState state = StateFor(vehicle);
        const ImVec4& color = ResolveColor(settings.box_colors, state);
        if (settings.colored_marker) {
            ImVec4 fill = color;
            fill.w *= 0.14f;
            draw->AddRectFilled(minimum, maximum, ImGui::GetColorU32(fill), 4.0f);
            draw->AddRect(minimum, maximum, ImGui::GetColorU32(color), 4.0f, 0, 1.7f);
        }
        if (settings.show_engine_bar) {
            const float fraction = std::clamp(vehicle.engine_health_fraction, 0.0f, 1.0f);
            const ImVec2 bar_min(minimum.x, maximum.y + 4.0f);
            const ImVec2 bar_max(maximum.x, maximum.y + 8.0f);
            draw->AddRectFilled(bar_min, bar_max, IM_COL32(5, 7, 11, 225), 2.0f);
            const ImVec4 bar_color = fraction >= 0.6f
                ? ImVec4(0.22f, 0.88f, 0.42f, 0.96f)
                : fraction >= 0.3f ? ImVec4(1.0f, 0.67f, 0.16f, 0.96f)
                                   : ImVec4(1.0f, 0.24f, 0.28f, 0.96f);
            draw->AddRectFilled(bar_min,
                ImVec2(bar_min.x + width * fraction, bar_max.y),
                ImGui::GetColorU32(bar_color), 2.0f);
        }
        char distance[32]{};
        std::snprintf(distance, sizeof(distance), "%.1f 格", vehicle.distance);
        const std::string name = vehicle.name.empty() ? "载具" : vehicle.name;
        const std::string title = settings.show_name && settings.show_distance
            ? name + "  " + distance
            : settings.show_name ? name : settings.show_distance ? distance : "";
        float text_y = minimum.y - 5.0f;
        if (!title.empty()) {
            const ImVec2 size = ImGui::CalcTextSize(title.c_str());
            text_y -= size.y;
            draw->AddText(ImVec2(vehicle.screen_x - size.x * 0.5f, text_y),
                ImGui::GetColorU32(ResolveColor(settings.name_colors, state)),
                title.c_str());
        }
        if (settings.show_details) {
            char engine[64]{};
            std::snprintf(engine, sizeof(engine), "发动机：%d%% · %s",
                static_cast<int>(vehicle.engine_health_fraction * 100.0f + 0.5f),
                vehicle.engine_working && vehicle.operational ? "可用" : "不可用");
            const char* lines[]{
                vehicle.can_drive_now ? "可直接开走" : "当前不能直接开走",
                DoorStatus(vehicle), StartStatus(vehicle), engine};
            float y = maximum.y + (settings.show_engine_bar ? 12.0f : 5.0f);
            for (const char* line : lines) {
                const ImVec4 line_color = line == lines[0]
                    ? (vehicle.can_drive_now
                        ? ImVec4(0.28f, 0.95f, 0.58f, 1.0f)
                        : ImVec4(1.0f, 0.56f, 0.30f, 1.0f))
                    : ImVec4(0.82f, 0.86f, 0.94f, 0.96f);
                draw->AddText(ImVec2(minimum.x, y), ImGui::GetColorU32(line_color), line);
                y += ImGui::GetTextLineHeight() + 1.0f;
            }
        }
    }
}

}  // namespace pztrainer::features::visual
