#include "features/aim/rage_aim.hpp"

#include <jni.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <unordered_map>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "features/aim/rage_ballistics_hook.hpp"

namespace pztrainer::features::aim {
namespace {

using Clock = std::chrono::steady_clock;

struct Bindings {
    bool ready = false;
    jclass iso_player = nullptr;
    jclass inventory_item = nullptr;
    jclass hand_weapon = nullptr;
    jclass ballistics_controller = nullptr;
    jclass vector3 = nullptr;
    jclass vector2 = nullptr;
    jclass mouse = nullptr;
    jclass iso_utils = nullptr;
    jclass game_client = nullptr;
    jclass udp_connection = nullptr;
    jclass combat_manager = nullptr;
    jclass swipe_state_player = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_primary_item = nullptr;
    jmethodID is_aiming = nullptr;
    jmethodID set_is_aiming = nullptr;
    jmethodID is_force_aim = nullptr;
    jmethodID set_force_aim = nullptr;
    jfieldID is_charging = nullptr;
    jmethodID update_ballistics = nullptr;
    jmethodID set_angle_from_aim = nullptr;
    jmethodID is_attack_started = nullptr;
    jmethodID get_ballistics_controller = nullptr;
    jmethodID set_recoil_x = nullptr;
    jmethodID set_recoil_y = nullptr;
    jmethodID get_aim_origin_x = nullptr;
    jmethodID get_aim_origin_y = nullptr;
    jmethodID get_forward_x = nullptr;
    jmethodID get_forward_y = nullptr;
    jmethodID set_move_delta = nullptr;
    jmethodID set_moving = nullptr;
    jmethodID set_running = nullptr;
    jmethodID set_sprinting = nullptr;
    jmethodID is_allow_run = nullptr;
    jmethodID set_allow_run = nullptr;
    jmethodID is_allow_sprint = nullptr;
    jmethodID set_allow_sprint = nullptr;
    jmethodID is_ignore_inputs_for_direction = nullptr;
    jmethodID set_ignore_inputs_for_direction = nullptr;
    jmethodID is_block_movement = nullptr;
    jmethodID set_block_movement = nullptr;
    jmethodID set_force_run = nullptr;
    jmethodID set_force_sprint = nullptr;
    jfieldID player_move_direction = nullptr;
    jmethodID get_move_forward_vector = nullptr;
    jmethodID get_full_type = nullptr;
    jmethodID get_current_ammo = nullptr;
    jmethodID is_ranged = nullptr;
    jmethodID get_max_range = nullptr;
    jmethodID get_projectile_spread = nullptr;
    jmethodID set_projectile_spread = nullptr;
    jmethodID get_hit_chance = nullptr;
    jmethodID set_hit_chance = nullptr;
    jmethodID get_min_damage = nullptr;
    jmethodID set_min_damage = nullptr;
    jmethodID get_max_damage = nullptr;
    jmethodID set_max_damage = nullptr;
    jmethodID get_aiming_time = nullptr;
    jmethodID set_aiming_time = nullptr;
    jmethodID set_aiming_delay = nullptr;
    jmethodID set_been_moving_for = nullptr;
    jmethodID get_iso_aiming_position = nullptr;
    jfieldID target_position = nullptr;
    jmethodID vector_set = nullptr;
    jmethodID vector2_set = nullptr;
    jmethodID mouse_get_x = nullptr;
    jmethodID mouse_get_y = nullptr;
    jmethodID x_to_screen = nullptr;
    jmethodID y_to_screen = nullptr;
    jfieldID game_client_active = nullptr;
    jfieldID game_client_connection = nullptr;
    jmethodID get_average_ping = nullptr;
    jmethodID get_id = nullptr;
    jmethodID combat_manager_get_instance = nullptr;
    jmethodID swipe_state_player_instance = nullptr;
    jmethodID get_attack_type = nullptr;
};

struct TargetBinding {
    TargetPoint point;
    std::size_t bone_index;
};

struct Candidate {
    bool valid = false;
    bool is_player = false;
    std::int32_t game_id = -1;
    std::int32_t identity = 0;
    bridge::WorldPoint point{};
    bridge::ScreenPoint screen_point{};
    int body_part = 2;
    float distance = 0.0f;
    float current_health = 0.0f;
    float score = -std::numeric_limits<float>::max();
};

struct WeaponOverride {
    jobject weapon = nullptr;
    float projectile_spread = 0.0f;
    int hit_chance = 0;
    float minimum_damage = 0.0f;
    float maximum_damage = 0.0f;
    int aiming_time = 0;
};

struct MotionSample {
    float x = 0.0f;
    float y = 0.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    Clock::time_point updated_at{};
};

struct MovementOverride {
    jobject player = nullptr;
    bool allow_run = true;
    bool allow_sprint = true;
    bool ignore_inputs_for_direction = false;
    bool block_movement = false;
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

int RagdollBodyPartFor(TargetPoint point) {
    switch (point) {
        case TargetPoint::Head:
        case TargetPoint::Neck:
            return 2;
        case TargetPoint::Chest:
        case TargetPoint::Abdomen:
            return 1;
        case TargetPoint::Pelvis:
            return 0;
        case TargetPoint::LeftThigh:
            return 3;
        case TargetPoint::LeftCalf:
        case TargetPoint::LeftFoot:
            return 4;
        case TargetPoint::RightThigh:
            return 5;
        case TargetPoint::RightCalf:
        case TargetPoint::RightFoot:
            return 6;
        case TargetPoint::LeftArm:
            return 7;
        case TargetPoint::LeftHand:
            return 8;
        case TargetPoint::RightArm:
            return 9;
        case TargetPoint::RightHand:
            return 10;
        default:
            return 2;
    }
}

constexpr auto kShotInterval = std::chrono::milliseconds(120);
constexpr auto kDoubleTapDelay = std::chrono::milliseconds(100);
constexpr auto kDoubleTapWindow = std::chrono::milliseconds(420);
constexpr auto kDoubleTapRecharge = std::chrono::milliseconds(1200);

Bindings g_bindings;
WeaponOverride g_override;
MovementOverride g_movement_override;
bridge::AsyncObjectMethodCall g_attack_call;
RageAimStatus g_status;
Clock::time_point g_next_shot{};
Clock::time_point g_double_tap_due{};
Clock::time_point g_double_tap_expires{};
Clock::time_point g_double_tap_recharged{};
Clock::time_point g_force_aim_hold_until{};
bool g_double_tap_pending = false;
bool g_forced_aiming = false;
Candidate g_double_tap_target{};
std::unordered_map<std::int32_t, MotionSample> g_motion_samples;

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
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.ballistics_controller = LoadGlobalClass(
        env, "zombie/core/physics/BallisticsController");
    g_bindings.vector3 = LoadGlobalClass(env, "zombie/iso/Vector3");
    g_bindings.vector2 = LoadGlobalClass(env, "zombie/iso/Vector2");
    g_bindings.mouse = LoadGlobalClass(env, "zombie/input/Mouse");
    g_bindings.iso_utils = LoadGlobalClass(env, "zombie/iso/IsoUtils");
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.udp_connection = LoadGlobalClass(
        env, "zombie/core/raknet/UdpConnection");
    g_bindings.combat_manager = LoadGlobalClass(env, "zombie/CombatManager");
    g_bindings.swipe_state_player = LoadGlobalClass(
        env, "zombie/ai/states/SwipeStatePlayer");
    if (g_bindings.iso_player == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.hand_weapon == nullptr ||
        g_bindings.ballistics_controller == nullptr ||
        g_bindings.vector3 == nullptr || g_bindings.vector2 == nullptr ||
        g_bindings.mouse == nullptr || g_bindings.iso_utils == nullptr ||
        g_bindings.game_client == nullptr || g_bindings.udp_connection == nullptr ||
        g_bindings.combat_manager == nullptr ||
        g_bindings.swipe_state_player == nullptr) {
        return false;
    }

    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_id = env->GetMethodID(g_bindings.iso_player, "getID", "()I");
    g_bindings.get_primary_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem",
        "()Lzombie/inventory/InventoryItem;");
    g_bindings.is_aiming = env->GetMethodID(g_bindings.iso_player, "isAiming", "()Z");
    g_bindings.set_is_aiming = env->GetMethodID(
        g_bindings.iso_player, "setIsAiming", "(Z)V");
    g_bindings.is_force_aim = env->GetMethodID(
        g_bindings.iso_player, "isForceAim", "()Z");
    g_bindings.set_force_aim = env->GetMethodID(
        g_bindings.iso_player, "setForceAim", "(Z)V");
    g_bindings.is_charging = env->GetFieldID(
        g_bindings.iso_player, "isCharging", "Z");
    g_bindings.update_ballistics = env->GetMethodID(
        g_bindings.iso_player, "updateBallistics", "()V");
    g_bindings.set_angle_from_aim = env->GetMethodID(
        g_bindings.iso_player, "setAngleFromAim", "()V");
    g_bindings.is_attack_started = env->GetMethodID(
        g_bindings.iso_player, "isAttackStarted", "()Z");
    g_bindings.get_ballistics_controller = env->GetMethodID(
        g_bindings.iso_player, "getBallisticsController",
        "()Lzombie/core/physics/BallisticsController;");
    g_bindings.set_recoil_x = env->GetMethodID(
        g_bindings.iso_player, "setRecoilVarX", "(F)V");
    g_bindings.set_recoil_y = env->GetMethodID(
        g_bindings.iso_player, "setRecoilVarY", "(F)V");
    g_bindings.get_aim_origin_x = env->GetMethodID(
        g_bindings.iso_player, "getAimOriginPosX", "()F");
    g_bindings.get_aim_origin_y = env->GetMethodID(
        g_bindings.iso_player, "getAimOriginPosY", "()F");
    g_bindings.get_forward_x = env->GetMethodID(
        g_bindings.iso_player, "getForwardDirectionX", "()F");
    g_bindings.get_forward_y = env->GetMethodID(
        g_bindings.iso_player, "getForwardDirectionY", "()F");
    g_bindings.set_move_delta = env->GetMethodID(
        g_bindings.iso_player, "setMoveDelta", "(F)V");
    g_bindings.set_moving = env->GetMethodID(
        g_bindings.iso_player, "setMoving", "(Z)V");
    g_bindings.set_running = env->GetMethodID(
        g_bindings.iso_player, "setRunning", "(Z)V");
    g_bindings.set_sprinting = env->GetMethodID(
        g_bindings.iso_player, "setSprinting", "(Z)V");
    g_bindings.is_allow_run = env->GetMethodID(
        g_bindings.iso_player, "isAllowRun", "()Z");
    g_bindings.set_allow_run = env->GetMethodID(
        g_bindings.iso_player, "setAllowRun", "(Z)V");
    g_bindings.is_allow_sprint = env->GetMethodID(
        g_bindings.iso_player, "isAllowSprint", "()Z");
    g_bindings.set_allow_sprint = env->GetMethodID(
        g_bindings.iso_player, "setAllowSprint", "(Z)V");
    g_bindings.is_ignore_inputs_for_direction = env->GetMethodID(
        g_bindings.iso_player, "isIgnoreInputsForDirection", "()Z");
    g_bindings.set_ignore_inputs_for_direction = env->GetMethodID(
        g_bindings.iso_player, "setIgnoreInputsForDirection", "(Z)V");
    g_bindings.is_block_movement = env->GetMethodID(
        g_bindings.iso_player, "isBlockMovement", "()Z");
    g_bindings.set_block_movement = env->GetMethodID(
        g_bindings.iso_player, "setBlockMovement", "(Z)V");
    g_bindings.set_force_run = env->GetMethodID(
        g_bindings.iso_player, "setForceRun", "(Z)V");
    g_bindings.set_force_sprint = env->GetMethodID(
        g_bindings.iso_player, "setForceSprint", "(Z)V");
    g_bindings.player_move_direction = env->GetFieldID(
        g_bindings.iso_player, "playerMoveDir", "Lzombie/iso/Vector2;");
    g_bindings.get_move_forward_vector = env->GetMethodID(
        g_bindings.iso_player, "getMoveForwardVec", "()Lzombie/iso/Vector2;");
    g_bindings.get_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.get_current_ammo = env->GetMethodID(
        g_bindings.inventory_item, "getCurrentAmmoCount", "()I");
    g_bindings.is_ranged = env->GetMethodID(g_bindings.hand_weapon, "isRanged", "()Z");
    g_bindings.get_max_range = env->GetMethodID(
        g_bindings.hand_weapon, "getMaxRange", "()F");
    g_bindings.get_projectile_spread = env->GetMethodID(
        g_bindings.hand_weapon, "getProjectileSpread", "()F");
    g_bindings.set_projectile_spread = env->GetMethodID(
        g_bindings.hand_weapon, "setProjectileSpread", "(F)V");
    g_bindings.get_hit_chance = env->GetMethodID(
        g_bindings.hand_weapon, "getHitChance", "()I");
    g_bindings.set_hit_chance = env->GetMethodID(
        g_bindings.hand_weapon, "setHitChance", "(I)V");
    g_bindings.get_min_damage = env->GetMethodID(
        g_bindings.hand_weapon, "getMinDamage", "()F");
    g_bindings.set_min_damage = env->GetMethodID(
        g_bindings.hand_weapon, "setMinDamage", "(F)V");
    g_bindings.get_max_damage = env->GetMethodID(
        g_bindings.hand_weapon, "getMaxDamage", "()F");
    g_bindings.set_max_damage = env->GetMethodID(
        g_bindings.hand_weapon, "setMaxDamage", "(F)V");
    g_bindings.get_aiming_time = env->GetMethodID(
        g_bindings.hand_weapon, "getAimingTime", "()I");
    g_bindings.set_aiming_time = env->GetMethodID(
        g_bindings.hand_weapon, "setAimingTime", "(I)V");
    g_bindings.set_aiming_delay = env->GetMethodID(
        g_bindings.iso_player, "setAimingDelay", "(F)V");
    g_bindings.set_been_moving_for = env->GetMethodID(
        g_bindings.iso_player, "setBeenMovingFor", "(F)V");
    g_bindings.get_iso_aiming_position = env->GetMethodID(
        g_bindings.ballistics_controller, "getIsoAimingPosition",
        "()Lzombie/iso/Vector3;");
    g_bindings.target_position = env->GetFieldID(
        g_bindings.ballistics_controller, "targetPosition", "Lzombie/iso/Vector3;");
    g_bindings.vector_set = env->GetMethodID(
        g_bindings.vector3, "set", "(FFF)Lzombie/iso/Vector3;");
    g_bindings.vector2_set = env->GetMethodID(
        g_bindings.vector2, "set", "(FF)Lzombie/iso/Vector2;");
    g_bindings.mouse_get_x = env->GetStaticMethodID(
        g_bindings.mouse, "getXA", "()I");
    g_bindings.mouse_get_y = env->GetStaticMethodID(
        g_bindings.mouse, "getYA", "()I");
    g_bindings.x_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "XToScreenExact", "(FFFI)F");
    g_bindings.y_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "YToScreenExact", "(FFFI)F");
    g_bindings.game_client_active = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.game_client_connection = env->GetStaticFieldID(
        g_bindings.game_client, "connection",
        "Lzombie/core/raknet/UdpConnection;");
    g_bindings.get_average_ping = env->GetMethodID(
        g_bindings.udp_connection, "getAveragePing", "()I");
    g_bindings.combat_manager_get_instance = env->GetStaticMethodID(
        g_bindings.combat_manager, "getInstance", "()Lzombie/CombatManager;");
    g_bindings.swipe_state_player_instance = env->GetStaticMethodID(
        g_bindings.swipe_state_player, "instance",
        "()Lzombie/ai/states/SwipeStatePlayer;");
    g_bindings.get_attack_type = env->GetMethodID(
        g_bindings.iso_player, "getAttackType", "()Lzombie/AttackType;");
    g_bindings.ready = !ClearException(env) && g_bindings.get_player != nullptr &&
        g_bindings.get_id != nullptr &&
        g_bindings.get_primary_item != nullptr && g_bindings.is_aiming != nullptr &&
        g_bindings.set_is_aiming != nullptr &&
        g_bindings.is_force_aim != nullptr && g_bindings.set_force_aim != nullptr &&
        g_bindings.is_charging != nullptr &&
        g_bindings.update_ballistics != nullptr &&
        g_bindings.set_angle_from_aim != nullptr &&
        g_bindings.is_attack_started != nullptr &&
        g_bindings.get_ballistics_controller != nullptr &&
        g_bindings.set_recoil_x != nullptr && g_bindings.set_recoil_y != nullptr &&
        g_bindings.get_aim_origin_x != nullptr &&
        g_bindings.get_aim_origin_y != nullptr &&
        g_bindings.get_forward_x != nullptr && g_bindings.get_forward_y != nullptr &&
        g_bindings.set_move_delta != nullptr &&
        g_bindings.set_moving != nullptr && g_bindings.set_running != nullptr &&
        g_bindings.set_sprinting != nullptr &&
        g_bindings.is_allow_run != nullptr && g_bindings.set_allow_run != nullptr &&
        g_bindings.is_allow_sprint != nullptr &&
        g_bindings.set_allow_sprint != nullptr &&
        g_bindings.is_ignore_inputs_for_direction != nullptr &&
        g_bindings.set_ignore_inputs_for_direction != nullptr &&
        g_bindings.is_block_movement != nullptr &&
        g_bindings.set_block_movement != nullptr &&
        g_bindings.set_force_run != nullptr &&
        g_bindings.set_force_sprint != nullptr &&
        g_bindings.player_move_direction != nullptr &&
        g_bindings.get_move_forward_vector != nullptr &&
        g_bindings.get_full_type != nullptr && g_bindings.get_current_ammo != nullptr &&
        g_bindings.is_ranged != nullptr && g_bindings.get_max_range != nullptr &&
        g_bindings.get_projectile_spread != nullptr &&
        g_bindings.set_projectile_spread != nullptr &&
        g_bindings.get_hit_chance != nullptr && g_bindings.set_hit_chance != nullptr &&
        g_bindings.get_min_damage != nullptr && g_bindings.set_min_damage != nullptr &&
        g_bindings.get_max_damage != nullptr && g_bindings.set_max_damage != nullptr &&
        g_bindings.get_aiming_time != nullptr &&
        g_bindings.set_aiming_time != nullptr &&
        g_bindings.set_aiming_delay != nullptr &&
        g_bindings.set_been_moving_for != nullptr &&
        g_bindings.get_iso_aiming_position != nullptr &&
        g_bindings.target_position != nullptr && g_bindings.vector_set != nullptr &&
        g_bindings.vector2_set != nullptr && g_bindings.mouse_get_x != nullptr &&
        g_bindings.mouse_get_y != nullptr &&
        g_bindings.x_to_screen != nullptr && g_bindings.y_to_screen != nullptr &&
        g_bindings.game_client_active != nullptr &&
        g_bindings.game_client_connection != nullptr &&
        g_bindings.get_average_ping != nullptr &&
        g_bindings.combat_manager_get_instance != nullptr &&
        g_bindings.swipe_state_player_instance != nullptr &&
        g_bindings.get_attack_type != nullptr;
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
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
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
        Contains(type, "uzi") || Contains(type, "vector")) return WeaponGroup::Smg;
    if (Contains(type, "pistol") || Contains(type, "revolver") ||
        Contains(type, "handgun")) return WeaponGroup::Pistol;
    if (Contains(type, "huntingrifle") || Contains(type, "varmintrifle") ||
        Contains(type, "sniper") || Contains(type, "msr7")) return WeaponGroup::Sniper;
    if (Contains(type, "m14")) return WeaponGroup::Rifle;
    if (maximum_range >= 35.0f) return WeaponGroup::Sniper;
    if (maximum_range <= 18.0f && !Contains(type, "rifle") &&
        !Contains(type, "carbine")) return WeaponGroup::Pistol;
    return WeaponGroup::Rifle;
}

