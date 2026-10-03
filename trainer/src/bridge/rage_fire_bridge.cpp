#include "bridge/rage_fire_bridge.hpp"

#include <jvmti.h>
#include <atomic>
#include <cmath>
#include <cstring>

#include "bridge/lua_ui_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "runtime/lua_bridge_resource.hpp"

namespace pztrainer::bridge {
namespace {

jvmtiEnv* g_tool = nullptr;
jclass g_transformer = nullptr;
jclass g_helper = nullptr;
jmethodID g_transform = nullptr;
jmethodID g_publish = nullptr;
jmethodID g_prepared = nullptr;
jmethodID g_clear = nullptr;
jmethodID g_needs_hit_list = nullptr;
jmethodID g_hit_list_written = nullptr;
jmethodID g_has_forced_aim = nullptr;
jfieldID g_shots = nullptr;
jfieldID g_directions = nullptr;
jfieldID g_hit_lists = nullptr;
jfieldID g_body_parts = nullptr;
jfieldID g_error = nullptr;
std::atomic<int> g_transformed{0};
bool g_ready = false;
AsyncObjectMethodCall g_prepare_call;
AsyncObjectMethodCall g_release_call;
std::chrono::steady_clock::time_point g_retry{};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

void JNICALL Transform(jvmtiEnv* tool, JNIEnv* env, jclass type, jobject, const char* name,
    jobject, jint size, const unsigned char* bytes, jint* new_size, unsigned char** new_bytes) {
    if (!type || !name || !g_transformer) return;
    const int flag = std::strcmp(name, "zombie/core/physics/BallisticsController") == 0 ? 1 :
        std::strcmp(name, "zombie/CombatManager") == 0 ? 2 : 0;
    if (!flag || env->PushLocalFrame(4) != JNI_OK) return;
    jbyteArray input = env->NewByteArray(size);
    if (input) env->SetByteArrayRegion(input, 0, size, reinterpret_cast<const jbyte*>(bytes));
    auto output = input && !env->ExceptionCheck() ? static_cast<jbyteArray>(
        env->CallStaticObjectMethod(g_transformer, g_transform, input)) : nullptr;
    if (output && !env->ExceptionCheck()) {
        const jint length = env->GetArrayLength(output);
        unsigned char* result = nullptr;
        if (length > 0 && tool->Allocate(length, &result) == JVMTI_ERROR_NONE) {
            env->GetByteArrayRegion(output, 0, length, reinterpret_cast<jbyte*>(result));
            if (!env->ExceptionCheck()) {
                *new_size = length;
                *new_bytes = result;
                g_transformed.fetch_or(flag);
            } else {
                tool->Deallocate(result);
            }
        }
    }
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    env->PopLocalFrame(nullptr);
}

bool Initialize(JNIEnv* env, std::string& error) {
    if (g_ready) return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_retry) return false;
    g_retry = now + std::chrono::seconds(2);
    error = "实际开火接口尚未就绪";
    if (!g_tool) {
        JavaVM* vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&g_tool), JVMTI_VERSION_1_2) != JNI_OK) return false;
        jvmtiCapabilities capabilities{};
        capabilities.can_retransform_classes = 1;
        const auto jar = runtime::EnsurePlayerOverridesJar(error);
        if (g_tool->AddCapabilities(&capabilities) != JVMTI_ERROR_NONE || jar.empty() ||
            g_tool->AddToSystemClassLoaderSearch(jar.u8string().c_str()) != JVMTI_ERROR_NONE) {
            g_tool->DisposeEnvironment();
            g_tool = nullptr;
            return false;
        }
    }
    if (!g_helper) {
        jclass helper = LoadEmbeddedJavaClass(env, "pztrainer.player.aim.RageFireOverrides", error);
        jclass transformer = LoadEmbeddedJavaClass(env, "pztrainer.player.aim.RageFireTransforms", error);
        if (!helper || !transformer) return false;
        g_publish = env->GetStaticMethodID(helper, "publish", "(IIIZZZZFFF)V");
        g_prepared = env->GetStaticMethodID(helper, "prepared", "()[F");
        g_clear = env->GetStaticMethodID(helper, "clear", "()V");
        g_needs_hit_list = env->GetStaticMethodID(helper, "needsHitList", "(I)Z");
        g_hit_list_written = env->GetStaticMethodID(helper, "hitListWritten", "()V");
        g_has_forced_aim = env->GetStaticMethodID(helper, "hasForcedAim", "()Z");
        g_shots = env->GetStaticFieldID(helper, "shotCalls", "J");
        g_directions = env->GetStaticFieldID(helper, "directionCalls", "J");
        g_hit_lists = env->GetStaticFieldID(helper, "hitListCalls", "J");
        g_body_parts = env->GetStaticFieldID(helper, "bodyPartCalls", "J");
        g_error = env->GetStaticFieldID(helper, "error", "Ljava/lang/String;");
        g_transform = env->GetStaticMethodID(transformer, "transform", "([B)[B");
        if (!ClearException(env)) {
            g_helper = static_cast<jclass>(env->NewGlobalRef(helper));
            g_transformer = static_cast<jclass>(env->NewGlobalRef(transformer));
        }
        env->DeleteLocalRef(helper);
        env->DeleteLocalRef(transformer);
        if (!g_helper || !g_transformer) return false;
    }
    jvmtiEventCallbacks callbacks{};
    callbacks.ClassFileLoadHook = Transform;
    if (g_tool->SetEventCallbacks(&callbacks, sizeof(callbacks)) != JVMTI_ERROR_NONE ||
        g_tool->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr) != JVMTI_ERROR_NONE) return false;
    jclass classes[]{LoadEmbeddedJavaClass(env, "zombie.core.physics.BallisticsController", error),
        LoadEmbeddedJavaClass(env, "zombie.CombatManager", error)};
    g_transformed.store(0);
    g_ready = classes[0] && classes[1] && g_tool->RetransformClasses(2, classes) == JVMTI_ERROR_NONE && g_transformed.load() == 3;
    for (jclass type : classes) if (type) env->DeleteLocalRef(type);
    g_tool->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    if (!g_ready) error = "当前游戏的实际开火方法校验失败";
    return g_ready;
}

}  // namespace

