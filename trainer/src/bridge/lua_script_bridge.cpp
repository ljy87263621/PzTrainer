#include "bridge/lua_script_bridge.hpp"

#include <Windows.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "runtime/lua_bridge_resource.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "settings/ui_preferences.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass lua_manager = nullptr;
    jclass lua_compiler = nullptr;
    jclass lua_return = nullptr;
    jclass lua_closure = nullptr;
    jclass java_function = nullptr;
    jclass object = nullptr;
    jfieldID lua_environment = nullptr;
    jfieldID lua_caller = nullptr;
    jfieldID lua_thread = nullptr;
    jmethodID return_is_success = nullptr;
    jmethodID return_get_error = nullptr;
    jmethodID return_get_stack = nullptr;
    jmethodID return_get_first = nullptr;
    jmethodID return_size = nullptr;
};

struct ScriptRecord {
    LuaScriptInfo info;
    std::filesystem::file_time_type write_time{};
    jobject cleanup = nullptr;
};

enum class OperationKind { None, Load, Reload, Unload };
enum class OperationPhase { Idle, Cleanup, Compile, Execute };

struct Operation {
    OperationKind kind = OperationKind::None;
    OperationPhase phase = OperationPhase::Idle;
    std::filesystem::path path;
    AsyncObjectMethodCall call;
    jobject compiled = nullptr;
    std::vector<LuaControlConfiguration> restore_controls;
};

struct PendingLuaConfiguration {
    bool active = false;
    bool initialized = false;
    std::vector<LuaScriptConfiguration> desired;
    std::vector<std::filesystem::path> unload;
    std::size_t unload_index = 0;
    std::size_t load_index = 0;
};