const RageWeaponSettings* ActivePreset(const AimSettings& settings,
                                       WeaponGroup group) {
    const RageWeaponSettings& specific = settings.rage_presets[
        static_cast<std::size_t>(group)];
    if (specific.enabled) return &specific;
    const RageWeaponSettings& global = settings.rage_presets[
        static_cast<std::size_t>(WeaponGroup::Global)];
    return global.enabled ? &global : nullptr;
}

bool ValidWorldPoint(const bridge::WorldPoint& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::isfinite(point.z) &&
        (std::abs(point.x) > 0.001f || std::abs(point.y) > 0.001f);
}

bool PointVisible(const bridge::ScreenPoint& point, float width, float height) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        point.x >= 0.0f && point.y >= 0.0f &&
        point.x <= width && point.y <= height;
}

void UpdateMotionSamples(const bridge::FrameSnapshot& frame, Clock::time_point now) {
    const auto update_sample = [now](std::int32_t identity, float x, float y) {
        MotionSample& sample = g_motion_samples[identity];
        if (sample.updated_at.time_since_epoch().count() != 0) {
            const float elapsed = std::chrono::duration<float>(
                now - sample.updated_at).count();
            const float delta_x = x - sample.x;
            const float delta_y = y - sample.y;
            const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (elapsed >= 0.005f && elapsed <= 0.5f && distance <= 2.5f) {
                float velocity_x = delta_x / elapsed;
                float velocity_y = delta_y / elapsed;
                const float speed = std::sqrt(
                    velocity_x * velocity_x + velocity_y * velocity_y);
                if (speed > 6.0f) {
                    const float scale = 6.0f / speed;
                    velocity_x *= scale;
                    velocity_y *= scale;
                }
                const float blend = 1.0f - std::exp(-10.0f * elapsed);
                sample.velocity_x += (velocity_x - sample.velocity_x) * blend;
                sample.velocity_y += (velocity_y - sample.velocity_y) * blend;
            } else {
                sample.velocity_x = 0.0f;
                sample.velocity_y = 0.0f;
            }
        }
        sample.x = x;
        sample.y = y;
        sample.updated_at = now;
    };
    for (const bridge::ZombieSnapshot& zombie : frame.zombies) {
        update_sample(zombie.identity, zombie.world_x, zombie.world_y);
    }
    for (const bridge::PlayerSnapshot& player : frame.players) {
        update_sample(player.identity, player.world_x, player.world_y);
    }
    for (auto entry = g_motion_samples.begin(); entry != g_motion_samples.end();) {
        if (now - entry->second.updated_at > std::chrono::seconds(2)) {
            entry = g_motion_samples.erase(entry);
        } else {
            ++entry;
        }
    }
}

