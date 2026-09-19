#include "bridge/pallet_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <utility>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

constexpr int kCarrierMarker = 0x505C;
constexpr std::chrono::milliseconds kAsyncCallTimeout{2000};

enum class PendingPhase {
    Idle,
    Preparing,
    WaitingForTrap,
    WaitingForAction,
};

enum class PendingStep {
    None,
    QuerySquare,
    QueryObjects,
    CarrierSquare,
    CarrierPacket,
    CarrierSet,
    CarrierSend,
    ActionSquare,
    ActionSend,
};

struct Bindings {
    bool ready = false;
    jclass inventory_item_factory = nullptr;
    jclass inventory_item = nullptr;
    jclass hand_weapon = nullptr;
    jclass object = nullptr;
    jclass iso_trap = nullptr;
    jclass list = nullptr;
    jclass double_class = nullptr;
    jclass add_explosive_trap_packet = nullptr;
    jclass net_timed_action_packet = nullptr;
    jclass packet_type = nullptr;
    jfieldID add_explosive_trap_type = nullptr;
    jmethodID create_item = nullptr;
    jmethodID item_id = nullptr;
    jmethodID item_full_type = nullptr;
    jmethodID set_remote_control_id = nullptr;
    jmethodID remote_control_id = nullptr;
    jmethodID explosion_range = nullptr;
    jmethodID explosion_power = nullptr;
    jmethodID fire_range = nullptr;
    jmethodID fire_starting_energy = nullptr;
    jmethodID fire_starting_chance = nullptr;
    jmethodID smoke_range = nullptr;
    jmethodID noise_range = nullptr;
    jmethodID sensor_range = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID trap_item = nullptr;
    jmethodID double_value_of = nullptr;
};

