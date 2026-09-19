#include "bridge/ammo_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

enum class PendingPhase {
    Idle,
    WaitingToUnload,
    WaitingForCompletion,
};

struct Bindings {
    bool ready = false;
    jclass inventory_item = nullptr;
    jclass hand_weapon = nullptr;
    jclass inventory_container = nullptr;
    jclass item_container = nullptr;
    jclass list = nullptr;
    jclass object = nullptr;
    jclass ammo_type = nullptr;
    jclass sync_hand_weapon_fields_packet = nullptr;
    jclass net_timed_action_packet = nullptr;
    jclass packet_type = nullptr;
    jfieldID sync_hand_weapon_fields_type = nullptr;
    jmethodID item_id = nullptr;
    jmethodID item_full_type = nullptr;
    jmethodID container_items = nullptr;
    jmethodID nested_container = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID weapon_ammo_type = nullptr;
    jmethodID ammo_item_key = nullptr;
    jmethodID current_ammo_count = nullptr;
    jmethodID set_current_ammo_count = nullptr;
    jmethodID uses_external_magazine = nullptr;
    jmethodID contains_clip = nullptr;
};

struct PendingRequest {
    PendingPhase phase = PendingPhase::Idle;
    std::string ammo_full_type;
    std::string weapon_full_type;
    int weapon_id = -1;
    int quantity = 0;
    std::chrono::steady_clock::time_point next_step{};
    std::chrono::steady_clock::time_point deadline{};
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

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* text = env->GetStringUTFChars(value, nullptr);
    if (text == nullptr || ClearException(env)) return {};
    std::string result(text);
    env->ReleaseStringUTFChars(value, text);
    return result;
}

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemAmmoBindings");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.inventory_container = LoadGlobalClass(
        env, "zombie/inventory/types/InventoryContainer");
    g_bindings.item_container = LoadGlobalClass(
        env, "zombie/inventory/ItemContainer");
    g_bindings.list = LoadGlobalClass(env, "java/util/List");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.ammo_type = LoadGlobalClass(
        env, "zombie/scripting/objects/AmmoType");
    g_bindings.sync_hand_weapon_fields_packet = LoadGlobalClass(
        env, "zombie/network/packets/SyncHandWeaponFieldsPacket");
    g_bindings.net_timed_action_packet = LoadGlobalClass(
        env, "zombie/network/packets/NetTimedActionPacket");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    if (g_bindings.inventory_item == nullptr ||
        g_bindings.hand_weapon == nullptr ||
        g_bindings.inventory_container == nullptr ||
        g_bindings.item_container == nullptr || g_bindings.list == nullptr ||
        g_bindings.object == nullptr || g_bindings.ammo_type == nullptr ||
        g_bindings.sync_hand_weapon_fields_packet == nullptr ||
        g_bindings.net_timed_action_packet == nullptr ||
        g_bindings.packet_type == nullptr) {
        return false;
    }

    g_bindings.sync_hand_weapon_fields_type = env->GetStaticFieldID(
        g_bindings.packet_type, "SyncHandWeaponFields",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.item_id = env->GetMethodID(
        g_bindings.inventory_item, "getID", "()I");
    g_bindings.item_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.container_items = env->GetMethodID(
        g_bindings.item_container, "getItems", "()Ljava/util/ArrayList;");
    g_bindings.nested_container = env->GetMethodID(
        g_bindings.inventory_container, "getItemContainer",
        "()Lzombie/inventory/ItemContainer;");
    g_bindings.list_size = env->GetMethodID(g_bindings.list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.list, "get", "(I)Ljava/lang/Object;");
    g_bindings.weapon_ammo_type = env->GetMethodID(
        g_bindings.hand_weapon, "getAmmoType",
        "()Lzombie/scripting/objects/AmmoType;");
    g_bindings.ammo_item_key = env->GetMethodID(
        g_bindings.ammo_type, "getItemKey", "()Ljava/lang/String;");
    g_bindings.current_ammo_count = env->GetMethodID(
        g_bindings.inventory_item, "getCurrentAmmoCount", "()I");
    g_bindings.set_current_ammo_count = env->GetMethodID(
        g_bindings.inventory_item, "setCurrentAmmoCount", "(I)V");
    g_bindings.uses_external_magazine = env->GetMethodID(
        g_bindings.hand_weapon, "usesExternalMagazine", "()Z");
    g_bindings.contains_clip = env->GetMethodID(
        g_bindings.hand_weapon, "isContainsClip", "()Z");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.sync_hand_weapon_fields_type != nullptr &&
        g_bindings.item_id != nullptr && g_bindings.item_full_type != nullptr &&
        g_bindings.container_items != nullptr &&
        g_bindings.nested_container != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.weapon_ammo_type != nullptr &&
        g_bindings.ammo_item_key != nullptr &&
        g_bindings.current_ammo_count != nullptr &&
        g_bindings.set_current_ammo_count != nullptr &&
        g_bindings.uses_external_magazine != nullptr &&
        g_bindings.contains_clip != nullptr;
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

bool SendPacket(JNIEnv* env, jobject packet) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, g_bindings.sync_hand_weapon_fields_type);
    const bool sent = packet_type != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "sendToServer", {packet_type});
    DeleteLocalRef(env, packet_type);
    return sent;
}

