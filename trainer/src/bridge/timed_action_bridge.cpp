#include "bridge/timed_action_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/packet_audit.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jclass role = nullptr;
    jclass capability = nullptr;
    jclass udp_connection = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID game_client_connection = nullptr;
    jfieldID use_timed_action_instant_cheat = nullptr;
    jmethodID get_player = nullptr;
    jmethodID connection_get_role = nullptr;
    jmethodID role_get_name = nullptr;
    jmethodID role_has_capability = nullptr;
    jmethodID is_instant = nullptr;
    jmethodID is_instant_cheat = nullptr;
    jmethodID set_instant_cheat = nullptr;
};

Bindings g_bindings;
TimedActionStatus g_status;
bool g_requested = false;
jobject g_player = nullptr;
bool g_original_instant_cheat = false;
bool g_original_captured = false;
bool g_multiplayer_audit_recorded = false;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (utf == nullptr || ClearException(env)) return {};
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

struct TimedActionPermission {
    bool available = false;
    std::string role_name = "unknown";
};

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

TimedActionPermission ReadTimedActionPermission(JNIEnv* env) {
    TimedActionPermission result;
    jobject connection = env->GetStaticObjectField(
        g_bindings.game_client, g_bindings.game_client_connection);
    if (connection == nullptr || ClearException(env)) {
        if (connection != nullptr) env->DeleteLocalRef(connection);
        return result;
    }
    jobject role = env->CallObjectMethod(connection, g_bindings.connection_get_role);
    if (role == nullptr || ClearException(env)) {
        if (role != nullptr) env->DeleteLocalRef(role);
        env->DeleteLocalRef(connection);
        return result;
    }
    jstring name = static_cast<jstring>(
        env->CallObjectMethod(role, g_bindings.role_get_name));
    if (!ClearException(env)) result.role_name = JavaString(env, name);
    if (name != nullptr) env->DeleteLocalRef(name);

    jobject capability = env->GetStaticObjectField(
        g_bindings.capability, g_bindings.use_timed_action_instant_cheat);
    if (capability != nullptr && !ClearException(env)) {
        result.available = env->CallBooleanMethod(
            role, g_bindings.role_has_capability, capability) == JNI_TRUE;
        if (ClearException(env)) result.available = false;
        env->DeleteLocalRef(capability);
    }
    env->DeleteLocalRef(role);
    env->DeleteLocalRef(connection);
    return result;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.role = LoadGlobalClass(env, "zombie/characters/Role");
    g_bindings.capability = LoadGlobalClass(env, "zombie/characters/Capability");
    g_bindings.udp_connection = LoadGlobalClass(env, "zombie/core/raknet/UdpConnection");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.role == nullptr || g_bindings.capability == nullptr ||
        g_bindings.udp_connection == nullptr) {
        return false;
    }
    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.game_client_connection = env->GetStaticFieldID(
        g_bindings.game_client, "connection",
        "Lzombie/core/raknet/UdpConnection;");
    g_bindings.use_timed_action_instant_cheat = env->GetStaticFieldID(
        g_bindings.capability, "UseTimedActionInstantCheat",
        "Lzombie/characters/Capability;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.connection_get_role = env->GetMethodID(
        g_bindings.udp_connection, "getRole", "()Lzombie/characters/Role;");
    g_bindings.role_get_name = env->GetMethodID(
        g_bindings.role, "getName", "()Ljava/lang/String;");
    g_bindings.role_has_capability = env->GetMethodID(
        g_bindings.role, "hasCapability", "(Lzombie/characters/Capability;)Z");
    g_bindings.is_instant = env->GetMethodID(
        g_bindings.iso_player, "isTimedActionInstant", "()Z");
    g_bindings.is_instant_cheat = env->GetMethodID(
        g_bindings.iso_player, "isTimedActionInstantCheat", "()Z");
    g_bindings.set_instant_cheat = env->GetMethodID(
        g_bindings.iso_player, "setTimedActionInstantCheat", "(Z)V");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr &&
        g_bindings.game_client_connection != nullptr &&
        g_bindings.use_timed_action_instant_cheat != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.is_instant != nullptr &&
        g_bindings.connection_get_role != nullptr &&
        g_bindings.role_get_name != nullptr &&
        g_bindings.role_has_capability != nullptr &&
        g_bindings.is_instant_cheat != nullptr &&
        g_bindings.set_instant_cheat != nullptr;
    return g_bindings.ready;
}

