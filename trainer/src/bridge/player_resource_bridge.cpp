#include "bridge/player_resource_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/endurance_compensation_bridge.hpp"
#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass stats = nullptr;
    jclass character_stat = nullptr;
    jclass moodles = nullptr;
    jclass inventory_item = nullptr;
    jclass packet_type = nullptr;
    jclass network_packet = nullptr;
    jclass object = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jfieldID endurance_stat = nullptr;
    jfieldID sync_item_fields = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_stats = nullptr;
    jmethodID stats_get = nullptr;
    jmethodID stats_set = nullptr;
    jmethodID stats_set_last_endurance = nullptr;
    jmethodID stat_maximum = nullptr;
    jmethodID get_moodles = nullptr;
    jmethodID moodles_update = nullptr;
    jmethodID get_primary_item = nullptr;
    jmethodID get_secondary_item = nullptr;
    jmethodID get_condition = nullptr;
    jmethodID get_condition_max = nullptr;
    jmethodID set_condition_no_sound = nullptr;
    jmethodID send_packet = nullptr;
};

Bindings g_bindings;
PlayerResourceStatus g_status;
bool g_endurance_recovery_requested = false;
bool g_durability_protection_requested = false;

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
    g_bindings.stats = LoadGlobalClass(env, "zombie/characters/Stats");
    g_bindings.character_stat = LoadGlobalClass(env, "zombie/characters/CharacterStat");
    g_bindings.moodles = LoadGlobalClass(env, "zombie/characters/Moodles/Moodles");
    g_bindings.inventory_item = LoadGlobalClass(env, "zombie/inventory/InventoryItem");
    g_bindings.packet_type = LoadGlobalClass(env, "zombie/network/PacketTypes$PacketType");
    g_bindings.network_packet = LoadGlobalClass(env, "zombie/network/packets/INetworkPacket");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.stats == nullptr ||
        g_bindings.character_stat == nullptr || g_bindings.moodles == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.packet_type == nullptr || g_bindings.network_packet == nullptr ||
        g_bindings.object == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.endurance_stat = env->GetStaticFieldID(
        g_bindings.character_stat, "ENDURANCE", "Lzombie/characters/CharacterStat;");
    g_bindings.sync_item_fields = env->GetStaticFieldID(
        g_bindings.packet_type, "SyncItemFields",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_stats = env->GetMethodID(
        g_bindings.iso_player, "getStats", "()Lzombie/characters/Stats;");
    g_bindings.stats_get = env->GetMethodID(
        g_bindings.stats, "get", "(Lzombie/characters/CharacterStat;)F");
    g_bindings.stats_set = env->GetMethodID(
        g_bindings.stats, "set", "(Lzombie/characters/CharacterStat;F)Z");
    g_bindings.stats_set_last_endurance = env->GetMethodID(
        g_bindings.stats, "setLastEndurance", "(F)V");
    g_bindings.stat_maximum = env->GetMethodID(
        g_bindings.character_stat, "getMaximumValue", "()F");
    g_bindings.get_moodles = env->GetMethodID(
        g_bindings.iso_player, "getMoodles",
        "()Lzombie/characters/Moodles/Moodles;");
    g_bindings.moodles_update = env->GetMethodID(
        g_bindings.moodles, "Update", "()V");
    g_bindings.get_primary_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.get_secondary_item = env->GetMethodID(
        g_bindings.iso_player, "getSecondaryHandItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.get_condition = env->GetMethodID(
        g_bindings.inventory_item, "getCondition", "()I");
    g_bindings.get_condition_max = env->GetMethodID(
        g_bindings.inventory_item, "getConditionMax", "()I");
    g_bindings.set_condition_no_sound = env->GetMethodID(
        g_bindings.inventory_item, "setConditionNoSound", "(I)V");
    g_bindings.send_packet = env->GetStaticMethodID(
        g_bindings.network_packet, "send",
        "(Lzombie/network/PacketTypes$PacketType;[Ljava/lang/Object;)V");

    g_bindings.ready = !ClearException(env) && g_bindings.client_flag != nullptr &&
        g_bindings.server_flag != nullptr && g_bindings.endurance_stat != nullptr &&
        g_bindings.sync_item_fields != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_stats != nullptr && g_bindings.stats_get != nullptr &&
        g_bindings.stats_set != nullptr &&
        g_bindings.stats_set_last_endurance != nullptr &&
        g_bindings.stat_maximum != nullptr &&
        g_bindings.get_moodles != nullptr &&
        g_bindings.moodles_update != nullptr &&
        g_bindings.get_primary_item != nullptr &&
        g_bindings.get_secondary_item != nullptr &&
        g_bindings.get_condition != nullptr &&
        g_bindings.get_condition_max != nullptr &&
        g_bindings.set_condition_no_sound != nullptr &&
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

bool SendItemFields(JNIEnv* env, jobject player, jobject item) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, g_bindings.sync_item_fields);
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (packet_type == nullptr || arguments == nullptr || ClearException(env)) {
        if (arguments != nullptr) env->DeleteLocalRef(arguments);
        if (packet_type != nullptr) env->DeleteLocalRef(packet_type);
        return false;
    }
    env->SetObjectArrayElement(arguments, 0, player);
    env->SetObjectArrayElement(arguments, 1, item);
    env->CallStaticVoidMethod(
        g_bindings.network_packet, g_bindings.send_packet,
        packet_type, arguments);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(arguments);
    env->DeleteLocalRef(packet_type);
    return succeeded;
}

