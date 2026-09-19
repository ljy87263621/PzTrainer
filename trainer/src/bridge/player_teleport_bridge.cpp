#include "bridge/player_teleport_bridge.hpp"

#include <Windows.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

constexpr int kMaximumUiDepth = 16;
constexpr auto kMultiplayerTeleportCooldown = std::chrono::seconds(240);

struct Bindings {
    bool ready = false;
    jclass ui_manager = nullptr;
    jclass ui_element = nullptr;
    jclass world_map = nullptr;
    jclass world_map_api = nullptr;
    jclass array_list = nullptr;
    jclass double_class = nullptr;
    jclass string_class = nullptr;
    jclass mouse = nullptr;
    jclass iso_player = nullptr;
    jclass game_client = nullptr;
    jmethodID get_ui = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jfieldID controls = nullptr;
    jfieldID lua_table = nullptr;
    jmethodID table_get = nullptr;
    jmethodID is_really_visible = nullptr;
    jmethodID get_absolute_x = nullptr;
    jmethodID get_absolute_y = nullptr;
    jmethodID get_width = nullptr;
    jmethodID get_height = nullptr;
    jmethodID double_value = nullptr;
    jmethodID get_map_api = nullptr;
    jmethodID ui_to_world_x = nullptr;
    jmethodID ui_to_world_y = nullptr;
    jmethodID get_min_x = nullptr;
    jmethodID get_min_y = nullptr;
    jmethodID get_max_x = nullptr;
    jmethodID get_max_y = nullptr;
    jmethodID get_mouse_x = nullptr;
    jmethodID get_mouse_y = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_player_z = nullptr;
    jmethodID ensure_not_in_vehicle = nullptr;
    jmethodID set_x = nullptr;
    jmethodID set_y = nullptr;
    jmethodID set_z = nullptr;
    jmethodID set_last_x = nullptr;
    jmethodID set_last_y = nullptr;
    jmethodID set_last_z = nullptr;
    jmethodID ensure_on_tile = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID game_client_instance = nullptr;
    jmethodID send_player = nullptr;
};

Bindings g_bindings;
PlayerTeleportStatus g_status;
bool g_enabled = false;
bool g_left_mouse_was_down = false;
std::chrono::steady_clock::time_point g_cooldown_until{};

void UpdateCooldownStatus() {
    if (!g_status.cooldown_active) return;

    const auto remaining = g_cooldown_until - std::chrono::steady_clock::now();
    if (remaining <= std::chrono::steady_clock::duration::zero()) {
        g_status.cooldown_active = false;
        g_status.cooldown_remaining_seconds = 0;
        return;
    }

    const auto remaining_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
    g_status.cooldown_remaining_seconds = static_cast<std::uint32_t>(
        (remaining_milliseconds + 999) / 1000);
}

