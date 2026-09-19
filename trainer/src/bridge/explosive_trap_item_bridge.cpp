#include "bridge/explosive_trap_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

enum class PendingPhase {
    Idle,
    WaitingForTrap,
    WaitingForPickup,
    WaitingForDetach,
};

enum class RequestKind {
    Weapon,
    WeaponPart,
};

struct Bindings {
    bool ready = false;
    jclass inventory_item_factory = nullptr;
    jclass hand_weapon = nullptr;
    jclass weapon_part = nullptr;
    jclass inventory_item = nullptr;
    jclass item_container = nullptr;
    jclass list = nullptr;
    jclass object = nullptr;
    jclass iso_trap = nullptr;
    jclass add_explosive_trap_packet = nullptr;
    jclass sync_item_fields_packet = nullptr;
    jclass net_timed_action_packet = nullptr;
    jclass packet_type = nullptr;
    jfieldID add_explosive_trap_type = nullptr;
    jfieldID sync_item_fields_type = nullptr;
    jmethodID create_item = nullptr;
    jmethodID item_id = nullptr;
    jmethodID explosion_range = nullptr;
    jmethodID explosion_power = nullptr;
    jmethodID fire_range = nullptr;
    jmethodID fire_starting_energy = nullptr;
    jmethodID fire_starting_chance = nullptr;
    jmethodID smoke_range = nullptr;
    jmethodID noise_range = nullptr;
    jmethodID sensor_range = nullptr;
    jmethodID set_remote_control_id = nullptr;
    jmethodID remote_control_id = nullptr;
    jmethodID item_full_type = nullptr;
    jmethodID weapon_part_mount_on = nullptr;
    jmethodID weapon_part_type = nullptr;
    jmethodID set_weapon_part = nullptr;
    jmethodID get_weapon_part = nullptr;
    jmethodID inventory_item_by_id = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID trap_item = nullptr;
};

struct PendingRequest {
    PendingPhase phase = PendingPhase::Idle;
    RequestKind kind = RequestKind::Weapon;
    std::string output_full_type;
    std::string carrier_full_type;
    std::string part_type;
    int remaining = 0;
    int item_id = -1;
    std::chrono::steady_clock::time_point deadline{};
    std::chrono::steady_clock::time_point next_step{};
};

Bindings g_bindings;
PendingRequest g_pending;
std::string g_weapon_status;
std::string g_part_status;

std::string& ActiveStatus() {
    return g_pending.kind == RequestKind::WeaponPart
        ? g_part_status
        : g_weapon_status;
}

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

