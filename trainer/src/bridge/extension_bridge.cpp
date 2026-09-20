#include "bridge/extension_bridge.hpp"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <unordered_map>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "features/vehicle/vehicle_physics_bridge.hpp"

namespace pztrainer::bridge {
namespace {

ExtensionOptions g_options;
std::string g_status = "等待游戏 Java 桥接";
std::string g_native_error;
std::string g_catalogue;
std::vector<CreationOption> g_creation;
std::unordered_map<std::string, std::string> g_access;
std::chrono::steady_clock::time_point g_access_time{};
jclass g_runtime = nullptr;
jmethodID g_update = nullptr;
jmethodID g_snapshot = nullptr;
jmethodID g_command = nullptr;
std::chrono::steady_clock::time_point g_next_update{};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    g_status = "扩展接口执行失败，请检查游戏版本与内嵌桥接";
    return true;
}

jstring ToJava(JNIEnv* env, const std::string& value) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (count == 0 && !value.empty()) return nullptr;
    std::wstring wide(count, L'\0');
    if (count > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), wide.data(), count);
    return env->NewString(reinterpret_cast<const jchar*>(wide.data()), count);
}

std::string FromJava(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const jsize length = env->GetStringLength(value);
    const jchar* chars = env->GetStringChars(value, nullptr);
    if (chars == nullptr) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0,
        reinterpret_cast<const wchar_t*>(chars), length, nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    if (count > 0) WideCharToMultiByte(CP_UTF8, 0,
        reinterpret_cast<const wchar_t*>(chars), length, result.data(), count, nullptr, nullptr);
    env->ReleaseStringChars(value, chars);
    return result;
}

bool Initialize(JNIEnv* env) {
    if (g_runtime != nullptr) return true;
    jclass local = LoadEmbeddedJavaClass(env, "pztrainer.extensions.ExtensionRuntime", g_status);
    if (local == nullptr) return false;
    g_update = env->GetStaticMethodID(local, "update", "(IIZ)V");
    g_snapshot = env->GetStaticMethodID(local, "snapshot", "()Ljava/lang/String;");
    g_command = env->GetStaticMethodID(local, "command", "(Ljava/lang/String;Ljava/lang/String;I)Z");
    if (!ClearException(env) && g_update && g_snapshot && g_command)
        g_runtime = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return g_runtime != nullptr;
}

}  // namespace

ExtensionOptions& GetExtensionOptions() { return g_options; }
const std::string& GetExtensionStatus() { return g_status; }
const std::vector<CreationOption>& GetCreationOptions() { return g_creation; }
bool ExtensionBridgeReady() { return g_runtime != nullptr; }
std::string ExtensionDisabledReason(const std::string& key) {
    if (!ExtensionBridgeReady()) return "等待游戏桥接初始化";
    if (std::chrono::steady_clock::now() - g_access_time > std::chrono::seconds(2)) return "等待会话状态更新";
    const auto found = g_access.find(key);
    return found == g_access.end() ? "等待功能状态更新" : found->second;
}

void UpdateExtensionBridge(bool controls_blocked) {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_next_update) return;
    g_next_update = now + std::chrono::milliseconds(g_runtime == nullptr ? 1000 : 100);
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return;
    if ((g_options.flags & 1024) != 0 && ExtensionDisabledReason("flag:1024").empty() &&
        !features::EnsureVehiclePhysicsBridge(env, g_status)) {
        g_native_error = g_status;
        g_options.flags &= ~1024;
        return;
    }
    if ((g_options.flags & 1024) != 0) g_native_error.clear();
    env->CallStaticVoidMethod(g_runtime, g_update,
        g_options.flags & 2047, std::clamp(g_options.kill_range, 1, 30), controls_blocked ? JNI_TRUE : JNI_FALSE);
    if (ClearException(env)) return;
    jstring result = static_cast<jstring>(env->CallStaticObjectMethod(g_runtime, g_snapshot));
    if (ClearException(env)) { if (result) env->DeleteLocalRef(result); return; }
    std::string text = FromJava(env, result);
    if (result) env->DeleteLocalRef(result);
    const auto access_end = text.find('\x1d');
    if (access_end == std::string::npos) { g_access.clear(); return; }
    g_access.clear();
    std::istringstream access_lines(text.substr(0, access_end));
    std::string access_line;
    while (std::getline(access_lines, access_line)) {
        const auto tab = access_line.find('\t');
        if (tab != std::string::npos) g_access.emplace(access_line.substr(0, tab), access_line.substr(tab + 1));
    }
    g_access_time = now;
    text.erase(0, access_end + 1);
    const auto separator = text.find('\x1e');
    g_status = (g_native_error.empty() ? "" : g_native_error + "\n") + text.substr(0, separator);
    const std::string catalogue = separator == std::string::npos ? "" : text.substr(separator + 1);
    if (catalogue == g_catalogue) return;
    g_catalogue = catalogue;
    g_creation.clear();
    std::istringstream lines(catalogue);
    std::string line;
    while (std::getline(lines, line)) {
        const auto first = line.find('\t');
        const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
        if (second == std::string::npos) continue;
        g_creation.push_back({line.substr(0, first), line.substr(first + 1, second - first - 1), line.substr(second + 1)});
    }
}

bool QueueExtensionCommand(const char* action, const std::string& payload, int radius) {
    const std::string reason = ExtensionDisabledReason(action);
    if (!reason.empty()) { g_status = reason; return false; }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || g_runtime == nullptr) { g_status = "扩展桥接尚未就绪"; return false; }
    jstring name = ToJava(env, action);
    jstring value = ToJava(env, payload);
    bool accepted = false;
    if (!ClearException(env) && name && value) {
        accepted = env->CallStaticBooleanMethod(g_runtime, g_command,
            name, value, std::clamp(radius, 1, 15)) == JNI_TRUE;
        if (ClearException(env)) accepted = false;
    }
    if (name) env->DeleteLocalRef(name);
    if (value) env->DeleteLocalRef(value);
    g_status = accepted ? "已提交，等待游戏主线程执行" : "操作未提交，请等待上一项操作完成";
    return accepted;
}

}  // namespace pztrainer::bridge
