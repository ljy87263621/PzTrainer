#include "runtime/overlay_runtime.hpp"

#include "vmprotect.hpp"

#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/game_startup_gate.hpp"
#include "bridge/experience_bridge.hpp"
#include "bridge/farming_mode_bridge.hpp"
#include "bridge/free_build_bridge.hpp"
#include "bridge/item_bridge.hpp"
#include "bridge/multi_hit_bridge.hpp"
#include "bridge/player_ammo_bridge.hpp"
#include "bridge/player_weapon_reliability_bridge.hpp"
#include "bridge/player_effect_bridge.hpp"
#include "bridge/server_player_effect_bridge.hpp"
#include "bridge/timed_action_bridge.hpp"
#include "bridge/lua_script_bridge.hpp"
#include "bridge/lua_feature_registry.hpp"
#include "bridge/extension_bridge.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "bridge/player_health_bridge.hpp"
#include "bridge/player_carry_bridge.hpp"
#include "bridge/player_condition_bridge.hpp"
#include "bridge/player_movement_bridge.hpp"
#include "bridge/player_teleport_bridge.hpp"
#include "bridge/player_resource_bridge.hpp"
#include "bridge/world_map_reveal_bridge.hpp"
#include "bridge/world_map_player_bridge.hpp"
#include "bridge/world_visual_bridge.hpp"
#include "features/aim/legit_aim.hpp"
#include "features/aim/rage_aim.hpp"
#include "features/visual/native_model_chams.hpp"
#include "features/visual/player_esp.hpp"
#include "features/visual/player_visual_settings.hpp"
#include "features/visual/animal_esp.hpp"
#include "features/visual/animal_visual_settings.hpp"
#include "features/visual/vehicle_esp.hpp"
#include "features/visual/vehicle_visual_settings.hpp"
#include "features/visual/visual_settings.hpp"
#include "features/visual/weapon_ray.hpp"
#include "features/visual/zombie_esp.hpp"
#include "settings/ui_preferences.hpp"
#include "runtime/lua_bridge_resource.hpp"
#include "ui/theme.hpp"
#include "ui/glass_blur.hpp"
#include "ui/menu_burn_reveal.hpp"
#include "ui/menu_hotkey_selector.hpp"
#include "ui/menu_startup_animation.hpp"
#include "ui/teleport_cooldown_overlay.hpp"
#include "ui/trainer_menu.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam);

