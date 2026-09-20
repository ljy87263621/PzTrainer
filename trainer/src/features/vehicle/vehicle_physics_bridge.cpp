#include "features/vehicle/vehicle_physics_bridge.hpp"

#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include "features/vehicle/bullet_vehicle_ghost.h"
#include "bridge/lua_ui_bridge.hpp"

namespace pztrainer::features {
namespace {
using Step = void (JNICALL*)(JNIEnv*, jclass, jfloat, jint, jfloat);
std::atomic<Step> g_original{nullptr};
jclass g_physics = nullptr;
jclass g_bullet = nullptr;
jmethodID g_before = nullptr, g_after = nullptr, g_set_static = nullptr;
void* g_target = nullptr;
bool g_enabled = false;

jlong JNICALL SetGhost(JNIEnv* env, jclass, jobject vehicle, jboolean is_static, jlong restore) {
    if (!vehicle_ghost::Begin(restore < 0 ? vehicle_ghost::Operation::Enter : vehicle_ghost::Operation::Restore,
                             static_cast<std::uint32_t>(restore))) return -1;
    const jint value = env->CallStaticIntMethod(g_bullet, g_set_static, vehicle, is_static);
    const auto result = vehicle_ghost::Finish();
    if (env->ExceptionCheck() || value < 0 || !result.observed || !result.applied) return -1;
    return static_cast<jlong>(result.originalFlags);
}

void JNICALL HookedStep(JNIEnv* env, jclass type, jfloat time, jint iterations, jfloat fixed) {
    Step original = g_original.load(std::memory_order_acquire);
    if (!original) return;
    if (env->ExceptionCheck()) { original(env, type, time, iterations, fixed); return; }
    env->CallStaticVoidMethod(g_physics, g_before, time);
    if (env->ExceptionCheck()) env->ExceptionClear();
    original(env, type, time, iterations, fixed);
    if (!env->ExceptionCheck()) {
        env->CallStaticVoidMethod(g_physics, g_after);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
}

bool EnsureVehiclePhysicsBridge(JNIEnv* env, std::string& error) {
    if (g_enabled) return true;
    if (g_physics == nullptr) {
        jclass physics = bridge::LoadEmbeddedJavaClass(env, "pztrainer.extensions.VehiclePhysics", error);
        jclass bullet = bridge::LoadEmbeddedJavaClass(env, "zombie.core.physics.Bullet", error);
        if (!physics || !bullet) {
            if (physics) env->DeleteLocalRef(physics);
            if (bullet) env->DeleteLocalRef(bullet);
            return false;
        }
        g_before = env->GetStaticMethodID(physics, "beforeStep", "(F)V");
        g_after = env->GetStaticMethodID(physics, "afterStep", "()V");
        g_set_static = env->GetStaticMethodID(bullet, "setVehicleStatic", "(Lzombie/vehicles/BaseVehicle;Z)I");
        JNINativeMethod method{const_cast<char*>("setGhost"),
            const_cast<char*>("(Lzombie/vehicles/BaseVehicle;ZJ)J"), reinterpret_cast<void*>(&SetGhost)};
        const bool failed = env->ExceptionCheck() || !g_before || !g_after || !g_set_static ||
            env->RegisterNatives(physics, &method, 1) != JNI_OK;
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!failed) {
            g_physics = static_cast<jclass>(env->NewGlobalRef(physics));
            g_bullet = static_cast<jclass>(env->NewGlobalRef(bullet));
        }
        env->DeleteLocalRef(physics); env->DeleteLocalRef(bullet);
        if (failed || !g_physics || !g_bullet) { error = "车辆物理桥接接口不可用"; return false; }
    }
    HMODULE module = GetModuleHandleW(L"PZBullet64.dll");
    void* static_export = module ? reinterpret_cast<void*>(GetProcAddress(module, "Java_zombie_core_physics_Bullet_setVehicleStatic")) : nullptr;
    if (!vehicle_ghost::Install(static_export)) { error = "当前 Bullet 版本未通过车辆物理签名校验"; return false; }
    if (g_target == nullptr) {
        void* target = reinterpret_cast<void*>(GetProcAddress(module, "Java_zombie_core_physics_Bullet_stepSimulation"));
        void* original = nullptr;
        if (!target || MH_CreateHook(target, reinterpret_cast<void*>(&HookedStep), &original) != MH_OK) {
            (void)vehicle_ghost::Cleanup();
            error = "无法安装车辆物理回调"; return false;
        }
        g_target = target;
        g_original.store(reinterpret_cast<Step>(original), std::memory_order_release);
    }
    if (MH_EnableHook(g_target) != MH_OK) {
        (void)vehicle_ghost::Cleanup();
        error = "无法启用车辆物理回调"; return false;
    }
    g_enabled = true;
    return true;
}
}
