#include "ui/menu_hotkey_selector.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

std::atomic_bool g_capture_active{false};
std::atomic_int g_pending_virtual_key{0};

float U(float value) {
    return value * settings::UiScale();
}

bool IsPureModifier(int virtual_key) {
    switch (virtual_key) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_LMENU:
        case VK_RMENU:
        case VK_LWIN:
        case VK_RWIN:
            return true;
        default:
            return false;
    }
}

std::string WideToUtf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') return {};
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

std::string VirtualKeyName(int virtual_key) {
    if (virtual_key >= 'A' && virtual_key <= 'Z') {
        return std::string(1, static_cast<char>(virtual_key));
    }
    if (virtual_key >= '0' && virtual_key <= '9') {
        return std::string(1, static_cast<char>(virtual_key));
    }
    if (virtual_key >= VK_F1 && virtual_key <= VK_F24) {
        return "F" + std::to_string(virtual_key - VK_F1 + 1);
    }
    switch (virtual_key) {
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_LEFT: return "Left";
        case VK_RIGHT: return "Right";
        case VK_UP: return "Up";
        case VK_DOWN: return "Down";
        case VK_SPACE: return "Space";
        case VK_TAB: return "Tab";
        case VK_BACK: return "Backspace";
        case VK_RETURN: return "Enter";
        case VK_CAPITAL: return "Caps Lock";
        case VK_ESCAPE: return "Esc";
        default: break;
    }

    UINT scan_code = MapVirtualKeyW(
        static_cast<UINT>(virtual_key), MAPVK_VK_TO_VSC);
    LONG key_data = static_cast<LONG>(scan_code << 16);
    switch (virtual_key) {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_DIVIDE:
        case VK_NUMLOCK:
            key_data |= 1 << 24;
            break;
        default:
            break;
    }
    wchar_t name[64]{};
    if (GetKeyNameTextW(key_data, name, static_cast<int>(_countof(name))) > 0) {
        const std::string converted = WideToUtf8(name);
        if (!converted.empty()) return converted;
    }
    char fallback[16]{};
    std::snprintf(fallback, sizeof(fallback), "VK 0x%02X", virtual_key);
    return fallback;
}

}  // namespace

bool DrawMenuHotkeySelector(int current_virtual_key,
                            int* selected_virtual_key) {
    const int pending = g_pending_virtual_key.exchange(0);
    const bool changed = pending != 0;
    if (changed && selected_virtual_key != nullptr) {
        *selected_virtual_key = pending;
    }

    ImGui::PushID("MenuHotkeySelector");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(settings::Translate("菜单呼出按键"));
    ImGui::SameLine(U(116.0f));

    const bool capturing = g_capture_active.load();
    const std::string key_name = capturing
        ? settings::Translate("请按下新的按键...")
        : VirtualKeyName(current_virtual_key);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, U(7.0f));
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        capturing ? ImVec4(0.08f, 0.17f, 0.34f, 0.92f)
                  : ImVec4(0.065f, 0.075f, 0.10f, 0.92f));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        capturing ? ImVec4(0.10f, 0.23f, 0.48f, 0.96f)
                  : ImVec4(0.09f, 0.11f, 0.15f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                         ImVec4(0.12f, 0.29f, 0.60f, 1.0f));
    const float width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button(key_name.c_str(), ImVec2(width, U(30.0f)))) {
        g_pending_virtual_key.store(0);
        g_capture_active.store(true);
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    if (g_capture_active.load() && ImGui::IsItemHovered()) {
        components::RoundedTooltip(settings::Translate("按 Esc 取消"));
    }
    ImGui::PopID();
    return changed;
}

bool HandleMenuHotkeyCaptureMessage(unsigned int message,
                                    std::uintptr_t wparam) {
    if (!g_capture_active.load()) return false;
    const bool keyboard_message =
        (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
        message == WM_CHAR || message == WM_SYSCHAR;
    if (!keyboard_message) return false;

    if (message != WM_KEYUP && message != WM_SYSKEYUP) return true;
    const int virtual_key = static_cast<int>(wparam);
    if (virtual_key == VK_ESCAPE) {
        g_capture_active.store(false);
        g_pending_virtual_key.store(0);
        return true;
    }
    if (IsPureModifier(virtual_key)) return true;
    if (virtual_key > 0 && virtual_key <= 0xFF) {
        g_pending_virtual_key.store(virtual_key);
        g_capture_active.store(false);
    }
    return true;
}

}  // namespace pztrainer::ui
