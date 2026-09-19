#include "bridge/player_health_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass body_damage = nullptr;
    jclass body_part = nullptr;
    jclass array_list = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_body_damage = nullptr;
    jmethodID get_body_parts = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID body_part_health = nullptr;
    jmethodID restore_full_health = nullptr;
    jmethodID send_player_damage = nullptr;
    jmethodID is_god_mode = nullptr;
    jmethodID avoid_damage = nullptr;
    jmethodID set_god_mode = nullptr;
    jmethodID set_avoid_damage = nullptr;
};

Bindings g_bindings;
PlayerHealthStatus g_status;
bool g_infinite_health_requested = false;
bool g_invincibility_requested = false;
bool g_invincibility_applied = false;
bool g_previous_god_mode = false;
bool g_previous_avoid_damage = false;
std::chrono::steady_clock::time_point g_last_server_sync{};

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
    g_bindings.body_damage = LoadGlobalClass(
        env, "zombie/characters/BodyDamage/BodyDamage");
    g_bindings.body_part = LoadGlobalClass(
        env, "zombie/characters/BodyDamage/BodyPart");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.body_damage == nullptr ||
        g_bindings.body_part == nullptr || g_bindings.array_list == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_body_damage = env->GetMethodID(
        g_bindings.iso_player, "getBodyDamage",
        "()Lzombie/characters/BodyDamage/BodyDamage;");
    g_bindings.get_body_parts = env->GetMethodID(
        g_bindings.body_damage, "getBodyParts", "()Ljava/util/ArrayList;");
    g_bindings.list_size = env->GetMethodID(g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.body_part_health = env->GetMethodID(
        g_bindings.body_part, "getHealth", "()F");
    g_bindings.restore_full_health = env->GetMethodID(
        g_bindings.body_damage, "RestoreToFullHealth", "()V");
    g_bindings.send_player_damage = env->GetStaticMethodID(
        g_bindings.game_client, "sendPlayerDamage", "(Lzombie/characters/IsoPlayer;)V");
    g_bindings.is_god_mode = env->GetMethodID(
        g_bindings.iso_player, "isGodMod", "()Z");
    g_bindings.avoid_damage = env->GetMethodID(
        g_bindings.iso_player, "avoidDamage", "()Z");
    g_bindings.set_god_mode = env->GetMethodID(
        g_bindings.iso_player, "setGodMod", "(ZZ)V");
    g_bindings.set_avoid_damage = env->GetMethodID(
        g_bindings.iso_player, "setAvoidDamage", "(Z)V");

    g_bindings.ready = !ClearException(env) && g_bindings.client_flag != nullptr &&
        g_bindings.server_flag != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_body_damage != nullptr && g_bindings.get_body_parts != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.body_part_health != nullptr &&
        g_bindings.restore_full_health != nullptr &&
        g_bindings.send_player_damage != nullptr && g_bindings.is_god_mode != nullptr &&
        g_bindings.avoid_damage != nullptr && g_bindings.set_god_mode != nullptr &&
        g_bindings.set_avoid_damage != nullptr;
    return g_bindings.ready;
}

PlayerHealthSessionMode ReadSessionMode(JNIEnv* env) {
    const bool client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool server = env->GetStaticBooleanField(
        g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) return PlayerHealthSessionMode::Unknown;
    if (client) return PlayerHealthSessionMode::MultiplayerClient;
    if (server) return PlayerHealthSessionMode::DedicatedServer;
    return PlayerHealthSessionMode::Local;
}

void RemoveInvincibility(JNIEnv* env, jobject player) {
    if (!g_invincibility_applied) return;
    env->CallVoidMethod(
        player, g_bindings.set_god_mode,
        g_previous_god_mode ? JNI_TRUE : JNI_FALSE, JNI_TRUE);
    env->CallVoidMethod(
        player, g_bindings.set_avoid_damage,
        g_previous_avoid_damage ? JNI_TRUE : JNI_FALSE);
    ClearException(env);
    g_invincibility_applied = false;
}

void ApplyInvincibility(JNIEnv* env, jobject player) {
    if (!g_invincibility_applied) {
        g_previous_god_mode =
            env->CallBooleanMethod(player, g_bindings.is_god_mode) == JNI_TRUE;
        g_previous_avoid_damage =
            env->CallBooleanMethod(player, g_bindings.avoid_damage) == JNI_TRUE;
        if (ClearException(env)) return;
        g_invincibility_applied = true;
    }
    env->CallVoidMethod(player, g_bindings.set_god_mode, JNI_TRUE, JNI_TRUE);
    env->CallVoidMethod(player, g_bindings.set_avoid_damage, JNI_TRUE);
    ClearException(env);
}

