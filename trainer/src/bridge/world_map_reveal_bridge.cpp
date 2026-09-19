#include "bridge/world_map_reveal_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

constexpr int kMaximumUiDepth = 16;

struct Bindings {
    bool ready = false;
    jclass ui_manager = nullptr;
    jclass ui_element = nullptr;
    jclass world_map = nullptr;
    jclass world_map_api = nullptr;
    jclass array_list = nullptr;
    jclass string_class = nullptr;
    jmethodID get_ui = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jfieldID controls = nullptr;
    jfieldID lua_table = nullptr;
    jmethodID table_get = nullptr;
    jmethodID is_really_visible = nullptr;
    jmethodID get_map_api = nullptr;
    jmethodID get_boolean = nullptr;
    jmethodID set_boolean = nullptr;
};

Bindings g_bindings;
WorldMapRevealStatus g_status;
bool g_enabled = false;
jobject g_overridden_map = nullptr;
bool g_original_hide_unvisited = true;

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
    g_bindings.string_class = LoadGlobalClass(env, "java/lang/String");
    if (g_bindings.ui_manager == nullptr || g_bindings.ui_element == nullptr ||
        g_bindings.world_map == nullptr ||
        g_bindings.world_map_api == nullptr ||
        g_bindings.array_list == nullptr || g_bindings.string_class == nullptr) {
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
    g_bindings.get_map_api = env->GetMethodID(
        g_bindings.world_map, "getAPI", "()Lzombie/worldMap/UIWorldMapV3;");
    g_bindings.get_boolean = env->GetMethodID(
        g_bindings.world_map_api, "getBoolean", "(Ljava/lang/String;)Z");
    g_bindings.set_boolean = env->GetMethodID(
        g_bindings.world_map_api, "setBoolean", "(Ljava/lang/String;Z)V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.get_ui != nullptr && g_bindings.list_size != nullptr &&
        g_bindings.list_get != nullptr && g_bindings.controls != nullptr &&
        g_bindings.lua_table != nullptr && g_bindings.table_get != nullptr &&
        g_bindings.is_really_visible != nullptr &&
        g_bindings.get_map_api != nullptr && g_bindings.get_boolean != nullptr &&
        g_bindings.set_boolean != nullptr;
    return g_bindings.ready;
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
        jobject element = env->CallObjectMethod(list, g_bindings.list_get, index);
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

bool SetHideUnvisited(JNIEnv* env, jobject map, bool hidden) {
    jobject api = env->CallObjectMethod(map, g_bindings.get_map_api);
    jstring option = env->NewStringUTF("HideUnvisited");
    if (api == nullptr || option == nullptr || ClearException(env)) {
        if (option != nullptr) env->DeleteLocalRef(option);
        if (api != nullptr) env->DeleteLocalRef(api);
        return false;
    }
    env->CallVoidMethod(api, g_bindings.set_boolean, option,
                        hidden ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(option);
    env->DeleteLocalRef(api);
    return !ClearException(env);
}

void RestoreMap(JNIEnv* env) {
    if (g_overridden_map == nullptr) return;
    SetHideUnvisited(env, g_overridden_map, g_original_hide_unvisited);
    env->DeleteGlobalRef(g_overridden_map);
    g_overridden_map = nullptr;
}

bool ApplyReveal(JNIEnv* env, jobject map) {
    if (g_overridden_map == nullptr ||
        env->IsSameObject(g_overridden_map, map) != JNI_TRUE) {
        RestoreMap(env);
        jobject api = env->CallObjectMethod(map, g_bindings.get_map_api);
        jstring option = env->NewStringUTF("HideUnvisited");
        if (api == nullptr || option == nullptr || ClearException(env)) {
            if (option != nullptr) env->DeleteLocalRef(option);
            if (api != nullptr) env->DeleteLocalRef(api);
            return false;
        }
        g_original_hide_unvisited = env->CallBooleanMethod(
            api, g_bindings.get_boolean, option) == JNI_TRUE;
        env->DeleteLocalRef(option);
        env->DeleteLocalRef(api);
        if (ClearException(env)) return false;
        g_overridden_map = env->NewGlobalRef(map);
        if (g_overridden_map == nullptr) return false;
    }
    return SetHideUnvisited(env, map, false);
}

}  // namespace

void UpdateWorldMapRevealBridge() {
    g_status.enabled = g_enabled;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.map_open = false;
        g_status.applied = false;
        g_status.message = "世界地图迷雾桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    if (!g_enabled) {
        RestoreMap(env);
        g_status.map_open = false;
        g_status.applied = false;
        g_status.message = "世界地图迷雾保持原样";
        return;
    }

    jobject ui = env->CallStaticObjectMethod(
        g_bindings.ui_manager, g_bindings.get_ui);
    jobject map = ui == nullptr || ClearException(env)
        ? nullptr : FindVisibleWorldMap(env, ui, 0);
    if (ui != nullptr) env->DeleteLocalRef(ui);
    g_status.map_open = map != nullptr;
    if (map == nullptr) {
        RestoreMap(env);
        g_status.applied = false;
        g_status.message = "请打开世界地图以解锁地图迷雾";
        return;
    }

    g_status.applied = ApplyReveal(env, map);
    g_status.message = g_status.applied
        ? "世界地图迷雾已在本地解除"
        : "解除世界地图迷雾失败";
    env->DeleteLocalRef(map);
}

const WorldMapRevealStatus& GetWorldMapRevealStatus() {
    return g_status;
}

void SetWorldMapRevealEnabled(bool enabled) {
    g_enabled = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