int ReadNetworkLatencyMs(JNIEnv* env) {
    if (env->GetStaticBooleanField(
            g_bindings.game_client, g_bindings.game_client_active) != JNI_TRUE ||
        ClearException(env)) {
        return 0;
    }
    jobject connection = env->GetStaticObjectField(
        g_bindings.game_client, g_bindings.game_client_connection);
    if (connection == nullptr || ClearException(env)) {
        if (connection != nullptr) env->DeleteLocalRef(connection);
        return 0;
    }
    const int latency = env->CallIntMethod(connection, g_bindings.get_average_ping);
    const bool failed = ClearException(env);
    env->DeleteLocalRef(connection);
    return failed ? 0 : std::clamp(latency, 0, 1000);
}

bool ProjectWorldPoint(JNIEnv* env, const bridge::WorldPoint& point,
                       float camera_zoom, bridge::ScreenPoint& projected) {
    if (camera_zoom <= 0.01f) return false;
    jvalue arguments[4]{};
    arguments[0].f = point.x;
    arguments[1].f = point.y;
    arguments[2].f = point.z;
    arguments[3].i = 0;
    projected.x = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.x_to_screen, arguments) / camera_zoom;
    projected.y = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.y_to_screen, arguments) / camera_zoom;
    return !ClearException(env) && std::isfinite(projected.x) &&
        std::isfinite(projected.y);
}