void BeginMultiplayerCooldown() {
    g_cooldown_until =
        std::chrono::steady_clock::now() + kMultiplayerTeleportCooldown;
    g_status.cooldown_active = true;
    g_status.cooldown_remaining_seconds =
        static_cast<std::uint32_t>(kMultiplayerTeleportCooldown.count());
}

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader_class, "getSystemClassLoader",
        "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass",
        "(Ljava/lang/String;)Ljava/lang/Class;");
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
    g_bindings.ui_manager = LoadGlobalClass(env, "zombie/ui/UIManager");
    g_bindings.ui_element = LoadGlobalClass(env, "zombie/ui/UIElement");
    g_bindings.world_map = LoadGlobalClass(env, "zombie/worldMap/UIWorldMap");
    g_bindings.world_map_api = LoadGlobalClass(
        env, "zombie/worldMap/UIWorldMapV3");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    g_bindings.double_class = LoadGlobalClass(env, "java/lang/Double");
    g_bindings.string_class = LoadGlobalClass(env, "java/lang/String");
    g_bindings.mouse = LoadGlobalClass(env, "zombie/input/Mouse");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    if (g_bindings.ui_manager == nullptr || g_bindings.ui_element == nullptr ||
        g_bindings.world_map == nullptr ||
        g_bindings.world_map_api == nullptr ||
        g_bindings.array_list == nullptr || g_bindings.double_class == nullptr ||
        g_bindings.string_class == nullptr ||
        g_bindings.mouse == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.game_client == nullptr) {
        return false;
    }

    g_bindings.get_ui = env->GetStaticMethodID(
        g_bindings.ui_manager, "getUI", "()Ljava/util/ArrayList;");
    g_bindings.list_size = env->GetMethodID(
        g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.controls = env->GetFieldID(
        g_bindings.ui_element, "controls", "Ljava/util/ArrayList;");
    g_bindings.lua_table = env->GetFieldID(
        g_bindings.ui_element, "table", "Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.table_get = env->GetStaticMethodID(
        g_bindings.ui_manager, "tableget",
        "(Lse/krka/kahlua/vm/KahluaTable;Ljava/lang/Object;)Ljava/lang/Object;");
    g_bindings.is_really_visible = env->GetMethodID(
        g_bindings.ui_element, "isReallyVisible", "()Z");
    g_bindings.get_absolute_x = env->GetMethodID(
        g_bindings.ui_element, "getAbsoluteX", "()Ljava/lang/Double;");
    g_bindings.get_absolute_y = env->GetMethodID(
        g_bindings.ui_element, "getAbsoluteY", "()Ljava/lang/Double;");
    g_bindings.get_width = env->GetMethodID(
        g_bindings.ui_element, "getWidth", "()Ljava/lang/Double;");
    g_bindings.get_height = env->GetMethodID(
        g_bindings.ui_element, "getHeight", "()Ljava/lang/Double;");
    g_bindings.double_value = env->GetMethodID(
        g_bindings.double_class, "doubleValue", "()D");
    g_bindings.get_map_api = env->GetMethodID(
        g_bindings.world_map, "getAPI", "()Lzombie/worldMap/UIWorldMapV3;");
    g_bindings.ui_to_world_x = env->GetMethodID(
        g_bindings.world_map_api, "uiToWorldX", "(FF)F");
    g_bindings.ui_to_world_y = env->GetMethodID(
        g_bindings.world_map_api, "uiToWorldY", "(FF)F");
    g_bindings.get_min_x = env->GetMethodID(
        g_bindings.world_map_api, "getMinXInSquares", "()I");
    g_bindings.get_min_y = env->GetMethodID(
        g_bindings.world_map_api, "getMinYInSquares", "()I");
    g_bindings.get_max_x = env->GetMethodID(
        g_bindings.world_map_api, "getMaxXInSquares", "()I");
    g_bindings.get_max_y = env->GetMethodID(
        g_bindings.world_map_api, "getMaxYInSquares", "()I");
    g_bindings.get_mouse_x = env->GetStaticMethodID(
        g_bindings.mouse, "getXA", "()I");
    g_bindings.get_mouse_y = env->GetStaticMethodID(
        g_bindings.mouse, "getYA", "()I");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance",
        "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_player_z = env->GetMethodID(
        g_bindings.iso_player, "getZ", "()F");
    g_bindings.ensure_not_in_vehicle = env->GetMethodID(
        g_bindings.iso_player, "ensureNotInVehicle", "()V");
    g_bindings.set_x = env->GetMethodID(g_bindings.iso_player, "setX", "(F)F");
    g_bindings.set_y = env->GetMethodID(g_bindings.iso_player, "setY", "(F)F");
    g_bindings.set_z = env->GetMethodID(g_bindings.iso_player, "setZ", "(F)F");
    g_bindings.set_last_x = env->GetMethodID(
        g_bindings.iso_player, "setLastX", "(F)F");
    g_bindings.set_last_y = env->GetMethodID(
        g_bindings.iso_player, "setLastY", "(F)F");
    g_bindings.set_last_z = env->GetMethodID(
        g_bindings.iso_player, "setLastZ", "(F)F");
    g_bindings.ensure_on_tile = env->GetMethodID(
        g_bindings.iso_player, "ensureOnTile", "()V");
    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.game_client_instance = env->GetStaticFieldID(
        g_bindings.game_client, "instance", "Lzombie/network/GameClient;");
    g_bindings.send_player = env->GetMethodID(
        g_bindings.game_client, "sendPlayer",
        "(Lzombie/characters/IsoPlayer;)V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.get_ui != nullptr && g_bindings.list_size != nullptr &&
        g_bindings.list_get != nullptr && g_bindings.controls != nullptr &&
        g_bindings.lua_table != nullptr && g_bindings.table_get != nullptr &&
        g_bindings.is_really_visible != nullptr &&
        g_bindings.get_absolute_x != nullptr &&
        g_bindings.get_absolute_y != nullptr &&
        g_bindings.get_width != nullptr && g_bindings.get_height != nullptr &&
        g_bindings.double_value != nullptr && g_bindings.get_map_api != nullptr &&
        g_bindings.ui_to_world_x != nullptr &&
        g_bindings.ui_to_world_y != nullptr && g_bindings.get_min_x != nullptr &&
        g_bindings.get_min_y != nullptr && g_bindings.get_max_x != nullptr &&
        g_bindings.get_max_y != nullptr && g_bindings.get_mouse_x != nullptr &&
        g_bindings.get_mouse_y != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_player_z != nullptr &&
        g_bindings.ensure_not_in_vehicle != nullptr &&
        g_bindings.set_x != nullptr && g_bindings.set_y != nullptr &&
        g_bindings.set_z != nullptr && g_bindings.set_last_x != nullptr &&
        g_bindings.set_last_y != nullptr && g_bindings.set_last_z != nullptr &&
        g_bindings.ensure_on_tile != nullptr && g_bindings.client_flag != nullptr &&
        g_bindings.game_client_instance != nullptr &&
        g_bindings.send_player != nullptr;
    return g_bindings.ready;
}

