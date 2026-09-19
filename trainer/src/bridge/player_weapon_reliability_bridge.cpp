#include "bridge/player_weapon_reliability_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass hand_weapon = nullptr;
    jclass packet_type = nullptr;
    jclass network_packet = nullptr;
    jclass object = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jfieldID sync_hand_weapon_fields = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_primary_item = nullptr;
    jmethodID is_ranged = nullptr;
    jmethodID get_jam_chance = nullptr;
    jmethodID set_jam_chance = nullptr;
    jmethodID is_jammed = nullptr;
    jmethodID set_jammed = nullptr;
    jmethodID send_packet = nullptr;
};

Bindings g_bindings;
PlayerWeaponReliabilityStatus g_status;
bool g_enabled_requested = false;
jobject g_tracked_weapon = nullptr;
float g_original_jam_chance = 0.0f;

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
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    g_bindings.network_packet = LoadGlobalClass(
        env, "zombie/network/packets/INetworkPacket");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.hand_weapon == nullptr ||
        g_bindings.packet_type == nullptr ||
        g_bindings.network_packet == nullptr || g_bindings.object == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(
        g_bindings.game_server, "server", "Z");
    g_bindings.sync_hand_weapon_fields = env->GetStaticFieldID(
        g_bindings.packet_type, "SyncHandWeaponFields",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_primary_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem",
        "()Lzombie/inventory/InventoryItem;");
    g_bindings.is_ranged = env->GetMethodID(
        g_bindings.hand_weapon, "isRanged", "()Z");
    g_bindings.get_jam_chance = env->GetMethodID(
        g_bindings.hand_weapon, "getJamGunChance", "()F");
    g_bindings.set_jam_chance = env->GetMethodID(
        g_bindings.hand_weapon, "setJamGunChance", "(F)V");
    g_bindings.is_jammed = env->GetMethodID(
        g_bindings.hand_weapon, "isJammed", "()Z");
    g_bindings.set_jammed = env->GetMethodID(
        g_bindings.hand_weapon, "setJammed", "(Z)V");
    g_bindings.send_packet = env->GetStaticMethodID(
        g_bindings.network_packet, "send",
        "(Lzombie/network/PacketTypes$PacketType;[Ljava/lang/Object;)V");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr && g_bindings.server_flag != nullptr &&
        g_bindings.sync_hand_weapon_fields != nullptr &&
        g_bindings.get_player != nullptr &&
        g_bindings.get_primary_item != nullptr &&
        g_bindings.is_ranged != nullptr &&
        g_bindings.get_jam_chance != nullptr &&
        g_bindings.set_jam_chance != nullptr &&
        g_bindings.is_jammed != nullptr && g_bindings.set_jammed != nullptr &&
        g_bindings.send_packet != nullptr;
    return g_bindings.ready;
}

PlayerResourceSessionMode ReadSessionMode(JNIEnv* env) {
    const bool client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool server = env->GetStaticBooleanField(
        g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) return PlayerResourceSessionMode::Unknown;
    if (client) return PlayerResourceSessionMode::MultiplayerClient;
    if (server) return PlayerResourceSessionMode::DedicatedServer;
    return PlayerResourceSessionMode::Local;
}

bool SendWeaponFields(JNIEnv* env, jobject player, jobject weapon) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, g_bindings.sync_hand_weapon_fields);
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (packet_type == nullptr || arguments == nullptr || ClearException(env)) {
        if (arguments != nullptr) env->DeleteLocalRef(arguments);
        if (packet_type != nullptr) env->DeleteLocalRef(packet_type);
        return false;
    }
    env->SetObjectArrayElement(arguments, 0, player);
    env->SetObjectArrayElement(arguments, 1, weapon);
    env->CallStaticVoidMethod(
        g_bindings.network_packet, g_bindings.send_packet,
        packet_type, arguments);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(arguments);
    env->DeleteLocalRef(packet_type);
    return succeeded;
}

void RestoreTrackedWeapon(JNIEnv* env) {
    if (g_tracked_weapon == nullptr) return;
    env->CallVoidMethod(
        g_tracked_weapon, g_bindings.set_jam_chance, g_original_jam_chance);
    ClearException(env);
    env->DeleteGlobalRef(g_tracked_weapon);
    g_tracked_weapon = nullptr;
    g_original_jam_chance = 0.0f;
}

