#include "bridge/player_movement_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/player_movement_state_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jmethodID get_player = nullptr;
    jmethodID is_no_clip = nullptr;
    jmethodID set_no_clip_forced = nullptr;
};

Bindings g_bindings;
PlayerMovementStatus g_status;
bool g_no_clip_requested = false;
bool g_no_clip_applied = false;

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
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.game_server = LoadGlobalClass(env, "zombie/network/GameServer");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(
        g_bindings.game_server, "server", "Z");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance",
        "()Lzombie/characters/IsoPlayer;");
    g_bindings.is_no_clip = env->GetMethodID(
        g_bindings.iso_player, "isNoClip", "()Z");
    g_bindings.set_no_clip_forced = env->GetMethodID(
        g_bindings.iso_player, "setNoClip", "(ZZ)V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr && g_bindings.server_flag != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.is_no_clip != nullptr &&
        g_bindings.set_no_clip_forced != nullptr;
    return g_bindings.ready;
}

PlayerMovementSessionMode ReadSessionMode(JNIEnv* env) {
    const bool client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool server = env->GetStaticBooleanField(
        g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) return PlayerMovementSessionMode::Unknown;
    if (client) return PlayerMovementSessionMode::MultiplayerClient;
    if (server) return PlayerMovementSessionMode::DedicatedServer;
    return PlayerMovementSessionMode::Local;
}

bool ForceNoClipOff(JNIEnv* env, jobject player) {
    if (player == nullptr) return false;
    env->CallVoidMethod(
        player, g_bindings.set_no_clip_forced, JNI_FALSE, JNI_TRUE);
    if (ClearException(env)) return false;
    const bool no_clip_enabled = env->CallBooleanMethod(
        player, g_bindings.is_no_clip) == JNI_TRUE;
    if (ClearException(env)) return false;
    g_no_clip_applied = no_clip_enabled;
    return !no_clip_enabled;
}

bool ApplyNoClip(JNIEnv* env, jobject player) {
    env->CallVoidMethod(
        player, g_bindings.set_no_clip_forced, JNI_TRUE, JNI_TRUE);
    if (ClearException(env)) return false;
    g_no_clip_applied = env->CallBooleanMethod(
        player, g_bindings.is_no_clip) == JNI_TRUE;
    if (ClearException(env)) return false;
    return g_no_clip_applied;
}

}  // namespace

void UpdatePlayerMovementBridge() {
    g_status.no_clip_enabled = g_no_clip_applied;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.session_mode = PlayerMovementSessionMode::Unknown;
        g_status.no_clip_available = false;
        g_status.no_clip_enabled = false;
        g_status.network_state_ready = false;
        g_status.message = "人物移动桥接尚未初始化";
        return;
    }

    g_status.initialized = true;
    g_status.session_mode = ReadSessionMode(env);

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        g_status.player_ready = false;
        g_no_clip_applied = false;
        g_status.no_clip_available = false;
        g_status.no_clip_enabled = false;
        g_status.network_state_ready = false;
        g_status.message = "等待进入存档并创建玩家";
        if (player != nullptr) env->DeleteLocalRef(player);
        return;
    }
    g_status.player_ready = true;

    if (g_status.session_mode == PlayerMovementSessionMode::MultiplayerClient) {
        if (!g_no_clip_requested) {
            const bool local_no_clip_disabled = ForceNoClipOff(env, player);
            UpdatePlayerMovementStateBridge(
                env, player, !local_no_clip_disabled);
            const PlayerMovementStateStatus& state_status =
                GetPlayerMovementStateStatus();
            g_status.no_clip_available =
                state_status.initialized && local_no_clip_disabled;
            g_status.network_state_ready = state_status.ready_for_movement;
            g_status.no_clip_enabled = false;
            g_status.message = local_no_clip_disabled
                ? state_status.message
                : "无法关闭本地人物穿墙，服务器保护保持启用";
            env->DeleteLocalRef(player);
            return;
        }
        UpdatePlayerMovementStateBridge(env, player, g_no_clip_requested);
        const PlayerMovementStateStatus& state_status =
            GetPlayerMovementStateStatus();
        g_status.no_clip_available = state_status.initialized;
        g_status.network_state_ready = state_status.ready_for_movement;
        if (!state_status.initialized) {
            g_no_clip_requested = false;
            ForceNoClipOff(env, player);
            g_status.no_clip_enabled = false;
            g_status.message = state_status.message;
            env->DeleteLocalRef(player);
            return;
        }
        if (g_no_clip_requested && !state_status.ready_for_movement) {
            ForceNoClipOff(env, player);
            g_status.no_clip_enabled = true;
            g_status.message = state_status.message;
            env->DeleteLocalRef(player);
            return;
        }
    } else {
        UpdatePlayerMovementStateBridge(env, player, false);
        g_status.network_state_ready = false;
        g_status.no_clip_available = true;
    }

    if (g_no_clip_requested) {
        ApplyNoClip(env, player);
    } else {
        ForceNoClipOff(env, player);
    }
    g_status.no_clip_enabled = g_no_clip_applied;
    g_status.message = g_status.no_clip_enabled
        ? "人物穿墙已启用" : "人物穿墙待命";
    env->DeleteLocalRef(player);
}

const PlayerMovementStatus& GetPlayerMovementStatus() {
    return g_status;
}

void SetNoClipEnabled(bool enabled) {
    g_no_clip_requested = enabled;
    g_status.no_clip_enabled = enabled && g_status.no_clip_available;
}

}  // namespace pztrainer::bridge