void DeleteLocalRef(JNIEnv* env, jobject value) {
    if (value != nullptr) env->DeleteLocalRef(value);
}

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* text = env->GetStringUTFChars(value, nullptr);
    if (text == nullptr || ClearException(env)) return {};
    std::string result(text);
    env->ReleaseStringUTFChars(value, text);
    return result;
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
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemExplosiveTrapBindings");
    g_bindings.inventory_item_factory = LoadGlobalClass(
        env, "zombie/inventory/InventoryItemFactory");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.weapon_part = LoadGlobalClass(
        env, "zombie/inventory/types/WeaponPart");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.item_container = LoadGlobalClass(
        env, "zombie/inventory/ItemContainer");
    g_bindings.list = LoadGlobalClass(env, "java/util/List");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.iso_trap = LoadGlobalClass(env, "zombie/iso/objects/IsoTrap");
    g_bindings.add_explosive_trap_packet = LoadGlobalClass(
        env, "zombie/network/packets/AddExplosiveTrapPacket");
    g_bindings.sync_item_fields_packet = LoadGlobalClass(
        env, "zombie/network/packets/SyncItemFieldsPacket");
    g_bindings.net_timed_action_packet = LoadGlobalClass(
        env, "zombie/network/packets/NetTimedActionPacket");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    if (g_bindings.inventory_item_factory == nullptr ||
        g_bindings.hand_weapon == nullptr || g_bindings.weapon_part == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.item_container == nullptr || g_bindings.list == nullptr ||
        g_bindings.object == nullptr ||
        g_bindings.iso_trap == nullptr ||
        g_bindings.add_explosive_trap_packet == nullptr ||
        g_bindings.sync_item_fields_packet == nullptr ||
        g_bindings.net_timed_action_packet == nullptr ||
        g_bindings.packet_type == nullptr) {
        return false;
    }

    g_bindings.add_explosive_trap_type = env->GetStaticFieldID(
        g_bindings.packet_type, "AddExplosiveTrap",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.sync_item_fields_type = env->GetStaticFieldID(
        g_bindings.packet_type, "SyncItemFields",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.create_item = env->GetStaticMethodID(
        g_bindings.inventory_item_factory, "CreateItem",
        "(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
    g_bindings.item_id = env->GetMethodID(
        g_bindings.inventory_item, "getID", "()I");
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
    g_bindings.set_remote_control_id = env->GetMethodID(
        g_bindings.inventory_item, "setRemoteControlID", "(I)V");
    g_bindings.remote_control_id = env->GetMethodID(
        g_bindings.inventory_item, "getRemoteControlID", "()I");
    g_bindings.item_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.weapon_part_mount_on = env->GetMethodID(
        g_bindings.weapon_part, "getMountOn", "()Ljava/util/List;");
    g_bindings.weapon_part_type = env->GetMethodID(
        g_bindings.weapon_part, "getPartType", "()Ljava/lang/String;");
    g_bindings.set_weapon_part = env->GetMethodID(
        g_bindings.hand_weapon, "setWeaponPart",
        "(Lzombie/inventory/types/WeaponPart;)V");
    g_bindings.get_weapon_part = env->GetMethodID(
        g_bindings.hand_weapon, "getWeaponPart",
        "(Ljava/lang/String;)Lzombie/inventory/types/WeaponPart;");
    g_bindings.inventory_item_by_id = env->GetMethodID(
        g_bindings.item_container, "getItemWithID",
        "(I)Lzombie/inventory/InventoryItem;");
    g_bindings.list_size = env->GetMethodID(g_bindings.list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.list, "get", "(I)Ljava/lang/Object;");
    g_bindings.trap_item = env->GetMethodID(
        g_bindings.iso_trap, "getItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.add_explosive_trap_type != nullptr &&
        g_bindings.create_item != nullptr && g_bindings.item_id != nullptr &&
        g_bindings.explosion_range != nullptr &&
        g_bindings.explosion_power != nullptr && g_bindings.fire_range != nullptr &&
        g_bindings.fire_starting_energy != nullptr &&
        g_bindings.fire_starting_chance != nullptr &&
        g_bindings.smoke_range != nullptr && g_bindings.noise_range != nullptr &&
        g_bindings.sensor_range != nullptr &&
        g_bindings.set_remote_control_id != nullptr &&
        g_bindings.remote_control_id != nullptr &&
        g_bindings.item_full_type != nullptr &&
        g_bindings.weapon_part_mount_on != nullptr &&
        g_bindings.weapon_part_type != nullptr &&
        g_bindings.set_weapon_part != nullptr &&
        g_bindings.get_weapon_part != nullptr &&
        g_bindings.sync_item_fields_type != nullptr &&
        g_bindings.inventory_item_by_id != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.trap_item != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

bool InvokeIgnoringResult(JNIEnv* env, jobject target, const char* method_name,
                          std::initializer_list<jobject> arguments) {
    bool invoked = false;
    jobject result = InvokeObjectMethodOnMainThread(
        env, target, method_name, arguments, &invoked);
    DeleteLocalRef(env, result);
    return invoked;
}

bool HasHazardousTrapEffect(JNIEnv* env, jobject weapon) {
    const int explosion_range = env->CallIntMethod(weapon, g_bindings.explosion_range);
    const int explosion_power = env->CallIntMethod(weapon, g_bindings.explosion_power);
    const int fire_range = env->CallIntMethod(weapon, g_bindings.fire_range);
    const int fire_energy = env->CallIntMethod(weapon, g_bindings.fire_starting_energy);
    const int fire_chance = env->CallIntMethod(weapon, g_bindings.fire_starting_chance);
    const int smoke_range = env->CallIntMethod(weapon, g_bindings.smoke_range);
    const int noise_range = env->CallIntMethod(weapon, g_bindings.noise_range);
    const int sensor_range = env->CallIntMethod(weapon, g_bindings.sensor_range);
    if (ClearException(env)) return true;
    return explosion_range > 0 || explosion_power > 0 || fire_range > 0 ||
        fire_energy > 0 || fire_chance > 0 || smoke_range > 0 ||
        noise_range > 0 || sensor_range > 0;
}

bool SendPacket(JNIEnv* env, jobject packet, jfieldID packet_type_field) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, packet_type_field);
    const bool sent = packet_type != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "sendToServer", {packet_type});
    DeleteLocalRef(env, packet_type);
    return sent;
}

bool SendNext(JNIEnv* env, jobject player) {
    jstring type = env->NewStringUTF(g_pending.carrier_full_type.c_str());
    jobject item = type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
              g_bindings.inventory_item_factory, g_bindings.create_item, type);
    DeleteLocalRef(env, type);
    if (item == nullptr || ClearException(env) ||
        env->IsInstanceOf(item, g_bindings.hand_weapon) != JNI_TRUE) {
        DeleteLocalRef(env, item);
        g_pending = PendingRequest{};
        return false;
    }
    if (g_pending.kind == RequestKind::WeaponPart) {
        jstring part_type = env->NewStringUTF(g_pending.output_full_type.c_str());
        jobject part = part_type == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item,
                  part_type);
        DeleteLocalRef(env, part_type);
        if (part == nullptr || ClearException(env) ||
            env->IsInstanceOf(part, g_bindings.weapon_part) != JNI_TRUE) {
            DeleteLocalRef(env, part);
            DeleteLocalRef(env, item);
            g_pending = PendingRequest{};
            return false;
        }
        env->CallVoidMethod(item, g_bindings.set_weapon_part, part);
        DeleteLocalRef(env, part);
        if (ClearException(env)) {
            DeleteLocalRef(env, item);
            g_pending = PendingRequest{};
            return false;
        }
    }
    env->CallVoidMethod(item, g_bindings.set_remote_control_id, 0x505A);
    if (ClearException(env)) {
        DeleteLocalRef(env, item);
        g_pending = PendingRequest{};
        return false;
    }

    bool invoked = false;
    jobject square = InvokeObjectMethodOnMainThread(
        env, player, "getCurrentSquare", {}, &invoked);
    jobject packet = invoked && square != nullptr
        ? InvokeObjectMethodOnMainThread(
              env, g_bindings.add_explosive_trap_packet, "new", {}, &invoked)
        : nullptr;
    const int item_id = env->CallIntMethod(item, g_bindings.item_id);
    const bool prepared = !ClearException(env) && invoked && packet != nullptr &&
        InvokeIgnoringResult(env, packet, "set", {item, player, square});
    const bool sent = prepared && SendPacket(
        env, packet, g_bindings.add_explosive_trap_type);
    DeleteLocalRef(env, packet);
    DeleteLocalRef(env, square);
    DeleteLocalRef(env, item);
    if (!sent) {
        g_pending = PendingRequest{};
        return false;
    }

    g_pending.item_id = item_id;
    g_pending.phase = PendingPhase::WaitingForTrap;
    g_pending.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    ActiveStatus() = g_pending.kind == RequestKind::WeaponPart
        ? "数据包已发送，等待服务端返回带配件的承载武器"
        : "数据包已发送，等待服务端返回落地武器";
    return true;
}

