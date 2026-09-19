#include "features/aim/legit_aim.hpp"

#include <jni.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "features/aim/aim_target_point.hpp"

namespace pztrainer::features::aim {
namespace {

struct Bindings {
    bool ready = false;
    jclass iso_player = nullptr;
    jclass inventory_item = nullptr;
    jclass hand_weapon = nullptr;
    jclass mouse = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_primary_item = nullptr;
    jmethodID is_aiming = nullptr;
    jmethodID set_recoil_x = nullptr;
    jmethodID set_recoil_y = nullptr;
    jmethodID get_full_type = nullptr;
    jmethodID is_ranged = nullptr;
    jmethodID get_max_range = nullptr;
    jmethodID get_max_damage = nullptr;
    jmethodID mouse_get_x = nullptr;
    jmethodID mouse_get_y = nullptr;
};

struct TargetBinding {
    TargetPoint point;
    std::size_t bone_index;
};

struct Candidate {
    bool valid = false;
    bool is_player = false;
    std::int32_t identity = 0;
    bridge::ScreenPoint point{};
    float cursor_distance_squared = std::numeric_limits<float>::max();
    float priority_score = -std::numeric_limits<float>::max();
};

constexpr std::array<TargetBinding, 15> kTargetBindings{{
    {TargetPoint::Head, 0},
    {TargetPoint::Neck, 1},
    {TargetPoint::Chest, 2},
    {TargetPoint::Abdomen, 3},
    {TargetPoint::Pelvis, 4},
    {TargetPoint::LeftArm, 6},
    {TargetPoint::RightArm, 10},
    {TargetPoint::LeftHand, 8},
    {TargetPoint::RightHand, 12},
    {TargetPoint::LeftThigh, 13},
    {TargetPoint::RightThigh, 16},
    {TargetPoint::LeftCalf, 14},
    {TargetPoint::RightCalf, 17},
    {TargetPoint::LeftFoot, 15},
    {TargetPoint::RightFoot, 18},
}};

Bindings g_bindings;
LegitAimStatus g_status;
std::int32_t g_locked_identity = 0;

constexpr float kCloseCrawlerPriorityRange = 2.0f;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(class_loader_class, get_system_loader);
    env->DeleteLocalRef(class_loader_class);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.inventory_item = LoadGlobalClass(env, "zombie/inventory/InventoryItem");
    g_bindings.hand_weapon = LoadGlobalClass(env, "zombie/inventory/types/HandWeapon");
    g_bindings.mouse = LoadGlobalClass(env, "zombie/input/Mouse");
    if (g_bindings.iso_player == nullptr || g_bindings.inventory_item == nullptr ||
        g_bindings.hand_weapon == nullptr || g_bindings.mouse == nullptr) {
        return false;
    }

    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_primary_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.is_aiming = env->GetMethodID(g_bindings.iso_player, "isAiming", "()Z");
    g_bindings.set_recoil_x = env->GetMethodID(
        g_bindings.iso_player, "setRecoilVarX", "(F)V");
    g_bindings.set_recoil_y = env->GetMethodID(
        g_bindings.iso_player, "setRecoilVarY", "(F)V");
    g_bindings.get_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.is_ranged = env->GetMethodID(g_bindings.hand_weapon, "isRanged", "()Z");
    g_bindings.get_max_range = env->GetMethodID(
        g_bindings.hand_weapon, "getMaxRange", "()F");
    g_bindings.get_max_damage = env->GetMethodID(
        g_bindings.hand_weapon, "getMaxDamage", "()F");
    g_bindings.mouse_get_x = env->GetStaticMethodID(g_bindings.mouse, "getXA", "()I");
    g_bindings.mouse_get_y = env->GetStaticMethodID(g_bindings.mouse, "getYA", "()I");
    g_bindings.ready = !ClearException(env) && g_bindings.get_player != nullptr &&
        g_bindings.get_primary_item != nullptr && g_bindings.is_aiming != nullptr &&
        g_bindings.set_recoil_x != nullptr &&
        g_bindings.set_recoil_y != nullptr && g_bindings.get_full_type != nullptr &&
        g_bindings.is_ranged != nullptr && g_bindings.get_max_range != nullptr &&
        g_bindings.get_max_damage != nullptr && g_bindings.mouse_get_x != nullptr &&
        g_bindings.mouse_get_y != nullptr;
    return g_bindings.ready;
}

std::string ReadJavaString(JNIEnv* env, jobject object, jmethodID method) {
    jstring value = static_cast<jstring>(env->CallObjectMethod(object, method));
    if (value == nullptr || ClearException(env)) {
        if (value != nullptr) env->DeleteLocalRef(value);
        return {};
    }
    std::string result;
    const char* text = env->GetStringUTFChars(value, nullptr);
    if (text != nullptr) {
        result = text;
        env->ReleaseStringUTFChars(value, text);
    }
    ClearException(env);
    env->DeleteLocalRef(value);
    return result;
}

std::string Lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool Contains(const std::string& value, const char* token) {
    return value.find(token) != std::string::npos;
}

WeaponGroup ClassifyWeapon(const std::string& full_type, float maximum_range) {
    const std::string type = Lowercase(full_type);
    if (Contains(type, "shotgun")) return WeaponGroup::Shotgun;
    if (Contains(type, "smg") || Contains(type, "mp5") ||
        Contains(type, "uzi") || Contains(type, "vector")) {
        return WeaponGroup::Smg;
    }
    if (Contains(type, "pistol") || Contains(type, "revolver") ||
        Contains(type, "handgun")) {
        return WeaponGroup::Pistol;
    }
    if (Contains(type, "huntingrifle") || Contains(type, "varmintrifle") ||
        Contains(type, "sniper") || Contains(type, "msr7")) {
        return WeaponGroup::Sniper;
    }
    if (Contains(type, "m14")) return WeaponGroup::Rifle;
    if (maximum_range >= 35.0f) return WeaponGroup::Sniper;
    if (maximum_range <= 18.0f && !Contains(type, "rifle") &&
        !Contains(type, "carbine")) {
        return WeaponGroup::Pistol;
    }
    return WeaponGroup::Rifle;
}

const LegitWeaponSettings* ActivePreset(const AimSettings& settings,
                                        WeaponGroup weapon_group) {
    const LegitWeaponSettings& specific = settings.legit_presets[
        static_cast<std::size_t>(weapon_group)];
    if (specific.enabled) return &specific;
    const LegitWeaponSettings& global = settings.legit_presets[
        static_cast<std::size_t>(WeaponGroup::Global)];
    return global.enabled ? &global : nullptr;
}

bool PointVisible(const bridge::ScreenPoint& point, float width, float height) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        point.x >= 0.0f && point.y >= 0.0f &&
        point.x <= width && point.y <= height;
}

