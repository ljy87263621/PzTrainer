#include "bridge/game_startup_gate.hpp"

#include <jni.h>

#include <algorithm>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct StartupBindings {
    bool initialized = false;
    jobject class_loader = nullptr;
    jmethodID load_class = nullptr;
    jclass game_window = nullptr;
    jclass game_state_machine = nullptr;
    jclass main_screen_state = nullptr;
    jfieldID states = nullptr;
    jfieldID current = nullptr;
    jmethodID is_ingame_state = nullptr;
};

StartupBindings g_startup_bindings;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* name) {
    std::string dotted_name(name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring java_name = env->NewStringUTF(dotted_name.c_str());
    if (java_name == nullptr || ClearException(env)) return nullptr;

    jobject local_class = env->CallObjectMethod(
        g_startup_bindings.class_loader,
        g_startup_bindings.load_class,
        java_name);
    env->DeleteLocalRef(java_name);
    if (local_class == nullptr || ClearException(env)) return nullptr;

    jclass global_class = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    return global_class;
}

bool InitializeStartupBindings(JNIEnv* env) {
    if (g_startup_bindings.initialized) return true;

    if (g_startup_bindings.class_loader == nullptr) {
        jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
        if (class_loader_class == nullptr || ClearException(env)) return false;
        jmethodID get_system_class_loader = env->GetStaticMethodID(
            class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        g_startup_bindings.load_class = env->GetMethodID(
            class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jobject local_loader = get_system_class_loader == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(class_loader_class, get_system_class_loader);
        if (local_loader != nullptr && !ClearException(env)) {
            g_startup_bindings.class_loader = env->NewGlobalRef(local_loader);
            env->DeleteLocalRef(local_loader);
        }
        env->DeleteLocalRef(class_loader_class);
        if (g_startup_bindings.class_loader == nullptr ||
            g_startup_bindings.load_class == nullptr) {
            ClearException(env);
            return false;
        }
    }

    if (g_startup_bindings.game_window == nullptr) {
        g_startup_bindings.game_window = LoadGlobalClass(env, "zombie/GameWindow");
    }
    if (g_startup_bindings.game_state_machine == nullptr) {
        g_startup_bindings.game_state_machine = LoadGlobalClass(
            env, "zombie/gameStates/GameStateMachine");
    }
    if (g_startup_bindings.main_screen_state == nullptr) {
        g_startup_bindings.main_screen_state = LoadGlobalClass(
            env, "zombie/gameStates/MainScreenState");
    }
    if (g_startup_bindings.game_window == nullptr ||
        g_startup_bindings.game_state_machine == nullptr ||
        g_startup_bindings.main_screen_state == nullptr) {
        return false;
    }

    g_startup_bindings.states = env->GetStaticFieldID(
        g_startup_bindings.game_window,
        "states",
        "Lzombie/gameStates/GameStateMachine;");
    g_startup_bindings.current = env->GetFieldID(
        g_startup_bindings.game_state_machine,
        "current",
        "Lzombie/gameStates/GameState;");
    g_startup_bindings.is_ingame_state = env->GetStaticMethodID(
        g_startup_bindings.game_window, "isIngameState", "()Z");
    if (ClearException(env) || g_startup_bindings.states == nullptr ||
        g_startup_bindings.current == nullptr ||
        g_startup_bindings.is_ingame_state == nullptr) {
        return false;
    }

    g_startup_bindings.initialized = true;
    return true;
}

}  // namespace

GameStartupGateResult PollGameStartupGate() {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr) {
        return {GameStartupGateState::WaitingForJvm, "waiting for attached JVM render thread"};
    }
    if (!InitializeStartupBindings(env)) {
        ClearException(env);
        return {GameStartupGateState::WaitingForBindings, "waiting for game state classes"};
    }

    jobject state_machine = env->GetStaticObjectField(
        g_startup_bindings.game_window, g_startup_bindings.states);
    jobject current_state = state_machine == nullptr
        ? nullptr
        : env->GetObjectField(state_machine, g_startup_bindings.current);
    const bool in_main_menu = current_state != nullptr &&
        env->IsInstanceOf(current_state, g_startup_bindings.main_screen_state) == JNI_TRUE;
    const bool in_game = env->CallStaticBooleanMethod(
        g_startup_bindings.game_window,
        g_startup_bindings.is_ingame_state) == JNI_TRUE;
    const bool failed = ClearException(env);

    if (current_state != nullptr) env->DeleteLocalRef(current_state);
    if (state_machine != nullptr) env->DeleteLocalRef(state_machine);
    if (failed) {
        return {GameStartupGateState::WaitingForBindings, "unable to read game state"};
    }
    if (!in_main_menu && !in_game) {
        return {GameStartupGateState::WaitingForMainMenu, "waiting for main menu"};
    }
    return {GameStartupGateState::Ready,
            in_main_menu ? "main menu ready" : "in-game state already active"};
}

}  // namespace pztrainer::bridge