jobject FindPendingTrap(JNIEnv* env, jobject player) {
    bool invoked = false;
    jobject square = InvokeObjectMethodOnMainThread(
        env, player, "getCurrentSquare", {}, &invoked);
    jobject objects = invoked && square != nullptr
        ? InvokeObjectMethodOnMainThread(env, square, "getObjects", {}, &invoked)
        : nullptr;
    DeleteLocalRef(env, square);
    if (!invoked || objects == nullptr) {
        DeleteLocalRef(env, objects);
        return nullptr;
    }

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
            bool item_matches = g_pending.item_id >= 0 &&
                item_id == g_pending.item_id;
            if (item != nullptr && g_pending.item_id < 0 && !ClearException(env)) {
                const int remote_id = env->CallIntMethod(
                    item, g_bindings.remote_control_id);
                jstring full_type = static_cast<jstring>(env->CallObjectMethod(
                    item, g_bindings.item_full_type));
                const char* text = full_type == nullptr
                    ? nullptr
                    : env->GetStringUTFChars(full_type, nullptr);
                item_matches = !ClearException(env) && remote_id == 0x505A &&
                    text != nullptr && g_pending.carrier_full_type == text;
                if (text != nullptr) env->ReleaseStringUTFChars(full_type, text);
                DeleteLocalRef(env, full_type);
            }
            DeleteLocalRef(env, item);
            if (!ClearException(env) && item_matches) {
                g_pending.item_id = item_id;
                match = candidate;
                break;
            }
        }
        DeleteLocalRef(env, candidate);
    }
    DeleteLocalRef(env, objects);
    return match;
}

