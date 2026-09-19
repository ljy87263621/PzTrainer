#include "ui/lua_page.hpp"

#include <Windows.h>
#include <Shellapi.h>
#include <ShObjIdl.h>

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "bridge/lua_script_bridge.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/components.hpp"

namespace pztrainer::ui {
namespace {

std::filesystem::path g_delete_path;
bool g_delete_popup_requested = false;

const char* T(const char* value) {
    return settings::Translate(value);
}

float U(float value) {
    return value * settings::UiScale();
}

bool PickLuaFile(std::filesystem::path& path, std::string& error) {
    const HRESULT initialize = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool uninitialize = SUCCEEDED(initialize);
    IFileOpenDialog* dialog = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (FAILED(result) || dialog == nullptr) {
        if (uninitialize) CoUninitialize();
        error = "Unable to create Lua file picker.";
        return false;
    }
    constexpr COMDLG_FILTERSPEC filters[]{{L"Lua scripts (*.lua)", L"*.lua"}};
    dialog->SetFileTypes(1, filters);
    dialog->SetFileTypeIndex(1);
    dialog->SetDefaultExtension(L"lua");
    dialog->SetTitle(L"PZ Solo Assist - Select Lua Script");
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
        path = raw_path;
    } else {
        error = "Unable to read selected Lua file.";
    }
    if (raw_path != nullptr) CoTaskMemFree(raw_path);
    if (item != nullptr) item->Release();
    dialog->Release();
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(result) && !path.empty();
}

const char* StateLabel(bridge::LuaScriptState state) {
    switch (state) {
        case bridge::LuaScriptState::Unloaded: return T("未加载");
        case bridge::LuaScriptState::Loading: return T("正在加载");
        case bridge::LuaScriptState::Loaded: return T("已加载");
        case bridge::LuaScriptState::Unloading: return T("正在停止");
        case bridge::LuaScriptState::Error: return T("加载错误");
    }
    return T("未加载");
}

ImVec4 StateColor(bridge::LuaScriptState state) {
    switch (state) {
        case bridge::LuaScriptState::Loaded:
            return ImVec4(0.36f, 0.85f, 0.61f, 1.0f);
        case bridge::LuaScriptState::Error:
            return ImVec4(0.96f, 0.38f, 0.42f, 1.0f);
        case bridge::LuaScriptState::Loading:
        case bridge::LuaScriptState::Unloading:
            return ImVec4(0.43f, 0.67f, 0.98f, 1.0f);
        default:
            return components::kMuted;
    }
}

void DrawLuaMark(ImDrawList* draw, const ImVec2& center, float scale) {
    const ImU32 white = ImGui::GetColorU32(ImVec4(
        245.0f / 255.0f, 248.0f / 255.0f, 1.0f, 1.0f));
    draw->AddCircle(center, 8.0f * scale, white, 0, 1.6f * scale);
    draw->AddCircleFilled(
        ImVec2(center.x + 4.1f * scale, center.y - 4.1f * scale),
        3.4f * scale, ImGui::GetColorU32(ImVec4(
            17.0f / 255.0f, 21.0f / 255.0f,
            31.0f / 255.0f, 1.0f)));
    draw->AddCircleFilled(
        ImVec2(center.x + 7.3f * scale, center.y - 7.3f * scale),
        2.1f * scale, white);
}

void DuplicateScript(const std::filesystem::path& source) {
    std::error_code error;
    std::filesystem::path target = source.parent_path() /
        (source.stem().wstring() + L" - Copy.lua");
    for (int suffix = 2; std::filesystem::exists(target, error); ++suffix) {
        target = source.parent_path() /
            (source.stem().wstring() + L" - Copy " +
             std::to_wstring(suffix) + L".lua");
    }
    std::filesystem::copy_file(
        source, target, std::filesystem::copy_options::none, error);
    bridge::RefreshLuaScriptDirectory();
}

void DrawScriptMenu(const bridge::LuaScriptInfo& script) {
    if (ImGui::MenuItem(T("重载"))) bridge::ReloadLuaScript(script.path);
    bool auto_reload = script.auto_reload;
    if (ImGui::MenuItem(T("自动重载"), nullptr, &auto_reload)) {
        bridge::SetLuaScriptAutoReload(script.path, auto_reload);
    }
    ImGui::Separator();
    if (ImGui::MenuItem(T("复制"))) DuplicateScript(script.path);
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.96f, 0.39f, 0.42f, 1.0f));
    if (ImGui::MenuItem(T("删除"))) {
        g_delete_path = script.path;
        g_delete_popup_requested = true;
    }
    ImGui::PopStyleColor();
}

