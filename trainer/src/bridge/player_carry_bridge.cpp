#include "bridge/player_carry_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jfieldID client_flag = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_max_weight = nullptr;
    jmethodID get_max_weight_delta = nullptr;
    jmethodID set_max_weight_delta = nullptr;
    jmethodID send_player_damage = nullptr;
};

Bindings g_bindings;
PlayerCarryStatus g_status;
bool g_requested = false;
float g_multiplier = 1.85f;
jobject g_player = nullptr;
float g_original_max_weight_delta = 1.0f;
bool g_original_delta_captured = false;
int g_last_synced_max_weight = 0;
bool g_has_synced_max_weight = false;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    if (class_loader == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr
        ? nullptr : env->CallStaticObjectMethod(class_loader, get_system_loader);
    env->DeleteLocalRef(class_loader);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }
    std::string dotted(binary_name);
    std::replace(dotted.begin(), dotted.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted.c_str());
    jclass local = name == nullptr ? nullptr : static_cast<jclass>(
        env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr) {
        return false;
    }
    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_max_weight = env->GetMethodID(
        g_bindings.iso_player, "getMaxWeight", "()I");
    g_bindings.get_max_weight_delta = env->GetMethodID(
        g_bindings.iso_player, "getMaxWeightDelta", "()F");
    g_bindings.set_max_weight_delta = env->GetMethodID(
        g_bindings.iso_player, "setMaxWeightDelta", "(F)V");
    g_bindings.send_player_damage = env->GetStaticMethodID(
        g_bindings.game_client, "sendPlayerDamage",
        "(Lzombie/characters/IsoPlayer;)V");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_max_weight != nullptr &&
        g_bindings.get_max_weight_delta != nullptr &&
        g_bindings.set_max_weight_delta != nullptr &&
        g_bindings.send_player_damage != nullptr;
    return g_bindings.ready;
}

void ResetPlayerState(JNIEnv* env, jobject player) {
    if (g_player != nullptr && env->IsSameObject(g_player, player) == JNI_TRUE) {
        return;
    }
    if (g_player != nullptr) env->DeleteGlobalRef(g_player);
    g_player = env->NewGlobalRef(player);
    g_original_max_weight_delta = 1.0f;
    g_original_delta_captured = false;
    g_last_synced_max_weight = 0;
    g_has_synced_max_weight = false;
}

void SyncMaxWeight(JNIEnv* env, jobject player, int max_weight) {
    if (!g_status.multiplayer ||
        (g_has_synced_max_weight && g_last_synced_max_weight == max_weight)) {
        return;
    }
    env->CallStaticVoidMethod(
        g_bindings.game_client, g_bindings.send_player_damage, player);
    if (!ClearException(env)) {
        g_last_synced_max_weight = max_weight;
        g_has_synced_max_weight = true;
    }
}

}  // namespace

void UpdatePlayerCarryBridge() {
    g_status.enabled = g_requested;
    g_status.multiplier = g_multiplier;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.available = false;
        g_status.message = "无限负重桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) g_status.multiplayer = false;

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.available = false;
        g_status.message = "等待进入存档并创建玩家";
        return;
    }
    g_status.player_ready = true;
    g_status.available = true;
    ResetPlayerState(env, player);

    const float observed_delta = env->CallFloatMethod(
        player, g_bindings.get_max_weight_delta);
    if (ClearException(env)) {
        g_status.message = "修改负重状态失败";
        env->DeleteLocalRef(player);
        return;
    }
    if (!g_original_delta_captured) {
        g_original_max_weight_delta = observed_delta;
        g_original_delta_captured = true;
    }

    const float target_delta = g_requested
        ? g_original_max_weight_delta * g_multiplier
        : g_original_max_weight_delta;
    if (std::fabs(observed_delta - target_delta) > 0.0001f) {
        env->CallVoidMethod(
            player, g_bindings.set_max_weight_delta, target_delta);
        if (ClearException(env)) {
            g_status.message = "修改负重状态失败";
            env->DeleteLocalRef(player);
            return;
        }
    }

    const int observed_max_weight = env->CallIntMethod(
        player, g_bindings.get_max_weight);
    if (!ClearException(env)) {
        SyncMaxWeight(env, player, observed_max_weight);
    }
    g_status.message = g_requested
        ? "无限负重运行中" : "无限负重待命";
    env->DeleteLocalRef(player);
}

const PlayerCarryStatus& GetPlayerCarryStatus() {
    return g_status;
}

void SetUnlimitedCarryEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
}

void SetCarryWeightMultiplier(float multiplier) {
    g_multiplier = std::clamp(multiplier, 1.0f, 100.0f);
    g_status.multiplier = g_multiplier;
}

}  // namespace pztrainer::bridge