Bindings g_bindings;
std::vector<ScriptRecord> g_records;
Operation g_operation;
std::chrono::steady_clock::time_point g_next_scan{};
bool g_refresh_requested = true;
std::filesystem::path g_lua_bridge_jar;
std::string g_lua_bridge_error;
PendingLuaConfiguration g_pending_configuration;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
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

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    if (loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_loader = env->GetStaticMethodID(
        loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_loader == nullptr ? nullptr :
        env->CallStaticObjectMethod(loader_class, get_loader);
    env->DeleteLocalRef(loader_class);
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
    g_bindings.lua_compiler = LoadGlobalClass(
        env, "se/krka/kahlua/luaj/compiler/LuaCompiler");
    g_bindings.lua_return = LoadGlobalClass(
        env, "se/krka/kahlua/integration/LuaReturn");
    g_bindings.lua_closure = LoadGlobalClass(env, "se/krka/kahlua/vm/LuaClosure");
    g_bindings.java_function = LoadGlobalClass(
        env, "se/krka/kahlua/vm/JavaFunction");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    if (g_bindings.lua_manager == nullptr || g_bindings.lua_compiler == nullptr ||
        g_bindings.lua_return == nullptr || g_bindings.lua_closure == nullptr ||
        g_bindings.java_function == nullptr || g_bindings.object == nullptr) {
        return false;
    }
    g_bindings.lua_environment = env->GetStaticFieldID(
        g_bindings.lua_manager, "env", "Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.lua_caller = env->GetStaticFieldID(
        g_bindings.lua_manager, "caller", "Lse/krka/kahlua/integration/LuaCaller;");
    g_bindings.lua_thread = env->GetStaticFieldID(
        g_bindings.lua_manager, "thread", "Lse/krka/kahlua/vm/KahluaThread;");
    g_bindings.return_is_success = env->GetMethodID(
        g_bindings.lua_return, "isSuccess", "()Z");
    g_bindings.return_get_error = env->GetMethodID(
        g_bindings.lua_return, "getErrorString", "()Ljava/lang/String;");
    g_bindings.return_get_stack = env->GetMethodID(
        g_bindings.lua_return, "getLuaStackTrace", "()Ljava/lang/String;");
    g_bindings.return_get_first = env->GetMethodID(
        g_bindings.lua_return, "getFirst", "()Ljava/lang/Object;");
    g_bindings.return_size = env->GetMethodID(
        g_bindings.lua_return, "size", "()I");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.lua_environment != nullptr && g_bindings.lua_caller != nullptr &&
        g_bindings.lua_thread != nullptr && g_bindings.return_is_success != nullptr &&
        g_bindings.return_get_error != nullptr && g_bindings.return_get_stack != nullptr &&
        g_bindings.return_get_first != nullptr && g_bindings.return_size != nullptr;
    return g_bindings.ready;
}

std::string Key(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    std::string key = (error ? path : absolute).lexically_normal().u8string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return key;
}

ScriptRecord* FindRecord(const std::filesystem::path& path) {
    const std::string key = Key(path);
    const auto found = std::find_if(
        g_records.begin(), g_records.end(), [&](const ScriptRecord& record) {
            return Key(record.info.path) == key;
        });
    return found == g_records.end() ? nullptr : &*found;
}

std::string ModifiedText(const std::filesystem::path& path) {
    std::error_code error;
    const auto file_time = std::filesystem::last_write_time(path, error);
    if (error) return "--";
    const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    const std::time_t value = std::chrono::system_clock::to_time_t(system_time);
    std::tm local{};
    localtime_s(&local, &value);
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M");
    return output.str();
}

ScriptRecord& EnsureRecord(const std::filesystem::path& path) {
    if (ScriptRecord* existing = FindRecord(path)) return *existing;
    ScriptRecord record;
    std::error_code error;
    record.info.path = std::filesystem::absolute(path, error).lexically_normal();
    if (error) record.info.path = path.lexically_normal();
    record.info.name = record.info.path.stem().u8string();
    record.info.modified = ModifiedText(record.info.path);
    record.write_time = std::filesystem::last_write_time(record.info.path, error);
    g_records.push_back(std::move(record));
    return g_records.back();
}

void ScanDirectory() {
    std::error_code error;
    std::filesystem::create_directories(settings::LuaDirectory(), error);
    for (ScriptRecord& record : g_records) record.info.file_exists = false;
    for (const auto& entry : std::filesystem::directory_iterator(
             settings::LuaDirectory(), error)) {
        if (error || !entry.is_regular_file(error)) continue;
        std::string extension = entry.path().extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
        if (extension != ".lua") continue;
        ScriptRecord& record = EnsureRecord(entry.path());
        record.info.file_exists = true;
        record.info.name = entry.path().stem().u8string();
        record.info.modified = ModifiedText(entry.path());
    }
    g_records.erase(
        std::remove_if(g_records.begin(), g_records.end(), [](const ScriptRecord& record) {
            return !record.info.file_exists && record.info.state == LuaScriptState::Unloaded &&
                record.cleanup == nullptr;
        }),
        g_records.end());
    std::sort(g_records.begin(), g_records.end(), [](const ScriptRecord& left,
                                                     const ScriptRecord& right) {
        return left.info.name < right.info.name;
    });
}

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (utf == nullptr) {
        ClearException(env);
        return {};
    }
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

void SetError(ScriptRecord& record, std::string message) {
    record.info.state = LuaScriptState::Error;
    record.info.message = message.empty() ? "Unknown Lua error" : std::move(message);
}

void FinishOperation(JNIEnv* env) {
    ResetObjectMethodCall(env, g_operation.call);
    if (g_operation.compiled != nullptr) {
        env->DeleteGlobalRef(g_operation.compiled);
    }
    g_operation = {};
    SetLuaUiRegistrationOwner({});
    g_refresh_requested = true;
}

bool QueueCall(JNIEnv* env, jobject function, OperationPhase phase) {
    jobject caller = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_caller);
    jobject thread = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_thread);
    jobjectArray arguments = env->NewObjectArray(0, g_bindings.object, nullptr);
    const bool queued = caller != nullptr && thread != nullptr && arguments != nullptr &&
        !ClearException(env) && QueueObjectMethodOnMainThread(
            env, caller, "protectedCall", {thread, function, arguments}, g_operation.call);
    if (arguments != nullptr) env->DeleteLocalRef(arguments);
    if (thread != nullptr) env->DeleteLocalRef(thread);
    if (caller != nullptr) env->DeleteLocalRef(caller);
    if (queued) g_operation.phase = phase;
    return queued;
}

