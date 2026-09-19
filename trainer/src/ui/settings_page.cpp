#include "ui/settings_page.hpp"

#include <Windows.h>
#include <Shellapi.h>
#include <ShObjIdl.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "settings/configuration_store.hpp"
#include "settings/localization.hpp"
#include "settings/safety_mode.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/components.hpp"
#include "ui/menu_hotkey_selector.hpp"

namespace pztrainer::ui {
namespace {

std::array<char, 65> g_configuration_name{};
std::string g_selected_configuration;
std::string g_status;
bool g_status_error = false;
float g_pending_ui_scale = 100.0f;
bool g_ui_scale_pending = false;

const char* T(const char* text) {
    return settings::Translate(text);
}

float U(float value) {
    return value * settings::UiScale();
}

bool PickConfigurationDirectory(std::filesystem::path& directory,
                                std::string& error) {
    const HRESULT initialize = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool uninitialize = SUCCEEDED(initialize);
    IFileDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(result) || dialog == nullptr) {
        if (uninitialize) CoUninitialize();
        error = "Unable to create folder picker.";
        return false;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                       FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"PZ Solo Assist - Configuration Folder");
    result = dialog->Show(nullptr);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        dialog->Release();
        if (uninitialize) CoUninitialize();
        return false;
    }
    IShellItem* item = nullptr;
    result = dialog->GetResult(&item);
    PWSTR raw_path = nullptr;
    if (SUCCEEDED(result) && item != nullptr) {
        result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
    }
    if (SUCCEEDED(result) && raw_path != nullptr) {
        directory = raw_path;
    } else {
        error = "Unable to read selected folder.";
    }
    if (raw_path != nullptr) CoTaskMemFree(raw_path);
    if (item != nullptr) item->Release();
    dialog->Release();
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(result) && !directory.empty();
}

void SetStatus(const char* message, bool error = false) {
    g_status = message != nullptr ? message : "";
    g_status_error = error;
}

void DrawAppearanceCard() {
    components::SectionLabel("界面与语言");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "InterfacePreferences", nullptr, ImVec2(0.0f, 0.0f));

    if (!g_ui_scale_pending) {
        g_pending_ui_scale = settings::UiScalePercent();
    }
    bool scale_interaction_active = false;
    if (components::StepperRow(
            "界面缩放", &g_pending_ui_scale,
            50.0f, 250.0f, 2.0f, "%.0f%%", true,
            &scale_interaction_active)) {
        g_ui_scale_pending = true;
    }
    if (g_ui_scale_pending && !scale_interaction_active) {
        settings::SetUiScalePercent(g_pending_ui_scale);
        std::string error;
        settings::SaveUiPreferences(error);
        g_ui_scale_pending = false;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(T("语言"));
    ImGui::SameLine(U(116.0f));
    ImGui::SetNextItemWidth(-1.0f);
    const settings::Language current = settings::GetLanguage();
    if (BeginAnimatedCombo(
            "##InterfaceLanguage", settings::LanguageName(current), 126.0f)) {
        constexpr settings::Language languages[]{
            settings::Language::Chinese,
            settings::Language::English,
            settings::Language::Russian,
            settings::Language::German,
        };
        for (const settings::Language language : languages) {
            if (ImGui::Selectable(
                    settings::LanguageName(language), language == current)) {
                settings::SetUiLanguage(language);
                std::string error;
                settings::SaveUiPreferences(error);
                CloseAnimatedDropdown();
            }
        }
        EndAnimatedDropdown();
    }

    int selected_hotkey = 0;
    if (DrawMenuHotkeySelector(settings::MenuHotkey(), &selected_hotkey)) {
        settings::SetMenuHotkey(selected_hotkey);
        std::string error;
        if (settings::SaveUiPreferences(error)) {
            SetStatus(T("菜单呼出按键已更新"));
        } else {
            SetStatus(error.c_str(), true);
        }
    }
    components::EndCard();
}

