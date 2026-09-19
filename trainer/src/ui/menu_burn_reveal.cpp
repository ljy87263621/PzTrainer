#include "ui/menu_burn_reveal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include "settings/ui_preferences.hpp"
#include "ui/trainer_menu.hpp"

namespace pztrainer::ui {
namespace {

using Clock = std::chrono::steady_clock;

constexpr int kRegionCount = 7;
constexpr float kInitialDelay = 0.0f;
constexpr float kBurnDuration = 0.40f;
constexpr float kRegionStagger = kBurnDuration;
constexpr float kRevealPhase = 0.72f;
constexpr float kPi = 3.14159265358979323846f;

enum class BurnDirection {
    LeftToRight,
    RightToLeft,
    TopToBottom,
};

struct BurnRegion {
    ImVec2 minimum;
    ImVec2 maximum;
    BurnDirection direction = BurnDirection::LeftToRight;
    float delay = 0.0f;
};

bool g_started = false;
Clock::time_point g_started_at{};

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float EaseOutCubic(float value) {
    const float inverse = 1.0f - Clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

float ElapsedSeconds() {
    if (!g_started) return 0.0f;
    return std::chrono::duration<float>(Clock::now() - g_started_at).count();
}

float Flicker(float elapsed, int index) {
    return 0.68f + 0.32f * std::sin(
        elapsed * 31.0f + static_cast<float>(index) * 2.173f);
}

float Wave(float elapsed, int index, float speed, float offset) {
    return 0.5f + 0.5f * std::sin(
        elapsed * speed + static_cast<float>(index) * offset);
}

void DrawFlamePoint(ImDrawList* draw, const ImVec2& point, float scale,
                    float elapsed, int index, float opacity,
                    const ImVec2& flame_direction) {
    const float flicker = Flicker(elapsed, index);
    const ImVec2 outer_point(
        point.x + flame_direction.x * (5.0f + flicker * 5.0f) * scale,
        point.y + flame_direction.y * (5.0f + flicker * 5.0f) * scale);
    draw->AddCircleFilled(
        outer_point, (13.0f + flicker * 5.0f) * scale,
        ImGui::GetColorU32(ImVec4(0.01f, 0.22f, 0.72f, opacity * 0.035f)), 20);
    draw->AddCircleFilled(
        point, (7.0f + flicker * 3.0f) * scale,
        ImGui::GetColorU32(ImVec4(0.02f, 0.42f, 1.0f, opacity * 0.11f)), 16);
    draw->AddCircleFilled(
        point, (3.1f + flicker * 1.6f) * scale,
        ImGui::GetColorU32(ImVec4(0.02f, 0.64f, 1.0f, opacity * 0.32f)), 12);
    draw->AddTriangleFilled(
        ImVec2(point.x - flame_direction.y * 2.8f * scale,
               point.y + flame_direction.x * 2.8f * scale),
        ImVec2(point.x + flame_direction.y * 2.8f * scale,
               point.y - flame_direction.x * 2.8f * scale),
        ImVec2(point.x + flame_direction.x * (12.0f + flicker * 8.0f) * scale,
               point.y + flame_direction.y * (12.0f + flicker * 8.0f) * scale),
        ImGui::GetColorU32(ImVec4(0.08f, 0.68f, 1.0f, opacity * 0.34f)));
    draw->AddCircleFilled(
        point, (1.15f + flicker * 0.65f) * scale,
        ImGui::GetColorU32(ImVec4(0.72f, 0.94f, 1.0f, opacity * 0.92f)), 10);
}

void DrawSparkTrails(ImDrawList* draw, const ImVec2& origin,
                     const ImVec2& direction, float scale, float elapsed,
                     int index, float opacity) {
    for (int spark = 0; spark < 3; ++spark) {
        const int seed = index * 5 + spark * 17;
        const float phase = Wave(elapsed, seed, 13.0f + spark * 2.7f, 1.37f);
        const float distance = (9.0f + spark * 7.0f + phase * 15.0f) * scale;
        const float lateral = (Wave(elapsed, seed + 7, 17.0f, 0.91f) - 0.5f) *
            (14.0f + spark * 5.0f) * scale;
        const ImVec2 perpendicular(-direction.y, direction.x);
        const ImVec2 head(
            origin.x + direction.x * distance + perpendicular.x * lateral,
            origin.y + direction.y * distance + perpendicular.y * lateral);
        const float trail_length = (4.0f + spark * 2.5f) * scale;
        const ImVec2 tail(
            head.x - direction.x * trail_length,
            head.y - direction.y * trail_length);
        const float spark_opacity = opacity * (0.42f + phase * 0.52f);
        draw->AddLine(
            tail, head,
            ImGui::GetColorU32(ImVec4(0.08f, 0.56f, 1.0f, spark_opacity * 0.34f)),
            3.2f * scale);
        draw->AddLine(
            tail, head,
            ImGui::GetColorU32(ImVec4(0.68f, 0.94f, 1.0f, spark_opacity)),
            1.0f * scale);
        draw->AddCircleFilled(
            head, (0.8f + phase * 0.8f) * scale,
            ImGui::GetColorU32(ImVec4(0.88f, 0.98f, 1.0f, spark_opacity)), 8);
    }
}

void DrawHorizontalBurn(ImDrawList* draw, const BurnRegion& region,
                        float progress, float elapsed, float scale,
                        bool reverse) {
    const float width = region.maximum.x - region.minimum.x;
    const float height = region.maximum.y - region.minimum.y;
    const float strip = std::max(4.0f, 7.0f * scale);
    const int strips = std::max(1, static_cast<int>(std::ceil(height / strip)));
    for (int index = 0; index < strips; ++index) {
        const float y0 = region.minimum.y + static_cast<float>(index) * strip;
        const float y1 = std::min(region.maximum.y, y0 + strip + 1.0f);
        const float edge_noise = std::sin(kPi * progress);
        const float noise = edge_noise * (std::sin(
            static_cast<float>(index) * 1.73f + elapsed * 19.0f) * 5.0f * scale +
            std::sin(static_cast<float>(index) * 0.47f - elapsed * 27.0f) * 2.5f * scale);
        float front = reverse
            ? region.maximum.x - width * progress + noise
            : region.minimum.x + width * progress + noise;
        front = std::clamp(front, region.minimum.x, region.maximum.x);
        if (reverse) {
            draw->AddRectFilled(
                ImVec2(region.minimum.x, y0), ImVec2(front, y1),
                ImGui::GetColorU32(ImVec4(0.006f, 0.011f, 0.019f, 0.992f)));
        } else {
            draw->AddRectFilled(
                ImVec2(front, y0), ImVec2(region.maximum.x, y1),
                ImGui::GetColorU32(ImVec4(0.006f, 0.011f, 0.019f, 0.992f)));
        }
        const float glow_direction = reverse ? 1.0f : -1.0f;
        const float glow_width = (18.0f + Flicker(elapsed, index) * 8.0f) * scale;
        const float glow_end = std::clamp(
            front + glow_direction * glow_width,
            region.minimum.x, region.maximum.x);
        const ImU32 transparent_blue =
            ImGui::GetColorU32(ImVec4(0.02f, 0.36f, 1.0f, 0.0f));
        const ImU32 glow_blue =
            ImGui::GetColorU32(ImVec4(0.02f, 0.48f, 1.0f, 0.18f));
        if (reverse) {
            draw->AddRectFilledMultiColor(
                ImVec2(front, y0), ImVec2(glow_end, y1),
                glow_blue, transparent_blue, transparent_blue, glow_blue);
        } else {
            draw->AddRectFilledMultiColor(
                ImVec2(glow_end, y0), ImVec2(front, y1),
                transparent_blue, glow_blue, glow_blue, transparent_blue);
        }
        if (progress > 0.01f && progress < 0.995f && index % 2 == 0) {
            const ImVec2 flame(front, (y0 + y1) * 0.5f);
            const ImVec2 direction(reverse ? 1.0f : -1.0f, 0.0f);
            DrawFlamePoint(draw, flame, scale, elapsed, index, 1.0f, direction);
            if (index % 4 == 0) {
                DrawSparkTrails(draw, flame, direction, scale, elapsed, index, 0.92f);
            }
        }
    }
}

void DrawVerticalBurn(ImDrawList* draw, const BurnRegion& region,
                      float progress, float elapsed, float scale) {
    const float width = region.maximum.x - region.minimum.x;
    const float height = region.maximum.y - region.minimum.y;
    const float strip = std::max(4.0f, 7.0f * scale);
    const int strips = std::max(1, static_cast<int>(std::ceil(width / strip)));
    for (int index = 0; index < strips; ++index) {
        const float x0 = region.minimum.x + static_cast<float>(index) * strip;
        const float x1 = std::min(region.maximum.x, x0 + strip + 1.0f);
        const float edge_noise = std::sin(kPi * progress);
        const float noise = edge_noise * (std::sin(
            static_cast<float>(index) * 1.51f + elapsed * 21.0f) * 5.0f * scale +
            std::sin(static_cast<float>(index) * 0.39f - elapsed * 24.0f) * 2.5f * scale);
        float front = region.minimum.y + height * progress + noise;
        front = std::clamp(front, region.minimum.y, region.maximum.y);
        draw->AddRectFilled(
            ImVec2(x0, front), ImVec2(x1, region.maximum.y),
            ImGui::GetColorU32(ImVec4(0.006f, 0.011f, 0.019f, 0.992f)));
        const float glow_height = (18.0f + Flicker(elapsed, index) * 8.0f) * scale;
        const float glow_top = std::clamp(
            front - glow_height, region.minimum.y, region.maximum.y);
        draw->AddRectFilledMultiColor(
            ImVec2(x0, glow_top), ImVec2(x1, front),
            ImGui::GetColorU32(ImVec4(0.02f, 0.36f, 1.0f, 0.0f)),
            ImGui::GetColorU32(ImVec4(0.02f, 0.36f, 1.0f, 0.0f)),
            ImGui::GetColorU32(ImVec4(0.02f, 0.48f, 1.0f, 0.18f)),
            ImGui::GetColorU32(ImVec4(0.02f, 0.48f, 1.0f, 0.18f)));
        if (progress > 0.01f && progress < 0.995f && index % 2 == 0) {
            const ImVec2 flame((x0 + x1) * 0.5f, front);
            const ImVec2 direction(0.0f, -1.0f);
            DrawFlamePoint(draw, flame, scale, elapsed, index, 1.0f, direction);
            if (index % 4 == 0) {
                DrawSparkTrails(draw, flame, direction, scale, elapsed, index, 0.92f);
            }
        }
    }
}

void DrawDisintegration(ImDrawList* draw, const BurnRegion& region,
                        const ImVec2& direction, float progress,
                        float elapsed, float scale) {
    if (progress <= 0.0f || progress >= 1.0f) return;

    const bool horizontal_edge = std::abs(direction.x) > 0.5f;
    const float edge_length = horizontal_edge
        ? region.maximum.y - region.minimum.y
        : region.maximum.x - region.minimum.x;
    const int shard_count = std::clamp(
        static_cast<int>(edge_length / std::max(8.0f, 11.0f * scale)),
        12, 42);
    const float opacity = 1.0f - EaseOutCubic(progress);
    const ImVec2 perpendicular(-direction.y, direction.x);

    for (int index = 0; index < shard_count; ++index) {
        const float along = (static_cast<float>(index) + 0.5f) /
            static_cast<float>(shard_count);
        ImVec2 origin;
        if (horizontal_edge) {
            origin = ImVec2(
                direction.x < 0.0f ? region.maximum.x : region.minimum.x,
                region.minimum.y + edge_length * along);
        } else {
            origin = ImVec2(
                region.minimum.x + edge_length * along,
                direction.y < 0.0f ? region.maximum.y : region.minimum.y);
        }

        const float spread =
            (Wave(elapsed, index + 31, 14.0f, 1.19f) - 0.5f) *
            (12.0f + progress * 28.0f) * scale;
        const float travel =
            (7.0f + Wave(elapsed, index + 73, 11.0f, 0.83f) * 34.0f) *
            progress * scale;
        const ImVec2 head(
            origin.x + direction.x * travel + perpendicular.x * spread,
            origin.y + direction.y * travel + perpendicular.y * spread);
        const float shard_length =
            (3.0f + Wave(elapsed, index + 11, 19.0f, 1.41f) * 7.0f) *
            (1.0f - progress * 0.55f) * scale;
        const float shard_width =
            (0.7f + Wave(elapsed, index + 19, 17.0f, 0.97f) * 1.3f) * scale;
        const ImVec2 tail(
            head.x - direction.x * shard_length,
            head.y - direction.y * shard_length);
        const ImVec2 side(
            perpendicular.x * shard_width,
            perpendicular.y * shard_width);

        draw->AddCircleFilled(
            head, (4.0f + shard_width) * scale,
            ImGui::GetColorU32(ImVec4(0.01f, 0.32f, 1.0f, opacity * 0.055f)),
            10);
        draw->AddTriangleFilled(
            head,
            ImVec2(tail.x + side.x, tail.y + side.y),
            ImVec2(tail.x - side.x, tail.y - side.y),
            ImGui::GetColorU32(ImVec4(
                0.06f, 0.62f, 1.0f,
                opacity * (0.38f + 0.42f * (index % 3 == 0)))));
        draw->AddCircleFilled(
            head, (0.65f + shard_width * 0.35f),
            ImGui::GetColorU32(ImVec4(0.78f, 0.96f, 1.0f, opacity * 0.94f)),
            8);
    }

    const float ring_opacity = opacity * std::sin(progress * kPi);
    const ImVec2 center(
        (region.minimum.x + region.maximum.x) * 0.5f,
        (region.minimum.y + region.maximum.y) * 0.5f);
    const float ring_radius =
        (10.0f + progress * std::min(edge_length * 0.22f, 72.0f * scale));
    draw->AddCircle(
        center, ring_radius,
        ImGui::GetColorU32(ImVec4(0.04f, 0.54f, 1.0f, ring_opacity * 0.22f)),
        32, 2.0f * scale);
}

std::array<BurnRegion, kRegionCount> BuildRegions(
        const TrainerMenuLayout& layout, float scale) {
    const float inset = 10.0f * scale;
    const float sidebar_width = 205.0f * scale;
    const float topbar_height = 66.0f * scale;
    const ImVec2 host_minimum = layout.host_position;
    const ImVec2 host_maximum(
        layout.host_position.x + layout.host_size.x,
        layout.host_position.y + layout.host_size.y);
    const ImVec2 content_minimum(
        host_minimum.x + sidebar_width + 12.0f * scale,
        host_minimum.y + topbar_height + 10.0f * scale);
    const ImVec2 content_maximum(
        host_maximum.x - inset,
        host_maximum.y - inset);
    const float middle_x = (content_minimum.x + content_maximum.x) * 0.5f;
    const float middle_y = (content_minimum.y + content_maximum.y) * 0.5f;
    const float gap = 4.0f * scale;

    std::array<BurnRegion, kRegionCount> regions{{
        {ImVec2(host_minimum.x + inset, host_minimum.y + inset),
         ImVec2(host_minimum.x + sidebar_width - inset, host_maximum.y - inset),
         BurnDirection::TopToBottom, kInitialDelay},
        {ImVec2(host_minimum.x + sidebar_width + inset, host_minimum.y + inset),
         ImVec2(host_maximum.x - inset, host_minimum.y + topbar_height - inset),
         BurnDirection::LeftToRight, kInitialDelay + kRegionStagger},
        {content_minimum,
         ImVec2(middle_x - gap, middle_y - gap),
         BurnDirection::LeftToRight, kInitialDelay + kRegionStagger * 2.0f},
        {ImVec2(middle_x + gap, content_minimum.y),
         ImVec2(content_maximum.x, middle_y - gap),
         BurnDirection::RightToLeft, kInitialDelay + kRegionStagger * 3.0f},
        {ImVec2(content_minimum.x, middle_y + gap),
         ImVec2(middle_x - gap, content_maximum.y),
         BurnDirection::LeftToRight, kInitialDelay + kRegionStagger * 4.0f},
        {ImVec2(middle_x + gap, middle_y + gap),
         content_maximum,
         BurnDirection::RightToLeft, kInitialDelay + kRegionStagger * 5.0f},
        {layout.detached_preview_position,
         ImVec2(layout.detached_preview_position.x + layout.detached_preview_size.x,
                layout.detached_preview_position.y + layout.detached_preview_size.y),
         BurnDirection::TopToBottom, kInitialDelay + kRegionStagger * 6.0f},
    }};
    if (!layout.detached_preview_visible) {
        regions.back().minimum = ImVec2{};
        regions.back().maximum = ImVec2{};
    }
    return regions;
}

}  // namespace

void BeginMenuBurnReveal() {
    g_started = true;
    g_started_at = Clock::now();
}

bool MenuBurnRevealComplete() {
    return g_started && ElapsedSeconds() >=
        kInitialDelay + kRegionStagger * static_cast<float>(kRegionCount - 1) +
        kBurnDuration;
}

void DrawMenuBurnReveal(const TrainerMenuLayout& layout) {
    if (!g_started || MenuBurnRevealComplete()) return;

    const float elapsed = ElapsedSeconds();
    const float scale = settings::UiScale();
    const std::array<BurnRegion, kRegionCount> regions = BuildRegions(layout, scale);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    for (const BurnRegion& region : regions) {
        if (region.maximum.x <= region.minimum.x ||
            region.maximum.y <= region.minimum.y) {
            continue;
        }
        const float local_progress = Clamp01(
            (elapsed - region.delay) / kBurnDuration);
        const float reveal_progress = Clamp01(local_progress / kRevealPhase);
        const float disintegration_progress = Clamp01(
            (local_progress - kRevealPhase) / (1.0f - kRevealPhase));
        draw->PushClipRect(region.minimum, region.maximum, true);
        if (local_progress <= kRevealPhase) {
            switch (region.direction) {
                case BurnDirection::LeftToRight:
                    DrawHorizontalBurn(
                        draw, region, reveal_progress, elapsed, scale, false);
                    break;
                case BurnDirection::RightToLeft:
                    DrawHorizontalBurn(
                        draw, region, reveal_progress, elapsed, scale, true);
                    break;
                case BurnDirection::TopToBottom:
                    DrawVerticalBurn(
                        draw, region, reveal_progress, elapsed, scale);
                    break;
            }
        } else {
            ImVec2 direction;
            switch (region.direction) {
                case BurnDirection::LeftToRight:
                    direction = ImVec2(-1.0f, 0.0f);
                    break;
                case BurnDirection::RightToLeft:
                    direction = ImVec2(1.0f, 0.0f);
                    break;
                case BurnDirection::TopToBottom:
                    direction = ImVec2(0.0f, -1.0f);
                    break;
            }
            DrawDisintegration(
                draw, region, direction, disintegration_progress, elapsed, scale);
        }
        draw->PopClipRect();
    }
}

}  // namespace pztrainer::ui
