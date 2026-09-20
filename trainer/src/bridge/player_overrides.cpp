#include "bridge/player_overrides.hpp"

#include <jvmti.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include "bridge/lua_ui_bridge.hpp"
#include "runtime/lua_bridge_resource.hpp"

namespace pztrainer::bridge {
namespace {
constexpr const char* kClasses[]{"zombie/characters/IsoGameCharacter", "zombie/inventory/ItemContainer",
    "zombie/network/packets/connection/ConnectPacket", "zombie/network/packets/connection/ConnectedPacket",
    "zombie/network/packets/ExtraInfoPacket"};
jvmtiEnv* g_tool = nullptr;
jclass g_transformer = nullptr;
jmethodID g_transform = nullptr;
std::atomic<int> g_transformed{0};
bool g_ready = false;
std::chrono::steady_clock::time_point g_retry{};

void JNICALL Transform(jvmtiEnv* tool, JNIEnv* env, jclass type, jobject, const char* name,
    jobject, jint size, const unsigned char* bytes, jint* new_size, unsigned char** new_bytes) {
    if (!type || !name || !g_transformer) return;
    int index = 0;
    while (index < 5 && std::strcmp(name, kClasses[index]) != 0) ++index;
    if (index == 5 || env->PushLocalFrame(8) != JNI_OK) return;
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
                *new_size = length; *new_bytes = result;
                g_transformed.fetch_or(1 << index);
            } else tool->Deallocate(result);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->PopLocalFrame(nullptr);
}

bool Initialize(JNIEnv* env, std::string& error) {
    if (g_ready) return true;
    if (std::chrono::steady_clock::now() < g_retry) return false;
    g_retry = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    error = "玩家联机接口尚未就绪";
    if (!g_tool) {
        JavaVM* vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&g_tool), JVMTI_VERSION_1_2) != JNI_OK) {
            g_tool = nullptr; error = "当前 JVM 不支持玩家接口更新"; return false;
        }
        jvmtiCapabilities capabilities{}; capabilities.can_retransform_classes = 1;
    // Keep extension/Lua packages in their original loader; appending them here
    // would split package-private classes when they are loaded lazily.
    const auto jar = runtime::EnsurePlayerOverridesJar(error);
        if (g_tool->AddCapabilities(&capabilities) != JVMTI_ERROR_NONE || jar.empty()
            || g_tool->AddToSystemClassLoaderSearch(jar.u8string().c_str()) != JVMTI_ERROR_NONE) {
            g_tool->DisposeEnvironment(); g_tool = nullptr;
            error = "无法初始化玩家联机接口加载器"; return false;
        }
    }
    if (!g_transformer) {
        jclass local = LoadEmbeddedJavaClass(env, "pztrainer.player.PlayerTransforms", error);
        if (!local) return false;
        g_transform = env->GetStaticMethodID(local, "transform", "([B)[B");
        if (!env->ExceptionCheck() && g_transform) g_transformer = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!g_transformer) return false;
    }
    jvmtiEventCallbacks callbacks{}; callbacks.ClassFileLoadHook = Transform;
    if (g_tool->SetEventCallbacks(&callbacks, sizeof(callbacks)) != JVMTI_ERROR_NONE
        || g_tool->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr) != JVMTI_ERROR_NONE) return false;
    jclass classes[5]{};
    bool loaded = true;
    for (int i = 0; i < 5; ++i) {
        std::string name(kClasses[i]);
        for (char& c : name) if (c == '/') c = '.';
        classes[i] = LoadEmbeddedJavaClass(env, name.c_str(), error);
        loaded = loaded && classes[i] != nullptr;
    }
    g_transformed.store(0);
    g_ready = loaded && g_tool->RetransformClasses(5, classes) == JVMTI_ERROR_NONE && g_transformed.load() == 31;
    for (jclass type : classes) if (type) env->DeleteLocalRef(type);
    g_tool->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    if (!g_ready) error = "当前游戏版本的玩家接口校验失败";
    return g_ready;
}
}

jclass LoadPlayerOverrideClass(JNIEnv* env, const char* name, std::string& error) {
    return Initialize(env, error) ? LoadEmbeddedJavaClass(env, name, error) : nullptr;
}

std::string PlayerOverrideMessage(JNIEnv* env, jclass type) {
    const auto method = env->GetStaticMethodID(type, "message", "()Ljava/lang/String;");
    auto text = method ? static_cast<jstring>(env->CallStaticObjectMethod(type, method)) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); return "玩家接口状态读取失败"; }
    if (!text) return {};
    const char* chars = env->GetStringUTFChars(text, nullptr);
    std::string result = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(text, chars);
    env->DeleteLocalRef(text);
    return result;
}
}
