#include "features/aim/rage_ballistics_hook.hpp"

#include <MinHook.h>
#include <Windows.h>
#include <jni.h>

#include <atomic>
#include <array>

#include "features/aim/magic_bullet_hit_list.hpp"

namespace pztrainer::features::aim {
namespace {

using UpdateMuzzleDirectionFn = void(JNICALL*)(
    JNIEnv*, jclass, jint, jfloat, jfloat, jfloat);
using GetSpreadTargetsFn = jint(JNICALL*)(
    JNIEnv*, jclass, jint, jfloat, jfloat, jfloat, jint, jint, jfloatArray);

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
std::atomic<int> g_last_character_id{-1};
UpdateMuzzleDirectionFn g_original_update_muzzle_direction = nullptr;
GetSpreadTargetsFn g_original_get_spread_targets = nullptr;

bool ShouldOverride(jint character_id) {
    return g_override_enabled.load(std::memory_order_acquire) &&
        character_id == g_character_id.load(std::memory_order_relaxed) &&
        g_target_id.load(std::memory_order_relaxed) >= 0 &&
        GetTickCount64() <= g_expires_at.load(std::memory_order_relaxed);
}

void WriteTarget(JNIEnv* env, jfloatArray output, bool include_body_part) {
    jfloat values[5]{
        static_cast<jfloat>(g_target_id.load(std::memory_order_relaxed)),
        g_target_x.load(std::memory_order_relaxed),
        g_target_z.load(std::memory_order_relaxed) * 2.44949f,
        g_target_y.load(std::memory_order_relaxed),
        static_cast<jfloat>(g_body_part.load(std::memory_order_relaxed)),
    };
    env->SetFloatArrayRegion(output, 0, include_body_part ? 5 : 4, values);
}

void JNICALL HookedUpdateMuzzleDirection(
    JNIEnv* env, jclass type, jint character_id, jfloat direction_x,
    jfloat direction_y, jfloat direction_z) {
    g_call_count.fetch_add(1, std::memory_order_relaxed);
    g_last_character_id.store(character_id, std::memory_order_relaxed);
    g_original_update_muzzle_direction(
        env, type, character_id, direction_x, direction_y, direction_z);
    if (ShouldOverride(character_id) &&
        g_replace_hit_list.load(std::memory_order_relaxed) &&
        TryReplaceMagicBulletHitList(
            env, character_id,
            g_target_id.load(std::memory_order_relaxed),
            g_body_part.load(std::memory_order_relaxed),
            g_target_is_player.load(std::memory_order_relaxed))) {
        g_override_count.fetch_add(1, std::memory_order_relaxed);
    }
}

jint JNICALL HookedGetSpreadTargets(
    JNIEnv* env, jclass type, jint character_id, jfloat range,
    jfloat spread, jfloat weight_center, jint projectile_count,
    jint maximum_targets, jfloatArray output) {
    const jint original_count = g_original_get_spread_targets(
        env, type, character_id, range, spread, weight_center,
        projectile_count, maximum_targets, output);
    if (!ShouldOverride(character_id) || output == nullptr || maximum_targets < 1) {
        return original_count;
    }
    WriteTarget(env, output, false);
    g_target_override_count.fetch_add(1, std::memory_order_relaxed);
    return env->ExceptionCheck() ? original_count : 1;
}

}  // namespace

bool InitializeRageBallisticsHook() {
    const int state = g_initialization_state.load(std::memory_order_acquire);
    if (state != 0) return state > 0;

    HMODULE bullet_module = GetModuleHandleW(L"PZBullet64.dll");
    if (bullet_module == nullptr) return false;
    void* update_muzzle_direction = reinterpret_cast<void*>(GetProcAddress(
        bullet_module,
        "Java_zombie_core_physics_Bullet_updateBallisticsMuzzleAimDirection"));
    void* get_spread_targets = reinterpret_cast<void*>(GetProcAddress(
        bullet_module,
        "Java_zombie_core_physics_Bullet_getBallisticsTargetsSpreadData"));
    if (update_muzzle_direction == nullptr || get_spread_targets == nullptr) {
        g_initialization_state.store(-1, std::memory_order_release);
        return false;
    }
    MH_STATUS hook_status = MH_CreateHook(
        update_muzzle_direction,
        reinterpret_cast<void*>(&HookedUpdateMuzzleDirection),
        reinterpret_cast<void**>(&g_original_update_muzzle_direction));
    if (hook_status == MH_OK) {
        hook_status = MH_CreateHook(
            get_spread_targets, reinterpret_cast<void*>(&HookedGetSpreadTargets),
            reinterpret_cast<void**>(&g_original_get_spread_targets));
    }
    if (hook_status != MH_OK) {
        g_initialization_state.store(-1, std::memory_order_release);
        return false;
    }
    const std::array<void*, 2> hooks{
        update_muzzle_direction,
        get_spread_targets,
    };
    for (void* hook : hooks) {
        hook_status = MH_QueueEnableHook(hook);
        if (hook_status != MH_OK) break;
    }
    if (hook_status == MH_OK) hook_status = MH_ApplyQueued();
    if (hook_status != MH_OK) {
        for (void* hook : hooks) MH_QueueDisableHook(hook);
        MH_ApplyQueued();
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
    ResetMagicBulletShotTracking();
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
    diagnostics.last_character_id =
        g_last_character_id.load(std::memory_order_relaxed);
    diagnostics.published_character_id =
        g_character_id.load(std::memory_order_relaxed);
    return diagnostics;
}

}  // namespace pztrainer::features::aim