bool StartTakeTrapAction(JNIEnv* env, jobject player, jobject trap) {
    jstring action_type = env->NewStringUTF("ISTakeTrap");
    jobjectArray action_arguments = env->NewObjectArray(
        2, g_bindings.object, nullptr);
    if (action_arguments != nullptr) {
        env->SetObjectArrayElement(action_arguments, 0, player);
        env->SetObjectArrayElement(action_arguments, 1, trap);
    }
    const bool started = action_type != nullptr &&
        action_arguments != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(
            env, g_bindings.net_timed_action_packet, "createNewAndSend",
            {action_type, player, action_arguments});
    DeleteLocalRef(env, action_arguments);
    DeleteLocalRef(env, action_type);
    return started;
}

bool StartRemoveWeaponPartAction(JNIEnv* env, jobject player, jobject weapon) {
    jstring action_type = env->NewStringUTF("ISRemoveWeaponUpgrade");
    jstring part_type = env->NewStringUTF(g_pending.part_type.c_str());
    jobjectArray action_arguments = env->NewObjectArray(
        3, g_bindings.object, nullptr);
    if (action_arguments != nullptr) {
        env->SetObjectArrayElement(action_arguments, 0, player);
        env->SetObjectArrayElement(action_arguments, 1, weapon);
        env->SetObjectArrayElement(action_arguments, 2, part_type);
    }
    const bool started = action_type != nullptr && part_type != nullptr &&
        action_arguments != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(
            env, g_bindings.net_timed_action_packet, "createNewAndSend",
            {action_type, player, action_arguments});
    DeleteLocalRef(env, action_arguments);
    DeleteLocalRef(env, part_type);
    DeleteLocalRef(env, action_type);
    return started;
}

bool ClearTemporaryRemoteId(JNIEnv* env, jobject player, jobject item) {
    env->CallVoidMethod(item, g_bindings.set_remote_control_id, -1);
    if (ClearException(env)) return false;

    jobjectArray packet_arguments = env->NewObjectArray(
        2, g_bindings.object, nullptr);
    if (packet_arguments != nullptr) {
        env->SetObjectArrayElement(packet_arguments, 0, player);
        env->SetObjectArrayElement(packet_arguments, 1, item);
    }
    bool invoked = false;
    jobject packet = packet_arguments == nullptr || ClearException(env)
        ? nullptr
        : InvokeObjectMethodOnMainThread(
        env, g_bindings.sync_item_fields_packet, "new", {}, &invoked);
    const bool prepared = invoked && packet != nullptr &&
        InvokeIgnoringResult(env, packet, "setData", {packet_arguments});
    const bool sent = prepared && SendPacket(
        env, packet, g_bindings.sync_item_fields_type);
    DeleteLocalRef(env, packet_arguments);
    DeleteLocalRef(env, packet);
    return sent;
}

}  // namespace

