#include "ui/menu_startup_animation.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>

#include "settings/ui_preferences.hpp"
#include "ui/glass_blur.hpp"
#include "ui/trainer_menu.hpp"

namespace pztrainer::ui {
namespace {

using Clock = std::chrono::steady_clock;

constexpr float kSpinnerDuration = 4.0f;
constexpr float kExpandDuration = 1.45f;
constexpr float kOverlayFadeDuration = 0.62f;
constexpr float kSplashSize = 230.0f;
constexpr float kPi = 3.14159265358979323846f;

bool g_started = false;
Clock::time_point g_started_at{};

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float EaseOutCubic(float value) {
    const float inverse = 1.0f - Clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

float Lerp(float from, float to, float amount) {
    return from + (to - from) * amount;
}

ImVec2 Lerp(const ImVec2& from, const ImVec2& to, float amount) {
    return ImVec2(Lerp(from.x, to.x, amount), Lerp(from.y, to.y, amount));
}

float ElapsedSeconds() {
    if (!g_started) return 0.0f;
    return std::chrono::duration<float>(Clock::now() - g_started_at).count();
}

ImVec2 RoundedPerimeterPoint(const ImVec2& minimum, const ImVec2& maximum,
                             float radius, float phase) {
    const float width = maximum.x - minimum.x;
    const float height = maximum.y - minimum.y;
    radius = std::min(radius, std::min(width, height) * 0.5f);
    const float horizontal = std::max(0.0f, width - radius * 2.0f);
    const float vertical = std::max(0.0f, height - radius * 2.0f);
    const float arc = radius * kPi * 0.5f;
    const float perimeter = horizontal * 2.0f + vertical * 2.0f + arc * 4.0f;
    float distance = std::fmod(phase, 1.0f);
    if (distance < 0.0f) distance += 1.0f;
    distance *= perimeter;

    const auto arc_point = [&](const ImVec2& center, float start, float amount) {
        const float angle = start + amount;
        return ImVec2(center.x + std::cos(angle) * radius,
                      center.y + std::sin(angle) * radius);
    };
    if (distance <= horizontal) return ImVec2(minimum.x + radius + distance, minimum.y);
    distance -= horizontal;
    if (distance <= arc) {
        return arc_point(ImVec2(maximum.x - radius, minimum.y + radius),
                         -kPi * 0.5f, distance / radius);
    }
    distance -= arc;
    if (distance <= vertical) return ImVec2(maximum.x, minimum.y + radius + distance);
    distance -= vertical;
    if (distance <= arc) {
        return arc_point(ImVec2(maximum.x - radius, maximum.y - radius),
                         0.0f, distance / radius);
    }
    distance -= arc;
    if (distance <= horizontal) return ImVec2(maximum.x - radius - distance, maximum.y);
    distance -= horizontal;
    if (distance <= arc) {
        return arc_point(ImVec2(minimum.x + radius, maximum.y - radius),
                         kPi * 0.5f, distance / radius);
    }
    distance -= arc;
    if (distance <= vertical) return ImVec2(minimum.x, maximum.y - radius - distance);
    distance -= vertical;
    return arc_point(ImVec2(minimum.x + radius, minimum.y + radius),
                     kPi, distance / radius);
}

void DrawSpinner(ImDrawList* draw, const ImVec2& center, float elapsed,
                 float opacity, float scale) {
    const float radius = 18.0f * scale;
    const float start = elapsed * 4.35f;
    const float sweep = 2.05f;
    draw->PathArcTo(center, radius, 0.0f, kPi * 2.0f, 64);
    draw->PathStroke(ImGui::GetColorU32(ImVec4(0.05f, 0.18f, 0.27f, opacity * 0.65f)),
                     ImDrawFlags_None, 4.2f * scale);
    for (int layer = 3; layer >= 1; --layer) {
        draw->PathArcTo(center, radius, start, start + sweep, 28);
        draw->PathStroke(
            ImGui::GetColorU32(ImVec4(0.05f, 0.62f, 1.0f,
                opacity * (0.055f * static_cast<float>(layer)))),
            ImDrawFlags_None,
            (4.0f + static_cast<float>(layer) * 3.0f) * scale);
    }
    draw->PathArcTo(center, radius, start, start + sweep, 28);
    draw->PathStroke(ImGui::GetColorU32(ImVec4(0.04f, 0.66f, 1.0f, opacity)),
                     ImDrawFlags_None, 4.0f * scale);
}

void DrawHostSurface(ImDrawList* draw, const ImVec2& minimum,
                     const ImVec2& maximum, float rounding,
                     float opacity, float startup_darkness) {
    ImVec4 window_background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    window_background.w *= opacity;
    draw->AddRectFilled(
        minimum, maximum, ImGui::GetColorU32(window_background), rounding);

    ImGuiStyle& style = ImGui::GetStyle();
    const float previous_alpha = style.Alpha;
    style.Alpha = previous_alpha * opacity;
    DrawGlassPanel(draw, minimum, maximum, rounding);
    style.Alpha = previous_alpha;

    if (startup_darkness > 0.001f) {
        draw->AddRectFilled(
            minimum, maximum,
            ImGui::GetColorU32(ImVec4(0.0f, 0.015f, 0.027f, startup_darkness)),
            rounding);
    }
    ImVec4 border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    border.w *= opacity;
    draw->AddRect(minimum, maximum, ImGui::GetColorU32(border),
                  rounding, 0, 1.0f);
}

}  // namespace

void BeginMenuStartupAnimation() {
    g_started = true;
    g_started_at = Clock::now();
}

bool MenuStartupAnimationAllowsMenu() {
    return g_started && ElapsedSeconds() >= kSpinnerDuration + kExpandDuration;
}

bool MenuStartupAnimationComplete() {
    return g_started && ElapsedSeconds() >=
        kSpinnerDuration + kExpandDuration + kOverlayFadeDuration;
}

void DrawMenuStartupAnimation() {
    if (!g_started || MenuStartupAnimationComplete()) return;

    const float elapsed = ElapsedSeconds();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float ui_scale = settings::UiScale();
    const float raw_expansion = Clamp01(
        (elapsed - kSpinnerDuration) / kExpandDuration);
    const float expansion = EaseOutCubic(raw_expansion);
    const float fade = 1.0f - EaseOutCubic(
        (elapsed - kSpinnerDuration - kExpandDuration) / kOverlayFadeDuration);
    const TrainerMenuLayout layout = GetTrainerMenuLayout();
    const ImVec2 splash_center(display.x * 0.5f, display.y * 0.5f);
    const ImVec2 target_center(
        layout.host_position.x + layout.host_size.x * 0.5f,
        layout.host_position.y + layout.host_size.y * 0.5f);
    const ImVec2 center = Lerp(splash_center, target_center, expansion);
    const float width = kSplashSize * ui_scale +
        (layout.host_size.x - kSplashSize * ui_scale) * expansion;
    const float height = kSplashSize * ui_scale +
        (layout.host_size.y - kSplashSize * ui_scale) * expansion;
    const ImVec2 minimum(center.x - width * 0.5f, center.y - height * 0.5f);
    const ImVec2 maximum(center.x + width * 0.5f, center.y + height * 0.5f);
    const float rounding = Lerp(18.0f * ui_scale,
                                ImGui::GetStyle().WindowRounding, expansion);
    ImDrawList* draw = ImGui::GetForegroundDrawList();

    draw->AddRectFilled(ImVec2(0.0f, 0.0f), display,
                        ImGui::GetColorU32(ImVec4(0.0f, 0.008f, 0.016f, 0.16f * fade)));
    for (int layer = 8; layer >= 1; --layer) {
        const float spread = static_cast<float>(layer) * 3.0f * ui_scale;
        draw->AddRect(
            ImVec2(minimum.x - spread, minimum.y - spread),
            ImVec2(maximum.x + spread, maximum.y + spread),
            ImGui::GetColorU32(ImVec4(0.04f, 0.48f, 0.92f,
                fade * (0.010f + static_cast<float>(9 - layer) * 0.004f))),
            rounding + spread, 0, 2.0f * ui_scale);
    }
    DrawHostSurface(
        draw, minimum, maximum, rounding, fade,
        fade * (1.0f - expansion) * 0.22f);
    draw->AddRect(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(
            0.08f, 0.50f, 0.82f, 0.34f * fade * (1.0f - expansion))),
        rounding, 0, 1.2f * ui_scale);