bool RestoreItem(JNIEnv* env, jobject player, jobject item,
                 PlayerResourceSessionMode session_mode,
                 int& condition, int& condition_max) {
    condition = 0;
    condition_max = 0;
    if (item == nullptr) return false;
    condition = env->CallIntMethod(item, g_bindings.get_condition);
    condition_max = env->CallIntMethod(item, g_bindings.get_condition_max);
    if (ClearException(env) || condition_max <= 0 || condition >= condition_max) {
        return false;
    }

    env->CallVoidMethod(item, g_bindings.set_condition_no_sound, condition_max);
    if (ClearException(env)) return false;
    condition = condition_max;
    ++g_status.durability_restore_count;
    if (session_mode == PlayerResourceSessionMode::MultiplayerClient &&
        SendItemFields(env, player, item)) {
        ++g_status.server_sync_count;
    }
    return true;
}

}  // namespace

void UpdatePlayerResourceBridge() {
    g_status.endurance_recovery_enabled = g_endurance_recovery_requested;
    g_status.durability_protection_enabled = g_durability_protection_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.session_mode = PlayerResourceSessionMode::Unknown;
        g_status.message = "体力与耐久桥接尚未初始化";
        return;
    }

    g_status.initialized = true;
    g_status.session_mode = ReadSessionMode(env);
    const bool multiplayer =
        g_status.session_mode == PlayerResourceSessionMode::MultiplayerClient;
    UpdateEnduranceCompensationBridge(
        multiplayer && g_endurance_recovery_requested);
    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.endurance = 0.0f;
        g_status.primary_condition = 0;
        g_status.primary_condition_max = 0;
        g_status.secondary_condition = 0;
        g_status.secondary_condition_max = 0;
        g_status.message = "等待进入存档并创建玩家";
        return;
    }
    g_status.player_ready = true;

    jobject endurance_stat = env->GetStaticObjectField(
        g_bindings.character_stat, g_bindings.endurance_stat);
    jobject stats = env->CallObjectMethod(player, g_bindings.get_stats);
    if (endurance_stat != nullptr && stats != nullptr && !ClearException(env)) {
        const float maximum = env->CallFloatMethod(
            endurance_stat, g_bindings.stat_maximum);
        g_status.endurance = env->CallFloatMethod(
            stats, g_bindings.stats_get, endurance_stat);
        if (!ClearException(env)) {
            if (!multiplayer && g_endurance_recovery_requested) {
                if (g_status.endurance < maximum - 0.001f) {
                    const jboolean changed = env->CallBooleanMethod(
                        stats, g_bindings.stats_set, endurance_stat, maximum);
                    if (!ClearException(env) && changed == JNI_TRUE) {
                        g_status.endurance = maximum;
                        ++g_status.endurance_restore_count;
                    }
                }
                env->CallVoidMethod(
                    stats, g_bindings.stats_set_last_endurance, maximum);
                ClearException(env);
                jobject moodles = env->CallObjectMethod(
                    player, g_bindings.get_moodles);
                if (moodles != nullptr && !ClearException(env)) {
                    env->CallVoidMethod(moodles, g_bindings.moodles_update);
                    ClearException(env);
                }
                if (moodles != nullptr) env->DeleteLocalRef(moodles);
            }
        }
    } else {
        ClearException(env);
    }

    jobject primary = env->CallObjectMethod(player, g_bindings.get_primary_item);
    jobject secondary = env->CallObjectMethod(player, g_bindings.get_secondary_item);
    bool restored_item = false;
    if (!ClearException(env)) {
        if (g_durability_protection_requested) {
            restored_item |= RestoreItem(
                env, player, primary, g_status.session_mode,
                g_status.primary_condition, g_status.primary_condition_max);
            if (secondary != nullptr &&
                (primary == nullptr || env->IsSameObject(primary, secondary) != JNI_TRUE)) {
                restored_item |= RestoreItem(
                    env, player, secondary, g_status.session_mode,
                    g_status.secondary_condition, g_status.secondary_condition_max);
            } else {
                g_status.secondary_condition = g_status.primary_condition;
                g_status.secondary_condition_max = g_status.primary_condition_max;
            }
        } else {
            if (primary != nullptr) {
                g_status.primary_condition = env->CallIntMethod(
                    primary, g_bindings.get_condition);
                g_status.primary_condition_max = env->CallIntMethod(
                    primary, g_bindings.get_condition_max);
            } else {
                g_status.primary_condition = 0;
                g_status.primary_condition_max = 0;
            }
            if (secondary != nullptr) {
                g_status.secondary_condition = env->CallIntMethod(
                    secondary, g_bindings.get_condition);
                g_status.secondary_condition_max = env->CallIntMethod(
                    secondary, g_bindings.get_condition_max);
            } else {
                g_status.secondary_condition = 0;
                g_status.secondary_condition_max = 0;
            }
            ClearException(env);
        }
    }

    if (restored_item &&
        g_status.session_mode == PlayerResourceSessionMode::MultiplayerClient) {
        g_status.message = "已恢复手持装备耐久并提交 SyncItemFields";
    } else if (restored_item) {
        g_status.message = "已恢复手持装备耐久";
    } else if (multiplayer && g_endurance_recovery_requested) {
        g_status.message = GetEnduranceCompensationStatus().message;
    } else if (g_endurance_recovery_requested ||
               g_durability_protection_requested) {
        g_status.message = "体力与手持装备保护运行中";
    } else {
        g_status.message = "体力与耐久保护待命";
    }

    if (secondary != nullptr) env->DeleteLocalRef(secondary);
    if (primary != nullptr) env->DeleteLocalRef(primary);
    if (stats != nullptr) env->DeleteLocalRef(stats);
    if (endurance_stat != nullptr) env->DeleteLocalRef(endurance_stat);
    env->DeleteLocalRef(player);
}

const PlayerResourceStatus& GetPlayerResourceStatus() {
    return g_status;
}

void SetEnduranceRecoveryEnabled(bool enabled) {
    g_endurance_recovery_requested = enabled;
    g_status.endurance_recovery_enabled = enabled;
}

void SetDurabilityProtectionEnabled(bool enabled) {
    g_durability_protection_requested = enabled;
    g_status.durability_protection_enabled = enabled;
}

}  // namespace pztrainer::bridge
