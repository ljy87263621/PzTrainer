#include "settings/ui_preferences.hpp"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <string>

namespace pztrainer::settings {
namespace {

float g_scale_percent = 100.0f;
std::filesystem::path g_configuration_directory;
std::filesystem::path g_lua_directory;
std::filesystem::path g_preferences_path;
std::uint64_t g_revision = 1;
bool g_initialized = false;
bool g_safe_mode_enabled = true;
std::atomic_int g_menu_hotkey{VK_INSERT};

bool IsAllowedMenuHotkey(int virtual_key) {
    if (virtual_key <= 0 || virtual_key > 0xFF) return false;
    switch (virtual_key) {
        case VK_LBUTTON:
        case VK_RBUTTON:
        case VK_MBUTTON:
        case VK_XBUTTON1:
        case VK_XBUTTON2:
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
            return false;
        default:
            return true;
    }
}

std::filesystem::path AppDataRoot() {
    PWSTR raw_path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(
            FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &raw_path)) &&
        raw_path != nullptr) {
        const std::filesystem::path result(raw_path);
        CoTaskMemFree(raw_path);
        return result;
    }
    wchar_t fallback[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(
        L"APPDATA", fallback, static_cast<DWORD>(std::size(fallback)));
    return length > 0 && length < std::size(fallback)
        ? std::filesystem::path(fallback)
        : std::filesystem::current_path();
}

void EnsureDefaults() {
    const std::filesystem::path root = AppDataRoot() / L"PZ Solo Assist";
    if (g_preferences_path.empty()) {
        g_preferences_path = root / L"preferences.ini";
    }
    if (g_configuration_directory.empty()) {
        g_configuration_directory = root / L"configs";
    }
    if (g_lua_directory.empty()) {
        g_lua_directory = root / L"lua";
    }
}

}  // namespace

void InitializeUiPreferences() {
    if (g_initialized) return;
    g_initialized = true;
    EnsureDefaults();

    std::ifstream input(g_preferences_path);
    std::string line;
    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        try {
            if (key == "scale_percent") {
                g_scale_percent = std::clamp(
                    std::stof(value), 50.0f, 250.0f);
            } else if (key == "language") {
                const int language = std::stoi(value);
                if (language >= 0 &&
                    language < static_cast<int>(Language::Count)) {
                    SetLanguage(static_cast<Language>(language));
                }
            } else if (key == "config_directory" && !value.empty()) {
                g_configuration_directory = std::filesystem::u8path(value);
            } else if (key == "lua_directory" && !value.empty()) {
                g_lua_directory = std::filesystem::u8path(value);
            } else if (key == "safe_mode") {
                g_safe_mode_enabled = std::stoi(value) != 0;
            } else if (key == "menu_hotkey") {
                const int virtual_key = std::stoi(value);
                if (IsAllowedMenuHotkey(virtual_key)) {
                    g_menu_hotkey.store(virtual_key);
                }
            }
        } catch (...) {
        }
    }

    std::error_code error;
    std::filesystem::create_directories(g_configuration_directory, error);
    error.clear();
    std::filesystem::create_directories(g_lua_directory, error);
}

bool IsSafeModeEnabled() {
    return g_safe_mode_enabled;
}

void SetSafeModeEnabled(bool enabled) {
    g_safe_mode_enabled = enabled;
}

float UiScalePercent() {
    return g_scale_percent;
}

float UiScale() {
    return g_scale_percent * 0.01f;
}

void SetUiScalePercent(float percent) {
    const float next = std::clamp(percent, 50.0f, 250.0f);
    if (next == g_scale_percent) return;
    g_scale_percent = next;
    ++g_revision;
}

void SetUiLanguage(Language language) {
    if (language == GetLanguage()) return;
    SetLanguage(language);
    ++g_revision;
}

int MenuHotkey() {
    return g_menu_hotkey.load();
}

void SetMenuHotkey(int virtual_key) {
    if (!IsAllowedMenuHotkey(virtual_key)) return;
    const int previous = g_menu_hotkey.exchange(virtual_key);
    if (previous != virtual_key) ++g_revision;
}

const std::filesystem::path& ConfigurationDirectory() {
    EnsureDefaults();
    return g_configuration_directory;
}

const std::filesystem::path& LuaDirectory() {
    EnsureDefaults();
    return g_lua_directory;
}

bool SetConfigurationDirectory(const std::filesystem::path& directory,
                               std::string& error) {
    if (directory.empty()) {
        error = "Configuration directory is empty.";
        return false;
    }
    std::error_code filesystem_error;
    const std::filesystem::path absolute = std::filesystem::absolute(
        directory, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    std::filesystem::create_directories(absolute, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    g_configuration_directory = absolute;
    ++g_revision;
    return SaveUiPreferences(error);
}

bool SetLuaDirectory(const std::filesystem::path& directory,
                     std::string& error) {
    if (directory.empty()) {
        error = "Lua directory is empty.";
        return false;
    }
    std::error_code filesystem_error;
    const std::filesystem::path absolute = std::filesystem::absolute(
        directory, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    std::filesystem::create_directories(absolute, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    g_lua_directory = absolute;
    ++g_revision;
    return SaveUiPreferences(error);
}

bool SaveUiPreferences(std::string& error) {
    EnsureDefaults();
    std::error_code filesystem_error;
    std::filesystem::create_directories(
        g_preferences_path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    std::ofstream output(g_preferences_path, std::ios::trunc);
    if (!output) {
        error = "Unable to open preferences file.";
        return false;
    }
    output << "scale_percent=" << g_scale_percent << '\n';
    output << "language=" << static_cast<int>(GetLanguage()) << '\n';
    output << "config_directory="
           << g_configuration_directory.u8string() << '\n';
    output << "lua_directory=" << g_lua_directory.u8string() << '\n';
    output << "safe_mode=" << (g_safe_mode_enabled ? 1 : 0) << '\n';
    output << "menu_hotkey=" << g_menu_hotkey.load() << '\n';
    if (!output) {
        error = "Unable to write preferences file.";
        return false;
    }
    return true;
}

std::uint64_t UiPreferencesRevision() {
    return g_revision;
}

}  // namespace pztrainer::settings
