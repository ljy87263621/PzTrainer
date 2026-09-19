#include "bridge/lua_ui_bridge.hpp"

#include <Windows.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "runtime/lua_bridge_resource.hpp"
#include "settings/ui_preferences.hpp"
#include "bridge/item_bridge.hpp"
#include "bridge/lua_feature_registry.hpp"
#include "ui/components.hpp"
#include "ui/lua_item_multiselect.hpp"
#include "ui/theme.hpp"

namespace pztrainer::bridge {
namespace {

constexpr std::int64_t kVersion = 2;
constexpr std::int64_t kVersionFunction = 1;
constexpr std::int64_t kCategoryFunction = 2;
constexpr std::int64_t kWindowFunction = 3;
constexpr std::int64_t kIconFunction = 4;
constexpr std::int64_t kItemsCatalogFunction = 5;
constexpr std::int64_t kCheatListFunction = 6;
constexpr std::int64_t kCheatGetFunction = 7;
constexpr std::int64_t kCheatSetFunction = 8;
constexpr std::int64_t kCategoryToggle = 0x100;
constexpr std::int64_t kCategorySlider = 0x101;
constexpr std::int64_t kCategoryAimMode = 0x102;
constexpr std::int64_t kCategoryInput = 0x103;
constexpr std::int64_t kCategoryColor = 0x104;
constexpr std::int64_t kCategoryItemMultiSelect = 0x105;
constexpr std::int64_t kIconLine = 0x200;
constexpr std::int64_t kIconRectangle = 0x201;
constexpr std::int64_t kIconCircle = 0x202;
constexpr std::int64_t kIconMethodNamespace = 0x4000000000000000LL;
constexpr const char* kCategoryIdKey = "__pzsa_category_id";
constexpr const char* kIconIdKey = "__pzsa_icon_id";

struct LuaUiIconDefinition {
    std::size_t id = 0;
    std::string owner_script;
    std::vector<LuaUiIconPrimitive> primitives;
};

struct Bindings {
    bool registered = false;
    bool native_registered = false;
    jclass lua_manager = nullptr;
    jclass platform = nullptr;
    jclass kahlua_table = nullptr;
    jclass object = nullptr;
    jclass string = nullptr;
    jclass number = nullptr;
    jclass double_class = nullptr;
    jclass boolean = nullptr;
    jclass bridge_class = nullptr;
    jclass utf8_reader_class = nullptr;
    jobject platform_instance = nullptr;
    jobject environment = nullptr;
    jfieldID lua_platform = nullptr;
    jfieldID lua_environment = nullptr;
    jmethodID new_table = nullptr;
    jmethodID rawset = nullptr;
    jmethodID rawget = nullptr;
    jmethodID boolean_value = nullptr;
    jmethodID number_value = nullptr;
    jmethodID bridge_constructor = nullptr;
    jmethodID utf8_reader_constructor = nullptr;
};

Bindings g_bindings;
std::vector<LuaUiCategory> g_categories;
std::vector<LuaUiIconDefinition> g_icons;
std::mutex g_mutex;
std::string g_error;
std::size_t g_selected_category = 0;
std::size_t g_next_category_id = 0;
std::size_t g_next_icon_id = 0;
std::string g_registration_owner;

bool ClearException(JNIEnv* env) {
    if (env == nullptr || !env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* name) {
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
    std::string dotted(name);
    std::replace(dotted.begin(), dotted.end(), '/', '.');
    jstring java_name = env->NewStringUTF(dotted.c_str());
    jclass local = java_name == nullptr ? nullptr : static_cast<jclass>(
        env->CallObjectMethod(loader, load_class, java_name));
    if (java_name != nullptr) env->DeleteLocalRef(java_name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

std::string StringValue(JNIEnv* env, jobject object) {
    if (object == nullptr || !env->IsInstanceOf(object, g_bindings.string)) return {};
    const jstring string = static_cast<jstring>(object);
    const jsize length = env->GetStringLength(string);
    const jchar* characters = env->GetStringChars(string, nullptr);
    if (characters == nullptr || ClearException(env)) return {};
    const int utf8_length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS,
        reinterpret_cast<LPCWCH>(characters), length,
        nullptr, 0, nullptr, nullptr);
    std::string result;
    if (utf8_length > 0) {
        result.resize(static_cast<std::size_t>(utf8_length));
        WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            reinterpret_cast<LPCWCH>(characters), length,
            result.data(), utf8_length, nullptr, nullptr);
    }
    env->ReleaseStringChars(string, characters);
    return result;
}

jstring JavaStringFromUtf8(JNIEnv* env, const std::string& value) {
    if (value.empty()) return env->NewString(nullptr, 0);
    const int utf16_length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (utf16_length <= 0) return nullptr;
    std::wstring utf16(static_cast<std::size_t>(utf16_length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), utf16.data(), utf16_length) !=
        utf16_length) {
        return nullptr;
    }
    return env->NewString(
        reinterpret_cast<const jchar*>(utf16.data()), utf16_length);
}

double NumberValue(JNIEnv* env, jobject object, double fallback) {
    if (object == nullptr || !env->IsInstanceOf(object, g_bindings.number)) return fallback;
    const double value = env->CallDoubleMethod(object, g_bindings.number_value);
    return ClearException(env) ? fallback : value;
}

bool BoolValue(JNIEnv* env, jobject object, bool fallback) {
    if (object == nullptr || !env->IsInstanceOf(object, g_bindings.boolean)) return fallback;
    const jboolean value = env->CallBooleanMethod(object, g_bindings.boolean_value);
    return ClearException(env) ? fallback : value == JNI_TRUE;
}

jobject BoxBoolean(JNIEnv* env, bool value) {
    return env->CallStaticObjectMethod(
        g_bindings.boolean, env->GetStaticMethodID(
            g_bindings.boolean, "valueOf", "(Z)Ljava/lang/Boolean;"),
        value ? JNI_TRUE : JNI_FALSE);
}

jobject BoxDouble(JNIEnv* env, double value) {
    return env->CallStaticObjectMethod(
        g_bindings.double_class, env->GetStaticMethodID(
            g_bindings.double_class, "valueOf", "(D)Ljava/lang/Double;"), value);
}

bool TableSet(JNIEnv* env, jobject table, const char* key, jobject value) {
    if (table == nullptr || value == nullptr) return false;
    jstring java_key = env->NewStringUTF(key);
    if (java_key == nullptr || ClearException(env)) {
        if (java_key != nullptr) env->DeleteLocalRef(java_key);
        return false;
    }
    env->CallVoidMethod(table, g_bindings.rawset, java_key, value);
    env->DeleteLocalRef(java_key);
    return !ClearException(env);
}

bool TableSet(JNIEnv* env, jobject table, jobject key, jobject value) {
    if (table == nullptr || key == nullptr || value == nullptr) return false;
    env->CallVoidMethod(table, g_bindings.rawset, key, value);
    return !ClearException(env);
}

bool TableSetString(
    JNIEnv* env, jobject table, const char* key, const std::string& value) {
    jstring string = JavaStringFromUtf8(env, value);
    const bool result = string != nullptr && TableSet(env, table, key, string);
    if (string != nullptr) env->DeleteLocalRef(string);
    return result;
}

bool TableSetBoolean(JNIEnv* env, jobject table, const char* key, bool value) {
    jobject boxed = BoxBoolean(env, value);
    const bool result = boxed != nullptr && TableSet(env, table, key, boxed);
    if (boxed != nullptr) env->DeleteLocalRef(boxed);
    return result;
}

bool TableSetNumber(JNIEnv* env, jobject table, const char* key, double value) {
    jobject boxed = BoxDouble(env, value);
    const bool result = boxed != nullptr && TableSet(env, table, key, boxed);
    if (boxed != nullptr) env->DeleteLocalRef(boxed);
    return result;
}

jobject FeatureValueObject(JNIEnv* env, const LuaFeatureValue& value) {
    switch (value.type) {
        case LuaFeatureValueType::Boolean:
            return BoxBoolean(env, value.boolean);
        case LuaFeatureValueType::Number:
            return BoxDouble(env, value.number);
        case LuaFeatureValueType::Integer:
            return BoxDouble(env, static_cast<double>(value.integer));
        case LuaFeatureValueType::Enumeration:
            return JavaStringFromUtf8(env, value.enumeration);
        case LuaFeatureValueType::Color: {
            jobject table = env->CallObjectMethod(
                g_bindings.platform_instance, g_bindings.new_table);
            if (table == nullptr || ClearException(env)) return nullptr;
            constexpr std::array<const char*, 4> keys{"r", "g", "b", "a"};
            for (std::size_t index = 0; index < keys.size(); ++index) {
                TableSetNumber(env, table, keys[index], value.color[index]);
            }
            return table;
        }
    }
    return nullptr;
}

jobject FeatureDescriptorTable(
    JNIEnv* env, const LuaFeatureDescriptor& descriptor) {
    jobject table = env->CallObjectMethod(
        g_bindings.platform_instance, g_bindings.new_table);
    if (table == nullptr || ClearException(env)) return nullptr;
    TableSetString(env, table, "path", descriptor.path);
    TableSetString(env, table, "group", descriptor.group);
    TableSetString(env, table, "label", descriptor.label);
    TableSetString(
        env, table, "type", LuaFeatureValueTypeName(descriptor.type));
    TableSetBoolean(env, table, "writable", descriptor.writable);
    TableSetBoolean(env, table, "available", descriptor.available);
    TableSetBoolean(env, table, "pending", descriptor.pending);
    TableSetString(env, table, "reason", descriptor.reason);
    if (descriptor.type == LuaFeatureValueType::Number ||
        descriptor.type == LuaFeatureValueType::Integer) {
        TableSetNumber(env, table, "minimum", descriptor.minimum);
        TableSetNumber(env, table, "maximum", descriptor.maximum);
        TableSetNumber(env, table, "step", descriptor.step);
    }
    jobject value = FeatureValueObject(env, descriptor.value);
    if (value != nullptr) {
        TableSet(env, table, "value", value);
        env->DeleteLocalRef(value);
    }
    if (!descriptor.options.empty()) {
        jobject options = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (options != nullptr && !ClearException(env)) {
            for (std::size_t index = 0; index < descriptor.options.size(); ++index) {
                jobject key = BoxDouble(env, static_cast<double>(index + 1));
                jstring option = JavaStringFromUtf8(
                    env, descriptor.options[index]);
                if (key != nullptr && option != nullptr) {
                    TableSet(env, options, key, option);
                }
                if (option != nullptr) env->DeleteLocalRef(option);
                if (key != nullptr) env->DeleteLocalRef(key);
            }
            TableSet(env, table, "options", options);
            env->DeleteLocalRef(options);
        }
    }
    return table;
}

jobject FeatureOperationResult(
    JNIEnv* env, bool ok, bool pending, const std::string& reason,
    const LuaFeatureDescriptor* feature) {
    jobject table = env->CallObjectMethod(
        g_bindings.platform_instance, g_bindings.new_table);
    if (table == nullptr || ClearException(env)) return nullptr;
    TableSetBoolean(env, table, "ok", ok);
    TableSetBoolean(env, table, "pending", pending);
    TableSetString(env, table, "reason", reason);
    if (feature != nullptr) {
        jobject descriptor = FeatureDescriptorTable(env, *feature);
        if (descriptor != nullptr) {
            TableSet(env, table, "feature", descriptor);
            env->DeleteLocalRef(descriptor);
        }
    }
    return table;
}

jobject TableGet(JNIEnv* env, jobject table, const char* key);

bool ParseFeatureValue(
    JNIEnv* env, jobject object, const LuaFeatureDescriptor& descriptor,
    LuaFeatureValue& value, std::string& error) {
    value.type = descriptor.type;
    if (descriptor.type == LuaFeatureValueType::Boolean) {
        if (object == nullptr ||
            !env->IsInstanceOf(object, g_bindings.boolean)) {
            error = "Expected a boolean value.";
            return false;
        }
        value.boolean = BoolValue(env, object, false);
        return true;
    }
    if (descriptor.type == LuaFeatureValueType::Number ||
        descriptor.type == LuaFeatureValueType::Integer) {
        if (object == nullptr ||
            !env->IsInstanceOf(object, g_bindings.number)) {
            error = "Expected a numeric value.";
            return false;
        }
        const double number = NumberValue(env, object, 0.0);
        if (!std::isfinite(number)) {
            error = "Numeric value must be finite.";
            return false;
        }
        if (descriptor.type == LuaFeatureValueType::Number) {
            value.number = number;
        } else {
            value.integer = static_cast<std::int64_t>(std::llround(
                std::clamp(number, descriptor.minimum, descriptor.maximum)));
        }
        return true;
    }
    if (descriptor.type == LuaFeatureValueType::Enumeration) {
        value.enumeration = StringValue(env, object);
        if (value.enumeration.empty()) {
            error = "Expected a non-empty enumeration string.";
            return false;
        }
        return true;
    }
    if (object == nullptr ||
        !env->IsInstanceOf(object, g_bindings.kahlua_table)) {
        error = "Expected a color table with r, g, b and a fields.";
        return false;
    }
    constexpr std::array<const char*, 4> keys{"r", "g", "b", "a"};
    for (std::size_t index = 0; index < keys.size(); ++index) {
        jobject component = TableGet(env, object, keys[index]);
        if (component == nullptr ||
            !env->IsInstanceOf(component, g_bindings.number)) {
            if (component != nullptr) env->DeleteLocalRef(component);
            error = "Color table requires numeric r, g, b and a fields.";
            return false;
        }
        const double number = NumberValue(env, component, 0.0);
        env->DeleteLocalRef(component);
        if (!std::isfinite(number)) {
            error = "Color components must be finite.";
            return false;
        }
        value.color[index] = static_cast<float>(number);
    }
    return true;
}

jobject TableGet(JNIEnv* env, jobject table, const char* key) {
    if (table == nullptr) return nullptr;
    jstring java_key = env->NewStringUTF(key);
    jobject value = java_key == nullptr
        ? nullptr
        : env->CallObjectMethod(table, g_bindings.rawget, java_key);
    if (java_key != nullptr) env->DeleteLocalRef(java_key);
    if (ClearException(env)) {
        if (value != nullptr) env->DeleteLocalRef(value);
        return nullptr;
    }
    return value;
}

jobject NewFunction(JNIEnv* env, std::int64_t id) {
    return env->NewObject(
        g_bindings.bridge_class, g_bindings.bridge_constructor,
        static_cast<jlong>(id));
}

bool InstallFunction(JNIEnv* env, jobject table, const char* name,
                     std::int64_t id) {
    jobject function = NewFunction(env, id);
    const bool result = function != nullptr && TableSet(env, table, name, function);
    if (function != nullptr) env->DeleteLocalRef(function);
    return result;
}

bool InstallNamespace(JNIEnv* env) {
    jobject pzsa = env->CallObjectMethod(
        g_bindings.platform_instance, g_bindings.new_table);
    jobject ui = env->CallObjectMethod(g_bindings.platform_instance, g_bindings.new_table);
    jobject imgui = env->CallObjectMethod(g_bindings.platform_instance, g_bindings.new_table);
    jobject items = env->CallObjectMethod(g_bindings.platform_instance, g_bindings.new_table);
    jobject cheat = env->CallObjectMethod(g_bindings.platform_instance, g_bindings.new_table);
    if (pzsa == nullptr || ui == nullptr || imgui == nullptr || items == nullptr || cheat == nullptr ||
        ClearException(env)) {
        if (pzsa != nullptr) env->DeleteLocalRef(pzsa);
        if (ui != nullptr) env->DeleteLocalRef(ui);
        if (imgui != nullptr) env->DeleteLocalRef(imgui);
        if (items != nullptr) env->DeleteLocalRef(items);
        if (cheat != nullptr) env->DeleteLocalRef(cheat);
        return false;
    }
    if (!InstallFunction(env, pzsa, "version", kVersionFunction) ||
        !TableSet(env, pzsa, "ui", ui) ||
        !TableSet(env, pzsa, "imgui", imgui) ||
        !TableSet(env, pzsa, "items", items) ||
        !TableSet(env, pzsa, "cheat", cheat) ||
        !InstallFunction(env, ui, "category", kCategoryFunction) ||
        !InstallFunction(env, ui, "icon", kIconFunction) ||
        !InstallFunction(env, imgui, "window", kWindowFunction) ||
        !InstallFunction(env, items, "catalog", kItemsCatalogFunction) ||
        !InstallFunction(env, cheat, "list", kCheatListFunction) ||
        !InstallFunction(env, cheat, "get", kCheatGetFunction) ||
        !InstallFunction(env, cheat, "set", kCheatSetFunction) ||
        !TableSet(env, g_bindings.environment, "PZSA", pzsa)) {
        env->DeleteLocalRef(cheat);
        env->DeleteLocalRef(items);
        env->DeleteLocalRef(imgui);
        env->DeleteLocalRef(ui);
        env->DeleteLocalRef(pzsa);
        return false;
    }
    env->DeleteLocalRef(cheat);
    env->DeleteLocalRef(items);
    env->DeleteLocalRef(imgui);
    env->DeleteLocalRef(ui);
    env->DeleteLocalRef(pzsa);
    return true;
}

jint JNICALL Dispatch(JNIEnv* env, jclass, jlong function_id,
                      jobject frame, jint argument_count) {
    if (frame == nullptr) return 0;
    if (function_id == kVersionFunction) {
        jstring version = env->NewStringUTF("PZSA Lua API 3");
        if (version == nullptr) return 0;
        const jmethodID push = env->GetMethodID(
            env->GetObjectClass(frame), "push", "(Ljava/lang/Object;)I");
        const jint result = push == nullptr ? 0 : env->CallIntMethod(frame, push, version);
        env->DeleteLocalRef(version);
        return ClearException(env) ? 0 : result;
    }
    if (function_id == kCheatListFunction) {
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID push = env->GetMethodID(
            frame_class, "push", "(Ljava/lang/Object;)I");
        jobject result = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (push == nullptr || result == nullptr || ClearException(env)) {
            if (result != nullptr) env->DeleteLocalRef(result);
            if (frame_class != nullptr) env->DeleteLocalRef(frame_class);
            return 0;
        }
        const std::vector<LuaFeatureDescriptor> features =
            SnapshotLuaFeatureRegistry();
        for (std::size_t index = 0; index < features.size(); ++index) {
            jobject descriptor = FeatureDescriptorTable(env, features[index]);
            jobject key = BoxDouble(env, static_cast<double>(index + 1));
            if (descriptor != nullptr && key != nullptr) {
                TableSet(env, result, key, descriptor);
            }
            if (key != nullptr) env->DeleteLocalRef(key);
            if (descriptor != nullptr) env->DeleteLocalRef(descriptor);
            if (ClearException(env)) break;
        }
        const jint pushed = env->CallIntMethod(frame, push, result);
        env->DeleteLocalRef(result);
        env->DeleteLocalRef(frame_class);
        return ClearException(env) ? 0 : pushed;
    }
    if (function_id == kCheatGetFunction ||
        function_id == kCheatSetFunction) {
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID get = env->GetMethodID(
            frame_class, "get", "(I)Ljava/lang/Object;");
        const jmethodID push = env->GetMethodID(
            frame_class, "push", "(Ljava/lang/Object;)I");
        jobject path_object = get == nullptr
            ? nullptr
            : env->CallObjectMethod(frame, get, 0);
        const std::string path = StringValue(env, path_object);
        if (path_object != nullptr) env->DeleteLocalRef(path_object);
        LuaFeatureDescriptor descriptor;
        const bool found = !path.empty() && FindLuaFeature(path, descriptor);
        jobject result = nullptr;
        if (!found) {
            result = FeatureOperationResult(
                env, false, false,
                path.empty() ? "Feature path is required."
                             : "Feature path was not found.",
                nullptr);
        } else if (function_id == kCheatGetFunction) {
            result = FeatureOperationResult(
                env, true, descriptor.pending, {}, &descriptor);
        } else {
            jobject requested_object = env->CallObjectMethod(frame, get, 1);
            LuaFeatureValue requested;
            std::string parse_error;
            if (!ParseFeatureValue(
                    env, requested_object, descriptor, requested,
                    parse_error)) {
                result = FeatureOperationResult(
                    env, false, false, parse_error, &descriptor);
            } else {
                const LuaFeatureWriteResult write = QueueLuaFeatureWrite(
                    path, std::move(requested));
                result = FeatureOperationResult(
                    env, write.ok, write.pending, write.reason,
                    write.feature.path.empty() ? nullptr : &write.feature);
            }
            if (requested_object != nullptr) {
                env->DeleteLocalRef(requested_object);
            }
        }
        const jint pushed = result == nullptr || push == nullptr
            ? 0
            : env->CallIntMethod(frame, push, result);
        if (result != nullptr) env->DeleteLocalRef(result);
        if (frame_class != nullptr) env->DeleteLocalRef(frame_class);
        return ClearException(env) ? 0 : pushed;
    }
    if (function_id == kItemsCatalogFunction) {
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID push = env->GetMethodID(
            frame_class, "push", "(Ljava/lang/Object;)I");
        jobject result = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (push == nullptr || result == nullptr || ClearException(env)) {
            if (result != nullptr) env->DeleteLocalRef(result);
            if (frame_class != nullptr) env->DeleteLocalRef(frame_class);
            return 0;
        }
        const std::vector<ItemCatalogEntry>& catalog = GetItemCatalog();
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            const ItemCatalogEntry& item = catalog[index];
            jobject entry = env->CallObjectMethod(
                g_bindings.platform_instance, g_bindings.new_table);
            jobject key = BoxDouble(env, static_cast<double>(index + 1));
            jobject texture_id = BoxDouble(env, item.texture_id);
            if (entry != nullptr && key != nullptr && texture_id != nullptr) {
                TableSetString(env, entry, "full_type", item.full_type);
                TableSetString(env, entry, "display_name", item.display_name);
                TableSetString(env, entry, "category", item.category);
                TableSet(env, entry, "texture_id", texture_id);
                TableSet(env, result, key, entry);
            }
            if (texture_id != nullptr) env->DeleteLocalRef(texture_id);
            if (key != nullptr) env->DeleteLocalRef(key);
            if (entry != nullptr) env->DeleteLocalRef(entry);
            if (ClearException(env)) break;
        }
        const jint pushed = env->CallIntMethod(frame, push, result);
        env->DeleteLocalRef(result);
        env->DeleteLocalRef(frame_class);
        return ClearException(env) ? 0 : pushed;
    }
    if (function_id == kIconFunction) {
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID push = env->GetMethodID(
            frame_class, "push", "(Ljava/lang/Object;)I");
        jobject table = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (table == nullptr || push == nullptr || ClearException(env)) {
            if (table != nullptr) env->DeleteLocalRef(table);
            if (frame_class != nullptr) env->DeleteLocalRef(frame_class);
            return 0;
        }
        std::size_t icon_id;
        {
            std::lock_guard lock(g_mutex);
            icon_id = g_next_icon_id++;
            LuaUiIconDefinition icon;
            icon.id = icon_id;
            icon.owner_script = g_registration_owner;
            g_icons.push_back(std::move(icon));
        }
        jobject id = BoxDouble(env, static_cast<double>(icon_id));
        TableSet(env, table, kIconIdKey, id);
        if (id != nullptr) env->DeleteLocalRef(id);
        for (const auto& method :
             std::vector<std::pair<const char*, std::int64_t>>{
                 {"line", kIconLine},
                 {"rect", kIconRectangle},
                 {"circle", kIconCircle}}) {
            const std::int64_t method_id = kIconMethodNamespace |
                (static_cast<std::int64_t>(icon_id + 1) * 0x1000) |
                method.second;
            jobject function = NewFunction(env, method_id);
            if (function != nullptr) {
                TableSet(env, table, method.first, function);
                env->DeleteLocalRef(function);
            }
        }
        const jint result = env->CallIntMethod(frame, push, table);
        env->DeleteLocalRef(table);
        env->DeleteLocalRef(frame_class);
        return ClearException(env) ? 0 : result;
    }
    if (function_id == kCategoryFunction || function_id == kWindowFunction) {
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID get = env->GetMethodID(
            frame_class, "get", "(I)Ljava/lang/Object;");
        const jmethodID push = env->GetMethodID(
            frame_class, "push", "(Ljava/lang/Object;)I");
        jobject name_object = env->CallObjectMethod(frame, get, 0);
        const std::string name = StringValue(env, name_object);
        if (name_object != nullptr) env->DeleteLocalRef(name_object);
        if (name.empty()) return 0;
        jobject table = env->CallObjectMethod(g_bindings.platform_instance, g_bindings.new_table);
        if (table == nullptr || ClearException(env)) return 0;
        std::vector<LuaUiIconPrimitive> icon_primitives;
        if (argument_count >= 2) {
            jobject icon_object = env->CallObjectMethod(frame, get, 1);
            if (icon_object != nullptr && env->IsInstanceOf(
                    icon_object, g_bindings.kahlua_table)) {
                jobject icon_id_object = TableGet(
                    env, icon_object, kIconIdKey);
                const std::size_t icon_id = static_cast<std::size_t>(
                    NumberValue(env, icon_id_object, -1.0));
                if (icon_id_object != nullptr) {
                    env->DeleteLocalRef(icon_id_object);
                }
                std::lock_guard lock(g_mutex);
                const auto icon = std::find_if(
                    g_icons.begin(), g_icons.end(),
                    [icon_id](const LuaUiIconDefinition& definition) {
                        return definition.id == icon_id;
                    });
                if (icon != g_icons.end()) {
                    icon_primitives = icon->primitives;
                }
            }
            if (icon_object != nullptr) env->DeleteLocalRef(icon_object);
        }
        std::size_t category_id;
        {
            std::lock_guard lock(g_mutex);
            category_id = g_next_category_id++;
            LuaUiCategory category;
            category.id = category_id;
            category.name = name;
            category.icon_primitives = std::move(icon_primitives);
            category.owner_script = g_registration_owner;
            category.standalone = function_id == kWindowFunction;
            g_categories.push_back(std::move(category));
        }
        jobject id = BoxDouble(env, static_cast<double>(category_id));
        TableSet(env, table, kCategoryIdKey, id);
        if (id != nullptr) env->DeleteLocalRef(id);
        for (const auto& method : std::vector<std::pair<const char*, std::int64_t>>{
                 {"toggle", kCategoryToggle}, {"slider", kCategorySlider},
                 {"input", kCategoryInput},
                 {"color", kCategoryColor},
                 {"item_multiselect", kCategoryItemMultiSelect},
                 {"aim_range", kCategoryAimMode}}) {
            jobject function = NewFunction(env,
                static_cast<std::int64_t>(category_id + 1) * 0x1000 +
                method.second);
            if (function != nullptr) {
                TableSet(env, table, method.first, function);
                env->DeleteLocalRef(function);
            }
        }
        const jint result = env->CallIntMethod(frame, push, table);
        env->DeleteLocalRef(frame_class);
        return result;
    }
    if ((function_id & kIconMethodNamespace) != 0) {
        const std::int64_t encoded = function_id & ~kIconMethodNamespace;
        const std::int64_t icon_number = encoded / 0x1000 - 1;
        const std::int64_t operation = encoded % 0x1000;
        if (icon_number < 0) return 0;
        jclass frame_class = env->GetObjectClass(frame);
        const jmethodID get = env->GetMethodID(
            frame_class, "get", "(I)Ljava/lang/Object;");
        auto number_argument = [&](int index, double fallback) {
            jobject value = env->CallObjectMethod(frame, get, index);
            const double result = NumberValue(env, value, fallback);
            if (value != nullptr) env->DeleteLocalRef(value);
            return static_cast<float>(result);
        };
        LuaUiIconPrimitive primitive;
        primitive.x1 = std::clamp(number_argument(1, 0.0), -16.0f, 16.0f);
        primitive.y1 = std::clamp(number_argument(2, 0.0), -16.0f, 16.0f);
        if (operation == kIconLine || operation == kIconRectangle) {
            primitive.x2 = std::clamp(
                number_argument(3, 0.0), -16.0f, 16.0f);
            primitive.y2 = std::clamp(
                number_argument(4, 0.0), -16.0f, 16.0f);
        }
        if (operation == kIconLine) {
            primitive.type = LuaUiIconPrimitive::Type::Line;
            primitive.thickness = std::clamp(
                number_argument(5, 1.5), 0.5f, 4.0f);
        } else if (operation == kIconRectangle) {
            primitive.type = LuaUiIconPrimitive::Type::Rectangle;
            primitive.rounding = std::clamp(
                number_argument(5, 0.0), 0.0f, 8.0f);
            primitive.thickness = std::clamp(
                number_argument(6, 1.5), 0.5f, 4.0f);
        } else if (operation == kIconCircle) {
            primitive.type = LuaUiIconPrimitive::Type::Circle;
            primitive.radius = std::clamp(
                number_argument(3, 4.0), 0.5f, 16.0f);
            jobject filled = env->CallObjectMethod(frame, get, 4);
            primitive.filled = BoolValue(env, filled, false);
            if (filled != nullptr) env->DeleteLocalRef(filled);
            primitive.thickness = std::clamp(
                number_argument(5, 1.5), 0.5f, 4.0f);
        } else {
            env->DeleteLocalRef(frame_class);
            return 0;
        }
        env->DeleteLocalRef(frame_class);
        std::lock_guard lock(g_mutex);
        const auto icon = std::find_if(
            g_icons.begin(), g_icons.end(),
            [icon_number](const LuaUiIconDefinition& definition) {
                return definition.id == static_cast<std::size_t>(icon_number);
            });
        if (icon != g_icons.end() && icon->primitives.size() < 64) {
            icon->primitives.push_back(primitive);
        }
        return 0;
    }
    const std::int64_t category_number = function_id / 0x1000 - 1;
    const std::int64_t operation = function_id % 0x1000;
    if (category_number < 0) return 0;
    jclass clazz = env->GetObjectClass(frame);
    const jmethodID get = env->GetMethodID(clazz, "get", "(I)Ljava/lang/Object;");
    const jmethodID push = env->GetMethodID(clazz, "push", "(Ljava/lang/Object;)I");
    jobject label_object = env->CallObjectMethod(frame, get, 1);
    const std::string label = StringValue(env, label_object);
    if (label_object != nullptr) env->DeleteLocalRef(label_object);
    if (label.empty()) return 0;
    std::lock_guard lock(g_mutex);
    const auto category_found = std::find_if(
        g_categories.begin(), g_categories.end(),
        [category_number](const LuaUiCategory& category) {
            return category.id == static_cast<std::size_t>(category_number);
        });
    if (category_found == g_categories.end()) return 0;
    LuaUiCategory& category = *category_found;
    auto found = std::find_if(category.controls.begin(), category.controls.end(),
        [&](const LuaUiControl& control) { return control.label == label; });
    if (operation == kCategoryToggle) {
        jobject initial = env->CallObjectMethod(frame, get, 2);
        const bool value = BoolValue(env, initial, false);
        if (initial != nullptr) env->DeleteLocalRef(initial);
        std::string tooltip;
        if (argument_count >= 4) {
            jobject tooltip_object = env->CallObjectMethod(frame, get, 3);
            tooltip = StringValue(env, tooltip_object);
            if (tooltip_object != nullptr) env->DeleteLocalRef(tooltip_object);
        }
        if (found == category.controls.end()) {
            LuaUiControl control;
            control.type = LuaUiControl::Type::Toggle;
            control.label = label;
            control.tooltip = tooltip;
            control.toggle = value;
            category.controls.push_back(std::move(control));
            found = std::prev(category.controls.end());
        } else if (argument_count >= 4) {
            found->tooltip = tooltip;
        }
        jobject result = BoxBoolean(env, found->toggle);
        const jint pushed = result == nullptr ? 0 : env->CallIntMethod(frame, push, result);
        if (result != nullptr) env->DeleteLocalRef(result);
        return pushed;
    }
    if (operation == kCategorySlider) {
        jobject minimum = env->CallObjectMethod(frame, get, 2);
        jobject maximum = env->CallObjectMethod(frame, get, 3);
        jobject initial = env->CallObjectMethod(frame, get, 4);
        const float min = static_cast<float>(NumberValue(env, minimum, 0.0));
        const float max = static_cast<float>(NumberValue(env, maximum, 1.0));
        const float value = static_cast<float>(NumberValue(env, initial, min));
        if (minimum != nullptr) env->DeleteLocalRef(minimum);
        if (maximum != nullptr) env->DeleteLocalRef(maximum);
        if (initial != nullptr) env->DeleteLocalRef(initial);
        if (found == category.controls.end()) {
            LuaUiControl control;
            control.type = LuaUiControl::Type::Slider;
            control.label = label;
            control.value = std::clamp(value, min, max);
            control.minimum = min;
            control.maximum = max;
            category.controls.push_back(std::move(control));
            found = std::prev(category.controls.end());
        }
        jobject result = BoxDouble(env, found->value);
        const jint pushed = result == nullptr ? 0 : env->CallIntMethod(frame, push, result);
        if (result != nullptr) env->DeleteLocalRef(result);
        return pushed;
    }
    if (operation == kCategoryInput) {
        jobject initial = env->CallObjectMethod(frame, get, 2);
        const std::string value = StringValue(env, initial);
        if (initial != nullptr) env->DeleteLocalRef(initial);
        if (found == category.controls.end()) {
            LuaUiControl control;
            control.type = LuaUiControl::Type::Input;
            control.label = label;
            control.text = value;
            category.controls.push_back(std::move(control));
            found = std::prev(category.controls.end());
        }
        jstring result = JavaStringFromUtf8(env, found->text);
        const jint pushed = result == nullptr
            ? 0
            : env->CallIntMethod(frame, push, result);
        if (result != nullptr) env->DeleteLocalRef(result);
        return pushed;
    }
    if (operation == kCategoryColor) {
        std::array<float, 4> initial_color{};
        constexpr std::array<float, 4> fallback{1.0f, 1.0f, 1.0f, 1.0f};
        for (int index = 0; index < 4; ++index) {
            jobject component = env->CallObjectMethod(frame, get, index + 2);
            initial_color[static_cast<std::size_t>(index)] = std::clamp(
                static_cast<float>(NumberValue(env, component, fallback[index])),
                0.0f, 1.0f);
            if (component != nullptr) env->DeleteLocalRef(component);
        }
        if (found == category.controls.end()) {
            LuaUiControl control;
            control.type = LuaUiControl::Type::Color;
            control.label = label;
            control.color = initial_color;
            category.controls.push_back(std::move(control));
            found = std::prev(category.controls.end());
        }
        jobject result = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (result == nullptr || ClearException(env)) return 0;
        constexpr std::array<const char*, 4> keys{"r", "g", "b", "a"};
        for (std::size_t index = 0; index < keys.size(); ++index) {
            jobject component = BoxDouble(env, found->color[index]);
            if (component != nullptr) {
                TableSet(env, result, keys[index], component);
                env->DeleteLocalRef(component);
            }
        }
        const jint pushed = env->CallIntMethod(frame, push, result);
        env->DeleteLocalRef(result);
        return ClearException(env) ? 0 : pushed;
    }
    if (operation == kCategoryItemMultiSelect) {
        if (found == category.controls.end()) {
            LuaUiControl control;
            control.type = LuaUiControl::Type::ItemMultiSelect;
            control.label = label;
            category.controls.push_back(std::move(control));
            found = std::prev(category.controls.end());
        }
        jobject result = env->CallObjectMethod(
            g_bindings.platform_instance, g_bindings.new_table);
        if (result == nullptr || ClearException(env)) return 0;
        jobject selected = BoxBoolean(env, true);
        if (selected != nullptr) {
            for (const std::string& full_type : found->selected_items) {
                jstring key = JavaStringFromUtf8(env, full_type);
                if (key != nullptr) {
                    TableSet(env, result, key, selected);
                    env->DeleteLocalRef(key);
                }
            }
            env->DeleteLocalRef(selected);
        }
        const jint pushed = env->CallIntMethod(frame, push, result);
        env->DeleteLocalRef(result);
        return ClearException(env) ? 0 : pushed;
    }
    if (operation == kCategoryAimMode) {
        jobject mode = env->CallObjectMethod(frame, get, 1);
        category.aim_mode = StringValue(env, mode);
        if (mode != nullptr) env->DeleteLocalRef(mode);
        return 0;
    }
    return 0;
}

}  // namespace

