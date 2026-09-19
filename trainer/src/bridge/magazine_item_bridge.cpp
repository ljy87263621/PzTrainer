#include "bridge/magazine_item_bridge.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

enum class PendingPhase {
    Idle,
    WaitingToEject,
    WaitingForCompletion,
};

struct Bindings {
    bool ready = false;
    jclass inventory_item = nullptr;
    jclass hand_weapon = nullptr;
    jclass object = nullptr;
    jclass sync_hand_weapon_fields_packet = nullptr;
    jclass net_timed_action_packet = nullptr;
    jclass packet_type = nullptr;
    jfieldID sync_hand_weapon_fields_type = nullptr;
    jmethodID item_id = nullptr;
    jmethodID magazine_type = nullptr;
    jmethodID current_ammo_count = nullptr;
    jmethodID set_current_ammo_count = nullptr;
    jmethodID uses_external_magazine = nullptr;
    jmethodID contains_clip = nullptr;
    jmethodID set_contains_clip = nullptr;
};

struct PendingRequest {
    PendingPhase phase = PendingPhase::Idle;
    std::string magazine_full_type;
    int weapon_id = -1;
    int requested = 0;
    int remaining = 0;
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
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemMagazineBindings");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.sync_hand_weapon_fields_packet = LoadGlobalClass(
        env, "zombie/network/packets/SyncHandWeaponFieldsPacket");
    g_bindings.net_timed_action_packet = LoadGlobalClass(
        env, "zombie/network/packets/NetTimedActionPacket");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    if (g_bindings.inventory_item == nullptr ||
        g_bindings.hand_weapon == nullptr || g_bindings.object == nullptr ||
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
    g_bindings.magazine_type = env->GetMethodID(
        g_bindings.hand_weapon, "getMagazineType", "()Ljava/lang/String;");
    g_bindings.current_ammo_count = env->GetMethodID(
        g_bindings.inventory_item, "getCurrentAmmoCount", "()I");
    g_bindings.set_current_ammo_count = env->GetMethodID(
        g_bindings.inventory_item, "setCurrentAmmoCount", "(I)V");
    g_bindings.uses_external_magazine = env->GetMethodID(
        g_bindings.hand_weapon, "usesExternalMagazine", "()Z");
    g_bindings.contains_clip = env->GetMethodID(
        g_bindings.hand_weapon, "isContainsClip", "()Z");
    g_bindings.set_contains_clip = env->GetMethodID(
        g_bindings.hand_weapon, "setContainsClip", "(Z)V");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.sync_hand_weapon_fields_type != nullptr &&
        g_bindings.item_id != nullptr && g_bindings.magazine_type != nullptr &&
        g_bindings.current_ammo_count != nullptr &&
        g_bindings.set_current_ammo_count != nullptr &&
        g_bindings.uses_external_magazine != nullptr &&
        g_bindings.contains_clip != nullptr &&
        g_bindings.set_contains_clip != nullptr;
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

bool SendSyncPacket(JNIEnv* env, jobject packet) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, g_bindings.sync_hand_weapon_fields_type);
    const bool sent = packet_type != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "sendToServer", {packet_type});
    DeleteLocalRef(env, packet_type);
    return sent;
}

bool MatchesMagazine(JNIEnv* env, jobject weapon,
                     const std::string& magazine_full_type) {
    if (weapon == nullptr ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE) {
        return false;
    }
    jstring type = static_cast<jstring>(env->CallObjectMethod(
        weapon, g_bindings.magazine_type));
    const std::string value = JavaString(env, type);
    DeleteLocalRef(env, type);
    return !ClearException(env) && value == magazine_full_type;
}

jobject GetEquippedWeapon(JNIEnv* env, jobject player) {
    bool invoked = false;
    jobject weapon = InvokeObjectMethodOnMainThread(
        env, player, "getPrimaryHandItem", {}, &invoked);
    if (!invoked || weapon == nullptr ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE) {
        DeleteLocalRef(env, weapon);
        return nullptr;
    }
    return weapon;
}

bool SyncInsertedEmptyMagazine(JNIEnv* env, jobject player, jobject weapon) {
    env->CallVoidMethod(
        weapon, g_bindings.set_current_ammo_count, 0);
    env->CallVoidMethod(
        weapon, g_bindings.set_contains_clip, JNI_TRUE);
    if (ClearException(env)) return false;

    bool invoked = false;
    jobject packet = InvokeObjectMethodOnMainThread(
        env, g_bindings.sync_hand_weapon_fields_packet, "new", {}, &invoked);
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (arguments != nullptr) {
        env->SetObjectArrayElement(arguments, 0, player);
        env->SetObjectArrayElement(arguments, 1, weapon);
    }
    const bool prepared = invoked && packet != nullptr && arguments != nullptr &&
        !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "setData", {arguments});
    const bool sent = prepared && SendSyncPacket(env, packet);
    DeleteLocalRef(env, arguments);
    DeleteLocalRef(env, packet);
    return sent;
}

