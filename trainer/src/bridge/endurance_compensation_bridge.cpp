#include "bridge/endurance_compensation_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

constexpr jshort kRunningFlags = 8 | 16;
constexpr auto kSendInterval = std::chrono::milliseconds(100);
constexpr auto kInvocationTimeout = std::chrono::seconds(2);

enum class MovementStateSubmissionStage {
    Idle,
    SettingPacket,
    SendingPacket,
};

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jclass network_player_ai = nullptr;
    jclass player_packet_reliable = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID need_update = nullptr;
    jfieldID boolean_variables = nullptr;
    jmethodID get_player = nullptr;
    jmethodID is_running = nullptr;
    jmethodID is_sprinting = nullptr;
    jmethodID get_vehicle = nullptr;
    jmethodID get_network_ai = nullptr;
    jmethodID packet_constructor = nullptr;
};

Bindings g_bindings;
EnduranceCompensationStatus g_status;
std::chrono::steady_clock::time_point g_next_send{};
bool g_was_enabled = false;
bool g_restore_requested = false;
AsyncObjectMethodCall g_movement_call;
MovementStateSubmissionStage g_submission_stage =
    MovementStateSubmissionStage::Idle;
jobject g_pending_packet = nullptr;
jobject g_pending_packet_type = nullptr;
bool g_pending_compensate_running = false;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

void DeleteLocalRef(JNIEnv* env, jobject value) {
    if (value != nullptr) env->DeleteLocalRef(value);
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
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
        DeleteLocalRef(env, loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    DeleteLocalRef(env, name);
    DeleteLocalRef(env, loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.network_player_ai = LoadGlobalClass(
        env, "zombie/characters/NetworkPlayerAI");
    g_bindings.player_packet_reliable = LoadGlobalClass(
        env, "zombie/network/packets/character/PlayerPacketReliable");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.network_player_ai == nullptr ||
        g_bindings.player_packet_reliable == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.need_update = env->GetFieldID(
        g_bindings.network_player_ai, "needUpdate", "Z");
    g_bindings.boolean_variables = env->GetFieldID(
        g_bindings.player_packet_reliable, "booleanVariables", "S");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.is_running = env->GetMethodID(
        g_bindings.iso_player, "isRunning", "()Z");
    g_bindings.is_sprinting = env->GetMethodID(
        g_bindings.iso_player, "isSprinting", "()Z");
    g_bindings.get_vehicle = env->GetMethodID(
        g_bindings.iso_player, "getVehicle", "()Lzombie/vehicles/BaseVehicle;");
    g_bindings.get_network_ai = env->GetMethodID(
        g_bindings.iso_player, "getNetworkCharacterAI",
        "()Lzombie/characters/NetworkPlayerAI;");
    g_bindings.packet_constructor = env->GetMethodID(
        g_bindings.player_packet_reliable, "<init>", "()V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr && g_bindings.need_update != nullptr &&
        g_bindings.boolean_variables != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.is_running != nullptr && g_bindings.is_sprinting != nullptr &&
        g_bindings.get_vehicle != nullptr && g_bindings.get_network_ai != nullptr &&
        g_bindings.packet_constructor != nullptr;
    return g_bindings.ready;
}

void ResetMovementStateSubmission(JNIEnv* env) {
    ResetObjectMethodCall(env, g_movement_call);
    if (g_pending_packet != nullptr) {
        env->DeleteGlobalRef(g_pending_packet);
        g_pending_packet = nullptr;
    }
    if (g_pending_packet_type != nullptr) {
        env->DeleteGlobalRef(g_pending_packet_type);
        g_pending_packet_type = nullptr;
    }
    g_pending_compensate_running = false;
    g_submission_stage = MovementStateSubmissionStage::Idle;
}

bool QueueMovementState(JNIEnv* env, jobject player, bool compensate_running) {
    if (g_submission_stage != MovementStateSubmissionStage::Idle) return false;

    jobject network_ai = env->CallObjectMethod(player, g_bindings.get_network_ai);
    if (network_ai == nullptr || ClearException(env)) {
        DeleteLocalRef(env, network_ai);
        return false;
    }
    env->SetBooleanField(network_ai, g_bindings.need_update, JNI_TRUE);
    DeleteLocalRef(env, network_ai);
    if (ClearException(env)) return false;

    jobject packet = env->NewObject(
        g_bindings.player_packet_reliable, g_bindings.packet_constructor);
    if (packet == nullptr || ClearException(env)) {
        DeleteLocalRef(env, packet);
        return false;
    }

    g_pending_packet = env->NewGlobalRef(packet);
    const bool queued = g_pending_packet != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, g_pending_packet, "set", {player}, g_movement_call);
    DeleteLocalRef(env, packet);
    if (!queued) {
        ResetMovementStateSubmission(env);
        return false;
    }

    g_pending_compensate_running = compensate_running;
    g_submission_stage = MovementStateSubmissionStage::SettingPacket;
    return true;
}

AsyncObjectMethodState PollMovementStateSubmission(JNIEnv* env) {
    if (g_submission_stage == MovementStateSubmissionStage::Idle) {
        return AsyncObjectMethodState::Idle;
    }

    jobject result = nullptr;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, g_movement_call, kInvocationTimeout, &result);
    if (state == AsyncObjectMethodState::Pending) {
        return state;
    }
    if (state != AsyncObjectMethodState::Succeeded) {
        DeleteLocalRef(env, result);
        ResetMovementStateSubmission(env);
        return state;
    }

    if (g_submission_stage == MovementStateSubmissionStage::SettingPacket) {
        if (result == nullptr) {
            ResetMovementStateSubmission(env);
            return AsyncObjectMethodState::Failed;
        }
        g_pending_packet_type = env->NewGlobalRef(result);
        DeleteLocalRef(env, result);
        if (g_pending_packet_type == nullptr || ClearException(env)) {
            ResetMovementStateSubmission(env);
            return AsyncObjectMethodState::Failed;
        }

        if (g_pending_compensate_running) {
            const jshort original = env->GetShortField(
                g_pending_packet, g_bindings.boolean_variables);
            env->SetShortField(
                g_pending_packet, g_bindings.boolean_variables,
                static_cast<jshort>(original & ~kRunningFlags));
            if (ClearException(env)) {
                ResetMovementStateSubmission(env);
                return AsyncObjectMethodState::Failed;
            }
        }
        if (!QueueObjectMethodOnMainThread(
                env, g_pending_packet, "sendToServer",
                {g_pending_packet_type}, g_movement_call)) {
            ResetMovementStateSubmission(env);
            return AsyncObjectMethodState::Failed;
        }
        g_submission_stage = MovementStateSubmissionStage::SendingPacket;
        return AsyncObjectMethodState::Pending;
    }

    DeleteLocalRef(env, result);
    ResetMovementStateSubmission(env);
    return AsyncObjectMethodState::Succeeded;
}

}  // namespace

