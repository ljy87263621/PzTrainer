#pragma once

#include <filesystem>
#include <string>

namespace launcher {

struct LaunchResult {
    bool success = false;
    bool launched_game = false;
    std::wstring message;
};

LaunchResult InjectRunningGame(const std::filesystem::path& dll_path);

LaunchResult LaunchAndInject(
    const std::filesystem::path& game_directory,
    const std::filesystem::path& dll_path);

}  // namespace launcher