Candidate SelectTarget(JNIEnv* env, const bridge::FrameSnapshot& frame,
                       const RageWeaponSettings& preset,
                       float viewport_width, float viewport_height,
                       float prediction_seconds) {
    Candidate best{};
    const auto consider_target = [&](std::int32_t game_id, std::int32_t identity,
                                     float distance, float current_health,
                                     float health_fraction,
                                     bool unstable_pose, bool crawling,
                                     bool is_player,
                                     const auto& bone_world_positions) {
        bridge::WorldPoint target_point{};
        bridge::ScreenPoint screen_point{};
        int body_part = 2;
        bool found_point = false;
        for (const TargetBinding& binding : kTargetBindings) {
            if ((preset.target_points & static_cast<std::uint32_t>(binding.point)) == 0) {
                continue;
            }
            bridge::WorldPoint predicted = bone_world_positions[binding.bone_index];
            const auto motion = g_motion_samples.find(identity);
            if (motion != g_motion_samples.end()) {
                predicted.x += motion->second.velocity_x * prediction_seconds;
                predicted.y += motion->second.velocity_y * prediction_seconds;
            }
            bridge::ScreenPoint projected{};
            if (ValidWorldPoint(predicted) &&
                ProjectWorldPoint(env, predicted, frame.camera_zoom, projected) &&
                PointVisible(projected, viewport_width, viewport_height)) {
                target_point = predicted;
                screen_point = projected;
                body_part = RagdollBodyPartFor(binding.point);
                found_point = true;
                break;
            }
        }
        if (!found_point) return;
        const float distance_score = 1.0f - std::clamp(
            distance / std::max(preset.range, 1.0f), 0.0f, 1.0f);
        const float health_score = 1.0f - std::clamp(health_fraction, 0.0f, 1.0f);
        float score = distance_score * 0.78f + health_score * 0.18f;
        if (!unstable_pose) score += 0.04f;
        if (crawling && distance <= 2.0f) score += 0.30f;
        if (!best.valid || (is_player && !best.is_player) ||
            (is_player == best.is_player && score > best.score)) {
            best.valid = true;
            best.is_player = is_player;
            best.game_id = game_id;
            best.identity = identity;
            best.point = target_point;
            best.screen_point = screen_point;
            best.body_part = body_part;
            best.distance = distance;
            best.current_health = current_health;
            best.score = score;
        }
    };
    if (preset.target_zombies) {
        for (const bridge::ZombieSnapshot& zombie : frame.zombies) {
            if (zombie.game_id < 0 || zombie.dead || zombie.current_health <= 0.0f ||
                zombie.health_fraction <= 0.0f ||
                zombie.distance > preset.range ||
                (preset.wall_check && !preset.magic_bullet &&
                 zombie.behind_wall) || !zombie.has_bones) {
                continue;
            }
            consider_target(
                zombie.game_id, zombie.identity, zombie.distance,
                zombie.current_health, zombie.health_fraction,
                zombie.unstable_pose, zombie.crawling,
                false,
                zombie.bone_world_positions);
        }
    }
    if (preset.target_players && frame.local_pvp_enabled) {
        for (const bridge::PlayerSnapshot& player : frame.players) {
            if (player.game_id < 0 || player.dead || !player.pvp_enabled ||
                player.current_health <= 0.0f || player.health_fraction <= 0.0f ||
                player.distance > preset.range ||
                (preset.wall_check && !preset.magic_bullet &&
                 player.behind_wall) || !player.has_bones) {
                continue;
            }
            consider_target(
                player.game_id, player.identity, player.distance,
                player.current_health, player.health_fraction,
                player.prone, false,
                true,
                player.bone_world_positions);
        }
    }
    return best;
}

bool MoveGameCursor(float viewport_x, float viewport_y,
                    float viewport_width, float viewport_height) {
    HWND window = GetForegroundWindow();
    if (window == nullptr || viewport_width <= 0.0f || viewport_height <= 0.0f ||
        !std::isfinite(viewport_x) || !std::isfinite(viewport_y)) {
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
        static_cast<LONG>(std::lround(
            std::clamp(viewport_x, 0.0f, viewport_width) * width / viewport_width)),
        static_cast<LONG>(std::lround(
            std::clamp(viewport_y, 0.0f, viewport_height) * height / viewport_height)),
    };
    if (ClientToScreen(window, &point) == FALSE) return false;
    return SetCursorPos(point.x, point.y) != FALSE;
}