void DrawSafetyCard() {
    components::SectionLabel("安全模式");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard("SafetyMode", nullptr, ImVec2(0.0f, 0.0f));
    bool enabled = settings::IsSafeModeEnabled();
    if (ImGui::Checkbox(T("启用安全模式"), &enabled)) {
        settings::SetSafeModeEnabled(enabled);
        std::string error;
        settings::SaveUiPreferences(error);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextWrapped("%s", T("安全模式开启时，直接调用原版 API 的高风险功能会被禁用；关闭后仅解锁明确标注的功能。"));
    ImGui::PopStyleColor();
    components::EndCard();
}

void DrawDirectoryCard() {
    components::SectionLabel("配置目录");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "ConfigurationDirectory", nullptr, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextWrapped(
        "%s", settings::ConfigurationDirectory().u8string().c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ImGui::Button(
            T("选择目录"), ImVec2(U(118.0f), U(32.0f)))) {
        std::filesystem::path selected;
        std::string error;
        if (PickConfigurationDirectory(selected, error)) {
            if (settings::SetConfigurationDirectory(selected, error)) {
                g_selected_configuration.clear();
                SetStatus(T("保存目录已更新"));
            } else {
                SetStatus(error.c_str(), true);
            }
        } else if (!error.empty()) {
            SetStatus(error.c_str(), true);
        }
    }
    ImGui::SameLine(0.0f, U(7.0f));
    if (ImGui::Button(
            T("打开目录"), ImVec2(U(118.0f), U(32.0f)))) {
        ShellExecuteW(
            nullptr, L"open", settings::ConfigurationDirectory().c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);
    }
    components::EndCard();
}

void DrawLuaDirectoryCard() {
    components::SectionLabel("Lua 目录");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "LuaDirectory", nullptr, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextWrapped("%s", settings::LuaDirectory().u8string().c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (ImGui::Button(
            T("选择目录"), ImVec2(U(118.0f), U(32.0f)))) {
        std::filesystem::path selected;
        std::string error;
        if (PickConfigurationDirectory(selected, error)) {
            if (settings::SetLuaDirectory(selected, error)) {
                SetStatus(T("Lua 目录已更新"));
            } else {
                SetStatus(error.c_str(), true);
            }
        } else if (!error.empty()) {
            SetStatus(error.c_str(), true);
        }
    }
    ImGui::SameLine(0.0f, U(7.0f));
    if (ImGui::Button(
            T("打开目录"), ImVec2(U(118.0f), U(32.0f)))) {
        ShellExecuteW(
            nullptr, L"open", settings::LuaDirectory().c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);
    }
    components::EndCard();
}

void DrawConfigurationManager() {
    components::SectionLabel("配置管理");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "ConfigurationManager", nullptr, ImVec2(0.0f, 0.0f));

    ImGui::SetNextItemWidth(U(260.0f));
    ImGui::InputTextWithHint(
        "##ConfigurationName", T("配置名称"),
        g_configuration_name.data(), g_configuration_name.size());
    ImGui::SameLine(0.0f, 8.0f);
    if (ImGui::Button(T("新建"), ImVec2(U(82.0f), U(32.0f)))) {
        std::string error;
        if (settings::SaveConfiguration(
                g_configuration_name.data(), error)) {
            g_selected_configuration = g_configuration_name.data();
            SetStatus(T("配置已创建"));
        } else {
            SetStatus(error.c_str(), true);
        }
    }

    const std::vector<settings::ConfigurationInfo> configurations =
        settings::ListConfigurations();
    ImGui::Dummy(ImVec2(0.0f, 7.0f));
    if (ImGui::BeginListBox(
            "##ConfigurationList", ImVec2(-1.0f, U(190.0f)))) {
        if (configurations.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
            ImGui::TextUnformatted(T("暂无配置"));
            ImGui::PopStyleColor();
        }
        for (const settings::ConfigurationInfo& configuration :
             configurations) {
            if (ImGui::Selectable(
                    configuration.name.c_str(),
                    configuration.name == g_selected_configuration)) {
                g_selected_configuration = configuration.name;
                std::fill(
                    g_configuration_name.begin(),
                    g_configuration_name.end(), '\0');
                const std::size_t length = std::min(
                    configuration.name.size(),
                    g_configuration_name.size() - 1);
                std::copy_n(
                    configuration.name.data(), length,
                    g_configuration_name.data());
            }
        }
        ImGui::EndListBox();
    }

    const std::string target = !g_selected_configuration.empty()
        ? g_selected_configuration
        : std::string(g_configuration_name.data());
    if (ImGui::Button(T("保存"), ImVec2(U(92.0f), U(32.0f)))) {
        std::string error;
        if (target.empty()) {
            SetStatus(T("请输入配置名称"), true);
        } else if (settings::SaveConfiguration(target, error)) {
            g_selected_configuration = target;
            SetStatus(T("配置已保存"));
        } else {
            SetStatus(error.c_str(), true);
        }
    }
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::BeginDisabled(target.empty());
    if (ImGui::Button(T("加载"), ImVec2(U(92.0f), U(32.0f)))) {
        std::string error;
        if (settings::LoadConfiguration(target, error)) {
            g_selected_configuration = target;
            SetStatus(T("配置已加载"));
        } else {
            SetStatus(error.c_str(), true);
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0.0f, U(8.0f));
    ImGui::BeginDisabled(target.empty());
    if (ImGui::Button(T("删除"), ImVec2(U(92.0f), U(32.0f)))) {
        ImGui::OpenPopup("##DeleteConfigurationConfirmation");
    }
    ImGui::EndDisabled();

    ImGui::SetNextWindowSize(ImVec2(U(340.0f), 0.0f));
    if (ImGui::BeginPopupModal(
            "##DeleteConfigurationConfirmation", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", T("确定删除选中的配置吗？"));
        ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
        if (ImGui::Button(T("确定"), ImVec2(U(92.0f), U(32.0f)))) {
            std::string error;
            if (settings::DeleteConfiguration(target, error)) {
                g_selected_configuration.clear();
                g_configuration_name.fill('\0');
                SetStatus(T("配置已删除"));
            } else {
                SetStatus(error.c_str(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, U(8.0f));
        if (ImGui::Button(T("取消"), ImVec2(U(92.0f), U(32.0f)))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!g_status.empty()) {
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::PushStyleColor(
            ImGuiCol_Text,
            g_status_error ? ImVec4(0.96f, 0.42f, 0.45f, 1.0f)
                           : ImVec4(0.42f, 0.82f, 0.62f, 1.0f));
        ImGui::TextWrapped("%s", g_status.c_str());
        ImGui::PopStyleColor();
    }
    components::EndCard();
}

}  // namespace

void DrawSettingsPage() {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f));
    if (ImGui::BeginTable(
            "SettingsCards", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawAppearanceCard();
        ImGui::TableSetColumnIndex(1);
        DrawDirectoryCard();
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f));
    if (ImGui::BeginTable(
            "SafetyAndLuaCards", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawSafetyCard();
        ImGui::TableSetColumnIndex(1);
        DrawLuaDirectoryCard();
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DrawConfigurationManager();
}

}  // namespace pztrainer::ui