void UpdateEnduranceCompensationBridge(bool enabled) {
    g_status.enabled = enabled;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.multiplayer = false;
        g_status.armed = false;
        g_status.message = "联机休息补偿尚未初始化";
        return;
    }

    g_status.initialized = true;
    const AsyncObjectMethodState submission_state =
        PollMovementStateSubmission(env);
    if (submission_state == AsyncObjectMethodState::Succeeded) {
        ++g_status.compensated_updates;
        g_status.message = "已异步提交普通移动状态，服务器按自然恢复处理";
    } else if (submission_state == AsyncObjectMethodState::Failed ||
               submission_state == AsyncObjectMethodState::TimedOut) {
        g_status.message = "联机休息补偿提交失败，等待重试";
    }
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) {
        g_status.armed = false;
        g_next_send = {};
        g_status.message = "无法读取联机会话状态";
        return;
    }
    if (!enabled) {
        if (g_was_enabled && g_status.multiplayer) {
            g_restore_requested = true;
        }
        if (g_restore_requested && g_status.multiplayer &&
            g_submission_stage == MovementStateSubmissionStage::Idle) {
            jobject player = env->CallStaticObjectMethod(
                g_bindings.iso_player, g_bindings.get_player);
            if (player != nullptr && !ClearException(env)) {
                g_restore_requested = !QueueMovementState(env, player, false);
            }
            DeleteLocalRef(env, player);
        }
        if (!g_status.multiplayer) g_restore_requested = false;
        g_was_enabled = false;
        g_status.armed = false;
        g_next_send = {};
        g_status.message = "联机休息补偿待命";
        return;
    }
    g_was_enabled = true;
    g_restore_requested = false;
    if (!g_status.multiplayer) {
        g_status.armed = false;
        g_status.message = "单人模式使用直接体力保护";
        return;
    }

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        DeleteLocalRef(env, player);
        g_status.armed = false;
        g_status.message = "等待联机玩家对象";
        return;
    }

    jobject vehicle = env->CallObjectMethod(player, g_bindings.get_vehicle);
    const bool in_vehicle = vehicle != nullptr;
    DeleteLocalRef(env, vehicle);
    const bool moving_fast = !ClearException(env) &&
        (env->CallBooleanMethod(player, g_bindings.is_running) == JNI_TRUE ||
         env->CallBooleanMethod(player, g_bindings.is_sprinting) == JNI_TRUE);
    if (ClearException(env) || in_vehicle || !moving_fast) {
        DeleteLocalRef(env, player);
        g_status.armed = !in_vehicle;
        g_status.message = in_vehicle
            ? "载具内不提交休息补偿"
            : "联机休息补偿已就绪；攻击消耗会自然恢复";
        return;
    }

    g_status.armed = true;
    const auto now = std::chrono::steady_clock::now();
    if (now >= g_next_send &&
        g_submission_stage == MovementStateSubmissionStage::Idle) {
        if (QueueMovementState(env, player, true)) {
            g_status.message = "联机休息补偿已排队，不阻塞游戏画面";
        } else {
            g_status.message = "联机休息补偿提交失败，等待重试";
        }
        g_next_send = now + kSendInterval;
    }
    DeleteLocalRef(env, player);
}

const EnduranceCompensationStatus& GetEnduranceCompensationStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