namespace pztrainer::runtime {
namespace {

using SwapBuffersFn = BOOL(WINAPI*)(HDC);

HMODULE g_module = nullptr;
HANDLE g_instance_mutex = nullptr;
SwapBuffersFn g_original_swap_buffers = nullptr;
WNDPROC g_original_wnd_proc = nullptr;
HWND g_window = nullptr;
std::atomic_bool g_menu_visible{true};
bool g_imgui_initialized = false;
bool g_game_startup_ready = false;
bool g_menu_startup_reveal_prepared = false;
bridge::GameStartupGateState g_last_startup_gate_state =
    bridge::GameStartupGateState::WaitingForJvm;
bridge::GateStatus g_last_gate_status = bridge::GateStatus::Initializing;
bool g_logged_first_zombie_snapshot = false;
bool g_logged_visible_bones = false;
std::uint64_t g_last_health_restore_count = 0;
std::uint64_t g_last_health_sync_count = 0;
std::uint64_t g_last_ammo_refill_count = 0;
std::uint64_t g_last_ammo_sync_count = 0;
std::uint64_t g_last_weapon_unjam_count = 0;
std::uint64_t g_last_weapon_unjam_sync_count = 0;
bool g_native_model_chams_initialized = false;
int g_last_model_chams_initialization_status = 0;
int g_last_model_chams_status = -1;
std::size_t g_last_captured_model_draws = 0;
int g_model_chams_zero_capture_frames = 0;
bool g_logged_model_chams_diagnostics = false;
std::string g_last_legit_status_key;
std::string g_last_rage_status_key;
std::string g_last_item_spawn_status;

std::filesystem::path ModuleDirectory() {
    wchar_t path[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(g_module, path, MAX_PATH);
    if (length == 0) {
        length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    }
    return length == 0 ? std::filesystem::path{} : std::filesystem::path(path).parent_path();
}

void Log(const std::string& message) {
    const std::string line = "[PZ Solo Assist] " + message + "\r\n";
    OutputDebugStringA(line.c_str());

    const std::filesystem::path log_path = ModuleDirectory() / L"pztrainer.log";
    HANDLE file = CreateFileW(
        log_path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD written = 0;
    WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    CloseHandle(file);
}

bool AcquireInstanceMarker() {
    const std::wstring name = L"Local\\PZTrainer.Instance." +
        std::to_wstring(GetCurrentProcessId());
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, FALSE, name.c_str());
    if (mutex == nullptr) {
        Log("Could not create the process instance marker.");
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        Log("Duplicate injection ignored; this process already has an active trainer instance.");
        return false;
    }
    g_instance_mutex = mutex;
    return true;
}

void ReleaseInstanceMarker() {
    if (g_instance_mutex == nullptr) return;
    CloseHandle(g_instance_mutex);
    g_instance_mutex = nullptr;
}

bool IsMouseMessage(UINT message) {
    return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
}

bool IsKeyboardMessage(UINT message) {
    return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
           message == WM_CHAR || message == WM_SYSCHAR;
}

bool IsImeMessage(UINT message) {
    switch (message) {
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_SETCONTEXT:
        case WM_IME_NOTIFY:
        case WM_IME_CONTROL:
        case WM_IME_COMPOSITIONFULL:
        case WM_IME_SELECT:
        case WM_IME_CHAR:
        case WM_IME_REQUEST:
        case WM_IME_KEYDOWN:
        case WM_IME_KEYUP:
            return true;
        default:
            return false;
    }
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ui::HandleMenuHotkeyCaptureMessage(message, wparam)) {
        return 0;
    }

    if (message == WM_KEYUP &&
        wparam == static_cast<WPARAM>(settings::MenuHotkey()) &&
        ui::MenuStartupAnimationComplete() &&
        ui::MenuBurnRevealComplete()) {
        const bool visible = !g_menu_visible.load();
        g_menu_visible.store(visible);
        Log(visible ? "Menu hotkey toggled the menu on."
                    : "Menu hotkey toggled the menu off.");
        return 0;
    }

    if (g_imgui_initialized && g_menu_visible.load() &&
        ui::MenuStartupAnimationComplete() &&
        ui::MenuBurnRevealComplete()) {
        ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam);
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput && IsImeMessage(message)) {
            return IsWindowUnicode(hwnd)
                ? DefWindowProcW(hwnd, message, wparam, lparam)
                : DefWindowProcA(hwnd, message, wparam, lparam);
        }
        if ((io.WantCaptureMouse && IsMouseMessage(message)) ||
            (io.WantCaptureKeyboard && IsKeyboardMessage(message))) {
            return 0;
        }
    }

    return CallWindowProcW(g_original_wnd_proc, hwnd, message, wparam, lparam);
}

bool WaitForGameStartup() {
    if (g_game_startup_ready) return true;

    const bridge::GameStartupGateResult result = bridge::PollGameStartupGate();
    if (result.state != g_last_startup_gate_state) {
        Log(std::string("Startup gate: ") + result.message);
        g_last_startup_gate_state = result.state;
    }
    if (result.state != bridge::GameStartupGateState::Ready) return false;

    g_game_startup_ready = true;
    Log(std::string("Startup gate released: ") + result.message);
    return true;
}

void TryInitializeNativeModelChams() {
    if (g_native_model_chams_initialized) return;
    if (g_last_model_chams_initialization_status != 0 &&
        g_last_model_chams_initialization_status != -1 &&
        g_last_model_chams_initialization_status != -2 &&
        g_last_model_chams_initialization_status != -3) {
        return;
    }

    g_native_model_chams_initialized =
        features::visual::NativeModelChams::Initialize();
    const features::visual::NativeModelChamsDiagnostics diagnostics =
        features::visual::NativeModelChams::Diagnostics();
    if (diagnostics.initialization_status !=
        g_last_model_chams_initialization_status) {
        Log(std::string(g_native_model_chams_initialized
            ? "Native model chams OpenGL hooks initialized, status="
            : "Native model chams OpenGL hooks waiting, status=") +
            std::to_string(diagnostics.initialization_status));
        g_last_model_chams_initialization_status =
            diagnostics.initialization_status;
    }
}

