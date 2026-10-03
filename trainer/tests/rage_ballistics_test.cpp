#include <Windows.h>
#include <jni.h>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>

#include "bridge/player_aiming_delay_bridge.hpp"
#include "../src/features/aim/rage_ballistics_hook.cpp"

namespace pztrainer::features::aim {
int g_test_hit_list_calls = 0;
bool TryReplaceMagicBulletHitList(JNIEnv*, int, int, int, bool, bool) {
    ++g_test_hit_list_calls;
    return false;
}
void ResetMagicBulletShotTracking() {}
}

JNIEnv* g_test_env = nullptr;
namespace pztrainer::bridge {
JNIEnv* GetCurrentJniEnvironment() { return g_test_env; }
bool RageAttackNeedsHitList(JNIEnv*, int) { return true; }
void MarkRageAttackHitListWritten(JNIEnv*) {}
}

namespace {
using namespace pztrainer::features::aim;
float g_recorded_x;
float g_recorded_y;
float g_recorded_z;
float g_recorded_spread;

void JNICALL RecordPoint(JNIEnv*, jclass, jint, jfloat x, jfloat y, jfloat z) {
    g_recorded_x = x;
    g_recorded_y = y;
    g_recorded_z = z;
}
jint JNICALL OriginalTargets(JNIEnv*, jclass, jint, jfloat, jint, jfloatArray) { return 2; }
jint JNICALL OriginalCameraTargets(JNIEnv*, jclass, jint, jfloat, jint, jboolean, jfloatArray) { return 2; }
jint JNICALL OriginalSpreadTargets(JNIEnv*, jclass, jint, jfloat, jfloat spread, jfloat,
                                   jint, jint, jfloatArray) {
    g_recorded_spread = spread;
    return 2;
}
bool Near(float actual, float expected) { return std::abs(actual - expected) < 0.001f; }

void TestNativeHooks(JNIEnv* env, const std::filesystem::path& game_root) {
    assert(LoadLibraryW((game_root / "PZBullet64.dll").c_str()) != nullptr);
    assert(MH_Initialize() == MH_OK);
    assert(InitializeRageBallisticsHook());
    g_original_update_position = RecordPoint;
    g_original_update_reticle = RecordPoint;
    g_original_update_muzzle_direction = RecordPoint;
    g_original_get_targets = OriginalTargets;
    g_original_get_camera_targets = OriginalCameraTargets;
    g_original_get_spread_targets = OriginalSpreadTargets;
    SetRageBallisticsOverride(13, 99, 2, 5, 10, 3, false, false);
    HookedUpdatePosition(env, nullptr, 13, 1, 2.44949f, 4);
    HookedUpdateReticle(env, nullptr, 13, 0, 0, 0);
    assert(Near(g_recorded_x, 5) && Near(g_recorded_y, 3 * 2.44949f) && Near(g_recorded_z, 10));
    HookedUpdateMuzzleDirection(env, nullptr, 13, 0, 0, 1);
    const float length = std::sqrt(16.0f + 4 * 2.44949f * 2.44949f + 36);
    assert(Near(g_recorded_x, 4 / length));
    assert(Near(g_recorded_y, 2 * 2.44949f / length));
    assert(Near(g_recorded_z, 6 / length));
    HookedUpdateMuzzleDirection(env, nullptr, 14, 0, 0, 1);
    assert(Near(g_recorded_x, 0) && Near(g_recorded_z, 1));
    jfloatArray data = env->NewFloatArray(80);
    assert(HookedGetTargets(env, nullptr, 13, 30, 20, data) == 1);
    float values[5]{};
    env->GetFloatArrayRegion(data, 0, 4, values);
    assert(values[0] == 99 && values[1] == 5 && Near(values[2], 3 * 2.44949f) && values[3] == 10);
    assert(HookedGetCameraTargets(env, nullptr, 13, 30, 20, JNI_TRUE, data) == 1);
    env->GetFloatArrayRegion(data, 0, 5, values);
    assert(values[4] == 2);
    assert(HookedGetTargets(env, nullptr, 14, 30, 20, data) == 2);
    assert(HookedGetTargets(env, nullptr, 13, 30, 0, data) == 2);
    jfloatArray short_data = env->NewFloatArray(3);
    assert(HookedGetTargets(env, nullptr, 13, 30, 20, short_data) == 2);
    assert(!env->ExceptionCheck());
    g_expires_at.store(GetTickCount64() - 1);
    assert(HookedGetTargets(env, nullptr, 13, 30, 20, data) == 2);
    SetRageSpreadOverride(13, true);
    assert(HookedGetSpreadTargets(env, nullptr, 13, 30, 0.3f, 1, 8, 9, data) == 2);
    assert(g_recorded_spread == 0);
    HookedGetSpreadTargets(env, nullptr, 14, 30, 0.3f, 1, 8, 9, data);
    assert(Near(g_recorded_spread, 0.3f));
    SetRageSpreadOverride(13, false);
    HookedGetSpreadTargets(env, nullptr, 13, 30, 0.3f, 1, 8, 9, data);
    assert(Near(g_recorded_spread, 0.3f));
    SetRageSpreadOverride(13, true);
    ClearRageBallisticsOverride();
    HookedGetSpreadTargets(env, nullptr, 13, 30, 0.3f, 1, 8, 9, data);
    assert(Near(g_recorded_spread, 0.3f));
    const auto diagnostics = GetRageBallisticsDiagnostics();
    assert(diagnostics.target_overrides == 2 && diagnostics.reticle_overrides == 1);
    assert(diagnostics.overrides == 1 && diagnostics.spread_overrides == 1);
    SetRageBallisticsOverride(13, 99, 2, 5, 10, 3, false, true);
    HookedUpdateMuzzleDirection(env, nullptr, 13, 0, 0, 1);
    assert(g_test_hit_list_calls == 0);
    HookedGetTargets(env, nullptr, 13, 30, 20, data);
    assert(g_test_hit_list_calls == 1);
    HookedGetSpreadTargets(env, nullptr, 13, 30, 0.3f, 1, 8, 9, data);
    assert(g_test_hit_list_calls == 2);
    ClearRageBallisticsOverride();
    env->DeleteLocalRef(short_data);
    env->DeleteLocalRef(data);
    assert(MH_Uninitialize() == MH_OK);
}

void TestAimingDelay(JNIEnv* env) {
    using namespace pztrainer::bridge;
    jclass type = env->FindClass("zombie/characters/IsoPlayer");
    jobject local = env->NewObject(type, env->GetMethodID(type, "<init>", "()V"));
    jobject remote = env->NewObject(type, env->GetMethodID(type, "<init>", "()V"));
    const auto instance_field = env->GetStaticFieldID(type, "instance", "Lzombie/characters/IsoPlayer;");
    const auto delay_field = env->GetFieldID(type, "delay", "F");
    const auto aiming_field = env->GetFieldID(type, "aiming", "Z");
    const auto clears_field = env->GetFieldID(type, "clears", "I");
    env->SetStaticObjectField(type, instance_field, local);
    env->SetFloatField(local, delay_field, 15);
    env->SetFloatField(remote, delay_field, 25);
    env->SetBooleanField(local, aiming_field, JNI_TRUE);
    SetFastAimingDelayEnabled(false);
    UpdateFastAimingDelay(true);
    assert(env->GetFloatField(local, delay_field) == 15);
    SetFastAimingDelayEnabled(true);
    UpdateFastAimingDelay(false);
    assert(env->GetFloatField(local, delay_field) == 15);
    env->SetBooleanField(local, aiming_field, JNI_FALSE);
    UpdateFastAimingDelay(true);
    assert(env->GetFloatField(local, delay_field) == 15);
    env->SetBooleanField(local, aiming_field, JNI_TRUE);
    UpdateFastAimingDelay(true);
    for (int attempt = 0; attempt < 100 && GetPlayerAimingDelayStatus().cleared_count == 0; ++attempt) {
        Sleep(5);
        UpdateFastAimingDelay(true);
    }
    assert(GetPlayerAimingDelayStatus().cleared_count == 1);
    assert(env->GetIntField(local, clears_field) == 1);
    assert(env->GetFloatField(local, delay_field) == 0);
    assert(env->GetFloatField(remote, delay_field) == 25);
    jclass client = env->FindClass("zombie/network/GameClient");
    env->SetStaticBooleanField(client, env->GetStaticFieldID(client, "client", "Z"), JNI_TRUE);
    env->SetFloatField(local, delay_field, 18);
    UpdateFastAimingDelay(true);
    for (int attempt = 0; attempt < 100 && GetPlayerAimingDelayStatus().cleared_count < 2; ++attempt) {
        Sleep(5);
        UpdateFastAimingDelay(true);
    }
    assert(GetPlayerAimingDelayStatus().cleared_count == 2);
    assert(env->GetFloatField(local, delay_field) == 0);
    assert(env->GetFloatField(remote, delay_field) == 25);
    SetFastAimingDelayEnabled(false);
    env->SetFloatField(local, delay_field, 20);
    UpdateFastAimingDelay(true);
    assert(env->GetFloatField(local, delay_field) == 20);
    assert(!env->ExceptionCheck());
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    assert(argc == 3);
    const std::filesystem::path game_root(argv[1]);
    const auto java_bin = game_root / "jre64" / "bin";
    SetDllDirectoryW(java_bin.c_str());
    HMODULE jvm_module = LoadLibraryW((java_bin / "server" / "jvm.dll").c_str());
    assert(jvm_module != nullptr);
    using CreateVm = jint(JNICALL*)(JavaVM**, void**, void*);
    auto create_vm = reinterpret_cast<CreateVm>(GetProcAddress(jvm_module, "JNI_CreateJavaVM"));
    std::string classpath = "-Djava.class.path=" + std::filesystem::path(argv[2]).u8string();
    JavaVMOption options[]{{classpath.data(), nullptr}};
    JavaVMInitArgs arguments{};
    arguments.version = JNI_VERSION_1_8;
    arguments.nOptions = 1;
    arguments.options = options;
    JavaVM* vm = nullptr;
    assert(create_vm(&vm, reinterpret_cast<void**>(&g_test_env), &arguments) == JNI_OK);
    TestNativeHooks(g_test_env, game_root);
    TestAimingDelay(g_test_env);
    assert(vm->DestroyJavaVM() == JNI_OK);
    return 0;
}
