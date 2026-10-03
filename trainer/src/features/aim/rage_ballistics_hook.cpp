#include "features/aim/rage_ballistics_hook.hpp"

#include <MinHook.h>
#include <Windows.h>
#include <jni.h>

#include <atomic>
#include <array>
#include <cmath>

#include "features/aim/magic_bullet_hit_list.hpp"
#include "bridge/rage_fire_bridge.hpp"

namespace pztrainer::features::aim {
namespace {

using UpdateMuzzleDirectionFn = void(JNICALL*)(
    JNIEnv*, jclass, jint, jfloat, jfloat, jfloat);
using GetSpreadTargetsFn = jint(JNICALL*)(
    JNIEnv*, jclass, jint, jfloat, jfloat, jfloat, jint, jint, jfloatArray);
using GetTargetsFn = jint(JNICALL*)(JNIEnv*, jclass, jint, jfloat, jint, jfloatArray);
using GetCameraTargetsFn = jint(JNICALL*)(
    JNIEnv*, jclass, jint, jfloat, jint, jboolean, jfloatArray);

std::atomic<int> g_initialization_state{0};
std::atomic<bool> g_override_enabled{false};
std::atomic<int> g_character_id{-1};
std::atomic<int> g_target_id{-1};
std::atomic<int> g_body_part{2};
std::atomic<float> g_target_x{0.0f};
std::atomic<float> g_target_y{0.0f};
std::atomic<float> g_target_z{0.0f};
std::atomic<bool> g_target_is_player{false};
std::atomic<bool> g_replace_hit_list{false};
std::atomic<ULONGLONG> g_expires_at{0};
std::atomic<std::uint64_t> g_call_count{0};
std::atomic<std::uint64_t> g_override_count{0};
std::atomic<std::uint64_t> g_reticle_override_count{0};
std::atomic<std::uint64_t> g_target_override_count{0};
std::atomic<std::uint64_t> g_spread_override_count{0};
std::atomic<int> g_muzzle_character_id{-1};
std::atomic<float> g_muzzle_x{0.0f};
std::atomic<float> g_muzzle_y{0.0f};
std::atomic<float> g_muzzle_z{0.0f};
std::atomic<ULONGLONG> g_muzzle_updated_at{0};
std::atomic<bool> g_no_spread{false};
std::atomic<int> g_spread_character_id{-1};
std::atomic<ULONGLONG> g_spread_expires_at{0};
std::atomic<int> g_last_character_id{-1};
UpdateMuzzleDirectionFn g_original_update_muzzle_direction = nullptr;
GetSpreadTargetsFn g_original_get_spread_targets = nullptr;
UpdateMuzzleDirectionFn g_original_update_position = nullptr;
UpdateMuzzleDirectionFn g_original_update_reticle = nullptr;
GetTargetsFn g_original_get_targets = nullptr;
GetCameraTargetsFn g_original_get_camera_targets = nullptr;

bool ShouldOverride(jint character_id) {
    return g_override_enabled.load(std::memory_order_acquire) &&
        character_id == g_character_id.load(std::memory_order_relaxed) &&
        g_target_id.load(std::memory_order_relaxed) >= 0 &&
        GetTickCount64() <= g_expires_at.load(std::memory_order_relaxed);
}

bool WriteTarget(JNIEnv* env, jfloatArray output, bool include_body_part) {
    const int length = include_body_part ? 5 : 4;
    if (output == nullptr || env->ExceptionCheck() || env->GetArrayLength(output) < length) {
        return false;
    }
    jfloat values[5]{
        static_cast<jfloat>(g_target_id.load(std::memory_order_relaxed)),
        g_target_x.load(std::memory_order_relaxed),
        g_target_z.load(std::memory_order_relaxed) * 2.44949f,
        g_target_y.load(std::memory_order_relaxed),
        static_cast<jfloat>(g_body_part.load(std::memory_order_relaxed)),
    };
    env->SetFloatArrayRegion(output, 0, length, values);
    return !env->ExceptionCheck();
}

void JNICALL HookedUpdatePosition(JNIEnv* env, jclass type, jint character_id,
                                  jfloat x, jfloat y, jfloat z) {
    g_original_update_position(env, type, character_id, x, y, z);
    if (ShouldOverride(character_id)) {
        g_muzzle_x.store(x, std::memory_order_relaxed);
        g_muzzle_y.store(y, std::memory_order_relaxed);
        g_muzzle_z.store(z, std::memory_order_relaxed);
        g_muzzle_character_id.store(character_id, std::memory_order_relaxed);
        g_muzzle_updated_at.store(GetTickCount64(), std::memory_order_release);
    }
}

void JNICALL HookedUpdateReticle(JNIEnv* env, jclass type, jint character_id,
                                 jfloat x, jfloat y, jfloat z) {
    if (ShouldOverride(character_id)) {
        x = g_target_x.load(std::memory_order_relaxed);
        y = g_target_z.load(std::memory_order_relaxed) * 2.44949f;
        z = g_target_y.load(std::memory_order_relaxed);
        g_reticle_override_count.fetch_add(1, std::memory_order_relaxed);
    }
    g_original_update_reticle(env, type, character_id, x, y, z);
}

void JNICALL HookedUpdateMuzzleDirection(
    JNIEnv* env, jclass type, jint character_id, jfloat direction_x,
    jfloat direction_y, jfloat direction_z) {
    g_call_count.fetch_add(1, std::memory_order_relaxed);
    g_last_character_id.store(character_id, std::memory_order_relaxed);
    if (ShouldOverride(character_id) &&
        GetTickCount64() - g_muzzle_updated_at.load(std::memory_order_acquire) <= 250 &&
        g_muzzle_character_id.load(std::memory_order_relaxed) == character_id) {
        const float x = g_target_x.load(std::memory_order_relaxed) -
            g_muzzle_x.load(std::memory_order_relaxed);
        const float y = g_target_z.load(std::memory_order_relaxed) * 2.44949f -
            g_muzzle_y.load(std::memory_order_relaxed);
        const float z = g_target_y.load(std::memory_order_relaxed) -
            g_muzzle_z.load(std::memory_order_relaxed);
        const float length = std::sqrt(x * x + y * y + z * z);
        if (std::isfinite(length) && length > 0.001f) {
            direction_x = x / length;
            direction_y = y / length;
            direction_z = z / length;
            g_override_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    g_original_update_muzzle_direction(
        env, type, character_id, direction_x, direction_y, direction_z);
}

void ReplaceShotHitList(JNIEnv* env, jint character_id) {
    if (ShouldOverride(character_id) &&
        g_replace_hit_list.load(std::memory_order_relaxed) &&
        bridge::RageAttackNeedsHitList(env, character_id) &&
        TryReplaceMagicBulletHitList(
            env, character_id,
            g_target_id.load(std::memory_order_relaxed),
            g_body_part.load(std::memory_order_relaxed),
            g_target_is_player.load(std::memory_order_relaxed), false)) {
        bridge::MarkRageAttackHitListWritten(env);
        g_override_count.fetch_add(1, std::memory_order_relaxed);
    }
}

jint JNICALL HookedGetTargets(JNIEnv* env, jclass type, jint character_id,
                              jfloat range, jint maximum_targets, jfloatArray output) {
    const jint count = g_original_get_targets(env, type, character_id, range,
        maximum_targets, output);
    if (!ShouldOverride(character_id) || maximum_targets < 1 ||
        !WriteTarget(env, output, false)) return count;
    ReplaceShotHitList(env, character_id);
    g_target_override_count.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

jint JNICALL HookedGetCameraTargets(JNIEnv* env, jclass type, jint character_id,
                                    jfloat range, jint maximum_targets,
                                    jboolean center, jfloatArray output) {
    const jint count = g_original_get_camera_targets(env, type, character_id,
        range, maximum_targets, center, output);
    if (!ShouldOverride(character_id) || maximum_targets < 1 ||
        !WriteTarget(env, output, true)) return count;
    g_target_override_count.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

jint JNICALL HookedGetSpreadTargets(
    JNIEnv* env, jclass type, jint character_id, jfloat range,
    jfloat spread, jfloat weight_center, jint projectile_count,
    jint maximum_targets, jfloatArray output) {
    if (g_no_spread.load(std::memory_order_acquire) &&
        character_id == g_spread_character_id.load(std::memory_order_relaxed) &&
        GetTickCount64() <= g_spread_expires_at.load(std::memory_order_relaxed)) {
        spread = 0.0f;
        g_spread_override_count.fetch_add(1, std::memory_order_relaxed);
    }
    const jint original_count = g_original_get_spread_targets(
        env, type, character_id, range, spread, weight_center,
        projectile_count, maximum_targets, output);
    if (!ShouldOverride(character_id) || maximum_targets < 1 ||
        !WriteTarget(env, output, false)) {
        return original_count;
    }
    ReplaceShotHitList(env, character_id);
    g_target_override_count.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

}  // namespace

bool InitializeRageBallisticsHook() {
    const int state = g_initialization_state.load(std::memory_order_acquire);
    if (state != 0) return state > 0;

    HMODULE bullet_module = GetModuleHandleW(L"PZBullet64.dll");
    if (bullet_module == nullptr) return false;
    struct Hook {
        const char* name;
        void* detour;
        void** original;
        void* address = nullptr;
    };
    std::array<Hook, 6> hooks{{
        {"Java_zombie_core_physics_Bullet_updateBallistics", reinterpret_cast<void*>(&HookedUpdatePosition), reinterpret_cast<void**>(&g_original_update_position)},
        {"Java_zombie_core_physics_Bullet_updateBallisticsAimReticlePosition", reinterpret_cast<void*>(&HookedUpdateReticle), reinterpret_cast<void**>(&g_original_update_reticle)},
        {"Java_zombie_core_physics_Bullet_updateBallisticsMuzzleAimDirection", reinterpret_cast<void*>(&HookedUpdateMuzzleDirection), reinterpret_cast<void**>(&g_original_update_muzzle_direction)},
        {"Java_zombie_core_physics_Bullet_getBallisticsTargets", reinterpret_cast<void*>(&HookedGetTargets), reinterpret_cast<void**>(&g_original_get_targets)},
        {"Java_zombie_core_physics_Bullet_getBallisticsCameraTargets", reinterpret_cast<void*>(&HookedGetCameraTargets), reinterpret_cast<void**>(&g_original_get_camera_targets)},
        {"Java_zombie_core_physics_Bullet_getBallisticsTargetsSpreadData", reinterpret_cast<void*>(&HookedGetSpreadTargets), reinterpret_cast<void**>(&g_original_get_spread_targets)},
    }};
    MH_STATUS hook_status = MH_OK;
    for (Hook& hook : hooks) {
        hook.address = reinterpret_cast<void*>(GetProcAddress(bullet_module, hook.name));
        if (hook.address == nullptr) {
            hook_status = MH_ERROR_FUNCTION_NOT_FOUND;
            break;
        }
        hook_status = MH_CreateHook(hook.address, hook.detour, hook.original);
        if (hook_status != MH_OK) break;
        hook_status = MH_QueueEnableHook(hook.address);
        if (hook_status != MH_OK) break;
    }
    if (hook_status == MH_OK) hook_status = MH_ApplyQueued();
    if (hook_status != MH_OK) {
        for (const Hook& hook : hooks) {
            if (hook.address != nullptr) MH_QueueDisableHook(hook.address);
        }
        MH_ApplyQueued();
        for (const Hook& hook : hooks) {
            if (hook.address != nullptr) MH_RemoveHook(hook.address);
        }
        g_initialization_state.store(-1, std::memory_order_release);
        return false;
    }
    g_initialization_state.store(1, std::memory_order_release);
    return true;
}

void SetRageBallisticsOverride(
    int character_id, int target_id, int body_part,
    float target_x, float target_y, float target_z,
    bool target_is_player, bool replace_hit_list) {
    g_override_enabled.store(false, std::memory_order_release);
    g_character_id.store(character_id, std::memory_order_relaxed);
    g_target_id.store(target_id, std::memory_order_relaxed);
    g_body_part.store(body_part, std::memory_order_relaxed);
    g_target_x.store(target_x, std::memory_order_relaxed);
    g_target_y.store(target_y, std::memory_order_relaxed);
    g_target_z.store(target_z, std::memory_order_relaxed);
    g_target_is_player.store(target_is_player, std::memory_order_relaxed);
    g_replace_hit_list.store(replace_hit_list, std::memory_order_relaxed);
    g_expires_at.store(GetTickCount64() + 250, std::memory_order_relaxed);
    g_override_enabled.store(true, std::memory_order_release);
}

void ClearRageBallisticsOverride() {
    g_override_enabled.store(false, std::memory_order_release);
    g_no_spread.store(false, std::memory_order_release);
    ResetMagicBulletShotTracking();
}

void SetRageSpreadOverride(int character_id, bool no_spread) {
    g_no_spread.store(false, std::memory_order_release);
    g_spread_character_id.store(character_id, std::memory_order_relaxed);
    g_spread_expires_at.store(GetTickCount64() + 250, std::memory_order_relaxed);
    g_no_spread.store(no_spread, std::memory_order_release);
}

RageBallisticsDiagnostics GetRageBallisticsDiagnostics() {
    RageBallisticsDiagnostics diagnostics;
    diagnostics.initialized =
        g_initialization_state.load(std::memory_order_acquire) > 0;
    diagnostics.calls = g_call_count.load(std::memory_order_relaxed);
    diagnostics.overrides = g_override_count.load(std::memory_order_relaxed);
    diagnostics.reticle_overrides =
        g_reticle_override_count.load(std::memory_order_relaxed);
    diagnostics.target_overrides =
        g_target_override_count.load(std::memory_order_relaxed);
    diagnostics.spread_overrides =
        g_spread_override_count.load(std::memory_order_relaxed);
    diagnostics.last_character_id =
        g_last_character_id.load(std::memory_order_relaxed);
    diagnostics.published_character_id =
        g_character_id.load(std::memory_order_relaxed);
    return diagnostics;
}

}  // namespace pztrainer::features::aim
