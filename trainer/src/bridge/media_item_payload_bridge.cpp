#include "bridge/media_item_payload_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "bridge/lua_ui_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

constexpr std::chrono::milliseconds kCallTimeout{2000};
constexpr std::chrono::milliseconds kServerStateDelay{650};
constexpr std::chrono::milliseconds kInventoryPollDelay{150};

enum class PendingPhase {
    Idle,
    Calling,
    WaitBeforeRemove,
    WaitForItem,
};

enum class PendingStep {
    None,
    FindCarrier,
    CountBefore,
    SendState,
    SendRemove,
    CountAfter,
};

struct Bindings {
    bool ready = false;
    jclass runtime = nullptr;
    jclass boolean_class = nullptr;
    jclass integer_class = nullptr;
    jmethodID boolean_value = nullptr;
    jmethodID integer_value = nullptr;
};

struct PendingRequest {
    PendingPhase phase = PendingPhase::Idle;
    PendingStep step = PendingStep::None;
    std::string target_full_type;
    int requested = 0;
    int completed = 0;
    int inventory_count = -1;
    std::chrono::steady_clock::time_point next_step{};
    std::chrono::steady_clock::time_point deadline{};
    AsyncObjectMethodCall call;
    jobject radio = nullptr;
};

Bindings g_bindings;
PendingRequest g_pending;
std::string g_status;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

void DeleteLocalRef(JNIEnv* env, jobject value) {
    if (value != nullptr) env->DeleteLocalRef(value);
}

void DeleteGlobalRef(JNIEnv* env, jobject& value) {
    if (env != nullptr && value != nullptr) env->DeleteGlobalRef(value);
    value = nullptr;
}

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemMediaPayloadBindings");
    std::string error;
    jclass runtime = LoadEmbeddedJavaClass(
        env, "pztrainer.items.MediaItemPayload", error);
    jclass boolean_class = env->FindClass("java/lang/Boolean");
    jclass integer_class = env->FindClass("java/lang/Integer");
    if (runtime == nullptr || boolean_class == nullptr || integer_class == nullptr ||
        ClearException(env)) {
        DeleteLocalRef(env, runtime);
        DeleteLocalRef(env, boolean_class);
        DeleteLocalRef(env, integer_class);
        g_status = error.empty() ? "媒体载荷 Java 桥接不可用" : error;
        return false;
    }
    g_bindings.boolean_value = env->GetMethodID(
        boolean_class, "booleanValue", "()Z");
    g_bindings.integer_value = env->GetMethodID(
        integer_class, "intValue", "()I");
    g_bindings.runtime = static_cast<jclass>(env->NewGlobalRef(runtime));
    g_bindings.boolean_class = static_cast<jclass>(
        env->NewGlobalRef(boolean_class));
    g_bindings.integer_class = static_cast<jclass>(
        env->NewGlobalRef(integer_class));
    DeleteLocalRef(env, runtime);
    DeleteLocalRef(env, boolean_class);
    DeleteLocalRef(env, integer_class);
    g_bindings.ready = !ClearException(env) && g_bindings.runtime != nullptr &&
        g_bindings.boolean_class != nullptr && g_bindings.integer_class != nullptr &&
        g_bindings.boolean_value != nullptr && g_bindings.integer_value != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

void ResetPending(JNIEnv* env) {
    ResetObjectMethodCall(env, g_pending.call);
    DeleteGlobalRef(env, g_pending.radio);
    g_pending = PendingRequest{};
}

void FailPending(JNIEnv* env, std::string message) {
    const int completed = g_pending.completed;
    ResetPending(env);
    if (completed > 0) {
        message += "（已完成 " + std::to_string(completed) + " 件）";
    }
    g_status = std::move(message);
}

bool StoreGlobalRef(JNIEnv* env, jobject value, jobject& destination) {
    DeleteGlobalRef(env, destination);
    if (value == nullptr) return false;
    destination = env->NewGlobalRef(value);
    return destination != nullptr && !ClearException(env);
}