int QueueExplosiveTrapWeaponRequests(JNIEnv* env, jobject player,
                                     const std::string& full_type, int quantity,
                                     std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "武器陷阱桥接尚未初始化";
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle || g_pending.remaining > 0) {
        detail = "上一组武器陷阱请求仍在回收中";
        return 0;
    }

    jstring type = env->NewStringUTF(full_type.c_str());
    jobject sample = type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
              g_bindings.inventory_item_factory, g_bindings.create_item, type);
    DeleteLocalRef(env, type);
    if (sample == nullptr || ClearException(env) ||
        env->IsInstanceOf(sample, g_bindings.hand_weapon) != JNI_TRUE) {
        DeleteLocalRef(env, sample);
        detail = "武器陷阱方案只接受 HandWeapon 类物品";
        return 0;
    }
    const bool hazardous = HasHazardousTrapEffect(env, sample);
    DeleteLocalRef(env, sample);
    if (hazardous) {
        detail = "该物品具有真实爆炸、燃烧、烟雾或诱敌参数，已拒绝发送";
        return 0;
    }

    g_pending.kind = RequestKind::Weapon;
    g_pending.output_full_type = full_type;
    g_pending.carrier_full_type = full_type;
    g_pending.remaining = std::clamp(quantity, 1, 100);
    g_pending.item_id = -1;
    jobject existing_trap = FindPendingTrap(env, player);
    if (existing_trap != nullptr) {
        const bool started = StartTakeTrapAction(env, player, existing_trap);
        DeleteLocalRef(env, existing_trap);
        if (!started) {
            g_pending = PendingRequest{};
            detail = "已找到脚下旧武器，但内置回收动作提交失败";
            g_weapon_status = detail;
            return 0;
        }
        g_pending.phase = PendingPhase::WaitingForPickup;
        g_pending.deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(5);
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        detail = "已找到脚下此前留下的同类武器，正在优先回收";
        g_weapon_status = detail;
        return quantity;
    }
    if (!SendNext(env, player)) {
        detail = "武器陷阱包构造或发送失败";
        return 0;
    }
    detail = "已排队 " + std::to_string(quantity) +
        " 件武器；服务端落地后会自动执行内置回收动作";
    g_weapon_status = detail;
    return quantity;
}

int QueueExplosiveTrapWeaponPartRequests(JNIEnv* env, jobject player,
                                         const std::string& full_type,
                                         int quantity, std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "武器配件桥接尚未初始化";
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle || g_pending.remaining > 0) {
        detail = "另一组武器或配件请求仍在执行";
        return 0;
    }

    jstring type = env->NewStringUTF(full_type.c_str());
    jobject part = type == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
              g_bindings.inventory_item_factory, g_bindings.create_item, type);
    DeleteLocalRef(env, type);
    if (part == nullptr || ClearException(env) ||
        env->IsInstanceOf(part, g_bindings.weapon_part) != JNI_TRUE) {
        DeleteLocalRef(env, part);
        detail = "配件拆出方案只接受 WeaponPart 类物品";
        return 0;
    }

    jobject mounts = env->CallObjectMethod(part, g_bindings.weapon_part_mount_on);
    jstring part_type = static_cast<jstring>(env->CallObjectMethod(
        part, g_bindings.weapon_part_type));
    const std::string part_slot = JavaString(env, part_type);
    DeleteLocalRef(env, part_type);
    std::string carrier_type;
    const jint mount_count = mounts == nullptr
        ? 0
        : env->CallIntMethod(mounts, g_bindings.list_size);
    for (jint index = 0; index < mount_count && !ClearException(env); ++index) {
        jstring mount = static_cast<jstring>(env->CallObjectMethod(
            mounts, g_bindings.list_get, index));
        const std::string candidate_type = JavaString(env, mount);
        DeleteLocalRef(env, mount);
        if (candidate_type.empty()) continue;
        jstring candidate_name = env->NewStringUTF(candidate_type.c_str());
        jobject candidate = candidate_name == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item,
                  candidate_name);
        DeleteLocalRef(env, candidate_name);
        const bool compatible = candidate != nullptr && !ClearException(env) &&
            env->IsInstanceOf(candidate, g_bindings.hand_weapon) == JNI_TRUE &&
            !HasHazardousTrapEffect(env, candidate);
        DeleteLocalRef(env, candidate);
        if (compatible) {
            carrier_type = candidate_type;
            break;
        }
    }
    DeleteLocalRef(env, mounts);
    DeleteLocalRef(env, part);
    if (ClearException(env) || carrier_type.empty() || part_slot.empty()) {
        detail = "该配件没有可安全用于陷阱序列化的承载武器";
        return 0;
    }

    g_pending.kind = RequestKind::WeaponPart;
    g_pending.output_full_type = full_type;
    g_pending.carrier_full_type = carrier_type;
    g_pending.part_type = part_slot;
    g_pending.remaining = std::clamp(quantity, 1, 100);
    g_pending.item_id = -1;
    if (!SendNext(env, player)) {
        detail = "带配件承载武器的数据包构造或发送失败";
        return 0;
    }
    detail = "已排队 " + std::to_string(quantity) +
        " 个配件；每个配件会额外留下承载武器 " + carrier_type;
    g_part_status = detail;
    return quantity;
}

