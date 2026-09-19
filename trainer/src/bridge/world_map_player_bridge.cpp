#include "bridge/world_map_player_bridge.hpp"

#include <jni.h>

#include <algorithm>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

constexpr jint kShowAllPlayers = 4;
constexpr int kMaximumPlayers = 512;

struct Bindings {
    bool ready = false;
    jclass server_options = nullptr;
    jclass integer_option = nullptr;
    jclass game_client = nullptr;
    jclass hash_map = nullptr;
    jclass collection = nullptr;
    jclass iterator = nullptr;
    jclass iso_player = nullptr;
    jclass remote_players = nullptr;
    jclass remote_player = nullptr;
    jmethodID get_server_options = nullptr;
    jfieldID map_player_visibility = nullptr;
    jmethodID option_get_value = nullptr;
    jmethodID option_set_value = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID client_player_map = nullptr;
    jmethodID map_values = nullptr;
    jmethodID collection_iterator = nullptr;
    jmethodID iterator_has_next = nullptr;
    jmethodID iterator_next = nullptr;
    jmethodID get_local_player = nullptr;
    jmethodID player_is_dead = nullptr;
    jfieldID remote_players_instance = nullptr;
    jmethodID get_or_create_remote_player = nullptr;
    jmethodID set_remote_player = nullptr;
};

Bindings g_bindings;
WorldMapPlayerStatus g_status;
bool g_requested = false;
jobject g_overridden_option = nullptr;
jint g_original_visibility = 1;

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
        ? nullptr
        : env->CallStaticObjectMethod(class_loader, get_system_loader);
    env->DeleteLocalRef(class_loader);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(
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
    g_bindings.server_options = LoadGlobalClass(
        env, "zombie/network/ServerOptions");
    g_bindings.integer_option = LoadGlobalClass(
        env, "zombie/config/IntegerConfigOption");
    g_bindings.game_client = LoadGlobalClass(
        env, "zombie/network/GameClient");
    g_bindings.hash_map = LoadGlobalClass(env, "java/util/HashMap");
    g_bindings.collection = LoadGlobalClass(env, "java/util/Collection");
    g_bindings.iterator = LoadGlobalClass(env, "java/util/Iterator");
    g_bindings.iso_player = LoadGlobalClass(
        env, "zombie/characters/IsoPlayer");
    g_bindings.remote_players = LoadGlobalClass(
        env, "zombie/worldMap/WorldMapRemotePlayers");
    g_bindings.remote_player = LoadGlobalClass(
        env, "zombie/worldMap/WorldMapRemotePlayer");
    if (g_bindings.server_options == nullptr ||
        g_bindings.integer_option == nullptr ||
        g_bindings.game_client == nullptr || g_bindings.hash_map == nullptr ||
        g_bindings.collection == nullptr || g_bindings.iterator == nullptr ||
        g_bindings.iso_player == nullptr ||
        g_bindings.remote_players == nullptr ||
        g_bindings.remote_player == nullptr) {
        return false;
    }

    g_bindings.get_server_options = env->GetStaticMethodID(
        g_bindings.server_options, "getInstance",
        "()Lzombie/network/ServerOptions;");
    g_bindings.map_player_visibility = env->GetFieldID(
        g_bindings.server_options, "mapRemotePlayerVisibility",
        "Lzombie/network/ServerOptions$IntegerServerOption;");
    g_bindings.option_get_value = env->GetMethodID(
        g_bindings.integer_option, "getValue", "()I");
    g_bindings.option_set_value = env->GetMethodID(
        g_bindings.integer_option, "setValue", "(I)V");
    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.client_player_map = env->GetStaticFieldID(
        g_bindings.game_client, "IDToPlayerMap", "Ljava/util/HashMap;");
    g_bindings.map_values = env->GetMethodID(
        g_bindings.hash_map, "values", "()Ljava/util/Collection;");
    g_bindings.collection_iterator = env->GetMethodID(
        g_bindings.collection, "iterator", "()Ljava/util/Iterator;");
    g_bindings.iterator_has_next = env->GetMethodID(
        g_bindings.iterator, "hasNext", "()Z");
    g_bindings.iterator_next = env->GetMethodID(
        g_bindings.iterator, "next", "()Ljava/lang/Object;");
    g_bindings.get_local_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance",
        "()Lzombie/characters/IsoPlayer;");
    g_bindings.player_is_dead = env->GetMethodID(
        g_bindings.iso_player, "isDead", "()Z");
    g_bindings.remote_players_instance = env->GetStaticFieldID(
        g_bindings.remote_players, "instance",
        "Lzombie/worldMap/WorldMapRemotePlayers;");
    g_bindings.get_or_create_remote_player = env->GetMethodID(
        g_bindings.remote_players, "getOrCreatePlayer",
        "(Lzombie/characters/IsoPlayer;)"
        "Lzombie/worldMap/WorldMapRemotePlayer;");
    g_bindings.set_remote_player = env->GetMethodID(
        g_bindings.remote_player, "setPlayer",
        "(Lzombie/characters/IsoPlayer;)V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.get_server_options != nullptr &&
        g_bindings.map_player_visibility != nullptr &&
        g_bindings.option_get_value != nullptr &&
        g_bindings.option_set_value != nullptr &&
        g_bindings.client_flag != nullptr &&
        g_bindings.client_player_map != nullptr &&
        g_bindings.map_values != nullptr &&
        g_bindings.collection_iterator != nullptr &&
        g_bindings.iterator_has_next != nullptr &&
        g_bindings.iterator_next != nullptr &&
        g_bindings.get_local_player != nullptr &&
        g_bindings.player_is_dead != nullptr &&
        g_bindings.remote_players_instance != nullptr &&
        g_bindings.get_or_create_remote_player != nullptr &&
        g_bindings.set_remote_player != nullptr;
    return g_bindings.ready;
}

