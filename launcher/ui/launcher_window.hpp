#pragma once

#include <Windows.h>

#include <filesystem>

namespace launcher::ui {

int RunLauncherWindow(
    HINSTANCE instance,
    int show_command,
    const std::filesystem::path& game_directory,
    const std::filesystem::path& dll_path);

}  // namespace launcher::ui
