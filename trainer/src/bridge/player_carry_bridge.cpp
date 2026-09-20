#include "bridge/player_carry_bridge.hpp"

#include <algorithm>
#include <cmath>
#include "bridge/jni_game_bridge.hpp"
#include "bridge/player_overrides.hpp"

namespace pztrainer::bridge {
namespace {
PlayerCarryStatus g_status;
bool g_requested = false;
float g_multiplier = 1.85f;
jclass g_helper = nullptr;
jmethodID g_configure = nullptr;
}

void UpdatePlayerCarryBridge() {
    g_status.enabled = g_requested;
    g_status.multiplier = g_multiplier;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (!env) { g_status.available = false; return; }
    if (!g_helper) {
        jclass local = LoadPlayerOverrideClass(env, "pztrainer.player.CarryOverrides", g_status.message);
        if (!local) { g_status.available = false; return; }
        g_configure = env->GetStaticMethodID(local, "configure", "(ZF)I");
        if (!env->ExceptionCheck() && g_configure) g_helper = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!g_helper) { g_status.available = false; return; }
    }
    const int state = env->CallStaticIntMethod(g_helper, g_configure, g_requested ? JNI_TRUE : JNI_FALSE, g_multiplier);
    if (env->ExceptionCheck()) {
        env->ExceptionClear(); g_status.available = false; g_status.message = "负重接口执行失败"; return;
    }
    g_status.initialized = true;
    g_status.available = (state & 1) != 0;
    g_status.player_ready = (state & 4) != 0;
    g_status.multiplayer = (state & 8) != 0;
    g_status.message = PlayerOverrideMessage(env, g_helper);
}

const PlayerCarryStatus& GetPlayerCarryStatus() { return g_status; }
void SetUnlimitedCarryEnabled(bool enabled) { g_requested = enabled; g_status.enabled = enabled; }
void SetCarryWeightMultiplier(float multiplier) {
    g_multiplier = std::isfinite(multiplier) ? std::clamp(multiplier, 1.0f, 100.0f) : 1.85f;
    g_status.multiplier = g_multiplier;
}
}  // namespace pztrainer::bridge