double ReadDouble(JNIEnv* env, jobject object, jmethodID method) {
    jobject boxed = env->CallObjectMethod(object, method);
    if (boxed == nullptr || ClearException(env)) {
        if (boxed != nullptr) env->DeleteLocalRef(boxed);
        return 0.0;
    }
    const double value = env->CallDoubleMethod(boxed, g_bindings.double_value);
    env->DeleteLocalRef(boxed);
    return ClearException(env) ? 0.0 : value;
}

bool IsMainWorldMap(JNIEnv* env, jobject element) {
    jobject table = env->GetObjectField(element, g_bindings.lua_table);
    if (table == nullptr || ClearException(env)) {
        if (table != nullptr) env->DeleteLocalRef(table);
        return false;
    }
    jstring key = env->NewStringUTF("Type");
    jobject type = key == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.ui_manager, g_bindings.table_get, table, key);
    if (key != nullptr) env->DeleteLocalRef(key);
    env->DeleteLocalRef(table);
    if (type == nullptr || ClearException(env) ||
        env->IsInstanceOf(type, g_bindings.string_class) != JNI_TRUE) {
        if (type != nullptr) env->DeleteLocalRef(type);
        return false;
    }

    const char* type_name = env->GetStringUTFChars(
        static_cast<jstring>(type), nullptr);
    const bool is_main_world_map = type_name != nullptr &&
        std::string(type_name) == "ISWorldMap";
    if (type_name != nullptr) {
        env->ReleaseStringUTFChars(static_cast<jstring>(type), type_name);
    }
    env->DeleteLocalRef(type);
    return !ClearException(env) && is_main_world_map;
}