void DrawScriptCard(const bridge::LuaScriptInfo& script) {
    ImGui::PushID(script.path.u8string().c_str());
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool show_message = script.state == bridge::LuaScriptState::Error ||
        (script.state == bridge::LuaScriptState::Unloaded && !script.message.empty());
    const float height = show_message ? U(82.0f) : U(60.0f);
    ImGui::Dummy(ImVec2(width, height));
    const ImVec2 after_card = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(
            8.0f / 255.0f, 11.0f / 255.0f,
            18.0f / 255.0f, 218.0f / 255.0f)), U(8.0f));
    draw->AddRect(
        start, ImVec2(start.x + width, start.y + height),
        ImGui::GetColorU32(ImVec4(
            45.0f / 255.0f, 56.0f / 255.0f,
            74.0f / 255.0f, 115.0f / 255.0f)), U(8.0f));

    const ImVec2 mark(start.x + U(20.0f), start.y + U(19.0f));
    DrawLuaMark(draw, mark, settings::UiScale() * 0.72f);
    draw->AddText(
        ImVec2(start.x + U(39.0f), start.y + U(7.0f)),
        ImGui::GetColorU32(components::kText), script.name.c_str());
    const std::string metadata =
        std::string(T("修改时间")) + ": " + script.modified;
    draw->AddText(
        ImVec2(start.x + U(39.0f), start.y + U(30.0f)),
        ImGui::GetColorU32(components::kMuted), metadata.c_str());
    const ImVec2 button_position(
        start.x + width - U(132.0f), start.y + U(14.0f));
    const ImVec2 button_size(U(78.0f), U(32.0f));
    const char* state_label = StateLabel(script.state);
    const ImVec2 state_size = ImGui::CalcTextSize(state_label);
    const float state_right = button_position.x - U(18.0f);
    draw->AddText(
        ImVec2(
            state_right - state_size.x,
            button_position.y + (button_size.y - state_size.y) * 0.5f),
        ImGui::GetColorU32(StateColor(script.state)), state_label);
    if (show_message) {
        std::string single_line = script.message;
        std::replace(single_line.begin(), single_line.end(), '\n', ' ');
        if (single_line.size() > 112) single_line.resize(112);
        draw->AddText(
            ImVec2(start.x + U(39.0f), start.y + U(54.0f)),
            script.state == bridge::LuaScriptState::Error
                ? ImGui::GetColorU32(ImVec4(
                    239.0f / 255.0f, 104.0f / 255.0f,
                    111.0f / 255.0f, 1.0f))
                : ImGui::GetColorU32(ImVec4(
                    213.0f / 255.0f, 168.0f / 255.0f,
                    91.0f / 255.0f, 1.0f)),
            single_line.c_str());
    }

    ImGui::SetCursorScreenPos(button_position);
    const bool busy = script.state == bridge::LuaScriptState::Loading ||
        script.state == bridge::LuaScriptState::Unloading;
    ImGui::BeginDisabled(busy);
    const bool loaded = script.state == bridge::LuaScriptState::Loaded;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, U(1.0f));
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        loaded
            ? ImVec4(0.40f, 0.65f, 0.96f, 0.90f)
            : ImVec4(0.48f, 0.56f, 0.68f, 0.72f));
    if (ImGui::Button(loaded ? T("停止") : T("加载"), button_size)) {
        if (loaded) bridge::UnloadLuaScript(script.path);
        else bridge::LoadLuaScript(script.path);
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(ImVec2(start.x + width - U(45.0f), start.y + U(14.0f)));
    const ImVec2 more_minimum = ImGui::GetCursorScreenPos();
    const bool more_clicked = ImGui::InvisibleButton(
        "MoreActions", ImVec2(U(32.0f), U(32.0f)));
    const bool more_hovered = ImGui::IsItemHovered();
    ImDrawList* more_draw = ImGui::GetWindowDrawList();
    if (more_hovered) {
        more_draw->AddRectFilled(
            more_minimum,
            ImVec2(more_minimum.x + U(32.0f), more_minimum.y + U(32.0f)),
            ImGui::GetColorU32(ImVec4(
                35.0f / 255.0f, 44.0f / 255.0f,
                59.0f / 255.0f, 145.0f / 255.0f)), U(6.0f));
    }
    const ImU32 more_color = ImGui::GetColorU32(
        more_hovered ? components::kText : components::kMuted);
    const float more_y = more_minimum.y + U(16.0f);
    for (int index = 0; index < 3; ++index) {
        more_draw->AddCircleFilled(
            ImVec2(more_minimum.x + U(10.0f + index * 6.0f), more_y),
            U(1.6f), more_color);
    }
    if (more_clicked) {
        ImGui::OpenPopup("ScriptActions");
    }
    if (ImGui::BeginPopup("ScriptActions")) {
        DrawScriptMenu(script);
        ImGui::EndPopup();
    }
    if (show_message && ImGui::IsMouseHoveringRect(
            start, ImVec2(start.x + width, start.y + height))) {
        components::RoundedTooltip(script.message.c_str());
    }
    ImGui::SetCursorScreenPos(after_card);
    ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
    ImGui::PopID();
}

