#include "ui/theme.hpp"

#include <imgui.h>

#include "settings/ui_preferences.hpp"

namespace pztrainer::ui {
namespace {

ImFont* g_regular_font = nullptr;
ImFont* g_semibold_font = nullptr;

}  // namespace

void SetInterfaceFonts(ImFont* regular, ImFont* semibold) {
    g_regular_font = regular;
    g_semibold_font = semibold != nullptr ? semibold : regular;
}

ImFont* RegularFont() {
    return g_regular_font;
}

ImFont* SemiboldFont() {
    return g_semibold_font != nullptr ? g_semibold_font : g_regular_font;
}

void ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 6.0f;
    style.WindowRounding = 12.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 7.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 7.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.88f, 0.89f, 0.93f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.48f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.010f, 0.028f, 0.046f, 0.76f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.014f, 0.030f, 0.048f, 0.66f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.050f, 0.048f, 0.062f, 0.90f);
    colors[ImGuiCol_Border] = ImVec4(0.29f, 0.32f, 0.39f, 0.23f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.067f, 0.066f, 0.086f, 0.82f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.095f, 0.105f, 0.135f, 0.88f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.13f, 0.15f, 0.20f, 0.92f);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_Button] = ImVec4(0.055f, 0.060f, 0.075f, 0.76f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.12f, 0.13f, 0.16f, 0.88f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.18f, 0.23f, 0.92f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.43f, 0.97f, 0.24f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.43f, 0.97f, 0.34f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.43f, 0.97f, 0.46f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.18f, 0.43f, 0.97f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.18f, 0.43f, 0.97f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.28f, 0.49f, 1.00f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.17f, 0.20f, 0.52f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.035f, 0.038f, 0.048f, 0.70f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.43f, 0.97f, 0.82f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    const float scale = settings::UiScale();
    style.ScaleAllSizes(scale);
    style.FontScaleMain = scale;
}

}  // namespace pztrainer::ui