void RestoreWeapon(JNIEnv* env) {
    if (g_override.weapon == nullptr) return;
    env->CallVoidMethod(g_override.weapon, g_bindings.set_projectile_spread,
                        g_override.projectile_spread);
    env->CallVoidMethod(g_override.weapon, g_bindings.set_hit_chance,
                        g_override.hit_chance);
    env->CallVoidMethod(g_override.weapon, g_bindings.set_min_damage,
                        g_override.minimum_damage);
    env->CallVoidMethod(g_override.weapon, g_bindings.set_max_damage,
                        g_override.maximum_damage);
    env->CallVoidMethod(g_override.weapon, g_bindings.set_aiming_time,
                        g_override.aiming_time);
    ClearException(env);
    env->DeleteGlobalRef(g_override.weapon);
    g_override = WeaponOverride{};
}

bool ApplyWeaponSettings(JNIEnv* env, jobject weapon,
                         const RageWeaponSettings& preset,
                         float target_current_health) {
    if (g_override.weapon == nullptr ||
        env->IsSameObject(g_override.weapon, weapon) != JNI_TRUE) {
        RestoreWeapon(env);
        g_override.weapon = env->NewGlobalRef(weapon);
        if (g_override.weapon == nullptr || ClearException(env)) {
            g_override = WeaponOverride{};
            return false;
        }
        g_override.projectile_spread = env->CallFloatMethod(
            weapon, g_bindings.get_projectile_spread);
        g_override.hit_chance = env->CallIntMethod(weapon, g_bindings.get_hit_chance);
        g_override.minimum_damage = env->CallFloatMethod(
            weapon, g_bindings.get_min_damage);
        g_override.maximum_damage = env->CallFloatMethod(
            weapon, g_bindings.get_max_damage);
        g_override.aiming_time = env->CallIntMethod(
            weapon, g_bindings.get_aiming_time);
        if (ClearException(env)) {
            RestoreWeapon(env);
            return false;
        }
    }
    const float minimum_damage_percent = std::clamp(
        preset.minimum_damage, 0.0f, 100.0f);
    const float maximum_damage_percent = std::clamp(
        std::max(preset.maximum_damage, minimum_damage_percent),
        0.0f, 100.0f);
    const bool has_target_health = std::isfinite(target_current_health) &&
        target_current_health > 0.0f;
    const float minimum_damage = has_target_health
        ? target_current_health * minimum_damage_percent / 100.0f
        : g_override.minimum_damage;
    const float maximum_damage = has_target_health
        ? target_current_health * maximum_damage_percent / 100.0f
        : g_override.maximum_damage;
    env->CallVoidMethod(weapon, g_bindings.set_projectile_spread,
                        preset.no_spread ? 0.0f : g_override.projectile_spread);
    env->CallVoidMethod(weapon, g_bindings.set_hit_chance,
                        static_cast<jint>(std::lround(
                            std::clamp(preset.accuracy, 0.0f, 100.0f))));
    env->CallVoidMethod(weapon, g_bindings.set_min_damage, minimum_damage);
    env->CallVoidMethod(weapon, g_bindings.set_max_damage, maximum_damage);
    const float accuracy = std::clamp(preset.accuracy, 0.0f, 100.0f) / 100.0f;
    env->CallVoidMethod(
        weapon, g_bindings.set_aiming_time,
        static_cast<jint>(std::lround(g_override.aiming_time * (1.0f - accuracy))));
    return !ClearException(env);
}

bool SetSilentTarget(JNIEnv* env, jobject player,
                     const bridge::WorldPoint& point) {
    jobject controller = env->CallObjectMethod(
        player, g_bindings.get_ballistics_controller);
    if (controller == nullptr || ClearException(env)) {
        if (controller != nullptr) env->DeleteLocalRef(controller);
        return false;
    }
    jobject target = env->GetObjectField(controller, g_bindings.target_position);
    jobject aiming = env->CallObjectMethod(
        controller, g_bindings.get_iso_aiming_position);
    if (target == nullptr || aiming == nullptr || ClearException(env)) {
        if (target != nullptr) env->DeleteLocalRef(target);
        if (aiming != nullptr) env->DeleteLocalRef(aiming);
        env->DeleteLocalRef(controller);
        return false;
    }
    jobject target_result = env->CallObjectMethod(
        target, g_bindings.vector_set, point.x, point.y, point.z);
    if (target_result != nullptr) env->DeleteLocalRef(target_result);
    jobject aiming_result = env->CallObjectMethod(
        aiming, g_bindings.vector_set, point.x, point.y, point.z);
    if (aiming_result != nullptr) env->DeleteLocalRef(aiming_result);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(target);
    env->DeleteLocalRef(aiming);
    env->DeleteLocalRef(controller);
    return succeeded;
}

void RestoreMovement(JNIEnv* env) {
    if (g_movement_override.player == nullptr) return;
    env->CallVoidMethod(g_movement_override.player, g_bindings.set_allow_run,
                        g_movement_override.allow_run ? JNI_TRUE : JNI_FALSE);
    env->CallVoidMethod(g_movement_override.player, g_bindings.set_allow_sprint,
                        g_movement_override.allow_sprint ? JNI_TRUE : JNI_FALSE);
    env->CallVoidMethod(
        g_movement_override.player, g_bindings.set_ignore_inputs_for_direction,
        g_movement_override.ignore_inputs_for_direction ? JNI_TRUE : JNI_FALSE);
    env->CallVoidMethod(
        g_movement_override.player, g_bindings.set_block_movement,
        g_movement_override.block_movement ? JNI_TRUE : JNI_FALSE);
    ClearException(env);
    env->DeleteGlobalRef(g_movement_override.player);
    g_movement_override = MovementOverride{};
}

