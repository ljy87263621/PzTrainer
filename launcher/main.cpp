#include "ui/launcher_window.hpp"

#include <Windows.h>
#include <Shellapi.h>

#include <filesystem>
#include <system_error>

namespace {

std::filesystem::path FindGameDirectory(const std::filesystem::path& launcher_directory) {
    std::error_code error;
    const std::filesystem::path working_directory = std::filesystem::current_path(error);
    if (!error && std::filesystem::exists(working_directory / L"ProjectZomboid64.exe")) {
        return working_directory;
    }

    std::filesystem::path candidate = launcher_directory;
    for (int depth = 0; depth < 6 && !candidate.empty(); ++depth) {
        error.clear();
        if (std::filesystem::exists(candidate / L"ProjectZomboid64.exe", error) && !error) {
            return candidate;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return !working_directory.empty() ? working_directory : launcher_directory;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    wchar_t executable_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    const std::filesystem::path output_directory =
        std::filesystem::path(executable_path).parent_path();

    int argument_count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    const std::filesystem::path game_directory = argument_count >= 2
        ? std::filesystem::path(arguments[1])
        : FindGameDirectory(output_directory);
    if (arguments != nullptr) {
        LocalFree(arguments);
    }

    return launcher::ui::RunLauncherWindow(
        instance,
        show_command,
        game_directory,
        output_directory / L"pztrainer.dll");
}
