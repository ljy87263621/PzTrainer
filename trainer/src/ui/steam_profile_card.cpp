#include "ui/steam_profile_card.hpp"

#include <Windows.h>
#include <GL/gl.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animation.hpp"
#include "ui/components.hpp"
#include "ui/theme.hpp"
#include "vmprotect.hpp"

namespace pztrainer::ui {
namespace {

using Clock = std::chrono::steady_clock;

const char* ProtectedAuthorCaption() {
    static const char* value = PZ_VMP_DECRYPT_STRING_A("by:InitLoader");
    return value;
}

const char* ProtectedAuthorName() {
    static const char* value = PZ_VMP_DECRYPT_STRING_A("InitLoader");
    return value;
}

const char* ProtectedGroupCaption() {
    static const char* value = PZ_VMP_DECRYPT_STRING_A("QQ群:1074183906");
    return value;
}

using SteamFactory = void* (__cdecl*)();
using SteamRunning = bool (__cdecl*)();
using SteamPersonaName = const char* (__cdecl*)(void*);
using SteamUserId = std::uint64_t (__cdecl*)(void*);
using SteamFriendAvatar = int (__cdecl*)(void*, std::uint64_t);
using SteamImageSize = bool (__cdecl*)(void*, int, std::uint32_t*, std::uint32_t*);
using SteamImageRgba = bool (__cdecl*)(void*, int, std::uint8_t*, int);

struct SteamProfileState {
    std::string name = "Steam User";
    GLuint avatar_texture = 0;
    Clock::time_point next_refresh{};
    bool popup_open = false;
    bool card_clicked = false;
    bool card_hovered = false;
    float popup_amount = 0.0f;
    float popup_velocity = 0.0f;
    ImVec2 card_minimum{};
    ImVec2 card_maximum{};
};

SteamProfileState g_profile;

float U(float value) {
    return value * settings::UiScale();
}

template <typename Function>
Function Resolve(HMODULE module, const char* name) {
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

bool UploadAvatar(const std::vector<std::uint8_t>& pixels,
                  std::uint32_t width, std::uint32_t height) {
    if (pixels.empty() || width == 0 || height == 0) return false;
    GLint previous_texture = 0;
    GLint previous_unpack_alignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    g_profile.avatar_texture = texture;
    return true;
}

void RefreshSteamProfile() {
    const Clock::time_point now = Clock::now();
    if (now < g_profile.next_refresh) return;
    g_profile.next_refresh = now + std::chrono::seconds(2);

    HMODULE module = GetModuleHandleW(L"steam_api64.dll");
    if (module == nullptr) return;
    const SteamRunning is_running = Resolve<SteamRunning>(
        module, "SteamAPI_IsSteamRunning");
    const SteamFactory friends_factory = Resolve<SteamFactory>(
        module, "SteamAPI_SteamFriends_v018");
    const SteamFactory user_factory = Resolve<SteamFactory>(
        module, "SteamAPI_SteamUser_v023");
    const SteamFactory utils_factory = Resolve<SteamFactory>(
        module, "SteamAPI_SteamUtils_v010");
    const SteamPersonaName get_name = Resolve<SteamPersonaName>(
        module, "SteamAPI_ISteamFriends_GetPersonaName");
    const SteamUserId get_user_id = Resolve<SteamUserId>(
        module, "SteamAPI_ISteamUser_GetSteamID");
    const SteamFriendAvatar get_avatar = Resolve<SteamFriendAvatar>(
        module, "SteamAPI_ISteamFriends_GetMediumFriendAvatar");
    const SteamImageSize get_image_size = Resolve<SteamImageSize>(
        module, "SteamAPI_ISteamUtils_GetImageSize");
    const SteamImageRgba get_image_rgba = Resolve<SteamImageRgba>(
        module, "SteamAPI_ISteamUtils_GetImageRGBA");
    if (is_running == nullptr || !is_running() ||
        friends_factory == nullptr || user_factory == nullptr ||
        utils_factory == nullptr || get_name == nullptr ||
        get_user_id == nullptr || get_avatar == nullptr ||
        get_image_size == nullptr || get_image_rgba == nullptr) {
        return;
    }

    void* friends = friends_factory();
    void* user = user_factory();
    void* utils = utils_factory();
    if (friends == nullptr || user == nullptr || utils == nullptr) return;
    const char* name = get_name(friends);
    if (name != nullptr && name[0] != '\0') g_profile.name = name;
    if (g_profile.avatar_texture != 0) return;

    const std::uint64_t steam_id = get_user_id(user);
    const int avatar = get_avatar(friends, steam_id);
    if (avatar <= 0) return;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!get_image_size(utils, avatar, &width, &height) ||
        width == 0 || height == 0 || width > 512 || height > 512) {
        return;
    }
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width) * height * 4);
    if (!get_image_rgba(
            utils, avatar, pixels.data(), static_cast<int>(pixels.size()))) {
        return;
    }
    UploadAvatar(pixels, width, height);
}