bool StopMovement(JNIEnv* env, jobject player) {
    if (g_movement_override.player == nullptr ||
        env->IsSameObject(g_movement_override.player, player) != JNI_TRUE) {
        RestoreMovement(env);
        g_movement_override.player = env->NewGlobalRef(player);
        if (g_movement_override.player == nullptr || ClearException(env)) {
            g_movement_override = MovementOverride{};
            return false;
        }
        g_movement_override.allow_run = env->CallBooleanMethod(
            player, g_bindings.is_allow_run) == JNI_TRUE;
        g_movement_override.allow_sprint = env->CallBooleanMethod(
            player, g_bindings.is_allow_sprint) == JNI_TRUE;
        g_movement_override.ignore_inputs_for_direction = env->CallBooleanMethod(
            player, g_bindings.is_ignore_inputs_for_direction) == JNI_TRUE;
        g_movement_override.block_movement = env->CallBooleanMethod(
            player, g_bindings.is_block_movement) == JNI_TRUE;
        if (ClearException(env)) {
            RestoreMovement(env);
            return false;
        }
    }
    env->CallVoidMethod(player, g_bindings.set_allow_run, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_allow_sprint, JNI_FALSE);
    env->CallVoidMethod(
        player, g_bindings.set_ignore_inputs_for_direction, JNI_TRUE);
    env->CallVoidMethod(player, g_bindings.set_block_movement, JNI_TRUE);
    env->CallVoidMethod(player, g_bindings.set_force_run, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_force_sprint, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_running, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_sprinting, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_moving, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_move_delta, 0.0f);
    jobject input_direction = env->GetObjectField(
        player, g_bindings.player_move_direction);
    jobject movement_direction = env->CallObjectMethod(
        player, g_bindings.get_move_forward_vector);
    if (input_direction == nullptr || movement_direction == nullptr ||
        ClearException(env)) {
        if (input_direction != nullptr) env->DeleteLocalRef(input_direction);
        if (movement_direction != nullptr) env->DeleteLocalRef(movement_direction);
        return false;
    }
    jobject input_result = env->CallObjectMethod(
        input_direction, g_bindings.vector2_set, 0.0f, 0.0f);
    jobject movement_result = env->CallObjectMethod(
        movement_direction, g_bindings.vector2_set, 0.0f, 0.0f);
    if (input_result != nullptr) env->DeleteLocalRef(input_result);
    if (movement_result != nullptr) env->DeleteLocalRef(movement_result);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(input_direction);
    env->DeleteLocalRef(movement_direction);
    return succeeded;
}

bool EnterAutomaticAim(JNIEnv* env, jobject player) {
    env->CallVoidMethod(player, g_bindings.set_force_aim, JNI_TRUE);
    env->CallVoidMethod(player, g_bindings.set_is_aiming, JNI_TRUE);
    env->SetBooleanField(player, g_bindings.is_charging, JNI_TRUE);
    if (ClearException(env)) return false;

    jobject controller = env->CallObjectMethod(
        player, g_bindings.get_ballistics_controller);
    if (controller == nullptr && !ClearException(env)) {
        env->CallVoidMethod(player, g_bindings.update_ballistics);
        if (!ClearException(env)) {
            controller = env->CallObjectMethod(
                player, g_bindings.get_ballistics_controller);
        }
    }
    const bool ready = controller != nullptr && !ClearException(env);
    if (controller != nullptr) env->DeleteLocalRef(controller);
    if (ready) g_forced_aiming = true;
    return ready;
}

bool AimAtTarget(JNIEnv* env, jobject player, const bridge::WorldPoint& point,
                 int target_id, int body_part, bool target_is_player,
                 bool silent_aim, bool magic_bullet, bool within_weapon_range,
                 float* angle_error_degrees) {
    if (angle_error_degrees != nullptr) {
        *angle_error_degrees = std::numeric_limits<float>::max();
    }
    if (silent_aim || magic_bullet) {
        if (!within_weapon_range || !InitializeRageBallisticsHook()) return false;
        const int character_id = env->CallIntMethod(player, g_bindings.get_id);
        if (ClearException(env)) return false;
        SetRageBallisticsOverride(
            character_id, target_id, body_part,
            point.x, point.y, point.z, target_is_player, true);
        if (angle_error_degrees != nullptr) *angle_error_degrees = 0.0f;
        return true;
    }

    const float origin_x = env->CallFloatMethod(player, g_bindings.get_aim_origin_x);
    const float origin_y = env->CallFloatMethod(player, g_bindings.get_aim_origin_y);
    const float forward_x = env->CallFloatMethod(player, g_bindings.get_forward_x);
    const float forward_y = env->CallFloatMethod(player, g_bindings.get_forward_y);
    if (ClearException(env)) return false;

    const float delta_x = point.x - origin_x;
    const float delta_y = point.y - origin_y;
    const float horizontal_distance = std::sqrt(
        delta_x * delta_x + delta_y * delta_y);
    if (!std::isfinite(horizontal_distance) || horizontal_distance <= 0.001f) {
        return false;
    }
    const float desired_x = delta_x / horizontal_distance;
    const float desired_y = delta_y / horizontal_distance;
    const float forward_length = std::sqrt(
        forward_x * forward_x + forward_y * forward_y);
    float angle_error = 180.0f;
    if (forward_length > 0.001f) {
        const float dot = std::clamp(
            (forward_x * desired_x + forward_y * desired_y) / forward_length,
            -1.0f, 1.0f);
        angle_error = std::acos(dot) * 57.2957795f;
    }
    if (!SetSilentTarget(env, player, point)) return false;
    ClearRageBallisticsOverride();
    env->CallVoidMethod(player, g_bindings.set_angle_from_aim);
    if (ClearException(env)) return false;
    const float applied_x = env->CallFloatMethod(player, g_bindings.get_forward_x);
    const float applied_y = env->CallFloatMethod(player, g_bindings.get_forward_y);
    if (ClearException(env)) return false;
    const float applied_length = std::sqrt(
        applied_x * applied_x + applied_y * applied_y);
    if (applied_length > 0.001f) {
        const float applied_dot = std::clamp(
            (applied_x * desired_x + applied_y * desired_y) / applied_length,
            -1.0f, 1.0f);
        angle_error = std::acos(applied_dot) * 57.2957795f;
    }
    if (angle_error_degrees != nullptr) *angle_error_degrees = angle_error;
    return true;
}

void ReleaseForcedAim(JNIEnv* env, jobject player) {
    if (!g_forced_aiming || player == nullptr) return;
    const bool attack_started = env->CallBooleanMethod(
        player, g_bindings.is_attack_started) == JNI_TRUE;
    if (ClearException(env) || attack_started ||
        g_attack_call.queue_item != nullptr || Clock::now() < g_force_aim_hold_until) {
        return;
    }
    env->CallVoidMethod(player, g_bindings.set_force_aim, JNI_FALSE);
    env->CallVoidMethod(player, g_bindings.set_is_aiming, JNI_FALSE);
    env->SetBooleanField(player, g_bindings.is_charging, JNI_FALSE);
    ClearException(env);
    g_forced_aiming = false;
}

void ReleaseForcedAim(JNIEnv* env) {
    if (!g_forced_aiming) return;
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player != nullptr && !ClearException(env)) {
        ReleaseForcedAim(env, player);
        env->DeleteLocalRef(player);
    } else {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_forced_aiming = false;
    }
}

bool QueueShot(JNIEnv* env, jobject player) {
    return bridge::QueueObjectMethodOnMainThread(
        env, player, "pressedAttack", {}, g_attack_call);
}

bool QueueDoubleTapShot(JNIEnv* env, jobject player, jobject weapon) {
    jobject combat_manager = env->CallStaticObjectMethod(
        g_bindings.combat_manager, g_bindings.combat_manager_get_instance);
    jobject swipe_state = env->CallStaticObjectMethod(
        g_bindings.swipe_state_player, g_bindings.swipe_state_player_instance);
    jobject attack_type = env->CallObjectMethod(
        player, g_bindings.get_attack_type);
    const bool queued = combat_manager != nullptr && swipe_state != nullptr &&
        attack_type != nullptr && !ClearException(env) &&
        bridge::QueueObjectMethodOnMainThread(
            env, combat_manager, "attackCollisionCheck",
            {player, weapon, swipe_state, attack_type}, g_attack_call);
    if (attack_type != nullptr) env->DeleteLocalRef(attack_type);
    if (swipe_state != nullptr) env->DeleteLocalRef(swipe_state);
    if (combat_manager != nullptr) env->DeleteLocalRef(combat_manager);
    return queued;
}