bool QueueRuntimeCall(JNIEnv* env, const char* method,
                      std::initializer_list<jobject> arguments,
                      PendingStep step) {
    if (!QueueObjectMethodOnMainThread(
            env, g_bindings.runtime, method, arguments, g_pending.call)) {
        return false;
    }
    g_pending.phase = PendingPhase::Calling;
    g_pending.step = step;
    return true;
}

bool QueueCount(JNIEnv* env, jobject player, PendingStep step) {
    jstring type = env->NewStringUTF(g_pending.target_full_type.c_str());
    const bool queued = type != nullptr && !ClearException(env) &&
        QueueRuntimeCall(env, "countInventory", {player, type}, step);
    DeleteLocalRef(env, type);
    return queued;
}

bool QueueSendState(JNIEnv* env, jobject player) {
    jstring type = env->NewStringUTF(g_pending.target_full_type.c_str());
    const bool queued = type != nullptr && !ClearException(env) &&
        QueueRuntimeCall(
            env, "sendState", {player, g_pending.radio, type},
            PendingStep::SendState);
    DeleteLocalRef(env, type);
    return queued;
}

bool ReadBoolean(JNIEnv* env, jobject value) {
    return value != nullptr &&
        env->IsInstanceOf(value, g_bindings.boolean_class) == JNI_TRUE &&
        env->CallBooleanMethod(value, g_bindings.boolean_value) == JNI_TRUE &&
        !ClearException(env);
}

int ReadInteger(JNIEnv* env, jobject value) {
    if (value == nullptr ||
        env->IsInstanceOf(value, g_bindings.integer_class) != JNI_TRUE) {
        return -1;
    }
    const int result = env->CallIntMethod(value, g_bindings.integer_value);
    return ClearException(env) ? -1 : result;
}

void HandleCompletedCall(JNIEnv* env, jobject player, PendingStep step,
                         jobject result) {
    switch (step) {
    case PendingStep::FindCarrier:
        if (!StoreGlobalRef(env, result, g_pending.radio)) {
            FailPending(
                env,
                "媒体载荷需要把便携收音机装备在主手或副手；仅放在背包中无法使用");
        } else if (!QueueCount(env, player, PendingStep::CountBefore)) {
            FailPending(env, "媒体载荷无法读取背包基准数量");
        }
        break;
    case PendingStep::CountBefore:
        g_pending.inventory_count = ReadInteger(env, result);
        if (g_pending.inventory_count < 0) {
            FailPending(env, "媒体载荷无法读取背包数量");
        } else if (!QueueSendState(env, player)) {
            FailPending(env, "媒体载荷无法发送收音机媒体状态");
        }
        break;
    case PendingStep::SendState:
        if (!ReadBoolean(env, result)) {
            FailPending(
                env,
                "便携收音机不可用：请保持装备在主手或副手，并先取出已有 CD/VHS");
        } else {
            g_pending.phase = PendingPhase::WaitBeforeRemove;
            g_pending.step = PendingStep::None;
            g_pending.next_step =
                std::chrono::steady_clock::now() + kServerStateDelay;
            g_status = "媒体载荷已写入服务端，等待执行取出媒体动作";
        }
        break;
    case PendingStep::SendRemove:
        if (!ReadBoolean(env, result)) {
            FailPending(
                env,
                "媒体载荷取出失败：便携收音机已离开主手或副手");
        } else {
            g_pending.phase = PendingPhase::WaitForItem;
            g_pending.step = PendingStep::None;
            g_pending.next_step =
                std::chrono::steady_clock::now() + kInventoryPollDelay;
            g_status = "媒体载荷取出动作已提交，等待物品进入背包";
        }
        break;
    case PendingStep::CountAfter: {
        const int count = ReadInteger(env, result);
        if (count < 0) {
            FailPending(env, "媒体载荷无法确认背包结果");
            break;
        }
        if (count <= g_pending.inventory_count) {
            g_pending.phase = PendingPhase::WaitForItem;
            g_pending.step = PendingStep::None;
            g_pending.next_step =
                std::chrono::steady_clock::now() + kInventoryPollDelay;
            break;
        }
        g_pending.inventory_count = count;
        ++g_pending.completed;
        if (g_pending.completed >= g_pending.requested) {
            const int completed = g_pending.completed;
            const std::string type = g_pending.target_full_type;
            ResetPending(env);
            g_status = "媒体载荷完成：已生成 " + std::to_string(completed) +
                " 件 " + type + " 到背包";
        } else if (!QueueSendState(env, player)) {
            FailPending(env, "媒体载荷无法继续写入下一件物品");
        } else {
            g_status = "媒体载荷正在继续：" +
                std::to_string(g_pending.completed) + "/" +
                std::to_string(g_pending.requested);
        }
        break;
    }
    default:
        FailPending(env, "媒体载荷异步状态无效");
        break;
    }
}

}  // namespace