bool QueueCompile(JNIEnv* env, ScriptRecord& record) {
    std::ifstream input(record.info.path, std::ios::binary);
    if (!input) {
        SetError(record, "Lua file could not be read");
        return false;
    }
    const std::string source{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    jobject environment = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_environment);
    jstring java_source = JavaStringFromUtf8(env, source);
    std::string reader_error;
    jobject source_reader = java_source == nullptr ? nullptr :
        CreateLuaUtf8Reader(env, java_source, reader_error);
    const std::string file_name = record.info.path.filename().u8string();
    jstring java_name = JavaStringFromUtf8(env, file_name);
    const bool queued = environment != nullptr && source_reader != nullptr &&
        java_name != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, g_bindings.lua_compiler, "loadis",
            {source_reader, java_name, environment}, g_operation.call);
    if (java_name != nullptr) env->DeleteLocalRef(java_name);
    if (source_reader != nullptr) env->DeleteLocalRef(source_reader);
    if (java_source != nullptr) env->DeleteLocalRef(java_source);
    if (environment != nullptr) env->DeleteLocalRef(environment);
    if (!queued) {
        SetError(record, reader_error.empty()
            ? "Unable to queue Lua compilation on the game thread"
            : reader_error);
        return false;
    }
    record.info.state = LuaScriptState::Loading;
    record.info.message = "Compiling";
    g_operation.phase = OperationPhase::Compile;
    return true;
}

bool BeginOperation(JNIEnv* env, OperationKind kind,
                    const std::filesystem::path& path) {
    if (g_operation.kind != OperationKind::None) return false;
    ScriptRecord& record = EnsureRecord(path);
    g_operation.kind = kind;
    g_operation.path = record.info.path;
    if (kind == OperationKind::Reload) {
        g_operation.restore_controls = CaptureLuaUiConfiguration(
            Key(record.info.path));
    }
    if ((kind == OperationKind::Reload || kind == OperationKind::Unload) &&
        record.cleanup != nullptr) {
        record.info.state = LuaScriptState::Unloading;
        record.info.message = "Running cleanup function";
        if (!QueueCall(env, record.cleanup, OperationPhase::Cleanup)) {
            SetError(record, "Unable to queue the cleanup function");
            FinishOperation(env);
            return false;
        }
        return true;
    }
    if (kind == OperationKind::Unload) {
        record.info.state = LuaScriptState::Unloaded;
        record.info.cleanup_available = false;
        record.info.message = "Stopped; this script did not return a cleanup function";
        RemoveLuaUiForScript(Key(record.info.path));
        FinishOperation(env);
        return true;
    }
    if (!QueueCompile(env, record)) {
        FinishOperation(env);
        return false;
    }
    return true;
}

bool ReadLuaReturn(JNIEnv* env, ScriptRecord& record, jobject result,
                   bool store_cleanup) {
    if (result == nullptr || !env->IsInstanceOf(result, g_bindings.lua_return)) {
        SetError(record, "Game Lua call returned an unexpected result");
        return false;
    }
    const bool success = env->CallBooleanMethod(
        result, g_bindings.return_is_success) == JNI_TRUE;
    if (ClearException(env)) {
        SetError(record, "Unable to read the Lua execution result");
        return false;
    }
    if (!success) {
        jstring error = static_cast<jstring>(env->CallObjectMethod(
            result, g_bindings.return_get_error));
        jstring stack = static_cast<jstring>(env->CallObjectMethod(
            result, g_bindings.return_get_stack));
        std::string detail = JavaString(env, error);
        const std::string trace = JavaString(env, stack);
        if (error != nullptr) env->DeleteLocalRef(error);
        if (stack != nullptr) env->DeleteLocalRef(stack);
        if (!trace.empty()) detail += detail.empty() ? trace : "\n" + trace;
        SetError(record, detail);
        return false;
    }
    if (store_cleanup) {
        const jint return_count = env->CallIntMethod(
            result, g_bindings.return_size);
        jobject first = return_count > 0 && !ClearException(env)
            ? env->CallObjectMethod(result, g_bindings.return_get_first)
            : nullptr;
        if (record.cleanup != nullptr) {
            env->DeleteGlobalRef(record.cleanup);
            record.cleanup = nullptr;
        }
        if (first != nullptr &&
            (env->IsInstanceOf(first, g_bindings.lua_closure) ||
             env->IsInstanceOf(first, g_bindings.java_function))) {
            record.cleanup = env->NewGlobalRef(first);
        }
        if (first != nullptr) env->DeleteLocalRef(first);
        ClearException(env);
        record.info.cleanup_available = record.cleanup != nullptr;
    }
    return true;
}