bool WeaponMatchesAmmo(JNIEnv* env, jobject weapon,
                       const std::string& ammo_full_type) {
    jobject ammo_type = env->CallObjectMethod(
        weapon, g_bindings.weapon_ammo_type);
    jstring item_key = ammo_type == nullptr
        ? nullptr
        : static_cast<jstring>(env->CallObjectMethod(
              ammo_type, g_bindings.ammo_item_key));
    const std::string key = JavaString(env, item_key);
    DeleteLocalRef(env, item_key);
    DeleteLocalRef(env, ammo_type);
    return !ClearException(env) && key == ammo_full_type;
}

jobject FindWeaponInContainer(JNIEnv* env, jobject container,
                              const std::string& ammo_full_type, int item_id,
                              int depth) {
    if (container == nullptr || depth > 8) return nullptr;
    jobject items = env->CallObjectMethod(container, g_bindings.container_items);
    if (items == nullptr || ClearException(env)) {
        DeleteLocalRef(env, items);
        return nullptr;
    }

    const jint count = env->CallIntMethod(items, g_bindings.list_size);
    for (jint index = 0; index < count && !ClearException(env); ++index) {
        jobject item = env->CallObjectMethod(items, g_bindings.list_get, index);
        if (item == nullptr || ClearException(env)) {
            DeleteLocalRef(env, item);
            continue;
        }
        if (env->IsInstanceOf(item, g_bindings.hand_weapon) == JNI_TRUE) {
            const int candidate_id = env->CallIntMethod(item, g_bindings.item_id);
            const bool id_matches = item_id < 0 || candidate_id == item_id;
            const bool ammo_matches = id_matches &&
                WeaponMatchesAmmo(env, item, ammo_full_type);
            const int ammo_count = ammo_matches
                ? env->CallIntMethod(item, g_bindings.current_ammo_count)
                : -1;
            const bool external_magazine = ammo_matches &&
                env->CallBooleanMethod(
                    item, g_bindings.uses_external_magazine) == JNI_TRUE;
            const bool has_clip = external_magazine &&
                env->CallBooleanMethod(item, g_bindings.contains_clip) == JNI_TRUE;
            if (!ClearException(env) && ammo_matches &&
                (item_id >= 0 || (ammo_count == 0 && !has_clip))) {
                DeleteLocalRef(env, items);
                return item;
            }
        }
        if (env->IsInstanceOf(item, g_bindings.inventory_container) == JNI_TRUE) {
            jobject nested = env->CallObjectMethod(
                item, g_bindings.nested_container);
            jobject match = FindWeaponInContainer(
                env, nested, ammo_full_type, item_id, depth + 1);
            DeleteLocalRef(env, nested);
            if (match != nullptr) {
                DeleteLocalRef(env, item);
                DeleteLocalRef(env, items);
                return match;
            }
        }
        DeleteLocalRef(env, item);
    }
    DeleteLocalRef(env, items);
    return nullptr;
}

bool SyncWeapon(JNIEnv* env, jobject player, jobject weapon) {
    env->CallVoidMethod(
        weapon, g_bindings.set_current_ammo_count, g_pending.quantity);
    if (ClearException(env)) return false;

    bool invoked = false;
    jobject packet = InvokeObjectMethodOnMainThread(
        env, g_bindings.sync_hand_weapon_fields_packet, "new", {}, &invoked);
    jobjectArray packet_arguments = env->NewObjectArray(
        2, g_bindings.object, nullptr);
    if (packet_arguments != nullptr) {
        env->SetObjectArrayElement(packet_arguments, 0, player);
        env->SetObjectArrayElement(packet_arguments, 1, weapon);
    }
    const bool prepared = invoked && packet != nullptr &&
        packet_arguments != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "setData", {packet_arguments});
    const bool sent = prepared && SendPacket(env, packet);
    DeleteLocalRef(env, packet_arguments);
    DeleteLocalRef(env, packet);
    return sent;
}

