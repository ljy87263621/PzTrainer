#include "bridge/farming_mode_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass lua_manager = nullptr;
    jclass boolean_class = nullptr;
    jfieldID lua_environment = nullptr;
    jmethodID boolean_value = nullptr;
    jmethodID boolean_value_of = nullptr;
};

Bindings g_bindings;
FarmingModeStatus g_status;
bool g_requested = false;
jobject g_menu = nullptr;
bool g_original_cheat = false;
bool g_original_captured = false;
std::chrono::steady_clock::time_point g_next_poll{};

enum class OperationPhase {
    Idle,
    LookupMenu,
    ReadCheat,
    WriteCheat,
    VerifyCheat,
};

AsyncObjectMethodCall g_call;
OperationPhase g_phase = OperationPhase::Idle;
bool g_target = false;

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

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.lua_manager = LoadGlobalClass(env, "zombie/Lua/LuaManager");
    g_bindings.boolean_class = LoadGlobalClass(env, "java/lang/Boolean");
    if (g_bindings.lua_manager == nullptr ||
        g_bindings.boolean_class == nullptr) {
        return false;
    }
    g_bindings.lua_environment = env->GetStaticFieldID(
        g_bindings.lua_manager, "env", "Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.boolean_value = env->GetMethodID(
        g_bindings.boolean_class, "booleanValue", "()Z");
    g_bindings.boolean_value_of = env->GetStaticMethodID(
        g_bindings.boolean_class, "valueOf", "(Z)Ljava/lang/Boolean;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.lua_environment != nullptr &&
        g_bindings.boolean_value != nullptr &&
        g_bindings.boolean_value_of != nullptr;
    return g_bindings.ready;
}

void ResetMenu(JNIEnv* env) {
    if (g_menu != nullptr) env->DeleteGlobalRef(g_menu);
    g_menu = nullptr;
    g_original_cheat = false;
    g_original_captured = false;
}

void FinishOperation(JNIEnv* env) {
    ResetObjectMethodCall(env, g_call);
    g_phase = OperationPhase::Idle;
    g_next_poll = std::chrono::steady_clock::now() + std::chrono::seconds(1);
}

bool QueueTableGet(JNIEnv* env, jobject table, const char* key,
                   OperationPhase phase) {
    jstring java_key = env->NewStringUTF(key);
    const bool queued = java_key != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, table, "rawget", {java_key}, g_call);
    if (java_key != nullptr) env->DeleteLocalRef(java_key);
    if (queued) g_phase = phase;
    return queued;
}

bool QueueTableSetBoolean(JNIEnv* env, jobject table, const char* key,
                          bool value) {
    jstring java_key = env->NewStringUTF(key);
    jobject boxed = env->CallStaticObjectMethod(
        g_bindings.boolean_class, g_bindings.boolean_value_of,
        value ? JNI_TRUE : JNI_FALSE);
    const bool queued = java_key != nullptr && boxed != nullptr &&
        !ClearException(env) && QueueObjectMethodOnMainThread(
            env, table, "rawset", {java_key, boxed}, g_call);
    if (boxed != nullptr) env->DeleteLocalRef(boxed);
    if (java_key != nullptr) env->DeleteLocalRef(java_key);
    if (queued) g_phase = OperationPhase::WriteCheat;
    return queued;
}

bool UnboxBoolean(JNIEnv* env, jobject boxed, bool& value) {
    if (boxed == nullptr ||
        !env->IsInstanceOf(boxed, g_bindings.boolean_class)) {
        ClearException(env);
        return false;
    }
    value = env->CallBooleanMethod(boxed, g_bindings.boolean_value) == JNI_TRUE;
    return !ClearException(env);
}

bool BeginOperation(JNIEnv* env) {
    jobject environment = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_environment);
    const bool queued = environment != nullptr && !ClearException(env) &&
        QueueTableGet(
            env, environment, "ISFarmingMenu", OperationPhase::LookupMenu);
    if (environment != nullptr) env->DeleteLocalRef(environment);
    if (!queued) {
        g_status.applied = false;
        g_status.message = "无法提交农业菜单读取任务";
    }
    return queued;
}

