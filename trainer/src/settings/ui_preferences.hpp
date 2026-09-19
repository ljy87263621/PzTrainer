#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "settings/localization.hpp"

namespace pztrainer::settings {

void InitializeUiPreferences();
float UiScalePercent();
float UiScale();
void SetUiScalePercent(float percent);
void SetUiLanguage(Language language);
int MenuHotkey();
void SetMenuHotkey(int virtual_key);
bool IsSafeModeEnabled();
void SetSafeModeEnabled(bool enabled);
const std::filesystem::path& ConfigurationDirectory();
const std::filesystem::path& LuaDirectory();
bool SetConfigurationDirectory(const std::filesystem::path& directory,
                               std::string& error);
bool SetLuaDirectory(const std::filesystem::path& directory,
                     std::string& error);
bool SaveUiPreferences(std::string& error);
std::uint64_t UiPreferencesRevision();

}  // namespace pztrainer::settings