bool StartEjectAction(JNIEnv* env, jobject player, jobject weapon) {
    jstring action_type = env->NewStringUTF("ISEjectMagazine");
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (arguments != nullptr) {
        env->SetObjectArrayElement(arguments, 0, player);
        env->SetObjectArrayElement(arguments, 1, weapon);
    }
    const bool started = action_type != nullptr && arguments != nullptr &&
        !ClearException(env) &&
        InvokeIgnoringResult(
            env, g_bindings.net_timed_action_packet, "createNewAndSend",
            {action_type, player, arguments});
    DeleteLocalRef(env, arguments);
    DeleteLocalRef(env, action_type);
    return started;
}

bool PrepareNext(JNIEnv* env, jobject player, jobject weapon) {
    if (!SyncInsertedEmptyMagazine(env, player, weapon)) return false;
    g_pending.phase = PendingPhase::WaitingToEject;
    g_pending.next_step =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
    g_pending.deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(12);
    return true;
}

}  // namespace

int QueueWeaponMagazineExtraction(JNIEnv* env, jobject player,
                                  const std::string& magazine_full_type,
                                  int quantity, std::string& detail) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        detail = "弹匣拆出桥接尚未初始化";
        return 0;
    }
    if (g_pending.phase != PendingPhase::Idle) {
        detail = "上一组弹匣仍在执行原生退匣动作";
        return 0;
    }

    jobject weapon = GetEquippedWeapon(env, player);
    if (weapon == nullptr || !MatchesMagazine(env, weapon, magazine_full_type)) {
        DeleteLocalRef(env, weapon);
        detail = "请先把使用该弹匣的空枪装备到主手";
        return 0;
    }
    const bool external = env->CallBooleanMethod(
        weapon, g_bindings.uses_external_magazine) == JNI_TRUE;
    const bool contains_clip = env->CallBooleanMethod(
        weapon, g_bindings.contains_clip) == JNI_TRUE;
    const int ammo_count = env->CallIntMethod(
        weapon, g_bindings.current_ammo_count);
    if (ClearException(env) || !external || contains_clip || ammo_count != 0) {
        DeleteLocalRef(env, weapon);
        detail = "主手枪必须为空枪、弹量为 0 且已取下原有弹匣";
        return 0;
    }

    g_pending.magazine_full_type = magazine_full_type;
    g_pending.weapon_id = env->CallIntMethod(weapon, g_bindings.item_id);
    g_pending.requested = std::clamp(quantity, 1, 100);
    g_pending.remaining = g_pending.requested;
    if (ClearException(env) || !PrepareNext(env, player, weapon)) {
        env->CallVoidMethod(weapon, g_bindings.set_contains_clip, JNI_FALSE);
        ClearException(env);
        g_pending = PendingRequest{};
        DeleteLocalRef(env, weapon);
        detail = "弹匣状态同步包构造或发送失败";
        return 0;
    }
    DeleteLocalRef(env, weapon);
    detail = "已写入空弹匣状态，等待服务端执行原生退匣";
    g_status = detail;
    return g_pending.requested;
}

void UpdateWeaponMagazineExtraction(JNIEnv* env, jobject player) {
    if (env == nullptr || player == nullptr || !Initialize(env) ||
        g_pending.phase == PendingPhase::Idle ||
        std::chrono::steady_clock::now() < g_pending.next_step) {
        return;
    }
    if (std::chrono::steady_clock::now() > g_pending.deadline) {
        g_status = "弹匣拆出超时；确认兼容空枪仍装备在主手";
        g_pending = PendingRequest{};
        return;
    }

    jobject weapon = GetEquippedWeapon(env, player);
    const int weapon_id = weapon == nullptr
        ? -1
        : env->CallIntMethod(weapon, g_bindings.item_id);
    if (weapon == nullptr || ClearException(env) ||
        weapon_id != g_pending.weapon_id ||
        !MatchesMagazine(env, weapon, g_pending.magazine_full_type)) {
        DeleteLocalRef(env, weapon);
        g_status = "等待兼容空枪重新装备到主手";
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        return;
    }

    if (g_pending.phase == PendingPhase::WaitingToEject) {
        if (!StartEjectAction(env, player, weapon)) {
            g_status = "弹匣状态已同步，但原生退匣动作提交失败";
            g_pending = PendingRequest{};
        } else {
            g_pending.phase = PendingPhase::WaitingForCompletion;
            g_pending.next_step =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
            g_status = "服务端正在创建并退回真实弹匣";
        }
        DeleteLocalRef(env, weapon);
        return;
    }

    const bool contains_clip = env->CallBooleanMethod(
        weapon, g_bindings.contains_clip) == JNI_TRUE;
    if (!ClearException(env) && !contains_clip) {
        --g_pending.remaining;
        const int completed = g_pending.requested - g_pending.remaining;
        if (g_pending.remaining <= 0) {
            g_status = "完成：服务端已生成 " +
                std::to_string(completed) + " 个 " +
                g_pending.magazine_full_type;
            g_pending = PendingRequest{};
        } else if (!PrepareNext(env, player, weapon)) {
            g_status = "已生成部分弹匣，但下一次状态同步失败";
            g_pending = PendingRequest{};
        } else {
            g_status = "已生成 " + std::to_string(completed) +
                " 个弹匣，继续执行";
        }
    } else {
        g_pending.next_step =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    }
    DeleteLocalRef(env, weapon);
}

const std::string& GetWeaponMagazineExtractionStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
