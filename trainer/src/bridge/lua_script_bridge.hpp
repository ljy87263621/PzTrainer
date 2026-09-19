#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "bridge/lua_configuration.hpp"

namespace pztrainer::bridge {

enum class LuaScriptState {
    Unloaded,
    Loading,
    Loaded,
    Unloading,
    Error,
};

struct LuaScriptInfo {
    std::filesystem::path path;
    std::string name;
    std::string modified;
    std::string message;
    LuaScriptState state = LuaScriptState::Unloaded;
    bool auto_reload = false;
    bool cleanup_available = false;
    bool file_exists = true;
};

void UpdateLuaScriptBridge();
const std::vector<LuaScriptInfo>& GetLuaScripts();
bool LoadLuaScript(const std::filesystem::path& path);
bool ReloadLuaScript(const std::filesystem::path& path);
bool UnloadLuaScript(const std::filesystem::path& path);
bool SetLuaScriptAutoReload(const std::filesystem::path& path, bool enabled);
std::vector<LuaScriptConfiguration> CaptureLuaScriptConfiguration();
void ApplyLuaScriptConfiguration(
    std::vector<LuaScriptConfiguration> configuration);
void RefreshLuaScriptDirectory();

}  // namespace pztrainer::bridge