void ResetRuntime(JNIEnv* env, const char* message) {
    ClearRageBallisticsOverride();
    ReleaseForcedAim(env);
    RestoreMovement(env);
    RestoreWeapon(env);
    bridge::ResetObjectMethodCall(env, g_attack_call);
    g_double_tap_pending = false;
    g_double_tap_target = Candidate{};
    g_motion_samples.clear();
    g_status.firearm_ready = false;
    g_status.target_locked = false;
    g_status.target_identity = 0;
    g_status.double_tap_charging = false;
    g_status.message = message;
}

}  // namespace

void UpdateRageAim(const bridge::FrameSnapshot& frame, bool menu_visible,
                   float viewport_width, float viewport_height) {
    AimSettings& settings = GetAimSettings();
    JNIEnv* env = bridge::GetCurrentJniEnvironment();
    if (env == nullptr) {
        g_status.initialized = false;
        g_status.message = "Rage JNI 环境尚未就绪";
        return;
    }
    if (!Initialize(env)) {
        g_status.initialized = false;
        g_status.message = "Rage JNI 绑定尚未就绪";
        return;
    }
    g_status.initialized = true;
    const RageBallisticsDiagnostics ballistics_diagnostics =
        GetRageBallisticsDiagnostics();
    g_status.ballistics_hook_ready = ballistics_diagnostics.initialized;
    g_status.ballistics_hook_calls = ballistics_diagnostics.calls;
    g_status.ballistics_override_calls = ballistics_diagnostics.overrides;
    g_status.ballistics_reticle_override_calls =
        ballistics_diagnostics.reticle_overrides;
    g_status.ballistics_target_override_calls =
        ballistics_diagnostics.target_overrides;
    g_status.ballistics_hook_character_id =
        ballistics_diagnostics.last_character_id;
    g_status.ballistics_published_character_id =
        ballistics_diagnostics.published_character_id;
    const bridge::AsyncObjectMethodState attack_state =
        bridge::PollObjectMethodOnMainThread(
            env, g_attack_call, std::chrono::milliseconds(900), nullptr);
    if (attack_state == bridge::AsyncObjectMethodState::Succeeded) {
        ++g_status.fired_count;
    }
    if (!settings.rage_enabled) {
        ResetRuntime(env, "Rage 总开关已关闭");
        return;
    }
    if (frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        ResetRuntime(env, "等待单机游戏世界");
        return;
    }

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    jobject weapon = player == nullptr
        ? nullptr : env->CallObjectMethod(player, g_bindings.get_primary_item);
    if (player == nullptr || weapon == nullptr || ClearException(env) ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE ||
        env->CallBooleanMethod(weapon, g_bindings.is_ranged) != JNI_TRUE ||
        ClearException(env)) {
        ClearRageBallisticsOverride();
        RestoreWeapon(env);
        RestoreMovement(env);
        ReleaseForcedAim(env, player);
        if (weapon != nullptr) env->DeleteLocalRef(weapon);
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.firearm_ready = false;
        g_status.target_locked = false;
        g_status.message = "当前主手不是远程枪械";
        return;
    }

    const std::string weapon_type = ReadJavaString(env, weapon, g_bindings.get_full_type);
    const float maximum_range = env->CallFloatMethod(weapon, g_bindings.get_max_range);
    if (ClearException(env)) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ResetRuntime(env, "读取 Rage 枪械参数失败");
        return;
    }
    const WeaponGroup weapon_group = ClassifyWeapon(weapon_type, maximum_range);
    const RageWeaponSettings* preset = ActivePreset(settings, weapon_group);
    g_status.firearm_ready = true;
    g_status.weapon_group = weapon_group;
    g_status.weapon_type = weapon_type;
    if (preset == nullptr) {
        ClearRageBallisticsOverride();
        RestoreWeapon(env);
        RestoreMovement(env);
        ReleaseForcedAim(env, player);
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        g_status.target_locked = false;
        g_status.message = "当前 Rage 武器预设未启用";
        return;
    }
    if (menu_visible) {
        ClearRageBallisticsOverride();
        ReleaseForcedAim(env, player);
        RestoreMovement(env);
        RestoreWeapon(env);
        g_double_tap_pending = false;
        g_double_tap_target = Candidate{};
        g_status.target_locked = false;
        g_status.target_identity = 0;
        g_status.message = "菜单打开时 Rage 完全暂停";
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }
    const auto now = Clock::now();
    if (!preset->double_tap && g_double_tap_pending) {
        g_double_tap_pending = false;
        g_double_tap_target = Candidate{};
    }
    UpdateMotionSamples(frame, now);
    const int network_latency_ms = ReadNetworkLatencyMs(env);
    const float prediction_seconds = std::clamp(
        0.055f + static_cast<float>(network_latency_ms) * 0.0005f,
        0.055f, 0.35f);
    Candidate target = SelectTarget(
        env, frame, *preset, viewport_width, viewport_height,
        prediction_seconds);
    if (g_double_tap_pending && now > g_double_tap_expires) {
        g_double_tap_pending = false;
        g_double_tap_target = Candidate{};
    }
    if (g_double_tap_pending && g_double_tap_target.valid) {
        target = g_double_tap_target;
    }
    if (!ApplyWeaponSettings(
            env, weapon, *preset,
            target.valid ? target.current_health : 0.0f)) {
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        ResetRuntime(env, "应用 Rage 枪械参数失败");
        return;
    }
    if (preset->no_recoil) {
        env->CallVoidMethod(player, g_bindings.set_recoil_x, 0.0f);
        env->CallVoidMethod(player, g_bindings.set_recoil_y, 0.0f);
        ClearException(env);
    }
    g_status.target_locked = target.valid;
    g_status.target_identity = target.valid ? target.identity : 0;
    bool aiming = env->CallBooleanMethod(player, g_bindings.is_aiming) == JNI_TRUE;
    const bool attack_started = env->CallBooleanMethod(
        player, g_bindings.is_attack_started) == JNI_TRUE;
    const int ammunition = env->CallIntMethod(weapon, g_bindings.get_current_ammo);
    if (ClearException(env)) {
        ClearRageBallisticsOverride();
        RestoreMovement(env);
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        g_status.message = "读取攻击状态失败";
        return;
    }
    g_status.double_tap_charging = preset->double_tap &&
        now < g_double_tap_recharged;
    bool aim_ready = !preset->automatic_aim;
    float angle_error = 0.0f;
    if (target.valid) {
        if (preset->automatic_stop) {
            StopMovement(env, player);
        } else {
            RestoreMovement(env);
        }
        if (preset->automatic_aim) {
            if (preset->accuracy >= 99.5f) {
                env->CallVoidMethod(player, g_bindings.set_aiming_delay, 0.0f);
                env->CallVoidMethod(player, g_bindings.set_been_moving_for, 0.0f);
                env->CallVoidMethod(player, g_bindings.set_recoil_x, 0.0f);
                env->CallVoidMethod(player, g_bindings.set_recoil_y, 0.0f);
                if (ClearException(env)) {
                    RestoreMovement(env);
                    env->DeleteLocalRef(weapon);
                    env->DeleteLocalRef(player);
                    g_status.message = "清除原版瞄准惩罚失败";
                    return;
                }
            }
            aiming = EnterAutomaticAim(env, player);
        } else if (!preset->automatic_aim) {
            ReleaseForcedAim(env, player);
        }
        if (aiming && (preset->automatic_aim || preset->magic_bullet)) {
            const bool adjusted = AimAtTarget(
                env, player, target.point, target.game_id,
                target.body_part, target.is_player, preset->silent_aim,
                preset->magic_bullet,
                target.distance <= maximum_range + 5.0f, &angle_error);
            const float tolerance = 0.35f +
                (1.0f - std::clamp(preset->accuracy, 0.0f, 100.0f) / 100.0f) *
                    7.65f;
            if (preset->magic_bullet) {
                aim_ready = adjusted;
            } else if (preset->silent_aim) {
                aim_ready = adjusted && angle_error <= tolerance;
            } else {
                const jint cursor_x = env->CallStaticIntMethod(
                    g_bindings.mouse, g_bindings.mouse_get_x);
                const jint cursor_y = env->CallStaticIntMethod(
                    g_bindings.mouse, g_bindings.mouse_get_y);
                const bool cursor_valid = !ClearException(env) &&
                    std::isfinite(target.screen_point.x) &&
                    std::isfinite(target.screen_point.y) &&
                    target.screen_point.x >= 0.0f && target.screen_point.y >= 0.0f &&
                    target.screen_point.x <= viewport_width &&
                    target.screen_point.y <= viewport_height;
                const float cursor_delta_x = target.screen_point.x - cursor_x;
                const float cursor_delta_y = target.screen_point.y - cursor_y;
                const float cursor_error = cursor_valid
                    ? std::sqrt(cursor_delta_x * cursor_delta_x +
                                cursor_delta_y * cursor_delta_y)
                    : std::numeric_limits<float>::max();
                const float allowed_cursor_error = 1.0f +
                    (1.0f - std::clamp(preset->accuracy, 0.0f, 100.0f) / 100.0f) *
                        8.0f;
                const bool cursor_locked = cursor_valid &&
                    cursor_error <= allowed_cursor_error;
                if (cursor_valid && !cursor_locked) {
                    MoveGameCursor(target.screen_point.x, target.screen_point.y,
                                   viewport_width, viewport_height);
                }
                aim_ready = adjusted && cursor_locked &&
                    angle_error <= tolerance;
            }
        }
    } else {
        RestoreMovement(env);
        ReleaseForcedAim(env, player);
    }
    if (!target.valid) {
        ClearRageBallisticsOverride();
        g_double_tap_pending = false;
        g_double_tap_target = Candidate{};
        g_status.message = "范围内没有符合条件的目标";
    } else if (!aiming) {
        g_status.message = preset->automatic_aim
            ? "正在进入原版强制瞄准状态"
            : "按住原版瞄准键以启用魔法子弹";
    } else if (!aim_ready) {
        g_status.message = "自动急停并对准所选骨骼位置";
    } else if (ammunition <= 0) {
        g_status.message = "当前枪械没有可用弹药";
    } else if (!preset->automatic_fire) {
        g_status.message = preset->magic_bullet
            ? "魔法子弹已锁定，等待手动开枪"
            : "已对准目标，自动开枪已关闭";
    } else if (g_attack_call.queue_item != nullptr) {
        g_status.message = g_double_tap_pending ? "等待原版状态允许第二枪" :
                                                  "等待原版射击状态恢复";
    } else {
        const bool second_shot = g_double_tap_pending &&
            now >= g_double_tap_due && now <= g_double_tap_expires;
        const bool normal_shot = !g_double_tap_pending && !attack_started &&
            now >= g_next_shot;
        if (second_shot) {
            if (QueueDoubleTapShot(env, player, weapon)) {
                g_next_shot = now + kShotInterval;
                g_force_aim_hold_until = now + std::chrono::milliseconds(250);
                g_double_tap_pending = false;
                g_double_tap_target = Candidate{};
                g_status.message = "DT 第二枪已提交，正在充能";
            } else {
                g_status.message = "DT 第二枪通过原版碰撞入口提交失败";
            }
        } else if (normal_shot) {
            if (QueueShot(env, player)) {
                g_next_shot = now + kShotInterval;
                g_force_aim_hold_until = now + std::chrono::milliseconds(250);
                if (preset->double_tap && ammunition >= 2 &&
                           now >= g_double_tap_recharged) {
                    g_double_tap_pending = true;
                    g_double_tap_target = target;
                    g_double_tap_due = now + kDoubleTapDelay;
                    g_double_tap_expires = now + kDoubleTapWindow;
                    g_double_tap_recharged = now + kDoubleTapRecharge;
                    g_status.double_tap_charging = true;
                    g_status.message = "首枪已提交，等待 DT 第二枪";
                } else {
                    g_double_tap_target = Candidate{};
                    g_status.message = "已通过原版攻击入口自动开枪";
                }
            } else {
                g_status.message = "原版射击任务排队失败";
            }
        } else if (g_double_tap_pending) {
            g_status.message = "等待 DT 双发间隔";
        } else if (attack_started) {
            g_status.message = "等待原版射击状态恢复";
        } else {
            g_status.message = g_status.double_tap_charging
                ? "DT 充能中" : "已锁定目标";
        }
    }
    env->DeleteLocalRef(weapon);
    env->DeleteLocalRef(player);
}