struct PendingRequest {
    PendingPhase phase = PendingPhase::Idle;
    std::string target_full_type;
    std::string carrier_full_type;
    int quantity = 0;
    int carrier_item_id = -1;
    std::chrono::steady_clock::time_point next_step{};
    std::chrono::steady_clock::time_point deadline{};
    PendingStep step = PendingStep::None;
    AsyncObjectMethodCall call;
    jobject carrier = nullptr;
    jobject trap = nullptr;
    jobject square = nullptr;
    jobject packet = nullptr;
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

void ClearPendingObjects(JNIEnv* env) {
    DeleteGlobalRef(env, g_pending.carrier);
    DeleteGlobalRef(env, g_pending.trap);
    DeleteGlobalRef(env, g_pending.square);
    DeleteGlobalRef(env, g_pending.packet);
}

void ResetPending(JNIEnv* env) {
    ResetObjectMethodCall(env, g_pending.call);
    ClearPendingObjects(env);
    g_pending = PendingRequest{};
}

void FailPending(JNIEnv* env, std::string message) {
    ResetPending(env);
    g_status = std::move(message);
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

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemPalletBindings");
    g_bindings.inventory_item_factory = LoadGlobalClass(
        env, "zombie/inventory/InventoryItemFactory");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.iso_trap = LoadGlobalClass(env, "zombie/iso/objects/IsoTrap");
    g_bindings.list = LoadGlobalClass(env, "java/util/List");
    g_bindings.double_class = LoadGlobalClass(env, "java/lang/Double");
    g_bindings.add_explosive_trap_packet = LoadGlobalClass(
        env, "zombie/network/packets/AddExplosiveTrapPacket");
    g_bindings.net_timed_action_packet = LoadGlobalClass(
        env, "zombie/network/packets/NetTimedActionPacket");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    if (g_bindings.inventory_item_factory == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.hand_weapon == nullptr || g_bindings.object == nullptr ||
        g_bindings.iso_trap == nullptr || g_bindings.list == nullptr ||
        g_bindings.double_class == nullptr ||
        g_bindings.add_explosive_trap_packet == nullptr ||
        g_bindings.net_timed_action_packet == nullptr ||
        g_bindings.packet_type == nullptr) {
        return false;
    }

    g_bindings.add_explosive_trap_type = env->GetStaticFieldID(
        g_bindings.packet_type, "AddExplosiveTrap",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.create_item = env->GetStaticMethodID(
        g_bindings.inventory_item_factory, "CreateItem",
        "(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
    g_bindings.item_id = env->GetMethodID(
        g_bindings.inventory_item, "getID", "()I");
    g_bindings.item_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.set_remote_control_id = env->GetMethodID(
        g_bindings.inventory_item, "setRemoteControlID", "(I)V");
    g_bindings.remote_control_id = env->GetMethodID(
        g_bindings.inventory_item, "getRemoteControlID", "()I");
    g_bindings.explosion_range = env->GetMethodID(
        g_bindings.hand_weapon, "getExplosionRange", "()I");
    g_bindings.explosion_power = env->GetMethodID(
        g_bindings.hand_weapon, "getExplosionPower", "()I");
    g_bindings.fire_range = env->GetMethodID(
        g_bindings.hand_weapon, "getFireRange", "()I");
    g_bindings.fire_starting_energy = env->GetMethodID(
        g_bindings.hand_weapon, "getFireStartingEnergy", "()I");
    g_bindings.fire_starting_chance = env->GetMethodID(
        g_bindings.hand_weapon, "getFireStartingChance", "()I");
    g_bindings.smoke_range = env->GetMethodID(
        g_bindings.hand_weapon, "getSmokeRange", "()I");
    g_bindings.noise_range = env->GetMethodID(
        g_bindings.hand_weapon, "getNoiseRange", "()I");
    g_bindings.sensor_range = env->GetMethodID(
        g_bindings.hand_weapon, "getSensorRange", "()I");
    g_bindings.list_size = env->GetMethodID(g_bindings.list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.list, "get", "(I)Ljava/lang/Object;");
    g_bindings.trap_item = env->GetMethodID(
        g_bindings.iso_trap, "getItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.double_value_of = env->GetStaticMethodID(
        g_bindings.double_class, "valueOf", "(D)Ljava/lang/Double;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.add_explosive_trap_type != nullptr &&
        g_bindings.create_item != nullptr && g_bindings.item_id != nullptr &&
        g_bindings.item_full_type != nullptr &&
        g_bindings.set_remote_control_id != nullptr &&
        g_bindings.remote_control_id != nullptr &&
        g_bindings.explosion_range != nullptr &&
        g_bindings.explosion_power != nullptr && g_bindings.fire_range != nullptr &&
        g_bindings.fire_starting_energy != nullptr &&
        g_bindings.fire_starting_chance != nullptr &&
        g_bindings.smoke_range != nullptr && g_bindings.noise_range != nullptr &&
        g_bindings.sensor_range != nullptr && g_bindings.list_size != nullptr &&
        g_bindings.list_get != nullptr && g_bindings.trap_item != nullptr &&
        g_bindings.double_value_of != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

bool HasHazardousTrapEffect(JNIEnv* env, jobject weapon) {
    const int explosion_range = env->CallIntMethod(
        weapon, g_bindings.explosion_range);
    const int explosion_power = env->CallIntMethod(
        weapon, g_bindings.explosion_power);
    const int fire_range = env->CallIntMethod(weapon, g_bindings.fire_range);
    const int fire_energy = env->CallIntMethod(
        weapon, g_bindings.fire_starting_energy);
    const int fire_chance = env->CallIntMethod(
        weapon, g_bindings.fire_starting_chance);
    const int smoke_range = env->CallIntMethod(weapon, g_bindings.smoke_range);
    const int noise_range = env->CallIntMethod(weapon, g_bindings.noise_range);
    const int sensor_range = env->CallIntMethod(weapon, g_bindings.sensor_range);
    if (ClearException(env)) return true;
    return explosion_range > 0 || explosion_power > 0 || fire_range > 0 ||
        fire_energy > 0 || fire_chance > 0 || smoke_range > 0 ||
        noise_range > 0 || sensor_range > 0;
}

bool SelectSafeCarrier(JNIEnv* env, std::string& full_type) {
    constexpr std::array<const char*, 4> candidates{
        "Base.Hammer", "Base.RollingPin", "Base.Saucepan", "Base.Fork"};
    for (const char* candidate_type : candidates) {
        jstring type = env->NewStringUTF(candidate_type);
        jobject item = type == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item, type);
        DeleteLocalRef(env, type);
        const bool safe = item != nullptr && !ClearException(env) &&
            env->IsInstanceOf(item, g_bindings.hand_weapon) == JNI_TRUE &&
            !HasHazardousTrapEffect(env, item);
        DeleteLocalRef(env, item);
        if (safe) {
            full_type = candidate_type;
            return true;
        }
        ClearException(env);
    }
    return false;
}

bool QueuePendingCall(JNIEnv* env, jobject target, const char* method_name,
                      std::initializer_list<jobject> arguments,
                      PendingStep step) {
    if (!QueueObjectMethodOnMainThread(
            env, target, method_name, arguments, g_pending.call)) {
        return false;
    }
    g_pending.step = step;
    return true;
}

bool StoreGlobalRef(JNIEnv* env, jobject value, jobject& destination) {
    DeleteGlobalRef(env, destination);
    if (value == nullptr) return false;
    destination = env->NewGlobalRef(value);
    return destination != nullptr && !ClearException(env);
}

jobject FindCarrierTrapInObjects(JNIEnv* env, jobject objects) {
    if (objects == nullptr) return nullptr;
    const jint count = env->CallIntMethod(objects, g_bindings.list_size);
    jobject match = nullptr;
    for (jint index = 0; index < count && !ClearException(env); ++index) {
        jobject candidate = env->CallObjectMethod(objects, g_bindings.list_get, index);
        if (candidate != nullptr &&
            env->IsInstanceOf(candidate, g_bindings.iso_trap) == JNI_TRUE) {
            jobject item = env->CallObjectMethod(candidate, g_bindings.trap_item);
            const int item_id = item == nullptr
                ? -1
                : env->CallIntMethod(item, g_bindings.item_id);
            bool item_matches = g_pending.carrier_item_id >= 0 &&
                item_id == g_pending.carrier_item_id;
            if (item != nullptr && g_pending.carrier_item_id < 0 &&
                !ClearException(env)) {
                const int marker = env->CallIntMethod(
                    item, g_bindings.remote_control_id);
                jstring type = static_cast<jstring>(env->CallObjectMethod(
                    item, g_bindings.item_full_type));
                const char* text = type == nullptr
                    ? nullptr
                    : env->GetStringUTFChars(type, nullptr);
                item_matches = !ClearException(env) && marker == kCarrierMarker &&
                    text != nullptr && g_pending.carrier_full_type == text;
                if (text != nullptr) env->ReleaseStringUTFChars(type, text);
                DeleteLocalRef(env, type);
            }
            DeleteLocalRef(env, item);
            if (!ClearException(env) && item_matches) {
                g_pending.carrier_item_id = item_id;
                match = candidate;
                break;
            }
        }
        DeleteLocalRef(env, candidate);
    }
    return match;
}

bool BeginTrapQuery(JNIEnv* env, jobject player) {
    return QueuePendingCall(
        env, player, "getCurrentSquare", {}, PendingStep::QuerySquare);
}

bool BeginCarrierSend(JNIEnv* env, jobject player) {
    jstring type = env->NewStringUTF(g_pending.carrier_full_type.c_str());
    jobject carrier = type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
              g_bindings.inventory_item_factory, g_bindings.create_item, type);
    DeleteLocalRef(env, type);
    if (carrier == nullptr || ClearException(env)) {
        DeleteLocalRef(env, carrier);
        return false;
    }
    env->CallVoidMethod(
        carrier, g_bindings.set_remote_control_id, kCarrierMarker);
    const int item_id = env->CallIntMethod(carrier, g_bindings.item_id);
    if (ClearException(env)) {
        DeleteLocalRef(env, carrier);
        return false;
    }
    const bool stored = StoreGlobalRef(env, carrier, g_pending.carrier);
    DeleteLocalRef(env, carrier);
    if (!stored) return false;
    g_pending.carrier_item_id = item_id;
    return QueuePendingCall(
        env, player, "getCurrentSquare", {}, PendingStep::CarrierSquare);
}

bool BeginTakeMaterialsAction(JNIEnv* env, jobject trap) {
    if (!StoreGlobalRef(env, trap, g_pending.trap)) return false;
    return QueuePendingCall(
        env, trap, "getSquare", {}, PendingStep::ActionSquare);
}

void HandleTrapQueryResult(JNIEnv* env, jobject player, jobject trap) {
    if (g_pending.phase == PendingPhase::Preparing) {
        const bool started = trap != nullptr
            ? BeginTakeMaterialsAction(env, trap)
            : BeginCarrierSend(env, player);
        if (!started) {
            FailPending(
                env, trap != nullptr
                    ? "找到旧的联机取物载体，但操作提交失败"
                    : "联机取物载体构造或发送失败");
        }
        return;
    }
    if (g_pending.phase == PendingPhase::WaitingForTrap) {
        if (trap == nullptr) {
            g_pending.next_step = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(100);
            return;
        }
        if (!BeginTakeMaterialsAction(env, trap)) {
            FailPending(env, "联机取物载体已落地，但操作提交失败");
        }
        return;
    }
    if (g_pending.phase == PendingPhase::WaitingForAction) {
        if (trap == nullptr) {
            ResetPending(env);
            g_status = "联机取物完成：服务端已移除临时载体并登记目标物品";
        } else {
            g_pending.next_step = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(150);
        }
    }
}

bool QueueTakeMaterialsPacket(JNIEnv* env, jobject player, jobject square) {
    if (!StoreGlobalRef(env, square, g_pending.square)) return false;
    jstring action_type = env->NewStringUTF("ISTakeBricks");
    jstring item_type = env->NewStringUTF(g_pending.target_full_type.c_str());
    jobject amount = env->CallStaticObjectMethod(
        g_bindings.double_class, g_bindings.double_value_of,
        static_cast<jdouble>(g_pending.quantity));
    jobjectArray arguments = env->NewObjectArray(6, g_bindings.object, nullptr);
    if (arguments != nullptr) {
        env->SetObjectArrayElement(arguments, 0, player);
        env->SetObjectArrayElement(arguments, 1, g_pending.trap);
        env->SetObjectArrayElement(arguments, 2, g_pending.square);
        env->SetObjectArrayElement(arguments, 3, nullptr);
        env->SetObjectArrayElement(arguments, 4, item_type);
        env->SetObjectArrayElement(arguments, 5, amount);
    }
    const bool ready = action_type != nullptr && item_type != nullptr &&
        amount != nullptr && arguments != nullptr && !ClearException(env);
    const bool queued = ready && QueuePendingCall(
        env, g_bindings.net_timed_action_packet, "createNewAndSend",
        {action_type, player, arguments}, PendingStep::ActionSend);
    DeleteLocalRef(env, arguments);
    DeleteLocalRef(env, amount);
    DeleteLocalRef(env, item_type);
    DeleteLocalRef(env, action_type);
    return queued;
}

void HandlePendingStep(JNIEnv* env, jobject player, PendingStep step,
                       jobject result) {
    switch (step) {
    case PendingStep::QuerySquare:
        if (result == nullptr) {
            HandleTrapQueryResult(env, player, nullptr);
        } else if (!QueuePendingCall(
                       env, result, "getObjects", {}, PendingStep::QueryObjects)) {
            FailPending(env, "联机取物无法查询当前位置物品");
        }
        break;
    case PendingStep::QueryObjects: {
        jobject trap = FindCarrierTrapInObjects(env, result);
        HandleTrapQueryResult(env, player, trap);
        DeleteLocalRef(env, trap);
        break;
    }
    case PendingStep::CarrierSquare:
        if (!StoreGlobalRef(env, result, g_pending.square) ||
            !QueuePendingCall(
                env, g_bindings.add_explosive_trap_packet, "new", {},
                PendingStep::CarrierPacket)) {
            FailPending(env, "联机取物无法读取当前位置");
        }
        break;
    case PendingStep::CarrierPacket:
        if (!StoreGlobalRef(env, result, g_pending.packet) ||
            !QueuePendingCall(
                env, g_pending.packet, "set",
                {g_pending.carrier, player, g_pending.square},
                PendingStep::CarrierSet)) {
            FailPending(env, "联机取物载体封包失败");
        }
        break;
    case PendingStep::CarrierSet: {
        jobject packet_type = env->GetStaticObjectField(
            g_bindings.packet_type, g_bindings.add_explosive_trap_type);
        const bool queued = packet_type != nullptr && !ClearException(env) &&
            QueuePendingCall(
                env, g_pending.packet, "sendToServer", {packet_type},
                PendingStep::CarrierSend);
        DeleteLocalRef(env, packet_type);
        if (!queued) FailPending(env, "联机取物载体发送失败");
        break;
    }
    case PendingStep::CarrierSend:
        ClearPendingObjects(env);
        g_pending.phase = PendingPhase::WaitingForTrap;
        g_pending.step = PendingStep::None;
        g_pending.next_step = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(150);
        g_pending.deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        g_status = "联机取物请求已发送，等待服务端返回物品";
        break;
    case PendingStep::ActionSquare:
        if (result == nullptr || !QueueTakeMaterialsPacket(env, player, result)) {
            FailPending(env, "联机取物操作封包失败");
        }
        break;
    case PendingStep::ActionSend:
        ClearPendingObjects(env);
        g_pending.phase = PendingPhase::WaitingForAction;
        g_pending.step = PendingStep::None;
        g_pending.next_step = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(150);
        g_pending.deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(8) +
            std::chrono::milliseconds(g_pending.quantity * 250);
        g_status = "服务端已接受联机取物请求，等待物品写入背包";
        break;
    default:
        FailPending(env, "联机取物异步状态无效");
        break;
    }
}

}  // namespace

int QueuePalletItemRequests(JNIEnv* env, jobject player,
                            const std::string& full_type, int quantity,
                            std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "联机取物尚未初始化";
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle) {
        detail = "上一组联机取物请求仍在执行";
        return 0;
    }

    jstring type = env->NewStringUTF(full_type.c_str());
    jobject sample = type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
              g_bindings.inventory_item_factory, g_bindings.create_item, type);
    DeleteLocalRef(env, type);
    if (sample == nullptr || ClearException(env)) {
        DeleteLocalRef(env, sample);
        detail = "目标不是当前版本中的有效物品类型";
        return 0;
    }
    DeleteLocalRef(env, sample);