void PollOperation(JNIEnv* env) {
    if (g_operation.kind == OperationKind::None) return;
    ScriptRecord* record = FindRecord(g_operation.path);
    if (record == nullptr) {
        FinishOperation(env);
        return;
    }
    jobject result = nullptr;
    std::string error;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, g_operation.call, std::chrono::seconds(8), &result, &error);
    if (state == AsyncObjectMethodState::Pending) return;
    if (state == AsyncObjectMethodState::Failed ||
        state == AsyncObjectMethodState::TimedOut) {
        SetError(*record, error.empty()
            ? (state == AsyncObjectMethodState::TimedOut
                ? "Lua game-thread operation timed out"
                : "Lua game-thread operation failed")
            : error);
        if (result != nullptr) env->DeleteLocalRef(result);
        FinishOperation(env);
        return;
    }
    if (state != AsyncObjectMethodState::Succeeded) return;

    if (g_operation.phase == OperationPhase::Cleanup) {
        const bool cleaned = ReadLuaReturn(env, *record, result, false);
        if (result != nullptr) env->DeleteLocalRef(result);
        if (!cleaned) {
            FinishOperation(env);
            return;
        }
        if (record->cleanup != nullptr) {
            env->DeleteGlobalRef(record->cleanup);
            record->cleanup = nullptr;
        }
        record->info.cleanup_available = false;
        if (g_operation.kind == OperationKind::Unload) {
            record->info.state = LuaScriptState::Unloaded;
            record->info.message = "Cleanup completed";
            RemoveLuaUiForScript(Key(record->info.path));
            FinishOperation(env);
            return;
        }
        RemoveLuaUiForScript(Key(record->info.path));
        if (!QueueCompile(env, *record)) FinishOperation(env);
        return;
    }

    if (g_operation.phase == OperationPhase::Compile) {
        if (result == nullptr) {
            SetError(*record, "Lua compiler returned no closure");
            FinishOperation(env);
            return;
        }
        g_operation.compiled = env->NewGlobalRef(result);
        env->DeleteLocalRef(result);
        const std::string owner = Key(record->info.path);
        RemoveLuaUiForScript(owner);
        SetLuaUiRegistrationOwner(owner);
        if (g_operation.compiled == nullptr || ClearException(env) ||
            !QueueCall(env, g_operation.compiled, OperationPhase::Execute)) {
            SetError(*record, "Unable to queue the compiled Lua script");
            FinishOperation(env);
        } else {
            record->info.message = "Executing";
        }
        return;
    }

    if (g_operation.phase == OperationPhase::Execute) {
        const bool success = ReadLuaReturn(env, *record, result, true);
        if (result != nullptr) env->DeleteLocalRef(result);
        if (success) {
            record->info.state = LuaScriptState::Loaded;
            record->info.message = record->cleanup != nullptr
                ? "Loaded; cleanup function registered"
                : "Loaded; no cleanup function returned";
            std::error_code filesystem_error;
            record->write_time = std::filesystem::last_write_time(
                record->info.path, filesystem_error);
            RestoreLuaUiConfiguration(
                Key(record->info.path), g_operation.restore_controls);
        }
        FinishOperation(env);
    }
}

const LuaScriptConfiguration* FindDesiredConfiguration(
    const std::filesystem::path& path) {
    const std::string key = Key(path);
    const auto found = std::find_if(
        g_pending_configuration.desired.begin(),
        g_pending_configuration.desired.end(),
        [&](const LuaScriptConfiguration& configuration) {
            return Key(configuration.path) == key;
        });
    return found == g_pending_configuration.desired.end() ? nullptr : &*found;
}

void ProcessPendingConfiguration(JNIEnv* env) {
    if (!g_pending_configuration.active ||
        g_operation.kind != OperationKind::None) {
        return;
    }
    if (!g_pending_configuration.initialized) {
        for (const ScriptRecord& record : g_records) {
            if (record.info.state == LuaScriptState::Loaded &&
                FindDesiredConfiguration(record.info.path) == nullptr) {
                g_pending_configuration.unload.push_back(record.info.path);
            }
        }
        g_pending_configuration.initialized = true;
    }
    while (g_pending_configuration.unload_index <
           g_pending_configuration.unload.size()) {
        const std::filesystem::path path =
            g_pending_configuration.unload[
                g_pending_configuration.unload_index++];
        ScriptRecord* record = FindRecord(path);
        if (record != nullptr &&
            record->info.state == LuaScriptState::Loaded &&
            BeginOperation(env, OperationKind::Unload, path)) {
            return;
        }
    }
    while (g_pending_configuration.load_index <
           g_pending_configuration.desired.size()) {
        const LuaScriptConfiguration configuration =
            g_pending_configuration.desired[
                g_pending_configuration.load_index++];
        ScriptRecord& record = EnsureRecord(configuration.path);
        record.info.auto_reload = configuration.auto_reload;
        std::error_code file_error;
        record.info.file_exists = std::filesystem::is_regular_file(
            record.info.path, file_error);
        if (record.info.state == LuaScriptState::Loaded) {
            RestoreLuaUiConfiguration(
                Key(record.info.path), configuration.controls);
            continue;
        }
        if (file_error || !record.info.file_exists) {
            SetError(record, "Configured Lua file was not found");
            continue;
        }
        if (BeginOperation(env, OperationKind::Load, record.info.path)) {
            g_operation.restore_controls = configuration.controls;
            return;
        }
    }
    g_pending_configuration = {};
}