template <typename Snapshot>
bridge::ScreenPoint PoseFallbackPoint(const Snapshot& target,
                                      TargetPoint target_point) {
    float vertical_ratio = 0.50f;
    switch (target_point) {
        case TargetPoint::Head: vertical_ratio = 0.08f; break;
        case TargetPoint::Neck: vertical_ratio = 0.18f; break;
        case TargetPoint::Chest: vertical_ratio = 0.32f; break;
        case TargetPoint::LeftArm:
        case TargetPoint::RightArm: vertical_ratio = 0.38f; break;
        case TargetPoint::Abdomen: vertical_ratio = 0.46f; break;
        case TargetPoint::LeftHand:
        case TargetPoint::RightHand: vertical_ratio = 0.54f; break;
        case TargetPoint::Pelvis: vertical_ratio = 0.58f; break;
        case TargetPoint::LeftThigh:
        case TargetPoint::RightThigh: vertical_ratio = 0.68f; break;
        case TargetPoint::LeftCalf:
        case TargetPoint::RightCalf: vertical_ratio = 0.82f; break;
        case TargetPoint::LeftFoot:
        case TargetPoint::RightFoot: vertical_ratio = 0.94f; break;
    }
    return {
        target.screen_x,
        target.screen_top_y +
            (target.screen_y - target.screen_top_y) * vertical_ratio,
    };
}

