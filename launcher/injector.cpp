#include "injector.hpp"

#include <Windows.h>
#include <Shellapi.h>
#include <TlHelp32.h>

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "manual_mapper.hpp"
#include "resource.h"
#include "vmprotect.hpp"

namespace launcher {
namespace {

DWORD FindProcess(const wchar_t* executable_name) {
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD process_id = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, executable_name) == 0) {
                process_id = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return process_id;
}

DWORD LaunchGame(std::wstring& error) {
    constexpr wchar_t kSteamLaunchUri[] = L"steam://run/108600";
    const HINSTANCE launch_result = ShellExecuteW(
        nullptr, L"open", kSteamLaunchUri, nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(launch_result) <= 32) {
        error = L"无法通过 Steam 启动游戏，请确认 Steam 已安装并已登录。";
        return 0;
    }

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(90);
    while (std::chrono::steady_clock::now() < deadline) {
        const DWORD process_id = FindProcess(L"ProjectZomboid64.exe");
        if (process_id != 0) {
            return process_id;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    error = L"已通过 Steam 请求启动游戏，但 90 秒内未检测到 ProjectZomboid64.exe。";
    return 0;
}

std::vector<std::uint8_t> LoadEmbeddedTrainer() {
    PZ_VMP_BEGIN_ULTRA("PZ.EXE.EmbeddedTrainerImage");
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDR_PZTRAINER_DLL), RT_RCDATA);
    if (resource == nullptr) return {};

    const HGLOBAL loaded = LoadResource(module, resource);
    if (loaded == nullptr) return {};
    const DWORD size = SizeofResource(module, resource);
    const auto* bytes = static_cast<const std::uint8_t*>(
        LockResource(loaded));
    if (bytes == nullptr || size == 0) return {};
    std::vector<std::uint8_t> image(bytes, bytes + size);
    PZ_VMP_END();
    return image;
}

bool InjectLibrary(DWORD process_id, std::wstring& error) {
    const std::vector<std::uint8_t> image = LoadEmbeddedTrainer();
    if (image.empty()) {
        error = L"启动器内没有可用的 pztrainer.dll 资源。";
        return false;
    }
    return ManualMapImage(process_id, image, error);
}

}  // namespace

LaunchResult InjectRunningGame(const std::filesystem::path&) {
    LaunchResult result;
    const DWORD process_id = FindProcess(L"ProjectZomboid64.exe");
    if (process_id == 0) {
        result.message =
            L"ProjectZomboid64.exe 尚未运行。请先自行启动游戏，再运行测试注入器。";
        return result;
    }

    if (!InjectLibrary(process_id, result.message)) {
        return result;
    }
    result.success = true;
    result.message = L"测试注入成功。进入游戏后按 Insert 打开菜单。";
    return result;
}

LaunchResult LaunchAndInject(
    const std::filesystem::path&,
    const std::filesystem::path&) {
    PZ_VMP_BEGIN_ULTRA("PZ.EXE.LaunchAndInject");
    LaunchResult result;
    DWORD process_id = FindProcess(L"ProjectZomboid64.exe");
    if (process_id == 0) {
        process_id = LaunchGame(result.message);
        if (process_id == 0) {
            return result;
        }
        result.launched_game = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }

    if (!InjectLibrary(process_id, result.message)) {
        return result;
    }
    result.success = true;
    result.message = result.launched_game
        ? L"加载成功，游戏已经启动。进入游戏后按 Insert 打开菜单。"
        : L"加载成功。进入游戏后按 Insert 打开菜单。";
    PZ_VMP_END();
    return result;
}

}  // namespace launcher