jobject FindVisibleWorldMap(JNIEnv* env, jobject list, int depth) {
    if (list == nullptr || depth > kMaximumUiDepth) return nullptr;
    const jint count = env->CallIntMethod(list, g_bindings.list_size);
    if (ClearException(env)) return nullptr;
    for (jint index = count - 1; index >= 0; --index) {
        jobject element = env->CallObjectMethod(
            list, g_bindings.list_get, index);
        if (element == nullptr || ClearException(env)) {
            if (element != nullptr) env->DeleteLocalRef(element);
            continue;
        }
        if (env->IsInstanceOf(element, g_bindings.world_map) == JNI_TRUE &&
            env->CallBooleanMethod(
                element, g_bindings.is_really_visible) == JNI_TRUE &&
            !ClearException(env) && IsMainWorldMap(env, element)) {
            return element;
        }
        if (env->IsInstanceOf(element, g_bindings.ui_element) == JNI_TRUE) {
            jobject controls = env->GetObjectField(element, g_bindings.controls);
            if (controls != nullptr && !ClearException(env)) {
                jobject found = FindVisibleWorldMap(env, controls, depth + 1);
                env->DeleteLocalRef(controls);
                if (found != nullptr) {
                    env->DeleteLocalRef(element);
                    return found;
                }
            } else if (controls != nullptr) {
                env->DeleteLocalRef(controls);
            }
        }
        env->DeleteLocalRef(element);
    }
    return nullptr;
}

bool TeleportPlayer(JNIEnv* env, jobject player, float x, float y, float z,
                    bool multiplayer) {
    env->CallVoidMethod(player, g_bindings.ensure_not_in_vehicle);
    env->CallFloatMethod(player, g_bindings.set_x, x);
    env->CallFloatMethod(player, g_bindings.set_y, y);
    env->CallFloatMethod(player, g_bindings.set_z, z);
    env->CallFloatMethod(player, g_bindings.set_last_x, x);
    env->CallFloatMethod(player, g_bindings.set_last_y, y);
    env->CallFloatMethod(player, g_bindings.set_last_z, z);
    env->CallVoidMethod(player, g_bindings.ensure_on_tile);
    if (ClearException(env)) return false;
    if (!multiplayer) return true;

    jobject client = env->GetStaticObjectField(
        g_bindings.game_client, g_bindings.game_client_instance);
    if (client == nullptr || ClearException(env)) {
        if (client != nullptr) env->DeleteLocalRef(client);
        return false;
    }
    env->CallVoidMethod(client, g_bindings.send_player, player);
    env->DeleteLocalRef(client);
    return !ClearException(env);
}

}  // namespace