void ResetPlayer(JNIEnv* env) {
    if (g_player != nullptr) env->DeleteGlobalRef(g_player);
    g_player = nullptr;
    g_original_instant_cheat = false;
    g_original_captured = false;
}

bool CaptureOriginal(JNIEnv* env, jobject player) {
    const bool instant = env->CallBooleanMethod(
        player, g_bindings.is_instant_cheat) == JNI_TRUE;
    if (ClearException(env)) return false;
    g_original_instant_cheat = instant;
    g_original_captured = true;
    return true;
}

bool RestoreOriginal(JNIEnv* env, jobject player) {
    if (!g_original_captured) return true;
    env->CallVoidMethod(
        player, g_bindings.set_instant_cheat,
        g_original_instant_cheat ? JNI_TRUE : JNI_FALSE);
    return !ClearException(env);
}

}  // namespace

void UpdateTimedActionBridge() {
    g_status.enabled = g_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.available = false;
        g_status.applied = false;
        g_status.message = "定时动作桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) g_status.multiplayer = false;
    TimedActionPermission permission;
    if (g_status.multiplayer) permission = ReadTimedActionPermission(env);
    g_status.available = !g_status.multiplayer || permission.available;

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.applied = false;
        g_status.message = "等待进入存档并创建玩家";
        return;
    }
    g_status.player_ready = true;

    if (g_status.multiplayer && !permission.available) {
        ResetPlayer(env);
        g_requested = false;
        g_status.enabled = false;
        g_status.applied = false;
        g_status.message = "联机角色 " + permission.role_name +
            " 未授予 UseTimedActionInstantCheat；访问级别名称为 admin 不等价于该能力。";
        if (!g_multiplayer_audit_recorded) {
            RecordPacketAudit(PacketAuditKind::BuildAction, false, "instant",
                              "missing UseTimedActionInstantCheat on connection role");
            RecordPacketAudit(PacketAuditKind::GeneralAction, false, "instant",
                              "server only handles reject/stop");
            RecordPacketAudit(PacketAuditKind::FishingAction, false, "skip-minigame",
                              "server owns fishing state and result");
            g_multiplayer_audit_recorded = true;
        }
        env->DeleteLocalRef(player);
        return;
    }
    g_multiplayer_audit_recorded = false;

    if (g_player == nullptr || env->IsSameObject(g_player, player) != JNI_TRUE) {
        ResetPlayer(env);
        g_player = env->NewGlobalRef(player);
    }
    if (g_player == nullptr || ClearException(env)) {
        env->DeleteLocalRef(player);
        g_status.applied = false;
        g_status.message = "记录当前玩家失败";
        return;
    }

    if (g_requested) {
        if (!g_original_captured && !CaptureOriginal(env, player)) {
            env->DeleteLocalRef(player);
            g_status.applied = false;
            g_status.message = "记录原始定时动作状态失败";
            return;
        }
        env->CallVoidMethod(
            player, g_bindings.set_instant_cheat, JNI_TRUE);
        bool effective = !ClearException(env) && env->CallBooleanMethod(
            player, g_bindings.is_instant) == JNI_TRUE;
        g_status.applied = effective && !ClearException(env);
        g_status.message = g_status.applied
            ? (g_status.multiplayer
                ? "连接角色具备 UseTimedActionInstantCheat；已调用原版开关，BuildAction/FishingAction 仍由服务端重算"
                : "定时动作将通过原版状态立即完成")
            : "游戏未接受定时动作立即完成状态";
    } else {
        const bool restored = RestoreOriginal(env, player);
        g_status.applied = restored;
        g_status.message = restored
            ? "定时动作速度已恢复" : "恢复原始定时动作状态失败";
        if (restored) {
            g_original_captured = false;
        }
    }
    env->DeleteLocalRef(player);
}

const TimedActionStatus& GetTimedActionStatus() {
    return g_status;
}

void SetTimedActionInstantEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
    g_status.applied = false;
    g_status.message = enabled
        ? "正在启用定时动作立即完成" : "正在恢复定时动作速度";
    RecordPacketAudit(PacketAuditKind::GeneralAction, false, "instant-request",
                      enabled ? "requested; awaiting local state" : "restored-request");
}

}  // namespace pztrainer::bridge
