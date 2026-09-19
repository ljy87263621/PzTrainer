#include "bridge/trap_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <cmath>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass c_global_objects = nullptr;
    jclass global_object_system = nullptr;
    jclass global_object = nullptr;
    jclass lua_manager = nullptr;
    jclass platform = nullptr;
    jclass kahlua_table = nullptr;
    jclass inventory_item_factory = nullptr;
    jclass food = nullptr;
    jclass double_class = nullptr;
    jclass boolean_class = nullptr;
    jfieldID lua_platform = nullptr;
    jmethodID get_system_by_name = nullptr;
    jmethodID get_object_count = nullptr;
    jmethodID get_object_by_index = nullptr;
    jmethodID get_x = nullptr;
    jmethodID get_y = nullptr;
    jmethodID get_z = nullptr;
    jmethodID new_table = nullptr;
    jmethodID rawset = nullptr;
    jmethodID create_item = nullptr;
    jmethodID food_base_hunger = nullptr;
    jmethodID double_value_of = nullptr;
    jmethodID boolean_value_of = nullptr;
};

Bindings g_bindings;

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

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemAnimalTrapBindings");
    g_bindings.c_global_objects = LoadGlobalClass(
        env, "zombie/globalObjects/CGlobalObjects");
    g_bindings.global_object_system = LoadGlobalClass(
        env, "zombie/globalObjects/GlobalObjectSystem");
    g_bindings.global_object = LoadGlobalClass(
        env, "zombie/globalObjects/GlobalObject");
    g_bindings.lua_manager = LoadGlobalClass(env, "zombie/Lua/LuaManager");
    g_bindings.platform = LoadGlobalClass(env, "se/krka/kahlua/j2se/J2SEPlatform");
    g_bindings.kahlua_table = LoadGlobalClass(env, "se/krka/kahlua/vm/KahluaTable");
    g_bindings.inventory_item_factory = LoadGlobalClass(
        env, "zombie/inventory/InventoryItemFactory");
    g_bindings.food = LoadGlobalClass(env, "zombie/inventory/types/Food");
    g_bindings.double_class = LoadGlobalClass(env, "java/lang/Double");
    g_bindings.boolean_class = LoadGlobalClass(env, "java/lang/Boolean");
    if (g_bindings.c_global_objects == nullptr ||
        g_bindings.global_object_system == nullptr ||
        g_bindings.global_object == nullptr || g_bindings.lua_manager == nullptr ||
        g_bindings.platform == nullptr || g_bindings.kahlua_table == nullptr ||
        g_bindings.inventory_item_factory == nullptr || g_bindings.food == nullptr ||
        g_bindings.double_class == nullptr || g_bindings.boolean_class == nullptr) {
        return false;
    }

    g_bindings.lua_platform = env->GetStaticFieldID(
        g_bindings.lua_manager, "platform", "Lse/krka/kahlua/j2se/J2SEPlatform;");
    g_bindings.get_system_by_name = env->GetStaticMethodID(
        g_bindings.c_global_objects, "getSystemByName",
        "(Ljava/lang/String;)Lzombie/globalObjects/CGlobalObjectSystem;");
    g_bindings.get_object_count = env->GetMethodID(
        g_bindings.global_object_system, "getObjectCount", "()I");
    g_bindings.get_object_by_index = env->GetMethodID(
        g_bindings.global_object_system, "getObjectByIndex",
        "(I)Lzombie/globalObjects/GlobalObject;");
    g_bindings.get_x = env->GetMethodID(g_bindings.global_object, "getX", "()I");
    g_bindings.get_y = env->GetMethodID(g_bindings.global_object, "getY", "()I");
    g_bindings.get_z = env->GetMethodID(g_bindings.global_object, "getZ", "()I");
    g_bindings.new_table = env->GetMethodID(
        g_bindings.platform, "newTable", "()Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.rawset = env->GetMethodID(
        g_bindings.kahlua_table, "rawset", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    g_bindings.create_item = env->GetStaticMethodID(
        g_bindings.inventory_item_factory, "CreateItem",
        "(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
    g_bindings.food_base_hunger = env->GetMethodID(
        g_bindings.food, "getBaseHunger", "()F");
    g_bindings.double_value_of = env->GetStaticMethodID(
        g_bindings.double_class, "valueOf", "(D)Ljava/lang/Double;");
    g_bindings.boolean_value_of = env->GetStaticMethodID(
        g_bindings.boolean_class, "valueOf", "(Z)Ljava/lang/Boolean;");
    g_bindings.ready = !ClearException(env) && g_bindings.lua_platform != nullptr &&
        g_bindings.get_system_by_name != nullptr &&
        g_bindings.get_object_count != nullptr &&
        g_bindings.get_object_by_index != nullptr && g_bindings.get_x != nullptr &&
        g_bindings.get_y != nullptr && g_bindings.get_z != nullptr &&
        g_bindings.new_table != nullptr && g_bindings.rawset != nullptr &&
        g_bindings.create_item != nullptr && g_bindings.food_base_hunger != nullptr &&
        g_bindings.double_value_of != nullptr && g_bindings.boolean_value_of != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

void DeleteLocalRef(JNIEnv* env, jobject value) {
    if (value != nullptr) env->DeleteLocalRef(value);
}

jobject BoxDouble(JNIEnv* env, double value) {
    jobject result = env->CallStaticObjectMethod(
        g_bindings.double_class, g_bindings.double_value_of, value);
    if (ClearException(env)) {
        DeleteLocalRef(env, result);
        return nullptr;
    }
    return result;
}

jobject BoxBoolean(JNIEnv* env, bool value) {
    jobject result = env->CallStaticObjectMethod(
        g_bindings.boolean_class, g_bindings.boolean_value_of,
        value ? JNI_TRUE : JNI_FALSE);
    if (ClearException(env)) {
        DeleteLocalRef(env, result);
        return nullptr;
    }
    return result;
}

bool TableSet(JNIEnv* env, jobject table, const char* key, jobject value) {
    if (table == nullptr || value == nullptr) return false;
    jstring java_key = env->NewStringUTF(key);
    if (java_key == nullptr || ClearException(env)) {
        DeleteLocalRef(env, java_key);
        return false;
    }
    env->CallVoidMethod(table, g_bindings.rawset, java_key, value);
    env->DeleteLocalRef(java_key);
    return !ClearException(env);
}

bool TableSetString(JNIEnv* env, jobject table, const char* key,
                    const std::string& value) {
    jstring java_value = env->NewStringUTF(value.c_str());
    const bool succeeded = java_value != nullptr &&
        TableSet(env, table, key, java_value);
    DeleteLocalRef(env, java_value);
    return succeeded;
}

bool TableSetNumber(JNIEnv* env, jobject table, const char* key, double value) {
    jobject boxed = BoxDouble(env, value);
    const bool succeeded = boxed != nullptr && TableSet(env, table, key, boxed);
    DeleteLocalRef(env, boxed);
    return succeeded;
}

}  // namespace

int SendTrapAnimalFoodRequests(JNIEnv* env, jobject player,
                               const std::string& full_type, int quantity,
                               std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "陷阱物品桥接尚未初始化";
        return 0;
    }

    jstring item_type = env->NewStringUTF(full_type.c_str());
    jobject sample = item_type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.inventory_item_factory, g_bindings.create_item, item_type);
    if (sample == nullptr || ClearException(env) ||
        !env->IsInstanceOf(sample, g_bindings.food)) {
        DeleteLocalRef(env, sample);
        DeleteLocalRef(env, item_type);
        detail = "陷阱猎物链只接受 Food 类物品";
        return 0;
    }
    const float base_hunger = env->CallFloatMethod(sample, g_bindings.food_base_hunger);
    DeleteLocalRef(env, sample);
    DeleteLocalRef(env, item_type);
    if (ClearException(env) || base_hunger >= -0.001f) {
        detail = "该 Food 物品没有可用的负饥饿值，服务端猎物脚本会除零";
        return 0;
    }

    jstring system_name = env->NewStringUTF("trap");
    jobject system = system_name == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.c_global_objects, g_bindings.get_system_by_name, system_name);
    DeleteLocalRef(env, system_name);
    if (system == nullptr || ClearException(env)) {
        DeleteLocalRef(env, system);
        detail = "客户端陷阱系统尚未加载";
        return 0;
    }
    const jint object_count = env->CallIntMethod(system, g_bindings.get_object_count);
    if (ClearException(env) || object_count <= 0) {
        DeleteLocalRef(env, system);
        detail = "当前世界没有可用陷阱；先正常放置任意陷阱";
        return 0;
    }
    jobject trap = env->CallObjectMethod(system, g_bindings.get_object_by_index, 0);
    if (trap == nullptr || ClearException(env)) {
        DeleteLocalRef(env, trap);
        DeleteLocalRef(env, system);
        detail = "无法读取陷阱坐标";
        return 0;
    }
    const jint x = env->CallIntMethod(trap, g_bindings.get_x);
    const jint y = env->CallIntMethod(trap, g_bindings.get_y);
    const jint z = env->CallIntMethod(trap, g_bindings.get_z);
    DeleteLocalRef(env, trap);
    if (ClearException(env)) {
        DeleteLocalRef(env, system);
        detail = "无法读取陷阱坐标";
        return 0;
    }

    jobject platform = env->GetStaticObjectField(g_bindings.lua_manager,
                                                  g_bindings.lua_platform);
    jobject args = platform == nullptr
        ? nullptr
        : env->CallObjectMethod(platform, g_bindings.new_table);
    jobject animal = platform == nullptr
        ? nullptr
        : env->CallObjectMethod(platform, g_bindings.new_table);
    DeleteLocalRef(env, platform);
    const int typical_size = std::max(1, static_cast<int>(std::lround(-base_hunger * 100.0f)));
    jobject can_be_alive = BoxBoolean(env, false);
    const bool table_ready = !ClearException(env) && args != nullptr && animal != nullptr &&
        TableSetNumber(env, args, "x", x) && TableSetNumber(env, args, "y", y) &&
        TableSetNumber(env, args, "z", z) &&
        TableSetString(env, animal, "type", "pztrainer") &&
        TableSetString(env, animal, "item", full_type) &&
        TableSetNumber(env, animal, "minSize", typical_size) &&
        TableSetNumber(env, animal, "maxSize", typical_size + 1) &&
        TableSet(env, animal, "canBeAlive", can_be_alive) &&
        TableSet(env, args, "animal", animal);
    DeleteLocalRef(env, can_be_alive);
    DeleteLocalRef(env, animal);
    if (!table_ready) {
        DeleteLocalRef(env, args);
        DeleteLocalRef(env, system);
        detail = "无法构造陷阱猎物参数";
        return 0;
    }

    jstring add_command = env->NewStringUTF("addAnimalDebug");
    jstring remove_command = env->NewStringUTF("removeAnimal");
    int sent = 0;
    for (int index = 0; index < quantity; ++index) {
        bool added = false;
        bool removed = false;
        InvokeObjectMethodOnMainThread(
            env, system, "sendCommand", {add_command, player, args}, &added);
        InvokeObjectMethodOnMainThread(
            env, system, "sendCommand", {remove_command, player, args}, &removed);
        if (!added || !removed || ClearException(env)) break;
        ++sent;
    }
    DeleteLocalRef(env, add_command);
    DeleteLocalRef(env, remove_command);
    DeleteLocalRef(env, args);
    DeleteLocalRef(env, system);
    detail = sent > 0
        ? "已发送 " + std::to_string(sent) +
              " 组陷阱猎物请求；物品由服务端创建并回发"
        : "陷阱猎物请求发送失败";
    return sent;
}

}  // namespace pztrainer::bridge