bool PrepareLuaApi(JNIEnv* env, const std::filesystem::path& path) {
    ScriptRecord& record = EnsureRecord(path);
    if (env == nullptr) {
        SetError(record, "JNI environment is not attached to the trainer thread");
        return false;
    }
    if (!Initialize(env)) {
        SetError(record, "Project Zomboid Lua bindings are not initialized yet");
        return false;
    }
    if (!EnsureLuaUiApi(env, g_lua_bridge_error)) {
        SetError(record, g_lua_bridge_error.empty()
            ? "Embedded PZSA Lua bridge could not be registered"
            : g_lua_bridge_error);
        return false;
    }
    return true;
}

}  // namespace

void UpdateLuaScriptBridge() {
    if (g_lua_bridge_jar.empty() && g_lua_bridge_error.empty()) {
        g_lua_bridge_jar = runtime::EnsureLuaBridgeJar(g_lua_bridge_error);
    }
    const auto now = std::chrono::steady_clock::now();
    if (g_refresh_requested || now >= g_next_scan) {
        ScanDirectory();
        g_refresh_requested = false;
        g_next_scan = now + std::chrono::seconds(1);
    }

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return;
    if (!EnsureLuaUiApi(env, g_lua_bridge_error)) return;
    PollOperation(env);
    if (g_operation.kind != OperationKind::None) return;
    ProcessPendingConfiguration(env);
    if (g_operation.kind != OperationKind::None) return;

    for (ScriptRecord& record : g_records) {
        if (!record.info.auto_reload || record.info.state != LuaScriptState::Loaded ||
            !record.info.file_exists) continue;
        std::error_code error;
        const auto current = std::filesystem::last_write_time(record.info.path, error);
        if (!error && current != record.write_time) {
            BeginOperation(env, OperationKind::Reload, record.info.path);
            break;
        }
    }
}

const std::vector<LuaScriptInfo>& GetLuaScripts() {
    static std::vector<LuaScriptInfo> result;
    result.clear();
    result.reserve(g_records.size());
    for (const ScriptRecord& record : g_records) result.push_back(record.info);
    return result;
}

bool LoadLuaScript(const std::filesystem::path& path) {
    JNIEnv* env = GetCurrentJniEnvironment();
    return PrepareLuaApi(env, path) &&
        BeginOperation(env, OperationKind::Load, path);
}

bool ReloadLuaScript(const std::filesystem::path& path) {
    JNIEnv* env = GetCurrentJniEnvironment();
    return PrepareLuaApi(env, path) &&
        BeginOperation(env, OperationKind::Reload, path);
}

bool UnloadLuaScript(const std::filesystem::path& path) {
    JNIEnv* env = GetCurrentJniEnvironment();
    return PrepareLuaApi(env, path) &&
        BeginOperation(env, OperationKind::Unload, path);
}

bool SetLuaScriptAutoReload(const std::filesystem::path& path, bool enabled) {
    ScriptRecord& record = EnsureRecord(path);
    record.info.auto_reload = enabled;
    return true;
}

std::vector<LuaScriptConfiguration> CaptureLuaScriptConfiguration() {
    std::vector<LuaScriptConfiguration> result;
    for (const ScriptRecord& record : g_records) {
        if (record.info.state != LuaScriptState::Loaded) continue;
        LuaScriptConfiguration configuration;
        configuration.path = record.info.path;
        configuration.auto_reload = record.info.auto_reload;
        configuration.controls = CaptureLuaUiConfiguration(
            Key(record.info.path));
        result.push_back(std::move(configuration));
    }
    return result;
}

void ApplyLuaScriptConfiguration(
    std::vector<LuaScriptConfiguration> configuration) {
    g_pending_configuration = {};
    g_pending_configuration.active = true;
    g_pending_configuration.desired = std::move(configuration);
}

void RefreshLuaScriptDirectory() {
    g_refresh_requested = true;
}

}  // namespace pztrainer::bridge