void RestoreVisibility(JNIEnv* env) {
    if (g_overridden_option == nullptr) return;
    env->CallVoidMethod(
        g_overridden_option, g_bindings.option_set_value,
        g_original_visibility);
    ClearException(env);
    env->DeleteGlobalRef(g_overridden_option);
    g_overridden_option = nullptr;
}

bool ApplyVisibility(JNIEnv* env, jobject option) {
    if (option == nullptr) return false;
    if (g_overridden_option != nullptr &&
        env->IsSameObject(g_overridden_option, option) != JNI_TRUE) {
        RestoreVisibility(env);
    }
    if (g_overridden_option == nullptr) {
        g_original_visibility = env->CallIntMethod(
            option, g_bindings.option_get_value);
        if (ClearException(env)) return false;
        g_overridden_option = env->NewGlobalRef(option);
        if (g_overridden_option == nullptr || ClearException(env)) return false;
    }
    env->CallVoidMethod(
        option, g_bindings.option_set_value, kShowAllPlayers);
    return !ClearException(env);
}

std::size_t PublishSynchronizedPlayers(JNIEnv* env) {
    jobject local_player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_local_player);
    jobject player_map = env->GetStaticObjectField(
        g_bindings.game_client, g_bindings.client_player_map);
    jobject remote_players = env->GetStaticObjectField(
        g_bindings.remote_players, g_bindings.remote_players_instance);
    if (ClearException(env) || local_player == nullptr ||
        player_map == nullptr || remote_players == nullptr) {
        if (remote_players != nullptr) env->DeleteLocalRef(remote_players);
        if (player_map != nullptr) env->DeleteLocalRef(player_map);
        if (local_player != nullptr) env->DeleteLocalRef(local_player);
        return 0;
    }

    jobject values = env->CallObjectMethod(player_map, g_bindings.map_values);
    jobject iterator = values == nullptr
        ? nullptr
        : env->CallObjectMethod(values, g_bindings.collection_iterator);
    if (ClearException(env)) {
        if (iterator != nullptr) env->DeleteLocalRef(iterator);
        iterator = nullptr;
    }

    std::size_t inspected = 0;
    std::size_t published = 0;
    while (iterator != nullptr && inspected < kMaximumPlayers &&
           env->CallBooleanMethod(
               iterator, g_bindings.iterator_has_next) == JNI_TRUE) {
        ++inspected;
        jobject player = env->CallObjectMethod(
            iterator, g_bindings.iterator_next);
        if (player == nullptr || ClearException(env)) {
            if (player != nullptr) env->DeleteLocalRef(player);
            continue;
        }
        const bool is_local =
            env->IsSameObject(player, local_player) == JNI_TRUE;
        const bool is_dead = !is_local &&
            env->CallBooleanMethod(
                player, g_bindings.player_is_dead) == JNI_TRUE;
        if (!ClearException(env) && !is_local && !is_dead) {
            jobject remote_player = env->CallObjectMethod(
                remote_players, g_bindings.get_or_create_remote_player,
                player);
            if (remote_player != nullptr && !ClearException(env)) {
                env->CallVoidMethod(
                    remote_player, g_bindings.set_remote_player, player);
                if (!ClearException(env)) ++published;
            }
            if (remote_player != nullptr) env->DeleteLocalRef(remote_player);
        }
        env->DeleteLocalRef(player);
    }

    if (iterator != nullptr) env->DeleteLocalRef(iterator);
    if (values != nullptr) env->DeleteLocalRef(values);
    env->DeleteLocalRef(remote_players);
    env->DeleteLocalRef(player_map);
    env->DeleteLocalRef(local_player);
    return published;
}

}  // namespace

void UpdateWorldMapPlayerBridge() {
    g_status.enabled = g_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.applied = false;
        g_status.multiplayer = false;
        g_status.synchronized_players = 0;
        g_status.message = "地图玩家标记桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) g_status.multiplayer = false;

    if (!g_requested || !g_status.multiplayer) {
        RestoreVisibility(env);
        g_status.applied = false;
        g_status.synchronized_players = 0;
        g_status.message = g_requested
            ? "进入联机服务器后显示地图玩家标记"
            : "地图玩家标记待命";
        return;
    }

    jobject options = env->CallStaticObjectMethod(
        g_bindings.server_options, g_bindings.get_server_options);
    jobject visibility = options == nullptr
        ? nullptr
        : env->GetObjectField(options, g_bindings.map_player_visibility);
    const bool applied = visibility != nullptr &&
        !ClearException(env) && ApplyVisibility(env, visibility);
    if (visibility != nullptr) env->DeleteLocalRef(visibility);
    if (options != nullptr) env->DeleteLocalRef(options);
    if (!applied) {
        g_status.applied = false;
        g_status.synchronized_players = 0;
        g_status.message = "无法启用原版地图玩家标记";
        return;
    }

    g_status.synchronized_players = PublishSynchronizedPlayers(env);
    g_status.applied = true;
    g_status.message = g_status.synchronized_players == 0
        ? "已启用，等待服务器同步其他玩家"
        : "已使用原版地图标记显示已同步玩家";
}

const WorldMapPlayerStatus& GetWorldMapPlayerStatus() {
    return g_status;
}

void SetWorldMapPlayerEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