void DrawFallbackAvatar(ImDrawList* draw, const ImVec2& minimum,
                        const ImVec2& maximum, ImU32 color) {
    const ImVec2 center(
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f);
    const float scale = settings::UiScale();
    draw->AddCircleFilled(
        ImVec2(center.x, center.y - U(6.0f)), U(4.6f), color);
    draw->AddBezierCubic(
        ImVec2(center.x - U(9.0f), center.y + U(10.0f)),
        ImVec2(center.x - U(8.0f), center.y - U(1.0f)),
        ImVec2(center.x + U(8.0f), center.y - U(1.0f)),
        ImVec2(center.x + U(9.0f), center.y + U(10.0f)),
        color, 2.0f * scale);
}

}  // namespace

void DrawSteamProfileCard() {
    RefreshSteamProfile();
    g_profile.card_clicked = false;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = U(48.0f);
    ImGui::PushID("SteamProfileCard");
    ImGui::InvisibleButton("profile", ImVec2(width, height));
    g_profile.card_hovered = ImGui::IsItemHovered();
    g_profile.card_clicked = ImGui::IsItemClicked();
    if (g_profile.card_clicked) {
        g_profile.popup_open = !g_profile.popup_open;
    }
    g_profile.card_minimum = start;
    g_profile.card_maximum = ImVec2(start.x + width, start.y + height);

    const float hover_amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("profile_hover"),
        g_profile.card_hovered || g_profile.popup_open ? 1.0f : 0.0f,
        230.0f, 21.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hover_amount > 0.001f) {
        draw->AddRectFilled(
            start, g_profile.card_maximum,
            ImGui::GetColorU32(ImVec4(
                components::kAccent.x, components::kAccent.y,
                components::kAccent.z, 0.10f * hover_amount)),
            U(8.0f));
    }
    const ImVec2 avatar_min(start.x + U(2.0f), start.y + U(4.0f));
    const ImVec2 avatar_max(
        avatar_min.x + U(40.0f), avatar_min.y + U(40.0f));
    draw->AddRectFilled(
        avatar_min, avatar_max,
        ImGui::GetColorU32(ImVec4(0.08f, 0.10f, 0.14f, 1.0f)),
        U(10.0f));
    if (g_profile.avatar_texture != 0) {
        draw->AddImageRounded(
            ImTextureRef(static_cast<ImTextureID>(g_profile.avatar_texture)),
            avatar_min, avatar_max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), U(10.0f));
    } else {
        DrawFallbackAvatar(
            draw, avatar_min, avatar_max,
            ImGui::GetColorU32(ImVec4(0.88f, 0.91f, 0.98f, 1.0f)));
    }
    draw->AddRect(
        avatar_min, avatar_max,
        ImGui::GetColorU32(ImVec4(
            components::kAccent.x, components::kAccent.y,
            components::kAccent.z, 0.32f + 0.36f * hover_amount)),
        U(10.0f), 0, U(1.0f));

    const ImVec2 text_min(start.x + U(52.0f), start.y + U(5.0f));
    draw->PushClipRect(
        text_min,
        ImVec2(g_profile.card_maximum.x - U(6.0f),
               g_profile.card_maximum.y), true);
    if (SemiboldFont() != nullptr) ImGui::PushFont(SemiboldFont(), 0.0f);
    draw->AddText(text_min, ImGui::GetColorU32(components::kText),
                  g_profile.name.c_str());
    if (SemiboldFont() != nullptr) ImGui::PopFont();
    draw->AddText(
        ImVec2(text_min.x, start.y + U(27.0f)),
        ImGui::GetColorU32(components::kMuted), ProtectedAuthorCaption());
    draw->PopClipRect();
    ImGui::PopID();
}