bool InitializeImGui(HDC device_context) {
    g_window = WindowFromDC(device_context);
    if (g_window == nullptr) {
        Log("WindowFromDC failed; waiting for the next frame.");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    settings::InitializeUiPreferences();

    constexpr const char* regular_font_path =
        "C:\\Windows\\Fonts\\segoeui.ttf";
    constexpr const char* semibold_font_path =
        "C:\\Windows\\Fonts\\seguisb.ttf";
    constexpr const char* chinese_font_path =
        "C:\\Windows\\Fonts\\msyh.ttc";
    constexpr float font_size = 16.0f;
    static const ImWchar chinese_ranges[] = {
        0x0020, 0x00FF, 0x3000, 0x303F, 0x4E00, 0x9FFF, 0};

    ImFont* regular_font = io.Fonts->AddFontFromFileTTF(
        regular_font_path, font_size);
    if (regular_font != nullptr) {
        ImFontConfig merge_config{};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            chinese_font_path, font_size, &merge_config, chinese_ranges);
    }

    ImFont* semibold_font = io.Fonts->AddFontFromFileTTF(
        semibold_font_path, font_size);
    if (semibold_font != nullptr) {
        ImFontConfig merge_config{};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            chinese_font_path, font_size, &merge_config, chinese_ranges);
    }

    if (regular_font == nullptr) {
        regular_font = io.Fonts->AddFontFromFileTTF(
            chinese_font_path, 16.0f, nullptr, chinese_ranges);
    }
    if (regular_font == nullptr) {
        io.Fonts->AddFontDefault();
        regular_font = io.Fonts->Fonts.back();
        Log("Segoe UI and Microsoft YaHei were unavailable; using the ImGui default font.");
    }
    io.FontDefault = regular_font;
    ui::SetInterfaceFonts(regular_font, semibold_font);

    ui::ApplyTheme();
    if (!ImGui_ImplWin32_Init(g_window)) {
        Log("ImGui Win32 backend initialization failed.");
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        Log("ImGui OpenGL backend initialization failed.");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    if (!ui::InitializeGlassBlur()) {
        Log(std::string("Glass blur unavailable: ") + ui::GlassBlurError());
    } else {
        Log("Glass blur initialized.");
    }

    SetLastError(ERROR_SUCCESS);
    g_original_wnd_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProcedure)));
    if (g_original_wnd_proc == nullptr && GetLastError() != ERROR_SUCCESS) {
        Log("Failed to install the game-window input hook.");
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_imgui_initialized = true;
    ui::BeginMenuStartupAnimation();
    Log("ImGui initialized. Use the configured menu hotkey to show or hide the menu.");
    return true;
}