Candidate EvaluateZombie(const bridge::ZombieSnapshot& zombie,
                         const LegitWeaponSettings& preset,
                         float weapon_damage, float cursor_x, float cursor_y,
                         float viewport_width, float viewport_height) {
    Candidate result{};
    if (zombie.dead || zombie.health_fraction <= 0.0f ||
        zombie.current_health <= 0.0f ||
        zombie.distance > preset.range ||
        (preset.wall_check && zombie.behind_wall)) {
        return result;
    }
    const float estimated_damage_percent = std::clamp(
        weapon_damage / zombie.current_health * 100.0f, 0.0f, 100.0f);

    for (const TargetBinding& binding : kTargetBindings) {
        const std::uint32_t bit = static_cast<std::uint32_t>(binding.point);
        if ((preset.target_points & bit) == 0) continue;
        const bridge::ScreenPoint fallback =
            PoseFallbackPoint(zombie, binding.point);
        bridge::ScreenPoint point = ResolveZombieAimPoint(
            zombie.has_bones, zombie.unstable_pose,
            zombie.bones[binding.bone_index], fallback);
        if (!PointVisible(point, viewport_width, viewport_height)) {
            point = PoseFallbackPoint(zombie, binding.point);
        }
        if (!PointVisible(point, viewport_width, viewport_height)) continue;
        const float delta_x = point.x - cursor_x;
        const float delta_y = point.y - cursor_y;
        const float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (distance_squared < result.cursor_distance_squared) {
            result.valid = true;
            result.identity = zombie.identity;
            result.point = point;
            result.cursor_distance_squared = distance_squared;
        }
    }
    if (result.valid) {
        const float distance_score = 1.0f - std::clamp(
            zombie.distance / std::max(preset.range, 1.0f), 0.0f, 1.0f);
        const float health_score = 1.0f - std::clamp(
            zombie.health_fraction, 0.0f, 1.0f);
        const float damage_score = preset.minimum_damage <= 0.01f
            ? 1.0f
            : std::clamp(
                estimated_damage_percent / preset.minimum_damage, 0.0f, 1.0f);
        const float viewport_diagonal = std::max(
            1.0f, std::sqrt(viewport_width * viewport_width +
                            viewport_height * viewport_height));
        const float cursor_score = 1.0f - std::clamp(
            std::sqrt(result.cursor_distance_squared) / viewport_diagonal,
            0.0f, 1.0f);
        result.priority_score = distance_score * 0.62f +
            health_score * 0.18f + damage_score * 0.12f +
            cursor_score * 0.08f;
        if (preset.prioritize_upright) {
            result.priority_score += zombie.unstable_pose ? -0.10f : 0.10f;
        }
        if (zombie.crawling && zombie.distance <= kCloseCrawlerPriorityRange) {
            const float threat = 1.0f - std::clamp(
                zombie.distance / kCloseCrawlerPriorityRange, 0.0f, 1.0f);
            result.priority_score += 0.35f + threat * 0.35f;
        }
        if (zombie.identity == g_locked_identity) {
            result.priority_score += 0.04f;
        }
    }
    return result;
}