    g_pending = PendingRequest{};
    g_pending.target_full_type = full_type;
    g_pending.quantity = std::clamp(quantity, 1, 100);
    if (!SelectSafeCarrier(env, g_pending.carrier_full_type)) {
        ResetPending(env);
        detail = "联机取物没有找到可安全使用的 HandWeapon 载体";
        g_status = detail;
        return 0;
    }
    g_pending.phase = PendingPhase::Preparing;
    g_pending.next_step = std::chrono::steady_clock::now();
    g_pending.deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    detail = "已排队联机取物请求：" +
        std::to_string(g_pending.quantity) + " 件 " + full_type;
    g_status = detail;
    return g_pending.quantity;
}

void UpdatePalletItemBridge(JNIEnv* env, jobject player) {
    if (env == nullptr || player == nullptr || !Initialize(env) ||
        g_pending.phase == PendingPhase::Idle) {
        return;
    }

    if (g_pending.call.queue_item != nullptr) {
        jobject result = nullptr;
        const PendingStep completed_step = g_pending.step;
        const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
            env, g_pending.call, kAsyncCallTimeout, &result);
        if (state == AsyncObjectMethodState::Pending) return;
        if (state != AsyncObjectMethodState::Succeeded) {
            DeleteLocalRef(env, result);
            FailPending(
                env, state == AsyncObjectMethodState::TimedOut
                    ? "联机取物主线程操作超时，已停止本次请求"
                    : "联机取物主线程操作失败，已停止本次请求");
            return;
        }
        g_pending.step = PendingStep::None;
        HandlePendingStep(env, player, completed_step, result);
        DeleteLocalRef(env, result);
        return;
    }

    if (std::chrono::steady_clock::now() >= g_pending.deadline) {
        const std::string message = g_pending.phase == PendingPhase::WaitingForTrap
            ? "服务端未返回联机取物载体，本次操作失败"
            : "联机取物超时，临时载体可能仍在脚下";
        FailPending(env, message);
        return;
    }
    if (std::chrono::steady_clock::now() < g_pending.next_step) return;
    if (!BeginTrapQuery(env, player)) {
        FailPending(env, "联机取物无法排队主线程查询");
    }
}

const std::string& GetPalletItemStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