bool EnsureLuaUiApi(JNIEnv* env, std::string& error) {
    if (env == nullptr) { error = "JNI environment unavailable"; return false; }
    if (g_bindings.registered) {
        jobject current_environment = env->GetStaticObjectField(
            g_bindings.lua_manager, g_bindings.lua_environment);
        jobject current_platform = env->GetStaticObjectField(
            g_bindings.lua_manager, g_bindings.lua_platform);
        if (current_environment == nullptr || current_platform == nullptr ||
            ClearException(env)) {
            if (current_environment != nullptr) {
                env->DeleteLocalRef(current_environment);
            }
            if (current_platform != nullptr) {
                env->DeleteLocalRef(current_platform);
            }
            error = "Project Zomboid Lua runtime unavailable";
            return false;
        }
        const bool environment_unchanged = env->IsSameObject(
            current_environment, g_bindings.environment) == JNI_TRUE;
        const bool platform_unchanged = env->IsSameObject(
            current_platform, g_bindings.platform_instance) == JNI_TRUE;
        if (environment_unchanged && platform_unchanged) {
            env->DeleteLocalRef(current_environment);
            env->DeleteLocalRef(current_platform);
            return true;
        }

        jobject replacement_environment = env->NewGlobalRef(current_environment);
        jobject replacement_platform = env->NewGlobalRef(current_platform);
        env->DeleteLocalRef(current_environment);
        env->DeleteLocalRef(current_platform);
        if (replacement_environment == nullptr || replacement_platform == nullptr ||
            ClearException(env)) {
            if (replacement_environment != nullptr) {
                env->DeleteGlobalRef(replacement_environment);
            }
            if (replacement_platform != nullptr) {
                env->DeleteGlobalRef(replacement_platform);
            }
            error = "Unable to retain the current Project Zomboid Lua runtime";
            return false;
        }

        jobject previous_environment = g_bindings.environment;
        jobject previous_platform = g_bindings.platform_instance;
        g_bindings.environment = replacement_environment;
        g_bindings.platform_instance = replacement_platform;
        if (!InstallNamespace(env)) {
            g_bindings.environment = previous_environment;
            g_bindings.platform_instance = previous_platform;
            env->DeleteGlobalRef(replacement_environment);
            env->DeleteGlobalRef(replacement_platform);
            error = "Unable to register PZSA in the current Lua environment";
            return false;
        }
        if (previous_environment != nullptr) {
            env->DeleteGlobalRef(previous_environment);
        }
        if (previous_platform != nullptr) {
            env->DeleteGlobalRef(previous_platform);
        }
        return true;
    }
    g_bindings.lua_manager = LoadGlobalClass(env, "zombie/Lua/LuaManager");
    g_bindings.platform = LoadGlobalClass(env, "se/krka/kahlua/j2se/J2SEPlatform");
    g_bindings.kahlua_table = LoadGlobalClass(env, "se/krka/kahlua/vm/KahluaTable");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.string = LoadGlobalClass(env, "java/lang/String");
    g_bindings.number = LoadGlobalClass(env, "java/lang/Number");
    g_bindings.boolean = LoadGlobalClass(env, "java/lang/Boolean");
    g_bindings.double_class = LoadGlobalClass(env, "java/lang/Double");
    if (g_bindings.lua_manager == nullptr || g_bindings.platform == nullptr ||
        g_bindings.kahlua_table == nullptr || g_bindings.object == nullptr ||
        g_bindings.string == nullptr || g_bindings.number == nullptr ||
        g_bindings.boolean == nullptr || g_bindings.double_class == nullptr) {
        error = "Kahlua classes unavailable"; return false;
    }
    g_bindings.lua_platform = env->GetStaticFieldID(
        g_bindings.lua_manager, "platform", "Lse/krka/kahlua/j2se/J2SEPlatform;");
    g_bindings.lua_environment = env->GetStaticFieldID(
        g_bindings.lua_manager, "env", "Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.new_table = env->GetMethodID(g_bindings.platform, "newTable",
        "()Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.rawset = env->GetMethodID(g_bindings.kahlua_table, "rawset",
        "(Ljava/lang/Object;Ljava/lang/Object;)V");
    g_bindings.rawget = env->GetMethodID(g_bindings.kahlua_table, "rawget",
        "(Ljava/lang/Object;)Ljava/lang/Object;");
    g_bindings.boolean_value = env->GetMethodID(g_bindings.boolean, "booleanValue", "()Z");
    g_bindings.number_value = env->GetMethodID(g_bindings.number, "doubleValue", "()D");
    if (ClearException(env) || g_bindings.lua_platform == nullptr ||
        g_bindings.lua_environment == nullptr || g_bindings.new_table == nullptr ||
        g_bindings.rawset == nullptr || g_bindings.rawget == nullptr ||
        g_bindings.boolean_value == nullptr ||
        g_bindings.number_value == nullptr) {
        error = "Kahlua methods unavailable";
        return false;
    }
    jobject local_platform = env->GetStaticObjectField(g_bindings.lua_manager, g_bindings.lua_platform);
    jobject local_environment = env->GetStaticObjectField(g_bindings.lua_manager, g_bindings.lua_environment);
    g_bindings.platform_instance = local_platform == nullptr ? nullptr : env->NewGlobalRef(local_platform);
    g_bindings.environment = local_environment == nullptr ? nullptr : env->NewGlobalRef(local_environment);
    if (local_platform != nullptr) env->DeleteLocalRef(local_platform);
    if (local_environment != nullptr) env->DeleteLocalRef(local_environment);
    std::string resource_error;
    const std::filesystem::path jar = runtime::EnsureLuaBridgeJar(resource_error);
    if (jar.empty()) { error = resource_error; return false; }

    // The bridge class is loaded from the extracted JAR by the game's JVM.
    // It is intentionally kept out of projectzomboid.jar.
    jclass loader_class = LoadGlobalClass(env, "java/net/URLClassLoader");
    jclass url_class = LoadGlobalClass(env, "java/net/URL");
    jclass class_loader = LoadGlobalClass(env, "java/lang/ClassLoader");
    const jmethodID url_ctor = env->GetMethodID(url_class, "<init>", "(Ljava/lang/String;)V");
    const jmethodID loader_ctor = env->GetMethodID(loader_class, "<init>",
        "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
    const jmethodID load_class = env->GetMethodID(class_loader, "loadClass",
        "(Ljava/lang/String;)Ljava/lang/Class;");
    const jmethodID get_system = env->GetStaticMethodID(class_loader, "getSystemClassLoader",
        "()Ljava/lang/ClassLoader;");
    std::string jar_url = jar.u8string();
    std::replace(jar_url.begin(), jar_url.end(), '\\', '/');
    jstring url_text = env->NewStringUTF((std::string("file:/") + jar_url).c_str());
    jobject url = env->NewObject(url_class, url_ctor, url_text);
    jobjectArray urls = env->NewObjectArray(1, url_class, url);
    jobject parent = env->CallStaticObjectMethod(class_loader, get_system);
    jobject loader = env->NewObject(loader_class, loader_ctor, urls, parent);
    jstring class_name = env->NewStringUTF("pztrainer.lua.PzsaJavaFunction");
    jclass bridge_local = static_cast<jclass>(env->CallObjectMethod(loader, load_class, class_name));
    g_bindings.bridge_class = bridge_local == nullptr ? nullptr : static_cast<jclass>(env->NewGlobalRef(bridge_local));
    g_bindings.bridge_constructor = g_bindings.bridge_class == nullptr ? nullptr :
        env->GetMethodID(g_bindings.bridge_class, "<init>", "(J)V");
    jstring reader_name = env->NewStringUTF("pztrainer.lua.Utf8ByteReader");
    jclass reader_local = reader_name == nullptr ? nullptr : static_cast<jclass>(
        env->CallObjectMethod(loader, load_class, reader_name));
    g_bindings.utf8_reader_class = reader_local == nullptr ? nullptr :
        static_cast<jclass>(env->NewGlobalRef(reader_local));
    g_bindings.utf8_reader_constructor = g_bindings.utf8_reader_class == nullptr
        ? nullptr
        : env->GetMethodID(
            g_bindings.utf8_reader_class, "<init>", "(Ljava/lang/String;)V");
    JNINativeMethod native_method{const_cast<char*>("dispatch"),
        const_cast<char*>("(JLse/krka/kahlua/vm/LuaCallFrame;I)I"),
        reinterpret_cast<void*>(&Dispatch)};
    if (!g_bindings.native_registered && g_bindings.bridge_class != nullptr &&
        g_bindings.bridge_constructor != nullptr) {
        g_bindings.native_registered =
            env->RegisterNatives(g_bindings.bridge_class, &native_method, 1) == JNI_OK;
    }
    if (url_text != nullptr) env->DeleteLocalRef(url_text);
    if (url != nullptr) env->DeleteLocalRef(url);
    if (urls != nullptr) env->DeleteLocalRef(urls);
    if (parent != nullptr) env->DeleteLocalRef(parent);
    if (loader != nullptr) env->DeleteLocalRef(loader);
    if (class_name != nullptr) env->DeleteLocalRef(class_name);
    if (bridge_local != nullptr) env->DeleteLocalRef(bridge_local);
    if (reader_name != nullptr) env->DeleteLocalRef(reader_name);
    if (reader_local != nullptr) env->DeleteLocalRef(reader_local);
    if (!g_bindings.native_registered ||
        g_bindings.utf8_reader_constructor == nullptr || ClearException(env) ||
        !InstallNamespace(env)) {
        error = "Unable to load or register the embedded Lua bridge";
        return false;
    }
    g_bindings.registered = true;
    return true;
}

jobject CreateLuaUtf8Reader(JNIEnv* env, jstring source, std::string& error) {
    if (env == nullptr || source == nullptr || !g_bindings.registered ||
        g_bindings.utf8_reader_class == nullptr ||
        g_bindings.utf8_reader_constructor == nullptr) {
        error = "Embedded UTF-8 Lua reader is unavailable";
        return nullptr;
    }
    jobject reader = env->NewObject(
        g_bindings.utf8_reader_class, g_bindings.utf8_reader_constructor, source);
    if (reader == nullptr || ClearException(env)) {
        error = "Unable to create the UTF-8 Lua source reader";
        return nullptr;
    }
    return reader;
}

void SetLuaUiRegistrationOwner(const std::string& owner_script) {
    std::lock_guard lock(g_mutex);
    g_registration_owner = owner_script;
}

std::vector<LuaControlConfiguration> CaptureLuaUiConfiguration(
    const std::string& owner_script) {
    std::lock_guard lock(g_mutex);
    std::vector<LuaControlConfiguration> result;
    for (const LuaUiCategory& category : g_categories) {
        if (category.owner_script != owner_script) continue;
        for (const LuaUiControl& control : category.controls) {
            LuaControlConfiguration saved;
            saved.category = category.name;
            saved.label = control.label;
            switch (control.type) {
                case LuaUiControl::Type::Toggle:
                    saved.type = LuaConfiguredControlType::Toggle;
                    break;
                case LuaUiControl::Type::Slider:
                    saved.type = LuaConfiguredControlType::Slider;
                    break;
                case LuaUiControl::Type::Input:
                    saved.type = LuaConfiguredControlType::Input;
                    break;
                case LuaUiControl::Type::Color:
                    saved.type = LuaConfiguredControlType::Color;
                    break;
                case LuaUiControl::Type::ItemMultiSelect:
                    saved.type = LuaConfiguredControlType::ItemMultiSelect;
                    break;
            }
            saved.standalone = category.standalone;
            saved.toggle = control.toggle;
            saved.value = control.value;
            saved.text = control.text;
            std::copy(
                control.color.begin(), control.color.end(), saved.color);
            saved.selected_items = control.selected_items;
            result.push_back(std::move(saved));
        }
    }
    return result;
}

void RestoreLuaUiConfiguration(
    const std::string& owner_script,
    const std::vector<LuaControlConfiguration>& controls) {
    std::lock_guard lock(g_mutex);
    for (LuaUiCategory& category : g_categories) {
        if (category.owner_script != owner_script) continue;
        for (LuaUiControl& control : category.controls) {
            const auto found = std::find_if(
                controls.begin(), controls.end(),
                [&](const LuaControlConfiguration& saved) {
                    LuaConfiguredControlType type =
                        LuaConfiguredControlType::Toggle;
                    if (control.type == LuaUiControl::Type::Slider) {
                        type = LuaConfiguredControlType::Slider;
                    } else if (control.type == LuaUiControl::Type::Input) {
                        type = LuaConfiguredControlType::Input;
                    } else if (control.type == LuaUiControl::Type::Color) {
                        type = LuaConfiguredControlType::Color;
                    } else if (
                        control.type == LuaUiControl::Type::ItemMultiSelect) {
                        type = LuaConfiguredControlType::ItemMultiSelect;
                    }
                    return saved.category == category.name &&
                        saved.standalone == category.standalone &&
                        saved.label == control.label && saved.type == type;
                });
            if (found == controls.end()) continue;
            if (control.type == LuaUiControl::Type::Toggle) {
                control.toggle = found->toggle;
            } else if (control.type == LuaUiControl::Type::Slider) {
                control.value = std::clamp(
                    found->value, control.minimum, control.maximum);
            } else if (control.type == LuaUiControl::Type::Input) {
                control.text = found->text;
            } else if (control.type == LuaUiControl::Type::Color) {
                for (std::size_t index = 0; index < control.color.size(); ++index) {
                    control.color[index] = std::clamp(
                        found->color[index], 0.0f, 1.0f);
                }
            } else {
                control.selected_items = found->selected_items;
                std::sort(
                    control.selected_items.begin(),
                    control.selected_items.end());
                control.selected_items.erase(
                    std::unique(
                        control.selected_items.begin(),
                        control.selected_items.end()),
                    control.selected_items.end());
            }
        }
    }
}

void RemoveLuaUiForScript(const std::string& owner_script) {
    std::lock_guard lock(g_mutex);
    g_categories.erase(
        std::remove_if(
            g_categories.begin(), g_categories.end(),
            [&](const LuaUiCategory& category) {
                return category.owner_script == owner_script;
            }),
        g_categories.end());
    g_icons.erase(
        std::remove_if(
            g_icons.begin(), g_icons.end(),
            [&](const LuaUiIconDefinition& icon) {
                return icon.owner_script == owner_script;
            }),
        g_icons.end());
    const auto selected = std::find_if(
        g_categories.begin(), g_categories.end(),
        [](const LuaUiCategory& category) {
            return category.id == g_selected_category;
        });
    if (selected == g_categories.end()) {
        const auto replacement = std::find_if(
            g_categories.begin(), g_categories.end(),
            [](const LuaUiCategory& category) {
                return !category.standalone;
            });
        g_selected_category = replacement == g_categories.end()
            ? 0
            : replacement->id;
    }
}

void DrawLuaCategoryIcon(
    const LuaUiCategory& category, ImDrawList* draw,
    const ImVec2& center, ImU32 color) {
    if (draw == nullptr) return;
    const float scale = settings::UiScale();
    for (const LuaUiIconPrimitive& primitive : category.icon_primitives) {
        const ImVec2 first(
            center.x + primitive.x1 * scale,
            center.y + primitive.y1 * scale);
        const ImVec2 second(
            center.x + primitive.x2 * scale,
            center.y + primitive.y2 * scale);
        const float thickness = primitive.thickness * scale;
        if (primitive.type == LuaUiIconPrimitive::Type::Line) {
            draw->AddLine(first, second, color, thickness);
        } else if (primitive.type == LuaUiIconPrimitive::Type::Rectangle) {
            const ImVec2 minimum(
                std::min(first.x, second.x), std::min(first.y, second.y));
            const ImVec2 maximum(
                std::max(first.x, second.x), std::max(first.y, second.y));
            draw->AddRect(
                minimum, maximum, color, primitive.rounding * scale,
                0, thickness);
        } else if (primitive.filled) {
            draw->AddCircleFilled(
                first, primitive.radius * scale, color);
        } else {
            draw->AddCircle(
                first, primitive.radius * scale, color, 0, thickness);
        }
    }
}

const std::vector<LuaUiCategory>& GetLuaUiCategories() { return g_categories; }

std::vector<LuaUiCategory> SnapshotLuaUiCategories() {
    std::lock_guard lock(g_mutex);
    return g_categories;
}

void SelectLuaUiCategory(std::size_t index) {
    std::lock_guard lock(g_mutex);
    const auto found = std::find_if(
        g_categories.begin(), g_categories.end(),
        [index](const LuaUiCategory& category) {
            return category.id == index;
        });
    if (found != g_categories.end()) g_selected_category = index;
}

std::size_t SelectedLuaUiCategory() {
    std::lock_guard lock(g_mutex);
    return g_selected_category;
}

void ClearLuaUi() {
    std::lock_guard lock(g_mutex);
    g_categories.clear();
    g_icons.clear();
    g_selected_category = 0;
    g_registration_owner.clear();
}

void DrawLuaTextInput(LuaUiControl& control) {
    std::array<char, 1024> buffer{};
    const std::size_t length = std::min(
        control.text.size(), buffer.size() - 1);
    if (length != 0) {
        std::memcpy(buffer.data(), control.text.data(), length);
    }
    ImGui::TextUnformatted(control.label.c_str());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputTextWithHint(
            "##LuaInput", "Base.Item, Display Name, Prefix*",
            buffer.data(), buffer.size())) {
        control.text = buffer.data();
    }
}