int CountDamagedParts(JNIEnv* env, jobject body_damage) {
    jobject body_parts = env->CallObjectMethod(body_damage, g_bindings.get_body_parts);
    if (body_parts == nullptr || ClearException(env)) return 0;
    const jint count = env->CallIntMethod(body_parts, g_bindings.list_size);
    int damaged = 0;
    for (jint index = 0; index < count && !env->ExceptionCheck(); ++index) {
        jobject body_part = env->CallObjectMethod(body_parts, g_bindings.list_get, index);
        if (body_part == nullptr) continue;
        const float health = env->CallFloatMethod(body_part, g_bindings.body_part_health);
        if (health < 99.999f) ++damaged;
        env->DeleteLocalRef(body_part);
    }
    ClearException(env);
    env->DeleteLocalRef(body_parts);
    return damaged;
}

}  // namespace

void UpdatePlayerHealthBridge() {
    g_status.infinite_health_enabled = g_infinite_health_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.session_mode = PlayerHealthSessionMode::Unknown;
        g_status.invincibility_available = false;
        g_status.message = "玩家桥接尚未初始化";
        return;
    }

    g_status.initialized = true;
    g_status.session_mode = ReadSessionMode(env);
    const bool local = g_status.session_mode == PlayerHealthSessionMode::Local;
    g_status.invincibility_available = local;
    if (!local) g_invincibility_requested = false;

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        g_status.player_ready = false;
        g_status.invincibility_enabled = false;
        g_status.message = "等待进入存档并创建玩家";
        if (player != nullptr) env->DeleteLocalRef(player);
        return;
    }
    g_status.player_ready = true;

    if (local && g_invincibility_requested) {
        ApplyInvincibility(env, player);
    } else {
        RemoveInvincibility(env, player);
    }
    g_status.invincibility_enabled = g_invincibility_applied && local;

    if (!g_infinite_health_requested) {
        g_status.last_restored_parts = 0;
        g_status.message = local ? "本地模式，生命保护待命" :
            "在线模式，生命保护待命";
        env->DeleteLocalRef(player);
        return;
    }

    jobject body_damage = env->CallObjectMethod(player, g_bindings.get_body_damage);
    if (body_damage == nullptr || ClearException(env)) {
        g_status.message = "无法读取玩家身体状态";
        if (body_damage != nullptr) env->DeleteLocalRef(body_damage);
        env->DeleteLocalRef(player);
        return;
    }

    const int damaged_parts = CountDamagedParts(env, body_damage);
    g_status.last_restored_parts = damaged_parts;
    if (damaged_parts > 0) {
        env->CallVoidMethod(body_damage, g_bindings.restore_full_health);
        if (!ClearException(env)) {
            ++g_status.restore_count;
            if (g_status.session_mode == PlayerHealthSessionMode::MultiplayerClient &&
                std::chrono::steady_clock::now() - g_last_server_sync >=
                    std::chrono::milliseconds(100)) {
                env->CallStaticVoidMethod(
                    g_bindings.game_client, g_bindings.send_player_damage, player);
                if (!ClearException(env)) {
                    ++g_status.server_sync_count;
                    g_last_server_sync = std::chrono::steady_clock::now();
                    g_status.message = "已恢复全部部位并发送 PlayerDamage 到服务器";
                } else {
                    g_status.message = "本地已恢复，但 PlayerDamage 发送失败";
                }
            } else {
                g_status.message = local ? "已恢复全部身体部位" :
                    "已恢复全部部位，等待同步限速窗口";
            }
        } else {
            g_status.message = "恢复全部身体部位失败";
        }
    } else {
        g_status.message = local ? "全部身体部位正常" :
            "全部身体部位正常，在线同步待命";
    }

    env->DeleteLocalRef(body_damage);
    env->DeleteLocalRef(player);
}

const PlayerHealthStatus& GetPlayerHealthStatus() {
    return g_status;
}

void SetInfiniteHealthEnabled(bool enabled) {
    g_infinite_health_requested = enabled;
    g_status.infinite_health_enabled = enabled;
}

void SetInvincibilityEnabled(bool enabled) {
    g_invincibility_requested = enabled && g_status.invincibility_available;
}

}  // namespace pztrainer::bridge