Candidate EvaluatePlayer(const bridge::PlayerSnapshot& player,
                         const LegitWeaponSettings& preset,
                         float weapon_damage, float cursor_x, float cursor_y,
                         float viewport_width, float viewport_height) {
    Candidate result{};
    if (player.dead || !player.pvp_enabled || player.health_fraction <= 0.0f ||
        player.current_health <= 0.0f || player.distance > preset.range ||
        (preset.wall_check && player.behind_wall)) {
        return result;
    }
    const float estimated_damage_percent = std::clamp(
        weapon_damage / player.current_health * 100.0f, 0.0f, 100.0f);
    for (const TargetBinding& binding : kTargetBindings) {
        const std::uint32_t bit = static_cast<std::uint32_t>(binding.point);
        if ((preset.target_points & bit) == 0) continue;
        bridge::ScreenPoint point = player.has_bones
            ? player.bones[binding.bone_index]
            : PoseFallbackPoint(player, binding.point);
        if (!PointVisible(point, viewport_width, viewport_height)) {
            point = PoseFallbackPoint(player, binding.point);
        }
        if (!PointVisible(point, viewport_width, viewport_height)) continue;
        const float delta_x = point.x - cursor_x;
        const float delta_y = point.y - cursor_y;
        const float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (distance_squared < result.cursor_distance_squared) {
            result.valid = true;
            result.is_player = true;
            result.identity = player.identity;
            result.point = point;
            result.cursor_distance_squared = distance_squared;
        }
    }
    if (result.valid) {
        const float distance_score = 1.0f - std::clamp(
            player.distance / std::max(preset.range, 1.0f), 0.0f, 1.0f);
        const float health_score = 1.0f - std::clamp(
            player.health_fraction, 0.0f, 1.0f);
        const float damage_score = preset.minimum_damage <= 0.01f
            ? 1.0f
            : std::clamp(
                estimated_damage_percent / preset.minimum_damage, 0.0f, 1.0f);
        const float viewport_diagonal = std::max(
            1.0f, std::sqrt(viewport_width * viewport_width +
                            viewport_height * viewport_height));
        const float cursor_score = 1.0f - std::clamp(
            std::sqrt(result.cursor_distance_squared) / viewport_diagonal,
            0.0f, 1.0f);
        result.priority_score = distance_score * 0.62f +
            health_score * 0.18f + damage_score * 0.12f +
            cursor_score * 0.08f;
        if (preset.prioritize_upright) {
            result.priority_score += player.prone ? -0.10f : 0.10f;
        }
        if (player.identity == g_locked_identity) {
            result.priority_score += 0.04f;
        }
    }
    return result;
}

bool BetterCandidate(const Candidate& candidate, const Candidate& best) {
    if (!candidate.valid) return false;
    if (!best.valid) return true;
    if (candidate.is_player != best.is_player) return candidate.is_player;
    return candidate.priority_score > best.priority_score + 0.0001f ||
        (std::abs(candidate.priority_score - best.priority_score) <= 0.0001f &&
         candidate.cursor_distance_squared < best.cursor_distance_squared);
}

Candidate SelectTarget(const bridge::FrameSnapshot& frame,
                       const LegitWeaponSettings& preset,
                       float weapon_damage, float cursor_x, float cursor_y,
                       float viewport_width, float viewport_height) {
    Candidate best{};
    if (preset.target_zombies) {
        for (const bridge::ZombieSnapshot& zombie : frame.zombies) {
            Candidate candidate = EvaluateZombie(
                zombie, preset, weapon_damage, cursor_x, cursor_y,
                viewport_width, viewport_height);
            if (BetterCandidate(candidate, best)) best = candidate;
        }
    }
    if (preset.target_players && frame.local_pvp_enabled) {
        for (const bridge::PlayerSnapshot& player : frame.players) {
            Candidate candidate = EvaluatePlayer(
                player, preset, weapon_damage, cursor_x, cursor_y,
                viewport_width, viewport_height);
            if (BetterCandidate(candidate, best)) best = candidate;
        }
    }
    return best;
}