void DrawLuaColorControl(LuaUiControl& control) {
    ImGui::TextUnformatted(control.label.c_str());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::ColorEdit4(
        "##LuaColor", control.color.data(),
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_AlphaPreviewHalf);
}

void DrawLuaControlDivider() {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

void DrawLuaUi() {
    std::lock_guard lock(g_mutex);
    const bool has_window = std::any_of(
        g_categories.begin(), g_categories.end(),
        [](const LuaUiCategory& category) { return category.standalone; });
    if (!has_window) return;
    ImFont* regular_font = ui::RegularFont();
    if (regular_font != nullptr) ImGui::PushFont(regular_font, 0.0f);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Lua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        if (regular_font != nullptr) ImGui::PopFont();
        return;
    }
    for (LuaUiCategory& category : g_categories) {
        if (!category.standalone) continue;
        ImGui::SeparatorText(category.name.c_str());
        ImGui::PushID(static_cast<int>(category.id));
        for (std::size_t index = 0; index < category.controls.size(); ++index) {
            LuaUiControl& control = category.controls[index];
            ImGui::PushID(static_cast<int>(index));
            if (control.type == LuaUiControl::Type::Toggle) {
                ImGui::Checkbox(control.label.c_str(), &control.toggle);
                if (!control.tooltip.empty() && ImGui::IsItemHovered()) {
                    ui::components::RoundedTooltip(control.tooltip.c_str());
                }
            } else if (control.type == LuaUiControl::Type::Slider) {
                ImGui::SliderFloat(control.label.c_str(), &control.value,
                                   control.minimum, control.maximum);
            } else if (control.type == LuaUiControl::Type::Input) {
                DrawLuaTextInput(control);
            } else if (control.type == LuaUiControl::Type::Color) {
                DrawLuaColorControl(control);
            } else {
                ui::DrawLuaItemMultiSelect(
                    control.label.c_str(), control.selected_items,
                    control.search);
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    ImGui::End();
    if (regular_font != nullptr) ImGui::PopFont();
}

void DrawLuaCategoryPage(std::size_t category_id) {
    std::lock_guard lock(g_mutex);
    const auto found = std::find_if(
        g_categories.begin(), g_categories.end(),
        [category_id](const LuaUiCategory& category) {
            return category.id == category_id && !category.standalone;
        });
    if (found == g_categories.end()) {
        ui::components::SectionLabel("Lua");
        ImGui::TextUnformatted("Lua category is no longer available.");
        return;
    }
    LuaUiCategory& category = *found;
    ImGui::PushID(static_cast<int>(category.id));
    ui::components::SectionLabel(category.name.c_str());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    ui::components::BeginCompactCard(
        "LuaRegisteredCategory", nullptr, ImVec2(0.0f, 0.0f));
    for (std::size_t index = 0; index < category.controls.size(); ++index) {
        LuaUiControl& control = category.controls[index];
        const bool divider = index + 1 < category.controls.size();
        if (control.type == LuaUiControl::Type::Toggle) {
            ui::components::CompactToggleRow(
                control.label.c_str(), &control.toggle, divider);
            if (!control.tooltip.empty() && ImGui::IsItemHovered()) {
                ui::components::RoundedTooltip(control.tooltip.c_str());
            }
        } else if (control.type == LuaUiControl::Type::Slider) {
            ui::components::StepperRow(
                control.label.c_str(), &control.value,
                control.minimum, control.maximum, 1.0f, "%.0f", divider);
        } else if (control.type == LuaUiControl::Type::Input) {
            ImGui::PushID(static_cast<int>(index));
            DrawLuaTextInput(control);
            if (divider) DrawLuaControlDivider();
            ImGui::PopID();
        } else if (control.type == LuaUiControl::Type::Color) {
            ImGui::PushID(static_cast<int>(index));
            DrawLuaColorControl(control);
            if (divider) DrawLuaControlDivider();
            ImGui::PopID();
        } else {
            ImGui::PushID(static_cast<int>(index));
            ui::DrawLuaItemMultiSelect(
                control.label.c_str(), control.selected_items,
                control.search);
            if (divider) DrawLuaControlDivider();
            ImGui::PopID();
        }
    }
    ui::components::EndCard();
    ImGui::PopID();
}

void DrawLuaAimRange() {
    std::lock_guard lock(g_mutex);
    for (const LuaUiCategory& category : g_categories) {
        const LuaUiControl* visible = nullptr;
        const LuaUiControl* range = nullptr;
        for (const LuaUiControl& control : category.controls) {
            if (control.type == LuaUiControl::Type::Toggle && control.label == "显示范围") visible = &control;
            if (control.type == LuaUiControl::Type::Slider && control.label == "范围") range = &control;
        }
        if (visible == nullptr || range == nullptr || !visible->toggle || range->value <= 0.0f) continue;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImVec2 center(display.x * 0.5f, display.y * 0.5f);
        const ImU32 color = category.aim_mode == "rage"
            ? IM_COL32(255, 92, 100, 190) : IM_COL32(92, 180, 255, 190);
        ImGui::GetForegroundDrawList()->AddCircle(center, range->value, color, 96, 2.0f);
    }
}

}  // namespace pztrainer::bridge