void DrawSteamProfilePopup(bool menu_visible) {
    if (!menu_visible) g_profile.popup_open = false;
    animation::SpringValue(
        g_profile.popup_amount, g_profile.popup_velocity,
        g_profile.popup_open ? 1.0f : 0.0f, 190.0f, 20.0f);
    const float amount = animation::Clamp01(g_profile.popup_amount);
    if (!g_profile.popup_open && amount < 0.002f) {
        g_profile.popup_amount = 0.0f;
        g_profile.popup_velocity = 0.0f;
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float popup_width = U(360.0f);
    const float content_width = popup_width - U(36.0f);
    const char* notice = settings::Translate(
        "本软件是免费的，如果你是购买的，那么你就被骗了。");
    const float notice_height = ImGui::CalcTextSize(
        notice, nullptr, false, content_width).y;
    const bool show_qq =
        settings::GetLanguage() == settings::Language::Chinese;
    const float localized_height =
        U(54.0f) + notice_height +
        (show_qq ? U(8.0f) + ImGui::GetTextLineHeight() : 0.0f) +
        U(22.0f);
    const ImVec2 size(popup_width, std::max(U(174.0f), localized_height));
    ImVec2 position(
        g_profile.card_maximum.x + U(14.0f) - (1.0f - amount) * U(18.0f),
        g_profile.card_maximum.y - size.y);
    if (position.x + size.x > io.DisplaySize.x - U(12.0f)) {
        position.x = g_profile.card_minimum.x - size.x - U(14.0f) +
            (1.0f - amount) * U(18.0f);
    }
    position.x = std::clamp(
        position.x, U(12.0f),
        std::max(U(12.0f), io.DisplaySize.x - size.x - U(12.0f)));
    position.y = std::clamp(
        position.y, U(12.0f),
        std::max(U(12.0f), io.DisplaySize.y - size.y - U(12.0f)));

    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * amount);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(18.0f), U(16.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.0f));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.025f, 0.030f, 0.042f, 0.88f));
    ImGui::PushStyleColor(
        ImGuiCol_Border, ImVec4(0.18f, 0.43f, 0.97f, 0.52f));
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    if (amount < 0.12f) flags |= ImGuiWindowFlags_NoInputs;

    bool popup_hovered = false;
    if (ImGui::Begin("##SteamProfilePopup", nullptr, flags)) {
        popup_hovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 minimum = ImGui::GetWindowPos();
        const ImVec2 maximum(
            minimum.x + ImGui::GetWindowWidth(),
            minimum.y + ImGui::GetWindowHeight());
        draw->AddRectFilled(
            minimum,
            ImVec2(minimum.x + U(4.0f), maximum.y),
            ImGui::GetColorU32(ImVec4(0.18f, 0.43f, 0.97f, 0.86f)),
            U(12.0f), ImDrawFlags_RoundCornersLeft);

        if (SemiboldFont() != nullptr) ImGui::PushFont(SemiboldFont(), 0.0f);
        ImGui::TextUnformatted(ProtectedAuthorName());
        if (SemiboldFont() != nullptr) ImGui::PopFont();
        const ImVec2 close_min(
            maximum.x - U(38.0f), minimum.y + U(10.0f));
        ImGui::SetCursorScreenPos(close_min);
        if (ImGui::InvisibleButton("close", ImVec2(U(26.0f), U(26.0f)))) {
            g_profile.popup_open = false;
        }
        const ImU32 close_color = ImGui::GetColorU32(
            ImGui::IsItemHovered() ? components::kText : components::kMuted);
        draw->AddLine(
            ImVec2(close_min.x + U(8.0f), close_min.y + U(8.0f)),
            ImVec2(close_min.x + U(18.0f), close_min.y + U(18.0f)),
            close_color, U(1.5f));
        draw->AddLine(
            ImVec2(close_min.x + U(18.0f), close_min.y + U(8.0f)),
            ImVec2(close_min.x + U(8.0f), close_min.y + U(18.0f)),
            close_color, U(1.5f));

        ImGui::SetCursorScreenPos(
            ImVec2(minimum.x + U(18.0f), minimum.y + U(54.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, components::kText);
        ImGui::PushTextWrapPos(
            ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("%s", notice);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        if (show_qq) {
            ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text, components::kAccent);
            ImGui::TextUnformatted(ProtectedGroupCaption());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);

    if (g_profile.popup_open &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !popup_hovered && !g_profile.card_clicked &&
        !g_profile.card_hovered) {
        g_profile.popup_open = false;
    }
}

}  // namespace pztrainer::ui