BOOL WINAPI HookedSwapBuffers(HDC device_context) {
    bridge::ScopedSynchronousMainThreadInvocationBlock invocation_block;
    if (!WaitForGameStartup()) {
        return g_original_swap_buffers(device_context);
    }
    TryInitializeNativeModelChams();
    if (!g_imgui_initialized) {
        InitializeImGui(device_context);
    }

    if (g_imgui_initialized) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (!ui::MenuStartupAnimationAllowsMenu()) {
            ui::PrepareGlassBlur();
            ui::DrawMenuStartupAnimation();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            return g_original_swap_buffers(device_context);
        }
        if (!g_menu_startup_reveal_prepared) {
            ui::PrepareTrainerMenuStartupReveal();
            ui::BeginMenuBurnReveal();
            g_menu_startup_reveal_prepared = true;
        }

        features::visual::VisualSettings& visual_settings =
            features::visual::GetVisualSettings();
        features::visual::PlayerVisualSettings& player_visual_settings =
            features::visual::GetPlayerVisualSettings();
        features::visual::AnimalVisualSettings& animal_visual_settings =
            features::visual::GetAnimalVisualSettings();
        features::visual::VehicleVisualSettings& vehicle_visual_settings =
            features::visual::GetVehicleVisualSettings();
        bridge::ZombieModelVisualRequest model_visual{};
        model_visual.enabled = visual_settings.zombie_esp &&
            visual_settings.model_effect != features::visual::ZombieModelEffect::Disabled;
        const auto copy_model_color = [](const ImVec4& source) {
            bridge::ZombieModelVisualColor color{};
            color.red = source.x;
            color.green = source.y;
            color.blue = source.z;
            color.alpha = source.w;
            return color;
        };
        model_visual.normal = copy_model_color(features::visual::ModelCaptureMarker(
            features::visual::ZombieVisualState::Default));
        model_visual.behind_wall = copy_model_color(features::visual::ModelCaptureMarker(
            features::visual::ZombieVisualState::BehindWall));
        model_visual.in_view = copy_model_color(features::visual::ModelCaptureMarker(
            features::visual::ZombieVisualState::InView));
        model_visual.behind_wall_enabled =
            visual_settings.model_colors.behind_wall_enabled;
        model_visual.in_view_enabled = visual_settings.model_colors.in_view_enabled;
        model_visual.force_visible = visual_settings.force_model_visibility;
        bridge::ZombieModelVisualRequest player_model_visual{};
        player_model_visual.enabled = player_visual_settings.player_esp &&
            player_visual_settings.model_effect !=
                features::visual::ZombieModelEffect::Disabled;
        player_model_visual.normal = copy_model_color(
            features::visual::PlayerModelCaptureMarker(
                features::visual::ZombieVisualState::Default));
        player_model_visual.behind_wall = copy_model_color(
            features::visual::PlayerModelCaptureMarker(
                features::visual::ZombieVisualState::BehindWall));
        player_model_visual.in_view = copy_model_color(
            features::visual::PlayerModelCaptureMarker(
                features::visual::ZombieVisualState::InView));
        player_model_visual.behind_wall_enabled =
            player_visual_settings.model_colors.behind_wall_enabled;
        player_model_visual.in_view_enabled =
            player_visual_settings.model_colors.in_view_enabled;
        player_model_visual.force_visible =
            player_visual_settings.force_model_visibility;
        bridge::ZombieModelVisualRequest animal_model_visual{};
        animal_model_visual.enabled = animal_visual_settings.animal_esp &&
            animal_visual_settings.model_effect !=
                features::visual::ZombieModelEffect::Disabled;
        animal_model_visual.normal = copy_model_color(
            features::visual::AnimalModelCaptureMarker(
                features::visual::ZombieVisualState::Default));
        animal_model_visual.behind_wall = copy_model_color(
            features::visual::AnimalModelCaptureMarker(
                features::visual::ZombieVisualState::BehindWall));
        animal_model_visual.in_view = copy_model_color(
            features::visual::AnimalModelCaptureMarker(
                features::visual::ZombieVisualState::InView));
        animal_model_visual.behind_wall_enabled =
            animal_visual_settings.model_colors.behind_wall_enabled;
        animal_model_visual.in_view_enabled =
            animal_visual_settings.model_colors.in_view_enabled;
        animal_model_visual.force_visible =
            animal_visual_settings.force_model_visibility;
        bridge::ZombieModelVisualRequest vehicle_model_visual{};
        vehicle_model_visual.enabled = vehicle_visual_settings.vehicle_esp &&
            vehicle_visual_settings.model_effect !=
                features::visual::ZombieModelEffect::Disabled;
        vehicle_model_visual.normal = copy_model_color(
            features::visual::VehicleModelCaptureMarker(
                features::visual::ZombieVisualState::Default));
        vehicle_model_visual.behind_wall = copy_model_color(
            features::visual::VehicleModelCaptureMarker(
                features::visual::ZombieVisualState::BehindWall));
        vehicle_model_visual.in_view = copy_model_color(
            features::visual::VehicleModelCaptureMarker(
                features::visual::ZombieVisualState::InView));
        vehicle_model_visual.behind_wall_enabled =
            vehicle_visual_settings.model_colors.behind_wall_enabled;
        vehicle_model_visual.in_view_enabled =
            vehicle_visual_settings.model_colors.in_view_enabled;
        vehicle_model_visual.force_visible =
            vehicle_visual_settings.force_model_visibility;
        const bool legit_needs_zombies = !g_menu_visible.load() &&
            features::aim::LegitAimNeedsZombieData();
        const bool rage_needs_zombies = !g_menu_visible.load() &&
            features::aim::RageAimNeedsZombieData();
        const bool legit_needs_players = !g_menu_visible.load() &&
            features::aim::LegitAimNeedsPlayerData();
        const bool rage_needs_players = !g_menu_visible.load() &&
            features::aim::RageAimNeedsPlayerData();
        const float zombie_collection_range = std::max(
            visual_settings.zombie_esp ? visual_settings.max_distance : 0.0f,
            std::max(
                legit_needs_zombies ? features::aim::LegitAimCollectionRange() : 0.0f,
                rage_needs_zombies ? features::aim::RageAimCollectionRange() : 0.0f));
        const bridge::FrameSnapshot& frame = bridge::CollectFrameSnapshot(
            visual_settings.zombie_esp || legit_needs_zombies || rage_needs_zombies,
            zombie_collection_range,
            visual_settings.show_skeleton || model_visual.enabled ||
                legit_needs_zombies || rage_needs_zombies,
            model_visual,
            player_visual_settings.player_esp || legit_needs_players ||
                rage_needs_players,
            std::max(
                player_visual_settings.player_esp
                    ? player_visual_settings.max_distance : 0.0f,
                std::max(
                    legit_needs_players
                        ? features::aim::LegitAimCollectionRange() : 0.0f,
                    rage_needs_players
                        ? features::aim::RageAimCollectionRange() : 0.0f)),
            player_visual_settings.show_skeleton || player_model_visual.enabled ||
                legit_needs_players || rage_needs_players,
            player_model_visual,
            animal_visual_settings.animal_esp,
            animal_visual_settings.max_distance,
            animal_visual_settings.show_skeleton || animal_model_visual.enabled,
            animal_model_visual,
            vehicle_visual_settings.vehicle_esp,
            vehicle_visual_settings.max_distance,
            vehicle_model_visual);
        features::visual::UpdateWeaponRay(
            frame.gate_status == bridge::GateStatus::SinglePlayerAllowed);
        features::aim::UpdateLegitAim(
            frame, g_menu_visible.load(), ImGui::GetIO().DisplaySize.x,
            ImGui::GetIO().DisplaySize.y, ImGui::GetIO().DeltaTime);
        features::aim::UpdateRageAim(
            frame, g_menu_visible.load(), ImGui::GetIO().DisplaySize.x,
            ImGui::GetIO().DisplaySize.y);
        const features::aim::LegitAimStatus& legit_status =
            features::aim::GetLegitAimStatus();
        const std::string legit_status_key = legit_status.message + "|" +
            legit_status.weapon_type + "|" +
            std::to_string(static_cast<int>(legit_status.weapon_group)) + "|" +
            std::to_string(legit_status.target_identity);
        if (legit_status_key != g_last_legit_status_key) {
            Log("Legit: " + legit_status.message +
                " weapon=" + (legit_status.weapon_type.empty()
                    ? std::string("none") : legit_status.weapon_type) +
                " group=" + features::aim::WeaponGroupName(
                    legit_status.weapon_group) +
                " target=" + std::to_string(legit_status.target_identity));
            g_last_legit_status_key = legit_status_key;
        }
        const features::aim::RageAimStatus& rage_status =
            features::aim::GetRageAimStatus();
        const std::string rage_status_key = rage_status.message + "|" +
            rage_status.weapon_type + "|" +
            std::to_string(static_cast<int>(rage_status.weapon_group)) + "|" +
            std::to_string(rage_status.target_identity) + "|" +
            std::to_string(rage_status.ballistics_hook_ready) + "|" +
            std::to_string(rage_status.ballistics_override_calls / 120) + "|" +
            std::to_string(rage_status.ballistics_reticle_override_calls / 120) + "|" +
            std::to_string(rage_status.ballistics_target_override_calls / 120) + "|" +
            std::to_string(rage_status.ballistics_hook_character_id) + "|" +
            std::to_string(rage_status.ballistics_published_character_id);
        if (rage_status_key != g_last_rage_status_key) {
            Log("Rage: " + rage_status.message +
                " weapon=" + (rage_status.weapon_type.empty()
                    ? std::string("none") : rage_status.weapon_type) +
                " group=" + features::aim::WeaponGroupName(
                    rage_status.weapon_group) +
                " target=" + std::to_string(rage_status.target_identity) +
                " bulletHook=" + (rage_status.ballistics_hook_ready ? "ready" : "off") +
                " calls=" + std::to_string(rage_status.ballistics_hook_calls) +
                " overrides=" + std::to_string(rage_status.ballistics_override_calls) +
                " reticle=" +
                    std::to_string(rage_status.ballistics_reticle_override_calls) +
                " targets=" +
                    std::to_string(rage_status.ballistics_target_override_calls) +
                " ids=" + std::to_string(rage_status.ballistics_hook_character_id) +
                "/" + std::to_string(rage_status.ballistics_published_character_id));
            g_last_rage_status_key = rage_status_key;
        }
        if (frame.gate_status == bridge::GateStatus::SinglePlayerAllowed) {
            bridge::UpdateItemBridge();
            const bridge::ItemSpawnResult& item_spawn =
                bridge::GetLastItemSpawnResult();
            if (item_spawn.attempted &&
                item_spawn.message != g_last_item_spawn_status) {
                Log("Item spawn: " + item_spawn.message);
                g_last_item_spawn_status = item_spawn.message;
            }
            bridge::UpdateExperienceBridge();
            bridge::UpdateServerPlayerEffectBridge();
            bridge::UpdatePlayerEffectBridge();
            bridge::UpdatePlayerResourceBridge();
            bridge::UpdatePlayerAmmoBridge();
            bridge::UpdatePlayerWeaponReliabilityBridge();
            bridge::UpdateMultiHitBridge();
            bridge::UpdateFreeBuildBridge();
            bridge::UpdateFarmingModeBridge();
            bridge::UpdateTimedActionBridge();
            const bridge::PlayerAmmoStatus& ammo = bridge::GetPlayerAmmoStatus();
            if (ammo.refill_count != g_last_ammo_refill_count ||
                ammo.server_sync_count != g_last_ammo_sync_count) {
                Log("Player ammo: " + ammo.message +
                    " ammo=" + std::to_string(ammo.current_ammo) + "/" +
                    std::to_string(ammo.maximum_ammo) +
                    " refills=" + std::to_string(ammo.refill_count) +
                    " serverSyncs=" + std::to_string(ammo.server_sync_count));
                g_last_ammo_refill_count = ammo.refill_count;
                g_last_ammo_sync_count = ammo.server_sync_count;
            }
            const bridge::PlayerWeaponReliabilityStatus& reliability =
                bridge::GetPlayerWeaponReliabilityStatus();
            if (reliability.unjam_count != g_last_weapon_unjam_count ||
                reliability.server_sync_count !=
                    g_last_weapon_unjam_sync_count) {
                Log("Weapon reliability: " + reliability.message +
                    " unjams=" + std::to_string(reliability.unjam_count) +
                    " serverSyncs=" +
                    std::to_string(reliability.server_sync_count));
                g_last_weapon_unjam_count = reliability.unjam_count;
                g_last_weapon_unjam_sync_count =
                    reliability.server_sync_count;
            }
            const bridge::PlayerHealthStatus& health =
                bridge::GetPlayerHealthStatus();
            if (health.restore_count != g_last_health_restore_count ||
                health.server_sync_count != g_last_health_sync_count) {
                Log("Player health: " + health.message +
                    " restores=" + std::to_string(health.restore_count) +
                    " serverSyncs=" + std::to_string(health.server_sync_count));
                g_last_health_restore_count = health.restore_count;
                g_last_health_sync_count = health.server_sync_count;
            }
        }
        bridge::ApplyPendingLuaFeatureWrites();
        bridge::RefreshItemCatalog();
        bridge::RefreshLuaFeatureRegistry();
        bridge::UpdateLuaScriptBridge();
        bridge::UpdateExtensionBridge(g_menu_visible.load());
        bridge::UpdatePlayerHealthBridge();
        bridge::UpdatePlayerMovementBridge();
        bridge::UpdatePlayerCarryBridge();
        bridge::UpdatePlayerConditionBridge();
        bridge::UpdatePlayerTeleportBridge(g_menu_visible.load());
        bridge::UpdateWorldMapRevealBridge();
        bridge::UpdateWorldMapPlayerBridge();
        bridge::UpdateWorldVisualBridge(
            frame.gate_status == bridge::GateStatus::SinglePlayerAllowed);
        if (frame.gate_status != g_last_gate_status) {
            Log(std::string("Single-player gate: ") + frame.gate_message);
            g_last_gate_status = frame.gate_status;
        }
        if (frame.model_chams_status != g_last_model_chams_status) {
            Log("Native model chams bridge status=" +
                std::to_string(frame.model_chams_status));
            g_last_model_chams_status = frame.model_chams_status;
        }
        if (!g_logged_first_zombie_snapshot && !frame.zombies.empty()) {
            const bridge::ZombieSnapshot& first = frame.zombies.front();
            Log("ESP first snapshot: count=" + std::to_string(frame.zombies.size()) +
                " firstScreen=(" + std::to_string(first.screen_x) + "," +
                std::to_string(first.screen_y) + ") distance=" +
                std::to_string(first.distance) + " health=" +
                std::to_string(first.health_fraction) + " bones=" +
                (first.has_bones ? "ready" : "unavailable") + " boneFailure=" +
                std::to_string(first.bone_failure) + " behindWall=" +
                (first.behind_wall ? "true" : "false") + " inView=" +
                (first.in_view ? "true" : "false") + " forward=(" +
                std::to_string(first.forward_x) + "," + std::to_string(first.forward_y) +
                ") modelInstances=" + std::to_string(first.model_instance_count) +
                " modelChamsStatus=" + std::to_string(frame.model_chams_status) +
                " display=(" +
                std::to_string(ImGui::GetIO().DisplaySize.x) + "," +
                std::to_string(ImGui::GetIO().DisplaySize.y) + ") zoom=" +
                std::to_string(frame.camera_zoom));
            g_logged_first_zombie_snapshot = true;
        }
        if (!g_logged_visible_bones) {
            for (const bridge::ZombieSnapshot& zombie : frame.zombies) {
                if (!zombie.has_bones || zombie.screen_x < 0.0f || zombie.screen_y < 0.0f ||
                    zombie.screen_x > ImGui::GetIO().DisplaySize.x ||
                    zombie.screen_y > ImGui::GetIO().DisplaySize.y) continue;
                const bridge::ScreenPoint& head = zombie.bones[0];
                const bridge::ScreenPoint& pelvis = zombie.bones[4];
                const bridge::ScreenPoint& left_foot = zombie.bones[15];
                const bridge::ScreenPoint& right_foot = zombie.bones[18];
                Log("Visible bones: box=(" + std::to_string(zombie.screen_x) + "," +
                    std::to_string(zombie.screen_top_y) + ".." + std::to_string(zombie.screen_y) +
                    ") head=(" + std::to_string(head.x) + "," + std::to_string(head.y) +
                    ") pelvis=(" + std::to_string(pelvis.x) + "," + std::to_string(pelvis.y) +
                    ") feet=(" + std::to_string(left_foot.x) + "," + std::to_string(left_foot.y) +
                    ")/(" + std::to_string(right_foot.x) + "," + std::to_string(right_foot.y) +
                    ") row=(" + std::to_string(zombie.bone_debug_row[0]) + "," +
                    std::to_string(zombie.bone_debug_row[1]) + "," +
                    std::to_string(zombie.bone_debug_row[2]) + ") column=(" +
                    std::to_string(zombie.bone_debug_column[0]) + "," +
                    std::to_string(zombie.bone_debug_column[1]) + "," +
                    std::to_string(zombie.bone_debug_column[2]) + ")");
                g_logged_visible_bones = true;
                break;
            }
        }
        if (frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
            g_logged_first_zombie_snapshot = false;
            g_logged_visible_bones = false;
        }
        const bool capture_model_draws = g_native_model_chams_initialized &&
            (model_visual.enabled || player_model_visual.enabled ||
             animal_model_visual.enabled || vehicle_model_visual.enabled) &&
            frame.gate_status == bridge::GateStatus::SinglePlayerAllowed &&
            frame.model_chams_status == 2;
        const std::size_t captured_model_draws =
            features::visual::NativeModelChams::CapturedDrawCount();
        if (captured_model_draws != g_last_captured_model_draws &&
            (captured_model_draws > 0 || g_last_captured_model_draws > 0)) {
            Log("Native model chams captured draws=" +
                std::to_string(captured_model_draws));
            g_last_captured_model_draws = captured_model_draws;
        }
        if (capture_model_draws && captured_model_draws == 0) {
            ++g_model_chams_zero_capture_frames;
            if (!g_logged_model_chams_diagnostics &&
                g_model_chams_zero_capture_frames >= 120) {
                const features::visual::NativeModelChamsDiagnostics diagnostics =
                    features::visual::NativeModelChams::Diagnostics();
                Log("Native model chams diagnostics: drawCalls=" +
                    std::to_string(diagnostics.draw_calls) + " instancedDrawCalls=" +
                    std::to_string(diagnostics.instanced_draw_calls) + " matrixUploads=" +
                    std::to_string(diagnostics.matrix_uploads) + " useProgramCalls=" +
                    std::to_string(diagnostics.use_program_calls) + " captureDrawCalls=" +
                    std::to_string(diagnostics.capture_draw_calls) +
                    " captureDrawsWithoutProgram=" +
                    std::to_string(diagnostics.capture_draws_without_program) +
                    " captureEnableCalls=" +
                    std::to_string(diagnostics.capture_enable_calls) +
                    " captureEnableTrueCalls=" +
                    std::to_string(diagnostics.capture_enable_true_calls) +
                    " captureContextBinds=" +
                    std::to_string(diagnostics.capture_context_binds) +
                    " foreignContextUpdatesIgnored=" +
                    std::to_string(diagnostics.foreign_context_updates_ignored) +
                    " foreignContextDrawsIgnored=" +
                    std::to_string(diagnostics.foreign_context_draws_ignored) +
                    " programs=" +
                    std::to_string(diagnostics.classified_programs) + " outlinePrograms=" +
                    std::to_string(diagnostics.outline_programs) + " instancedPrograms=" +
                    std::to_string(diagnostics.instanced_programs) + " vehiclePrograms=" +
                    std::to_string(diagnostics.vehicle_programs) + " vehicleTintedDrawCalls=" +
                    std::to_string(diagnostics.vehicle_tinted_draw_calls));
                g_logged_model_chams_diagnostics = true;
            }
        } else {
            g_model_chams_zero_capture_frames = 0;
            if (captured_model_draws > 0) g_logged_model_chams_diagnostics = false;
        }
        features::visual::NativeModelChams::Replay(
            frame, visual_settings, player_visual_settings,
            animal_visual_settings, vehicle_visual_settings);
        if (g_menu_visible.load()) {
            ui::PrepareGlassBlur();
        }
        features::visual::DrawZombieEsp(frame, visual_settings);
        features::visual::DrawPlayerEsp(frame, player_visual_settings);
        features::visual::DrawAnimalEsp(frame, animal_visual_settings);
        features::visual::DrawVehicleEsp(frame, vehicle_visual_settings);
        features::visual::DrawWeaponRay();
        ui::DrawTeleportCooldownOverlay();

        ui::DrawTrainerMenu(g_menu_visible, frame);
        bridge::DrawLuaAimRange();
        bridge::DrawLuaUi();
        ui::DrawMenuBurnReveal(ui::GetTrainerMenuLayout());
        ui::DrawMenuStartupAnimation();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        features::visual::NativeModelChams::SetCaptureEnabled(
            capture_model_draws, frame, visual_settings, player_visual_settings,
            animal_visual_settings, vehicle_visual_settings);
    }

    return g_original_swap_buffers(device_context);
}