bool EnsureRageFireBridge(JNIEnv* env, std::string& error) { return Initialize(env, error); }

bool PrepareRageFireTarget(JNIEnv* env, int character_id, int target_id, int bone,
                          bool target_is_player, bool visible, bool automatic, bool redirect_hits, WorldPoint& point, std::string& error) {
    if (!Initialize(env, error)) return false;
    env->CallStaticVoidMethod(g_helper, g_publish, character_id, target_id, bone,
        target_is_player ? JNI_TRUE : JNI_FALSE, visible ? JNI_TRUE : JNI_FALSE, automatic ? JNI_TRUE : JNI_FALSE,
        redirect_hits ? JNI_TRUE : JNI_FALSE,
        point.x, point.y, point.z);
    if (ClearException(env)) return false;
    if (g_prepare_call.queue_item != nullptr) {
        PollObjectMethodOnMainThread(env, g_prepare_call, std::chrono::milliseconds(700), nullptr, &error);
    }
    if (g_prepare_call.queue_item == nullptr) QueueObjectMethodOnMainThread(env, g_helper, "prepare", {}, g_prepare_call);
    auto result = static_cast<jfloatArray>(env->CallStaticObjectMethod(g_helper, g_prepared));
    if (ClearException(env) || result == nullptr) {
        error = "等待游戏主线程读取目标骨骼";
        auto detail = static_cast<jstring>(env->GetStaticObjectField(g_helper, g_error));
        if (detail && !env->ExceptionCheck()) {
            const char* text = env->GetStringUTFChars(detail, nullptr);
            if (text) {
                if (*text) error = std::string("目标骨骼准备失败：") + text;
                env->ReleaseStringUTFChars(detail, text);
            }
            env->DeleteLocalRef(detail);
        }
        ClearException(env);
        return false;
    }
    float values[5]{};
    const bool length_valid = env->GetArrayLength(result) >= 5;
    if (length_valid) env->GetFloatArrayRegion(result, 0, 5, values);
    env->DeleteLocalRef(result);
    if (ClearException(env) || !length_valid || values[0] != target_id || values[4] != bone ||
        !std::isfinite(values[1]) || !std::isfinite(values[2]) || !std::isfinite(values[3])) return false;
    point = {values[1], values[2], values[3]};
    return true;
}

void ClearRageFireTarget(JNIEnv* env) {
    if (env && g_helper) {
        env->CallStaticVoidMethod(g_helper, g_clear);
        ClearException(env);
        ReleaseRageAutomaticAim(env);
    }
}
void ReleaseRageAutomaticAim(JNIEnv* env) {
    if (!env || !g_helper) return;
    if (g_release_call.queue_item != nullptr) {
        PollObjectMethodOnMainThread(env, g_release_call, std::chrono::milliseconds(700), nullptr);
    }
    const bool owned = env->CallStaticBooleanMethod(g_helper, g_has_forced_aim) == JNI_TRUE;
    if (!ClearException(env) && owned && g_release_call.queue_item == nullptr) {
        QueueObjectMethodOnMainThread(env, g_helper, "releaseAim", {}, g_release_call);
    }
}
bool RageAttackNeedsHitList(JNIEnv* env, int character_id) {
    if (!g_ready) return false;
    const bool needed = env->CallStaticBooleanMethod(g_helper, g_needs_hit_list, character_id) == JNI_TRUE;
    return !ClearException(env) && needed;
}
void MarkRageAttackHitListWritten(JNIEnv* env) {
    if (g_ready) {
        env->CallStaticVoidMethod(g_helper, g_hit_list_written);
        ClearException(env);
    }
}
std::uint64_t RageActualShotCalls(JNIEnv* env) {
    return g_ready ? static_cast<std::uint64_t>(env->GetStaticLongField(g_helper, g_shots)) : 0;
}
std::uint64_t RageJavaDirectionCalls(JNIEnv* env) {
    return g_ready ? static_cast<std::uint64_t>(env->GetStaticLongField(g_helper, g_directions)) : 0;
}
std::uint64_t RageJavaHitListCalls(JNIEnv* env) {
    return g_ready ? static_cast<std::uint64_t>(env->GetStaticLongField(g_helper, g_hit_lists)) : 0;
}
std::uint64_t RageJavaBodyPartCalls(JNIEnv* env) {
    return g_ready ? static_cast<std::uint64_t>(env->GetStaticLongField(g_helper, g_body_parts)) : 0;
}

}  // namespace pztrainer::bridge