bool TrackWeapon(JNIEnv* env, jobject weapon) {
    if (g_tracked_weapon != nullptr &&
        env->IsSameObject(g_tracked_weapon, weapon) == JNI_TRUE) {
        return true;
    }
    RestoreTrackedWeapon(env);
    g_original_jam_chance = env->CallFloatMethod(
        weapon, g_bindings.get_jam_chance);
    if (ClearException(env)) return false;
    g_tracked_weapon = env->NewGlobalRef(weapon);
    return g_tracked_weapon != nullptr && !ClearException(env);
}

void ResetWeaponState(const char* message) {
    g_status.firearm_ready = false;
    g_status.jammed = false;
    g_status.original_jam_chance = 0.0f;
    g_status.message = message;
}

}  // namespace

void UpdatePlayerWeaponReliabilityBridge() {
    g_status.enabled = g_enabled_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.session_mode = PlayerResourceSessionMode::Unknown;
        ResetWeaponState("枪械防卡壳桥接尚未初始化");
        return;
    }

    g_status.initialized = true;
    g_status.session_mode = ReadSessionMode(env);
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        RestoreTrackedWeapon(env);
        g_status.player_ready = false;
        ResetWeaponState("等待进入存档并创建玩家");
        return;
    }
    g_status.player_ready = true;

    jobject weapon = env->CallObjectMethod(player, g_bindings.get_primary_item);
    if (weapon == nullptr || ClearException(env) ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE ||
        env->CallBooleanMethod(weapon, g_bindings.is_ranged) != JNI_TRUE ||
        ClearException(env)) {
        if (weapon != nullptr) env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        RestoreTrackedWeapon(env);
        ResetWeaponState("当前主手不是远程枪械");
        return;
    }
    g_status.firearm_ready = true;

    if (!g_enabled_requested) {
        const float original_chance = g_tracked_weapon != nullptr &&
            env->IsSameObject(g_tracked_weapon, weapon) == JNI_TRUE
            ? g_original_jam_chance
            : env->CallFloatMethod(weapon, g_bindings.get_jam_chance);
        RestoreTrackedWeapon(env);
        g_status.original_jam_chance = original_chance;
        g_status.jammed = env->CallBooleanMethod(
            weapon, g_bindings.is_jammed) == JNI_TRUE;
        g_status.message = "枪械防卡壳待命";
        ClearException(env);
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }

    if (!TrackWeapon(env, weapon)) {
        ResetWeaponState("读取枪械卡壳参数失败");
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }
    const float current_jam_chance = env->CallFloatMethod(
        weapon, g_bindings.get_jam_chance);
    if (ClearException(env)) {
        ResetWeaponState("读取枪械卡壳参数失败");
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }
    if (current_jam_chance != 0.0f) {
        g_original_jam_chance = current_jam_chance;
    }
    g_status.original_jam_chance = g_original_jam_chance;
    env->CallVoidMethod(weapon, g_bindings.set_jam_chance, 0.0f);
    const bool jammed = env->CallBooleanMethod(
        weapon, g_bindings.is_jammed) == JNI_TRUE;
    if (ClearException(env)) {
        ResetWeaponState("写入枪械防卡壳参数失败");
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }

    g_status.jammed = jammed;
    if (!jammed) {
        g_status.message = "枪械防卡壳运行中";
    } else {
        env->CallVoidMethod(weapon, g_bindings.set_jammed, JNI_FALSE);
        if (ClearException(env)) {
            g_status.message = "清除枪械卡壳状态失败";
        } else {
            g_status.jammed = false;
            ++g_status.unjam_count;
            if (g_status.session_mode ==
                PlayerResourceSessionMode::MultiplayerClient) {
                if (SendWeaponFields(env, player, weapon)) {
                    ++g_status.server_sync_count;
                    g_status.message = "已清除卡壳并同步服务器";
                } else {
                    g_status.message = "本地已清除卡壳，但服务器同步失败";
                }
            } else {
                g_status.message = "已清除枪械卡壳";
            }
        }
    }

    env->DeleteLocalRef(weapon);
    env->DeleteLocalRef(player);
}

const PlayerWeaponReliabilityStatus& GetPlayerWeaponReliabilityStatus() {
    return g_status;
}

void SetNoWeaponJamEnabled(bool enabled) {
    g_enabled_requested = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
