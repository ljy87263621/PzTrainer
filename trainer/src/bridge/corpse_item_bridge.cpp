#include "bridge/corpse_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

constexpr std::chrono::milliseconds kAsyncCallTimeout{2000};

enum class PendingStep {
    None,
    QuerySquare,
    QueryCell,
    ConstructZombie,
    SetZombieSquare,
    SetZombieCurrent,
    SetZombieX,
    SetZombieY,
    SetZombieZ,
    DressZombie,
    ConstructBody,
    QueryContainer,
    AddItems,
    SetExplored,
    AddCorpse,
};

struct Bindings {
    bool ready = false;
    jclass inventory_item_factory = nullptr;
    jclass iso_dead_body = nullptr;
    jclass iso_zombie = nullptr;
    jclass iso_grid_square = nullptr;
    jclass list = nullptr;
    jclass integer_class = nullptr;
    jclass boolean_class = nullptr;
    jfieldID boolean_false = nullptr;
    jfieldID boolean_true = nullptr;
    jmethodID create_item = nullptr;
    jmethodID square_x = nullptr;
    jmethodID square_y = nullptr;
    jmethodID square_z = nullptr;
    jmethodID list_size = nullptr;
    jmethodID integer_value_of = nullptr;
};

struct PendingRequest {
    bool active = false;
    std::vector<std::pair<std::string, int>> items;
    std::size_t item_index = 0;
    int total_quantity = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::chrono::steady_clock::time_point deadline{};
    PendingStep step = PendingStep::None;
    AsyncObjectMethodCall call;
    jobject square = nullptr;
    jobject cell = nullptr;
    jobject zombie = nullptr;
    jobject body = nullptr;
    jobject container = nullptr;
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
    DeleteGlobalRef(env, g_pending.square);
    DeleteGlobalRef(env, g_pending.cell);
    DeleteGlobalRef(env, g_pending.zombie);
    DeleteGlobalRef(env, g_pending.body);
    DeleteGlobalRef(env, g_pending.container);
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
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemCorpsePayloadBindings");
    g_bindings.inventory_item_factory = LoadGlobalClass(
        env, "zombie/inventory/InventoryItemFactory");
    g_bindings.iso_dead_body = LoadGlobalClass(
        env, "zombie/iso/objects/IsoDeadBody");
    g_bindings.iso_zombie = LoadGlobalClass(
        env, "zombie/characters/IsoZombie");
    g_bindings.iso_grid_square = LoadGlobalClass(
        env, "zombie/iso/IsoGridSquare");
    g_bindings.list = LoadGlobalClass(env, "java/util/List");
    g_bindings.integer_class = LoadGlobalClass(env, "java/lang/Integer");
    g_bindings.boolean_class = LoadGlobalClass(env, "java/lang/Boolean");
    if (g_bindings.inventory_item_factory == nullptr ||
        g_bindings.iso_dead_body == nullptr ||
        g_bindings.iso_zombie == nullptr ||
        g_bindings.iso_grid_square == nullptr || g_bindings.list == nullptr ||
        g_bindings.integer_class == nullptr ||
        g_bindings.boolean_class == nullptr) {
        return false;
    }