bool StartUnloadAction(JNIEnv* env, jobject player, jobject weapon) {
    jstring action_type = env->NewStringUTF("ISUnloadBulletsFromFirearm");
    jobjectArray action_arguments = env->NewObjectArray(
        2, g_bindings.object, nullptr);
    if (action_arguments != nullptr) {
        env->SetObjectArrayElement(action_arguments, 0, player);
        env->SetObjectArrayElement(action_arguments, 1, weapon);
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

}  // namespace

int QueueWeaponAmmoExtraction(JNIEnv* env, jobject player,
                              const std::string& ammo_full_type, int quantity,
                              std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "弹药拆出桥接尚未初始化";
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle) {
        detail = "上一组弹药仍在执行原生卸弹动作";
        return 0;
    }

    bool invoked = false;
    jobject inventory = InvokeObjectMethodOnMainThread(
        env, player, "getInventory", {}, &invoked);
    jobject weapon = invoked && inventory != nullptr
        ? FindWeaponInContainer(env, inventory, ammo_full_type, -1, 0)
        : nullptr;
    if (weapon == nullptr) {
        DeleteLocalRef(env, inventory);
        detail = "背包中没有同口径的空枪；外置弹匣枪需先取下弹匣";
        return 0;
    }

    g_pending.ammo_full_type = ammo_full_type;
    g_pending.quantity = std::clamp(quantity, 1, 100);
    g_pending.weapon_id = env->CallIntMethod(weapon, g_bindings.item_id);
    jstring weapon_type = static_cast<jstring>(env->CallObjectMethod(
        weapon, g_bindings.item_full_type));
    g_pending.weapon_full_type = JavaString(env, weapon_type);
    DeleteLocalRef(env, weapon_type);
    const bool synced = !ClearException(env) && SyncWeapon(env, player, weapon);
    if (!synced) {
        env->CallVoidMethod(weapon, g_bindings.set_current_ammo_count, 0);
        ClearException(env);
        g_pending = PendingRequest{};
        DeleteLocalRef(env, weapon);
        DeleteLocalRef(env, inventory);
        detail = "弹量同步包构造或发送失败";
        return 0;
    }

    const auto now = std::chrono::steady_clock::now();
    g_pending.phase = PendingPhase::WaitingToUnload;
    g_pending.next_step = now + std::chrono::milliseconds(350);
    g_pending.deadline = now + std::chrono::seconds(
        std::clamp(15 + g_pending.quantity, 20, 120));
    detail = "已用 " + g_pending.weapon_full_type + " 写入 " +
        std::to_string(g_pending.quantity) + " 发弹量，等待服务端原生卸弹";
    g_status = detail;
    DeleteLocalRef(env, weapon);
    DeleteLocalRef(env, inventory);
    return g_pending.quantity;
}

void UpdateWeaponAmmoExtraction(JNIEnv* env, jobject player) {
    if (env == nullptr || player == nullptr || !Initialize(env) ||
        g_pending.phase == PendingPhase::Idle ||
        std::chrono::steady_clock::now() < g_pending.next_step) {
        return;
    }
    if (std::chrono::steady_clock::now() > g_pending.deadline) {
        g_status = "弹药拆出超时；请检查该枪是否仍在背包并查看游戏日志";
        g_pending = PendingRequest{};
        return;
    }

    bool invoked = false;
    jobject inventory = InvokeObjectMethodOnMainThread(
        env, player, "getInventory", {}, &invoked);
    jobject weapon = invoked && inventory != nullptr
        ? FindWeaponInContainer(
              env, inventory, g_pending.ammo_full_type,
              g_pending.weapon_id, 0)
        : nullptr;
    if (weapon == nullptr) {
        DeleteLocalRef(env, inventory);
        g_status = "等待同口径武器同步回背包";
        return;
    }

    if (g_pending.phase == PendingPhase::WaitingToUnload) {
        if (!StartUnloadAction(env, player, weapon)) {
            g_status = "弹量已同步，但原生卸弹动作提交失败";
            g_pending = PendingRequest{};
        } else {
            g_pending.phase = PendingPhase::WaitingForCompletion;
            g_pending.next_step =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            g_status = "服务端正在逐发生成并登记 " +
                g_pending.ammo_full_type;
        }
        DeleteLocalRef(env, weapon);
        DeleteLocalRef(env, inventory);
        return;
    }

    const int remaining = env->CallIntMethod(
        weapon, g_bindings.current_ammo_count);
    if (!ClearException(env) && remaining <= 0) {
        g_status = "完成：服务端已生成 " +
            std::to_string(g_pending.quantity) + " 发 " +
            g_pending.ammo_full_type + "，空枪仍保留在背包";
        g_pending = PendingRequest{};
    } else {
        g_status = "服务端正在卸弹，剩余 " + std::to_string(remaining) + " 发";
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    }
    DeleteLocalRef(env, weapon);
    DeleteLocalRef(env, inventory);
}

const std::string& GetWeaponAmmoExtractionStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
