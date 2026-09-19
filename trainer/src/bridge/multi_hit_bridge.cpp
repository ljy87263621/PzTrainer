#include "bridge/multi_hit_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass sandbox_options = nullptr;
    jclass boolean_option = nullptr;
    jclass iso_player = nullptr;
    jmethodID get_sandbox_options = nullptr;
    jfieldID multi_hit_zombies = nullptr;
    jmethodID get_value = nullptr;
    jmethodID set_value = nullptr;
    jmethodID get_player = nullptr;
};

Bindings g_bindings;
MultiHitStatus g_status;
bool g_requested = false;
bool g_original_value = false;
bool g_original_captured = false;

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
    g_bindings.sandbox_options = LoadGlobalClass(env, "zombie/SandboxOptions");
    g_bindings.boolean_option = LoadGlobalClass(
        env, "zombie/SandboxOptions$BooleanSandboxOption");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    if (g_bindings.sandbox_options == nullptr ||
        g_bindings.boolean_option == nullptr || g_bindings.iso_player == nullptr) {
        return false;
    }
    g_bindings.get_sandbox_options = env->GetStaticMethodID(
        g_bindings.sandbox_options, "getInstance", "()Lzombie/SandboxOptions;");
    g_bindings.multi_hit_zombies = env->GetFieldID(
        g_bindings.sandbox_options, "multiHitZombies",
        "Lzombie/SandboxOptions$BooleanSandboxOption;");
    g_bindings.get_value = env->GetMethodID(
        g_bindings.boolean_option, "getValue", "()Z");
    g_bindings.set_value = env->GetMethodID(
        g_bindings.boolean_option, "setValue", "(Z)V");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.get_sandbox_options != nullptr &&
        g_bindings.multi_hit_zombies != nullptr &&
        g_bindings.get_value != nullptr && g_bindings.set_value != nullptr &&
        g_bindings.get_player != nullptr;
    return g_bindings.ready;
}

}  // namespace

void UpdateMultiHitBridge() {
    g_status.enabled = g_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.applied = false;
        g_status.message = "多目标攻击桥接尚未初始化";
        return;
    }
    g_status.initialized = true;

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

    jobject options = env->CallStaticObjectMethod(
        g_bindings.sandbox_options, g_bindings.get_sandbox_options);
    jobject multi_hit = options == nullptr ? nullptr : env->GetObjectField(
        options, g_bindings.multi_hit_zombies);
    if (options == nullptr || multi_hit == nullptr || ClearException(env)) {
        if (multi_hit != nullptr) env->DeleteLocalRef(multi_hit);
        if (options != nullptr) env->DeleteLocalRef(options);
        env->DeleteLocalRef(player);
        g_status.applied = false;
        g_status.message = "读取武器群攻选项失败";
        return;
    }

    bool observed = env->CallBooleanMethod(
        multi_hit, g_bindings.get_value) == JNI_TRUE;
    if (ClearException(env)) {
        env->DeleteLocalRef(multi_hit);
        env->DeleteLocalRef(options);
        env->DeleteLocalRef(player);
        g_status.applied = false;
        g_status.message = "读取武器群攻选项失败";
        return;
    }
    if (g_requested && !g_original_captured) {
        g_original_value = observed;
        g_original_captured = true;
    }
    g_status.original_value = g_original_value;
    const bool target = g_requested
        ? true
        : (g_original_captured ? g_original_value : observed);
    if (observed != target) {
        env->CallVoidMethod(
            multi_hit, g_bindings.set_value,
            target ? JNI_TRUE : JNI_FALSE);
        if (ClearException(env)) {
            env->DeleteLocalRef(multi_hit);
            env->DeleteLocalRef(options);
            env->DeleteLocalRef(player);
            g_status.applied = false;
            g_status.message = "修改武器群攻选项失败";
            return;
        }
        observed = target;
    }
    if (!g_requested && g_original_captured && observed == g_original_value) {
        g_original_captured = false;
    }
    g_status.applied = g_requested && observed;
    g_status.message = g_requested
        ? "多目标攻击已作用于原版命中列表"
        : "多目标攻击待命";

    env->DeleteLocalRef(multi_hit);
    env->DeleteLocalRef(options);
    env->DeleteLocalRef(player);
}

const MultiHitStatus& GetMultiHitStatus() {
    return g_status;
}

void SetMultiHitEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