DWORD WINAPI BootstrapThread(LPVOID) {
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.HookBootstrap");
    if (!AcquireInstanceMarker()) {
        PZ_VMP_END();
        return 0;
    }
    Log("DLL loaded; waiting for the OpenGL presentation function.");

    std::string lua_bridge_error;
    const std::filesystem::path lua_bridge_jar =
        EnsureLuaBridgeJar(lua_bridge_error);
    if (lua_bridge_jar.empty()) {
        Log("Lua bridge cache extraction failed: " + lua_bridge_error);
    } else {
        Log("Lua bridge JAR ready: " + lua_bridge_jar.u8string());
    }

    HMODULE gdi = nullptr;
    while (gdi == nullptr) {
        gdi = GetModuleHandleW(L"gdi32.dll");
        if (gdi == nullptr) {
            Sleep(100);
        }
    }

    void* swap_buffers = reinterpret_cast<void*>(GetProcAddress(gdi, "SwapBuffers"));
    if (swap_buffers == nullptr) {
        Log("Could not resolve gdi32!SwapBuffers.");
        ReleaseInstanceMarker();
        return 1;
    }

    if (MH_Initialize() != MH_OK) {
        Log("MinHook initialization failed.");
        ReleaseInstanceMarker();
        return 1;
    }
    if (MH_CreateHook(
            swap_buffers,
            reinterpret_cast<void*>(&HookedSwapBuffers),
            reinterpret_cast<void**>(&g_original_swap_buffers)) != MH_OK) {
        Log("Failed to create the SwapBuffers hook.");
        ReleaseInstanceMarker();
        return 1;
    }
    if (MH_EnableHook(swap_buffers) != MH_OK) {
        Log("Failed to enable the SwapBuffers hook.");
        ReleaseInstanceMarker();
        return 1;
    }

    Log("SwapBuffers hook enabled.");
    PZ_VMP_END();
    return 0;
}

}  // namespace

void Start(HMODULE module) {
    g_module = module;
    SetLuaBridgeResourceModule(module);
    HANDLE thread = CreateThread(nullptr, 0, BootstrapThread, nullptr, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

}  // namespace pztrainer::runtime