bool MoveGameCursor(float viewport_x, float viewport_y,
                    float viewport_width, float viewport_height) {
    HWND window = GetForegroundWindow();
    if (window == nullptr || viewport_width <= 0.0f || viewport_height <= 0.0f) {
        return false;
    }
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != GetCurrentProcessId()) return false;

    RECT client{};
    if (GetClientRect(window, &client) == FALSE) return false;
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return false;

    POINT point{
        static_cast<LONG>(std::lround(viewport_x * width / viewport_width)),
        static_cast<LONG>(std::lround(viewport_y * height / viewport_height)),
    };
    if (ClientToScreen(window, &point) == FALSE) return false;
    return SetCursorPos(point.x, point.y) != FALSE;
}

void ClearTarget(const char* message) {
    g_locked_identity = 0;
    g_status.aiming = false;
    g_status.target_locked = false;
    g_status.target_identity = 0;
    g_status.message = message;
}

}  // namespace

void UpdateLegitAim(const bridge::FrameSnapshot& frame, bool menu_visible,
                    float viewport_width, float viewport_height,
                    float delta_time) {
    AimSettings& settings = GetAimSettings();
    g_status.firearm_ready = false;
    if (!settings.legit_enabled) {
        ClearTarget("辅助瞄准总开关已关闭");
        return;
    }
    if (settings.rage_enabled) {
        ClearTarget("Rage 开启时暂停 Legit 辅助");
        return;
    }
    if (frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        ClearTarget("等待游戏世界");
        return;
    }
    JNIEnv* env = bridge::GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        ClearTarget("Legit JNI 绑定尚未就绪");
        return;
    }
    g_status.initialized = true;

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    jobject weapon = player == nullptr
        ? nullptr : env->CallObjectMethod(player, g_bindings.get_primary_item);
    if (player == nullptr || weapon == nullptr || ClearException(env) ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE ||
        env->CallBooleanMethod(weapon, g_bindings.is_ranged) != JNI_TRUE ||
        ClearException(env)) {
        if (weapon != nullptr) env->DeleteLocalRef(weapon);
        if (player != nullptr) env->DeleteLocalRef(player);
        ClearTarget("当前主手不是远程枪械");
        return;
    }

    const std::string weapon_type = ReadJavaString(env, weapon, g_bindings.get_full_type);
    const float maximum_range = env->CallFloatMethod(weapon, g_bindings.get_max_range);
    const float maximum_damage = env->CallFloatMethod(weapon, g_bindings.get_max_damage);
    if (ClearException(env)) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget("读取枪械参数失败");
        return;
    }
    const WeaponGroup weapon_group = ClassifyWeapon(weapon_type, maximum_range);
    g_status.firearm_ready = true;
    g_status.weapon_group = weapon_group;
    g_status.weapon_type = weapon_type;
    const LegitWeaponSettings* preset = ActivePreset(settings, weapon_group);
    if (preset == nullptr) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget("当前枪械预设未启用");
        return;
    }

    if (preset->remove_visual_recoil) {
        env->CallVoidMethod(player, g_bindings.set_recoil_x, 0.0f);
        env->CallVoidMethod(player, g_bindings.set_recoil_y, 0.0f);
        ClearException(env);
    }
    const bool aiming = env->CallBooleanMethod(player, g_bindings.is_aiming) == JNI_TRUE;
    if (ClearException(env) || menu_visible || !aiming || !preset->automatic_aim) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget(menu_visible ? "菜单打开时暂停辅助瞄准" :
                    !preset->automatic_aim ? "当前预设自动辅助已关闭" :
                    "按住瞄准键后开始辅助");
        return;
    }
    g_status.aiming = true;

    const jint cursor_x = env->CallStaticIntMethod(g_bindings.mouse, g_bindings.mouse_get_x);
    const jint cursor_y = env->CallStaticIntMethod(g_bindings.mouse, g_bindings.mouse_get_y);
    if (ClearException(env)) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget("读取瞄准光标失败");
        return;
    }
    const Candidate target = SelectTarget(
        frame, *preset, std::max(maximum_damage, 0.01f),
        static_cast<float>(cursor_x), static_cast<float>(cursor_y),
        viewport_width, viewport_height);
    if (!target.valid) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget("范围内没有符合条件的可见目标");
        return;
    }

    g_locked_identity = target.identity;
    g_status.target_locked = true;
    g_status.target_identity = g_locked_identity;

    const float accuracy = std::clamp(preset->accuracy, 0.0f, 100.0f) / 100.0f;
    const float allowed_error_pixels = 1.0f + (1.0f - accuracy) * 8.0f;
    const float delta_x = target.point.x - static_cast<float>(cursor_x);
    const float delta_y = target.point.y - static_cast<float>(cursor_y);
    const float cursor_error = std::sqrt(delta_x * delta_x + delta_y * delta_y);
    if (!std::isfinite(cursor_error)) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ClearTarget("目标屏幕坐标无效");
        return;
    }

    const float smoothing_percent = std::clamp(preset->smoothing, 1.0f, 100.0f);
    const float smoothing = (smoothing_percent - 1.0f) / 99.0f;
    const float response_speed = 34.0f - smoothing * 30.0f;
    const float frame_response = 1.0f - std::exp(
        -response_speed * std::clamp(delta_time, 0.001f, 0.05f));
    const float response = smoothing_percent <= 1.0f
        ? 1.0f : std::max(frame_response, 1.0f - smoothing);
    float next_x = static_cast<float>(cursor_x);
    float next_y = static_cast<float>(cursor_y);
    if (cursor_error > allowed_error_pixels) {
        float move_x = delta_x * response;
        float move_y = delta_y * response;
        if (std::abs(move_x) < 1.0f && std::abs(delta_x) > allowed_error_pixels) {
            move_x = std::copysign(1.0f, delta_x);
        }
        if (std::abs(move_y) < 1.0f && std::abs(delta_y) > allowed_error_pixels) {
            move_y = std::copysign(1.0f, delta_y);
        }
        next_x = std::clamp(next_x + move_x, 0.0f, viewport_width);
        next_y = std::clamp(next_y + move_y, 0.0f, viewport_height);
    }

    if (cursor_error <= allowed_error_pixels) {
        g_status.message = "原版准星已对准目标";
    } else if (MoveGameCursor(next_x, next_y, viewport_width, viewport_height)) {
        ++g_status.adjustment_count;
        g_status.message = "正在平滑移动原版准星到目标";
    } else {
        ClearTarget("游戏窗口未处于前台，无法移动准星");
    }
    env->DeleteLocalRef(weapon);
    env->DeleteLocalRef(player);
}

const LegitAimStatus& GetLegitAimStatus() {
    return g_status;
}

bool LegitAimNeedsZombieData() {
    const AimSettings& settings = GetAimSettings();
    if (!settings.legit_enabled) return false;
    for (const LegitWeaponSettings& preset : settings.legit_presets) {
        if (preset.enabled && preset.automatic_aim && preset.target_zombies &&
            preset.target_points != 0) {
            return true;
        }
    }
    return false;
}

bool LegitAimNeedsPlayerData() {
    const AimSettings& settings = GetAimSettings();
    if (!settings.legit_enabled) return false;
    for (const LegitWeaponSettings& preset : settings.legit_presets) {
        if (preset.enabled && preset.automatic_aim && preset.target_players &&
            preset.target_points != 0) {
            return true;
        }
    }
    return false;
}

float LegitAimCollectionRange() {
    const AimSettings& settings = GetAimSettings();
    float maximum = 0.0f;
    for (const LegitWeaponSettings& preset : settings.legit_presets) {
        if (preset.enabled && preset.automatic_aim && preset.target_points != 0) {
            maximum = std::max(maximum, preset.range);
        }
    }
    return maximum;
}

}  // namespace pztrainer::features::aim