void DrawDeleteConfirmation() {
    ImGui::SetNextWindowSize(ImVec2(U(370.0f), 0.0f));
    if (ImGui::BeginPopupModal(
            "##DeleteLuaConfirmation", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", T("确定删除这个 Lua 文件吗？此操作无法撤销。"));
        ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
        if (ImGui::Button(T("确定"), ImVec2(U(92.0f), U(32.0f)))) {
            bridge::UnloadLuaScript(g_delete_path);
            std::error_code error;
            std::filesystem::remove(g_delete_path, error);
            bridge::RefreshLuaScriptDirectory();
            g_delete_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0.0f, U(8.0f));
        if (ImGui::Button(T("取消"), ImVec2(U(92.0f), U(32.0f)))) {
            g_delete_path.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace

void DrawLuaPage() {
    components::SectionLabel("Lua");
    ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
    components::BeginCompactCard("LuaDirectorySummary", nullptr, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
    ImGui::TextWrapped("%s", settings::LuaDirectory().u8string().c_str());
    ImGui::TextWrapped("%s", T("脚本可在末尾返回一个清理函数；停止或重载时会调用它。没有清理函数的脚本无法自动撤销已经注册的事件。"));
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, U(5.0f)));
    if (ImGui::Button(T("选择 Lua 文件"), ImVec2(U(132.0f), U(32.0f)))) {
        std::filesystem::path selected;
        std::string error;
        if (PickLuaFile(selected, error)) bridge::LoadLuaScript(selected);
    }
    ImGui::SameLine(0.0f, U(7.0f));
    if (ImGui::Button(T("刷新"), ImVec2(U(88.0f), U(32.0f)))) {
        bridge::RefreshLuaScriptDirectory();
    }
    ImGui::SameLine(0.0f, U(7.0f));
    if (ImGui::Button(T("打开目录"), ImVec2(U(104.0f), U(32.0f)))) {
        ShellExecuteW(nullptr, L"open", settings::LuaDirectory().c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
    }
    components::EndCard();
    ImGui::Dummy(ImVec2(0.0f, U(10.0f)));

    const std::vector<bridge::LuaScriptInfo>& scripts = bridge::GetLuaScripts();
    components::BeginCard("LuaScriptList", nullptr, ImVec2(0.0f, U(440.0f)));
    ImGui::PushStyleVar(
        ImGuiStyleVar_ItemSpacing,
        ImVec2(ImGui::GetStyle().ItemSpacing.x, U(3.0f)));
    ImGui::BeginChild("LuaScripts", ImVec2(0.0f, 0.0f));
    if (scripts.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, components::kMuted);
        ImGui::TextWrapped("%s", T("目录中没有 Lua 文件。可以选择文件直接加载，或在设置中更改 Lua 目录。"));
        ImGui::PopStyleColor();
    }
    for (const bridge::LuaScriptInfo& script : scripts) DrawScriptCard(script);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    components::EndCard();

    if (g_delete_popup_requested) {
        ImGui::OpenPopup("##DeleteLuaConfirmation");
        g_delete_popup_requested = false;
    }
    DrawDeleteConfirmation();
}

}  // namespace pztrainer::ui