    const float orbit_opacity = fade * (1.0f - expansion);
    for (int trail = 9; trail >= 0; --trail) {
        const float amount = 1.0f - static_cast<float>(trail) / 10.0f;
        const ImVec2 point = RoundedPerimeterPoint(
            minimum, maximum, rounding,
            elapsed * 0.30f - static_cast<float>(trail) * 0.012f);
        draw->AddCircleFilled(
            point,
            (2.0f + amount * 5.0f) * ui_scale,
            ImGui::GetColorU32(ImVec4(0.06f, 0.58f, 1.0f,
                orbit_opacity * amount * 0.16f)), 20);
    }

    const float detached_progress = EaseOutCubic(
        (raw_expansion - 0.42f) / 0.58f);
    if (layout.detached_preview_visible && detached_progress > 0.001f) {
        const ImVec2 host_edge(maximum.x, center.y);
        const ImVec2 detached_center(
            layout.detached_preview_position.x +
                layout.detached_preview_size.x * 0.5f,
            layout.detached_preview_position.y +
                layout.detached_preview_size.y * 0.5f);
        const ImVec2 animated_center = Lerp(
            host_edge, detached_center, detached_progress);
        const ImVec2 animated_size(
            layout.detached_preview_size.x * detached_progress,
            layout.detached_preview_size.y * detached_progress);
        const ImVec2 detached_minimum(
            animated_center.x - animated_size.x * 0.5f,
            animated_center.y - animated_size.y * 0.5f);
        const ImVec2 detached_maximum(
            animated_center.x + animated_size.x * 0.5f,
            animated_center.y + animated_size.y * 0.5f);
        const float detached_rounding = 10.0f * ui_scale;
        draw->AddRectFilled(
            detached_minimum, detached_maximum,
            ImGui::GetColorU32(ImVec4(
                0.018f, 0.021f, 0.030f,
                0.97f * fade * detached_progress)),
            detached_rounding);
        draw->AddRect(
            detached_minimum, detached_maximum,
            ImGui::GetColorU32(ImVec4(
                0.42f, 0.47f, 0.58f,
                0.34f * fade * detached_progress)),
            detached_rounding, 0, 1.0f * ui_scale);
    }

    const float spinner_opacity = fade * (1.0f - EaseOutCubic(
        (elapsed - kSpinnerDuration) / 0.54f));
    if (spinner_opacity > 0.001f) {
        DrawSpinner(draw, center, elapsed, spinner_opacity, ui_scale);
    }
}

}  // namespace pztrainer::ui
