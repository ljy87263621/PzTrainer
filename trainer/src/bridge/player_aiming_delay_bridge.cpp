#include "bridge/player_aiming_delay_bridge.hpp"

#include <jni.h>

#include <cmath>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

PlayerAimingDelayStatus g_status;
AsyncObjectMethodCall g_pending;
jclass g_player_type = nullptr;
jmethodID g_get_player = nullptr;
jmethodID g_is_aiming = nullptr;
jmethodID g_get_delay = nullptr;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

bool Initialize(JNIEnv* env) {
    if (g_player_type != nullptr) return true;
    jclass type = env->FindClass("zombie/characters/IsoPlayer");
    if (ClearException(env) || type == nullptr) return false;
    g_get_player = env->GetStaticMethodID(type, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_is_aiming = env->GetMethodID(type, "isAiming", "()Z");
    g_get_delay = env->GetMethodID(type, "getAimingDelay", "()F");
    const bool ready = !ClearException(env) && g_get_player != nullptr &&
        g_is_aiming != nullptr && g_get_delay != nullptr;
    if (ready) g_player_type = static_cast<jclass>(env->NewGlobalRef(type));
    env->DeleteLocalRef(type);
    return ready && g_player_type != nullptr;
}

}  // namespace

void SetFastAimingDelayEnabled(bool enabled) {
    g_status.enabled = enabled;
    g_status.message = enabled ? "等待本地玩家进入瞄准状态" : "快速结束瞄准延迟已关闭";
}

void UpdateFastAimingDelay(bool world_ready) {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr) return;
    if (g_pending.queue_item != nullptr) {
        const auto state = PollObjectMethodOnMainThread(
            env, g_pending, std::chrono::milliseconds(1000), nullptr);
        if (state == AsyncObjectMethodState::Pending) return;
        if (state == AsyncObjectMethodState::Succeeded) ++g_status.cleared_count;
        if (state == AsyncObjectMethodState::Failed || state == AsyncObjectMethodState::TimedOut) {
            g_status.message = "游戏主线程未完成瞄准延迟更新";
            return;
        }
    }
    if (!g_status.enabled || !world_ready) return;
    if (!Initialize(env)) {
        g_status.message = "瞄准延迟接口尚未就绪";
        return;
    }
    jobject player = env->CallStaticObjectMethod(g_player_type, g_get_player);
    if (ClearException(env) || player == nullptr) {
        g_status.message = "等待本地玩家";
        return;
    }
    const bool aiming = env->CallBooleanMethod(player, g_is_aiming) == JNI_TRUE;
    const float delay = env->CallFloatMethod(player, g_get_delay);
    if (ClearException(env) || !aiming || !std::isfinite(delay) || delay <= 0.0f) {
        env->DeleteLocalRef(player);
        return;
    }
    jobject zero = BoxFloat(env, 0.0f);
    const bool queued = zero != nullptr && QueueObjectMethodOnMainThread(
        env, player, "setAimingDelay", {zero}, g_pending);
    if (zero != nullptr) env->DeleteLocalRef(zero);
    env->DeleteLocalRef(player);
    g_status.message = queued ? "正在结束本地玩家瞄准延迟" : "提交瞄准延迟更新失败";
}

const PlayerAimingDelayStatus& GetPlayerAimingDelayStatus() { return g_status; }

}  // namespace pztrainer::bridge