void PollOperation(JNIEnv* env) {
    if (g_phase == OperationPhase::Idle) return;

    jobject result = nullptr;
    std::string error;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, g_call, std::chrono::seconds(8), &result, &error);
    if (state == AsyncObjectMethodState::Pending) return;
    if (state != AsyncObjectMethodState::Succeeded) {
        if (result != nullptr) env->DeleteLocalRef(result);
        g_status.applied = false;
        g_status.message = error.empty()
            ? "农业菜单主线程任务执行失败" : error;
        FinishOperation(env);
        return;
    }

    const OperationPhase completed_phase = g_phase;
    if (completed_phase == OperationPhase::LookupMenu) {
        if (result == nullptr) {
            g_status.applied = false;
            g_status.message = "等待原版农业菜单加载";
            FinishOperation(env);
            return;
        }
        if (g_menu == nullptr || env->IsSameObject(g_menu, result) != JNI_TRUE) {
            ResetMenu(env);
            g_menu = env->NewGlobalRef(result);
        }
        env->DeleteLocalRef(result);
        if (g_menu == nullptr || ClearException(env) ||
            !QueueTableGet(
                env, g_menu, "cheat", OperationPhase::ReadCheat)) {
            g_status.applied = false;
            g_status.message = "读取原版农业模式失败";
            FinishOperation(env);
        }
        return;
    }

    if (completed_phase == OperationPhase::ReadCheat ||
        completed_phase == OperationPhase::VerifyCheat) {
        bool observed = false;
        const bool valid = UnboxBoolean(env, result, observed);
        if (result != nullptr) env->DeleteLocalRef(result);
        if (!valid) {
            g_status.applied = false;
            g_status.message = completed_phase == OperationPhase::ReadCheat
                ? "读取原版农业模式失败" : "验证原版农业模式失败";
            FinishOperation(env);
            return;
        }

        if (completed_phase == OperationPhase::ReadCheat) {
            if (g_requested && !g_original_captured) {
                g_original_cheat = observed;
                g_original_captured = true;
            }
            g_target = g_requested
                ? true : (g_original_captured ? g_original_cheat : observed);
            if (observed != g_target) {
                if (!QueueTableSetBoolean(
                        env, g_menu, "cheat", g_target)) {
                    g_status.applied = false;
                    g_status.message = "切换原版农业模式失败";
                    FinishOperation(env);
                }
                return;
            }
        }

        g_status.applied = observed == g_target && g_target == g_requested;
        g_status.message = g_status.applied
            ? (g_requested ? "作物耕种模式已启用" : "作物耕种模式已关闭")
            : "原版农业模式未保持目标状态";
        if (!g_requested && g_status.applied && g_original_captured) {
            g_original_captured = false;
        }
        FinishOperation(env);
        return;
    }

    if (result != nullptr) env->DeleteLocalRef(result);
    if (!QueueTableGet(
            env, g_menu, "cheat", OperationPhase::VerifyCheat)) {
        g_status.applied = false;
        g_status.message = "验证原版农业模式失败";
        FinishOperation(env);
    }
}

}  // namespace

void UpdateFarmingModeBridge() {
    g_status.enabled = g_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.applied = false;
        g_status.message = "作物耕种桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    PollOperation(env);
    if (g_phase != OperationPhase::Idle) return;

    const auto now = std::chrono::steady_clock::now();
    if (g_status.applied && !g_original_captured && !g_requested) return;
    if (now < g_next_poll) return;
    BeginOperation(env);
}

const FarmingModeStatus& GetFarmingModeStatus() {
    return g_status;
}

void SetFarmingModeEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
    g_status.applied = false;
    g_next_poll = {};
    g_status.message = enabled
        ? "正在启用作物耕种模式" : "正在关闭作物耕种模式";
}

}  // namespace pztrainer::bridge