bool RageAimNeedsZombieData() {
    const AimSettings& settings = GetAimSettings();
    if (!settings.rage_enabled) return false;
    for (const RageWeaponSettings& preset : settings.rage_presets) {
        if (preset.enabled && preset.target_zombies &&
            (preset.automatic_aim || preset.automatic_fire ||
             preset.magic_bullet) &&
            preset.target_points != 0) {
            return true;
        }
    }
    return false;
}

bool RageAimNeedsPlayerData() {
    const AimSettings& settings = GetAimSettings();
    if (!settings.rage_enabled) return false;
    for (const RageWeaponSettings& preset : settings.rage_presets) {
        if (preset.enabled && preset.target_players &&
            (preset.automatic_aim || preset.automatic_fire ||
             preset.magic_bullet) &&
            preset.target_points != 0) {
            return true;
        }
    }
    return false;
}

float RageAimCollectionRange() {
    const AimSettings& settings = GetAimSettings();
    float maximum = 0.0f;
    for (const RageWeaponSettings& preset : settings.rage_presets) {
        if (preset.enabled && (preset.automatic_aim || preset.automatic_fire ||
                               preset.magic_bullet) &&
            preset.target_points != 0) {
            maximum = std::max(maximum, preset.range);
        }
    }
    return maximum;
}

const RageAimStatus& GetRageAimStatus() {
    return g_status;
}

}  // namespace pztrainer::features::aim