int QueueMediaItemPayload(JNIEnv* env, jobject player,
                          const std::string& full_type, int quantity,
                          std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = g_status.empty() ? "媒体载荷桥接尚未初始化" : g_status;
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle) {
        detail = "上一组媒体载荷仍在执行";
        return 0;
    }
    if (full_type.empty()) {
        detail = "媒体载荷目标物品类型为空";
        return 0;
    }

    g_pending = PendingRequest{};
    g_pending.target_full_type = full_type;
    g_pending.requested = std::clamp(quantity, 1, 100);
    g_pending.deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(8 + g_pending.requested * 4);
    if (!QueueRuntimeCall(
            env, "findEquippedRadio", {player}, PendingStep::FindCarrier)) {
        ResetPending(env);
        detail = "媒体载荷无法检查主手和副手";
        g_status = detail;
        return 0;
    }
    detail = "已排队媒体载荷：" + std::to_string(g_pending.requested) +
        " 件 " + full_type;
    g_status = detail;
    return g_pending.requested;
}

void UpdateMediaItemPayloadBridge(JNIEnv* env, jobject player) {
    if (env == nullptr || player == nullptr || !Initialize(env) ||
        g_pending.phase == PendingPhase::Idle) {
        return;
    }
    if (std::chrono::steady_clock::now() >= g_pending.deadline) {
        FailPending(
            env,
            "媒体载荷超时；请确认收音机一直装备在主手或副手，然后重试");
        return;
    }
    if (g_pending.call.queue_item != nullptr) {
        jobject result = nullptr;
        std::string error;
        const PendingStep step = g_pending.step;
        const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
            env, g_pending.call, kCallTimeout, &result, &error);
        if (state == AsyncObjectMethodState::Pending) return;
        if (state != AsyncObjectMethodState::Succeeded) {
            DeleteLocalRef(env, result);
            std::string message = state == AsyncObjectMethodState::TimedOut
                ? "媒体载荷主线程步骤超时"
                : "媒体载荷主线程步骤失败";
            if (!error.empty()) message += "：" + error;
            FailPending(env, std::move(message));
            return;
        }
        g_pending.step = PendingStep::None;
        HandleCompletedCall(env, player, step, result);
        DeleteLocalRef(env, result);
        return;
    }
    if (std::chrono::steady_clock::now() < g_pending.next_step) return;
    if (g_pending.phase == PendingPhase::WaitBeforeRemove) {
        if (!QueueRuntimeCall(
                env, "sendRemove", {player, g_pending.radio},
                PendingStep::SendRemove)) {
            FailPending(env, "媒体载荷无法提交取出媒体动作");
        }
    } else if (g_pending.phase == PendingPhase::WaitForItem) {
        if (!QueueCount(env, player, PendingStep::CountAfter)) {
            FailPending(env, "媒体载荷无法排队背包结果检查");
        }
    }
}

const std::string& GetMediaItemPayloadStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