void UpdatePlayerTeleportBridge(bool input_blocked) {
    g_status.enabled = g_enabled;
    UpdateCooldownStatus();
    const bool left_mouse_down =
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool click_started = left_mouse_down && !g_left_mouse_was_down;
    g_left_mouse_was_down = left_mouse_down;

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.map_open = false;
        g_status.message = "地图传送桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) g_status.multiplayer = false;
    if (!g_status.multiplayer && g_status.cooldown_active) {
        g_cooldown_until = {};
        g_status.cooldown_active = false;
        g_status.cooldown_remaining_seconds = 0;
    }

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.map_open = false;
        g_status.message = "等待进入存档并创建玩家";
        return;
    }
    g_status.player_ready = true;

    if (!g_enabled) {
        g_status.map_open = false;
        g_status.message = "地图传送待命";
        env->DeleteLocalRef(player);
        return;
    }

    if (g_status.cooldown_active) {
        g_status.map_open = false;
        g_status.message = "地图传送冷却中";
        env->DeleteLocalRef(player);
        return;
    }

    jobject ui = env->CallStaticObjectMethod(
        g_bindings.ui_manager, g_bindings.get_ui);
    jobject map = ui == nullptr || ClearException(env)
        ? nullptr : FindVisibleWorldMap(env, ui, 0);
    if (ui != nullptr) env->DeleteLocalRef(ui);
    g_status.map_open = map != nullptr;
    if (map == nullptr) {
        g_status.message = "请打开世界地图";
        env->DeleteLocalRef(player);
        return;
    }

    const jint mouse_x = env->CallStaticIntMethod(
        g_bindings.mouse, g_bindings.get_mouse_x);
    const jint mouse_y = env->CallStaticIntMethod(
        g_bindings.mouse, g_bindings.get_mouse_y);
    const double absolute_x = ReadDouble(env, map, g_bindings.get_absolute_x);
    const double absolute_y = ReadDouble(env, map, g_bindings.get_absolute_y);
    const double width = ReadDouble(env, map, g_bindings.get_width);
    const double height = ReadDouble(env, map, g_bindings.get_height);
    const float local_x = static_cast<float>(mouse_x - absolute_x);
    const float local_y = static_cast<float>(mouse_y - absolute_y);
    const bool mouse_over_map = local_x >= 0.0f && local_y >= 0.0f &&
        local_x < width && local_y < height;
    const bool alt_down = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    if (input_blocked || !click_started || !alt_down || !mouse_over_map) {
        g_status.message = "在世界地图上按住 Alt 并点击鼠标左键";
        env->DeleteLocalRef(map);
        env->DeleteLocalRef(player);
        return;
    }

    jobject api = env->CallObjectMethod(map, g_bindings.get_map_api);
    if (api == nullptr || ClearException(env)) {
        if (api != nullptr) env->DeleteLocalRef(api);
        g_status.message = "无法读取世界地图坐标";
        env->DeleteLocalRef(map);
        env->DeleteLocalRef(player);
        return;
    }
    const float world_x = env->CallFloatMethod(
        api, g_bindings.ui_to_world_x, local_x, local_y);
    const float world_y = env->CallFloatMethod(
        api, g_bindings.ui_to_world_y, local_x, local_y);
    const jint min_x = env->CallIntMethod(api, g_bindings.get_min_x);
    const jint min_y = env->CallIntMethod(api, g_bindings.get_min_y);
    const jint max_x = env->CallIntMethod(api, g_bindings.get_max_x);
    const jint max_y = env->CallIntMethod(api, g_bindings.get_max_y);
    const float world_z = env->CallFloatMethod(
        player, g_bindings.get_player_z);
    const bool coordinate_error = ClearException(env) ||
        !std::isfinite(world_x) || !std::isfinite(world_y) ||
        world_x < static_cast<float>(min_x) ||
        world_y < static_cast<float>(min_y) ||
        world_x > static_cast<float>(max_x + 1) ||
        world_y > static_cast<float>(max_y + 1);
    env->DeleteLocalRef(api);
    env->DeleteLocalRef(map);
    if (coordinate_error) {
        g_status.message = "所选位置不在当前世界地图范围内";
        env->DeleteLocalRef(player);
        return;
    }

    const float target_x = std::floor(world_x) + 0.5f;
    const float target_y = std::floor(world_y) + 0.5f;
    const float target_z = std::floor(world_z);
    if (TeleportPlayer(
            env, player, target_x, target_y, target_z,
            g_status.multiplayer)) {
        g_status.last_world_x = target_x;
        g_status.last_world_y = target_y;
        g_status.last_world_z = target_z;
        ++g_status.teleport_count;
        if (g_status.multiplayer) BeginMultiplayerCooldown();
        g_status.message = g_status.multiplayer
            ? "已修改玩家位置并发送联机位置更新"
            : "已修改玩家位置";
    } else {
        g_status.message = "修改玩家位置失败";
    }
    env->DeleteLocalRef(player);
}

const PlayerTeleportStatus& GetPlayerTeleportStatus() {
    return g_status;
}

void SetPlayerTeleportEnabled(bool enabled) {
    g_enabled = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