    g_bindings.boolean_false = env->GetStaticFieldID(
        g_bindings.boolean_class, "FALSE", "Ljava/lang/Boolean;");
    g_bindings.boolean_true = env->GetStaticFieldID(
        g_bindings.boolean_class, "TRUE", "Ljava/lang/Boolean;");
    g_bindings.create_item = env->GetStaticMethodID(
        g_bindings.inventory_item_factory, "CreateItem",
        "(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
    g_bindings.square_x = env->GetMethodID(
        g_bindings.iso_grid_square, "getX", "()I");
    g_bindings.square_y = env->GetMethodID(
        g_bindings.iso_grid_square, "getY", "()I");
    g_bindings.square_z = env->GetMethodID(
        g_bindings.iso_grid_square, "getZ", "()I");
    g_bindings.list_size = env->GetMethodID(
        g_bindings.list, "size", "()I");
    g_bindings.integer_value_of = env->GetStaticMethodID(
        g_bindings.integer_class, "valueOf", "(I)Ljava/lang/Integer;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.boolean_false != nullptr &&
        g_bindings.boolean_true != nullptr &&
        g_bindings.create_item != nullptr && g_bindings.square_x != nullptr &&
        g_bindings.square_y != nullptr && g_bindings.square_z != nullptr &&
        g_bindings.list_size != nullptr &&
        g_bindings.integer_value_of != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

bool StoreGlobalRef(JNIEnv* env, jobject value, jobject& destination) {
    DeleteGlobalRef(env, destination);
    if (value == nullptr) return false;
    destination = env->NewGlobalRef(value);
    return destination != nullptr && !ClearException(env);
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

jobject BoxInteger(JNIEnv* env, int value) {
    jobject boxed = env->CallStaticObjectMethod(
        g_bindings.integer_class, g_bindings.integer_value_of,
        static_cast<jint>(value));
    if (ClearException(env)) {
        DeleteLocalRef(env, boxed);
        return nullptr;
    }
    return boxed;
}

bool QueueCurrentPayloadItem(JNIEnv* env) {
    if (g_pending.item_index >= g_pending.items.size()) return false;
    const auto& item = g_pending.items[g_pending.item_index];
    jstring item_type = env->NewStringUTF(item.first.c_str());
    jobject quantity = BoxInteger(env, item.second);
    const bool queued = item_type != nullptr && quantity != nullptr &&
        !ClearException(env) && QueuePendingCall(
            env, g_pending.container, "AddItems", {item_type, quantity},
            PendingStep::AddItems);
    DeleteLocalRef(env, quantity);
    DeleteLocalRef(env, item_type);
    return queued;
}

bool QueueZombieCoordinate(JNIEnv* env, const char* method_name, float value,
                           PendingStep step) {
    jobject boxed = BoxFloat(env, value);
    const bool queued = boxed != nullptr && QueuePendingCall(
        env, g_pending.zombie, method_name, {boxed}, step);
    DeleteLocalRef(env, boxed);
    return queued;
}

void HandlePendingStep(JNIEnv* env, PendingStep step, jobject result) {
    switch (step) {
    case PendingStep::QuerySquare:
        if (!StoreGlobalRef(env, result, g_pending.square)) {
            FailPending(env, "尸体载荷无法读取玩家所在格子");
            break;
        }
        g_pending.x = static_cast<float>(env->CallIntMethod(
            g_pending.square, g_bindings.square_x)) + 0.5f;
        g_pending.y = static_cast<float>(env->CallIntMethod(
            g_pending.square, g_bindings.square_y)) + 0.5f;
        g_pending.z = static_cast<float>(env->CallIntMethod(
            g_pending.square, g_bindings.square_z));
        if (ClearException(env) || !QueuePendingCall(
                env, g_pending.square, "getCell", {}, PendingStep::QueryCell)) {
            FailPending(env, "尸体载荷无法读取当前地图单元");
        }
        break;
    case PendingStep::QueryCell:
        if (!StoreGlobalRef(env, result, g_pending.cell) ||
            !QueuePendingCall(
                env, g_bindings.iso_zombie, "new", {g_pending.cell},
                PendingStep::ConstructZombie)) {
            FailPending(env, "尸体载荷无法构造僵尸来源");
        }
        break;
    case PendingStep::ConstructZombie:
        if (!StoreGlobalRef(env, result, g_pending.zombie) ||
            !QueuePendingCall(
                env, g_pending.zombie, "setSquare", {g_pending.square},
                PendingStep::SetZombieSquare)) {
            FailPending(env, "尸体载荷无法设置僵尸所在格子");
        }
        break;
    case PendingStep::SetZombieSquare:
        if (!QueuePendingCall(
                env, g_pending.zombie, "setCurrent", {g_pending.square},
                PendingStep::SetZombieCurrent)) {
            FailPending(env, "尸体载荷无法设置僵尸当前位置");
        }
        break;
    case PendingStep::SetZombieCurrent:
        if (!QueueZombieCoordinate(
                env, "setX", g_pending.x, PendingStep::SetZombieX)) {
            FailPending(env, "尸体载荷无法设置僵尸 X 坐标");
        }
        break;
    case PendingStep::SetZombieX:
        if (!QueueZombieCoordinate(
                env, "setY", g_pending.y, PendingStep::SetZombieY)) {
            FailPending(env, "尸体载荷无法设置僵尸 Y 坐标");
        }
        break;
    case PendingStep::SetZombieY:
        if (!QueueZombieCoordinate(
                env, "setZ", g_pending.z, PendingStep::SetZombieZ)) {
            FailPending(env, "尸体载荷无法设置僵尸 Z 坐标");
        }
        break;
    case PendingStep::SetZombieZ:
        if (!QueuePendingCall(
                env, g_pending.zombie, "dressInRandomOutfit", {},
                PendingStep::DressZombie)) {
            FailPending(env, "尸体载荷无法生成僵尸外观");
        }
        break;
    case PendingStep::DressZombie: {
        jobject false_value = env->GetStaticObjectField(
            g_bindings.boolean_class, g_bindings.boolean_false);
        const bool queued = false_value != nullptr && !ClearException(env) &&
            QueuePendingCall(
                env, g_bindings.iso_dead_body, "new",
                {g_pending.zombie, false_value, false_value},
                PendingStep::ConstructBody);
        DeleteLocalRef(env, false_value);
        if (!queued) FailPending(env, "尸体载荷无法构造僵尸尸体");
        break;
    }
    case PendingStep::ConstructBody:
        if (!StoreGlobalRef(env, result, g_pending.body) ||
            !QueuePendingCall(
                env, g_pending.body, "getContainer", {},
                PendingStep::QueryContainer)) {
            FailPending(env, "尸体载荷无法读取僵尸尸体容器");
        }
        break;
    case PendingStep::QueryContainer: {
        if (!StoreGlobalRef(env, result, g_pending.container)) {
            FailPending(env, "尸体载荷没有获得有效僵尸尸体容器");
            break;
        }
        if (!QueueCurrentPayloadItem(env)) {
            FailPending(env, "尸体载荷无法写入第一种目标物品");
        }
        break;
    }
    case PendingStep::AddItems: {
        const auto& item = g_pending.items[g_pending.item_index];
        const int created = result == nullptr
            ? 0
            : env->CallIntMethod(result, g_bindings.list_size);
        if (ClearException(env) || created != item.second) {
            FailPending(
                env, "尸体载荷写入数量与请求不一致：" + item.first);
            break;
        }
        ++g_pending.item_index;
        if (g_pending.item_index < g_pending.items.size()) {
            if (!QueueCurrentPayloadItem(env)) {
                FailPending(env, "尸体载荷无法继续写入下一种目标物品");
            }
            break;
        }
        jobject true_value = env->GetStaticObjectField(
            g_bindings.boolean_class, g_bindings.boolean_true);
        const bool queued = true_value != nullptr && !ClearException(env) &&
            QueuePendingCall(
                env, g_pending.container, "setExplored", {true_value},
                PendingStep::SetExplored);
        DeleteLocalRef(env, true_value);
        if (!queued) FailPending(env, "尸体载荷无法锁定服务器战利品扫描");
        break;
    }
    case PendingStep::SetExplored: {
        jobject false_value = env->GetStaticObjectField(
            g_bindings.boolean_class, g_bindings.boolean_false);
        const bool queued = false_value != nullptr && !ClearException(env) &&
            QueuePendingCall(
                env, g_pending.square, "addCorpse",
                {g_pending.body, false_value}, PendingStep::AddCorpse);
        DeleteLocalRef(env, false_value);
        if (!queued) FailPending(env, "尸体载荷无法发送到服务器");
        break;
    }
    case PendingStep::AddCorpse: {
        const int total_quantity = g_pending.total_quantity;
        const std::size_t type_count = g_pending.items.size();
        ResetPending(env);
        g_status = "尸体载荷已发送：脚下僵尸尸体包含 " +
            std::to_string(type_count) + " 种、共 " +
            std::to_string(total_quantity) + " 件物品";
        break;
    }
    default:
        FailPending(env, "尸体载荷异步状态无效");
        break;
    }
}

}  // namespace

int QueueCorpsePayloadRequests(
    JNIEnv* env, jobject player,
    const std::vector<std::pair<std::string, int>>& items,
    std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "尸体载荷桥接尚未初始化";
        return 0;
    }
    if (g_pending.active) {
        detail = "上一组尸体载荷仍在执行";
        return 0;
    }
    if (items.empty()) {
        detail = "尸体载荷清单为空";
        return 0;
    }

    std::vector<std::pair<std::string, int>> normalized;
    normalized.reserve(items.size());
    int total_quantity = 0;
    for (const auto& item : items) {
        if (item.first.empty()) continue;
        jstring type = env->NewStringUTF(item.first.c_str());
        jobject sample = type == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item, type);
        DeleteLocalRef(env, type);
        if (sample == nullptr || ClearException(env)) {
            DeleteLocalRef(env, sample);
            detail = "目标不是当前版本中的有效物品类型：" + item.first;
            return 0;
        }
        DeleteLocalRef(env, sample);
        const int quantity = std::clamp(item.second, 1, 100);
        normalized.emplace_back(item.first, quantity);
        total_quantity += quantity;
    }
    if (normalized.empty()) {
        detail = "尸体载荷清单中没有有效物品";
        return 0;
    }

    g_pending = PendingRequest{};
    g_pending.active = true;
    g_pending.items = std::move(normalized);
    g_pending.total_quantity = total_quantity;
    g_pending.deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(12) +
        std::chrono::milliseconds(g_pending.items.size() * 250);
    if (!QueuePendingCall(
            env, player, "getCurrentSquare", {}, PendingStep::QuerySquare)) {
        ResetPending(env);
        detail = "尸体载荷无法排队主线程操作";
        g_status = detail;
        return 0;
    }

    detail = "已排队尸体载荷：" +
        std::to_string(g_pending.items.size()) + " 种、共 " +
        std::to_string(g_pending.total_quantity) + " 件物品";
    g_status = detail;
    return g_pending.total_quantity;
}

void UpdateCorpsePayloadBridge(JNIEnv* env, jobject player) {
    (void)player;
    if (env == nullptr || !Initialize(env) || !g_pending.active) return;

    if (std::chrono::steady_clock::now() >= g_pending.deadline) {
        FailPending(env, "尸体载荷主线程操作超时，本次请求已停止");
        return;
    }
    if (g_pending.call.queue_item == nullptr) {
        FailPending(env, "尸体载荷没有待处理的主线程步骤");
        return;
    }

    jobject result = nullptr;
    std::string error;
    const PendingStep completed_step = g_pending.step;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, g_pending.call, kAsyncCallTimeout, &result, &error);
    if (state == AsyncObjectMethodState::Pending) return;
    if (state != AsyncObjectMethodState::Succeeded) {
        DeleteLocalRef(env, result);
        std::string message = state == AsyncObjectMethodState::TimedOut
            ? "尸体载荷主线程步骤超时"
            : "尸体载荷主线程步骤失败";
        if (!error.empty()) message += "：" + error;
        FailPending(env, std::move(message));
        return;
    }

    g_pending.step = PendingStep::None;
    HandlePendingStep(env, completed_step, result);
    DeleteLocalRef(env, result);
}

const std::string& GetCorpsePayloadStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