void UpdateExplosiveTrapWeaponBridge(JNIEnv* env, jobject player) {
    if (env == nullptr || player == nullptr || !Initialize(env) ||
        g_pending.phase == PendingPhase::Idle ||
        std::chrono::steady_clock::now() < g_pending.next_step) {
        return;
    }
    if (std::chrono::steady_clock::now() > g_pending.deadline) {
        if (g_pending.phase == PendingPhase::WaitingForTrap) {
            ActiveStatus() = "超时：服务端已收包，但客户端没有收到对应落地对象";
        } else if (g_pending.phase == PendingPhase::WaitingForDetach) {
            ActiveStatus() = "超时：承载武器已回收，但原生拆配件动作未完成";
        } else {
            ActiveStatus() = "超时：回收动作未把武器加入背包";
        }
        g_pending = PendingRequest{};
        return;
    }

    if (g_pending.phase == PendingPhase::WaitingForTrap) {
        jobject trap = FindPendingTrap(env, player);
        if (trap == nullptr) return;
        const bool started = StartTakeTrapAction(env, player, trap);
        DeleteLocalRef(env, trap);
        if (!started) {
            ActiveStatus() = "已找到服务端落地对象，但内置回收动作提交失败";
            g_pending = PendingRequest{};
            return;
        }
        ActiveStatus() = "已找到服务端落地对象，正在执行内置回收动作";
        g_pending.phase = PendingPhase::WaitingForPickup;
        g_pending.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        return;
    }

    bool invoked = false;
    jobject inventory = InvokeObjectMethodOnMainThread(
        env, player, "getInventory", {}, &invoked);
    jobject item = invoked && inventory != nullptr
        ? env->CallObjectMethod(
              inventory, g_bindings.inventory_item_by_id, g_pending.item_id)
        : nullptr;
    const bool picked_up = item != nullptr && !ClearException(env);
    if (!picked_up) {
        DeleteLocalRef(env, item);
        DeleteLocalRef(env, inventory);
        return;
    }

    if (g_pending.phase == PendingPhase::WaitingForDetach) {
        jstring part_type = env->NewStringUTF(g_pending.part_type.c_str());
        jobject part = part_type == nullptr
            ? nullptr
            : env->CallObjectMethod(
                  item, g_bindings.get_weapon_part, part_type);
        DeleteLocalRef(env, part_type);
        const bool detached = part == nullptr && !ClearException(env);
        DeleteLocalRef(env, part);
        DeleteLocalRef(env, item);
        DeleteLocalRef(env, inventory);
        if (!detached) {
            g_pending.next_step = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(250);
            return;
        }
        --g_pending.remaining;
        if (g_pending.remaining <= 0) {
            g_part_status = "配件已由服务端拆入背包；承载武器仍保留";
            g_pending = PendingRequest{};
            return;
        }
        g_pending.next_step = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(250);
        if (!SendNext(env, player)) {
            g_part_status = "已生成部分配件，但下一件承载武器发送失败";
        }
        return;
    }

    const bool normalized = ClearTemporaryRemoteId(env, player, item);
    if (!normalized) {
        DeleteLocalRef(env, item);
        DeleteLocalRef(env, inventory);
        return;
    }

    if (g_pending.kind == RequestKind::WeaponPart) {
        const bool started = StartRemoveWeaponPartAction(env, player, item);
        DeleteLocalRef(env, item);
        DeleteLocalRef(env, inventory);
        if (!started) {
            g_part_status = "承载武器已回收，但原生拆配件动作提交失败";
            g_pending = PendingRequest{};
            return;
        }
        g_part_status = "承载武器已回收，服务端正在拆下真实配件";
        g_pending.phase = PendingPhase::WaitingForDetach;
        g_pending.deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(12);
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        return;
    }

    DeleteLocalRef(env, item);
    DeleteLocalRef(env, inventory);

    --g_pending.remaining;
    if (g_pending.remaining <= 0) {
        g_weapon_status = "武器已进入背包，临时遥控字段已在服务端恢复";
        g_pending = PendingRequest{};
        return;
    }
    g_pending.next_step =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    SendNext(env, player);
}

const std::string& GetExplosiveTrapWeaponStatus() {
    return g_weapon_status;
}

const std::string& GetExplosiveTrapWeaponPartStatus() {
    return g_part_status;
}

}  // namespace pztrainer::bridge
