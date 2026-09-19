#include "bridge/maintenance_training_bridge.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace pztrainer::bridge {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kInitialCooldown = std::chrono::milliseconds(1500);
constexpr auto kSendInterval = std::chrono::milliseconds(2000);
constexpr auto kAttackCooldown = std::chrono::milliseconds(1500);

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jclass hand_weapon = nullptr;
    jclass iso_grid_square = nullptr;
    jclass array_list = nullptr;
    jmethodID get_primary_hand_item = nullptr;
    jmethodID get_current_square = nullptr;
    jmethodID is_attacking = nullptr;
    jmethodID get_floor = nullptr;
    jmethodID array_list_constructor = nullptr;
    jmethodID send_player_hit = nullptr;
};

Bindings g_bindings;
MaintenanceTrainingStatus g_status;
Clock::time_point g_next_send{};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    if (loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(loader_class, get_system_loader);
    env->DeleteLocalRef(loader_class);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.hand_weapon = LoadGlobalClass(env, "zombie/inventory/types/HandWeapon");
    g_bindings.iso_grid_square = LoadGlobalClass(env, "zombie/iso/IsoGridSquare");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.hand_weapon == nullptr || g_bindings.iso_grid_square == nullptr ||
        g_bindings.array_list == nullptr) {
        return false;
    }

    g_bindings.get_primary_hand_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem", "()Lzombie/inventory/InventoryItem;");
    g_bindings.get_current_square = env->GetMethodID(
        g_bindings.iso_player, "getCurrentSquare", "()Lzombie/iso/IsoGridSquare;");
    g_bindings.is_attacking = env->GetMethodID(
        g_bindings.iso_player, "isAttacking", "()Z");
    g_bindings.get_floor = env->GetMethodID(
        g_bindings.iso_grid_square, "getFloor", "()Lzombie/iso/IsoObject;");
    g_bindings.array_list_constructor = env->GetMethodID(
        g_bindings.array_list, "<init>", "()V");
    g_bindings.send_player_hit = env->GetStaticMethodID(
        g_bindings.game_client, "sendPlayerHit",
        "(Lzombie/characters/IsoGameCharacter;Lzombie/iso/IsoObject;"
        "Lzombie/inventory/types/HandWeapon;FZFZLjava/util/List;ZZZZ)V");
    g_bindings.ready = g_bindings.get_primary_hand_item != nullptr &&
        g_bindings.get_current_square != nullptr && g_bindings.is_attacking != nullptr &&
        g_bindings.get_floor != nullptr && g_bindings.array_list_constructor != nullptr &&
        g_bindings.send_player_hit != nullptr && !ClearException(env);
    return g_bindings.ready;
}

}  // namespace

bool StartMaintenanceTraining(float current_xp, float amount) {
    if (amount <= 0.0f) return false;
    g_status.active = true;
    g_status.packets_sent = 0;
    g_status.target_xp = current_xp + amount;
    g_status.message = "低频维修训练已开始；训练期间不要普通攻击";
    g_next_send = Clock::now() + kInitialCooldown;
    return true;
}

void StopMaintenanceTraining(const char* message) {
    g_status.active = false;
    g_status.message = message == nullptr ? "维修训练已停止" : message;
}

void UpdateMaintenanceTraining(JNIEnv* env, jobject player, float current_xp) {
    if (!g_status.active) return;
    if (current_xp >= g_status.target_xp) {
        StopMaintenanceTraining("维修训练已达到目标经验");
        return;
    }
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        StopMaintenanceTraining("维修训练接口初始化失败");
        return;
    }

    const auto now = Clock::now();
    const jboolean attacking = env->CallBooleanMethod(player, g_bindings.is_attacking);
    if (ClearException(env)) {
        StopMaintenanceTraining("无法读取玩家攻击状态");
        return;
    }
    if (attacking == JNI_TRUE) {
        g_next_send = now + kAttackCooldown;
        g_status.message = "检测到普通攻击，维修训练正在冷却";
        return;
    }
    if (now < g_next_send) return;

    jobject weapon = env->CallObjectMethod(player, g_bindings.get_primary_hand_item);
    jobject square = env->CallObjectMethod(player, g_bindings.get_current_square);
    jobject floor = square == nullptr
        ? nullptr
        : env->CallObjectMethod(square, g_bindings.get_floor);
    jobject tracers = env->NewObject(
        g_bindings.array_list, g_bindings.array_list_constructor);
    const bool valid = weapon != nullptr && square != nullptr && floor != nullptr &&
        tracers != nullptr && env->IsInstanceOf(weapon, g_bindings.hand_weapon) &&
        !ClearException(env);
    if (!valid) {
        if (tracers != nullptr) env->DeleteLocalRef(tracers);
        if (floor != nullptr) env->DeleteLocalRef(floor);
        if (square != nullptr) env->DeleteLocalRef(square);
        if (weapon != nullptr) env->DeleteLocalRef(weapon);
        g_status.message = "请在主手装备有耐久的近战武器";
        g_next_send = now + kSendInterval;
        return;
    }

    env->CallStaticVoidMethod(
        g_bindings.game_client, g_bindings.send_player_hit,
        player, floor, weapon, 0.0f, JNI_TRUE, 0.0f, JNI_FALSE, tracers,
        JNI_FALSE, JNI_FALSE, JNI_FALSE, JNI_FALSE);
    const bool sent = !ClearException(env);
    env->DeleteLocalRef(tracers);
    env->DeleteLocalRef(floor);
    env->DeleteLocalRef(square);
    env->DeleteLocalRef(weapon);
    if (!sent) {
        StopMaintenanceTraining("维修训练请求发送失败");
        return;
    }

    ++g_status.packets_sent;
    g_status.message = "低频请求已提交；经验以服务器稍后同步为准";
    g_next_send = now + kSendInterval;
}

const MaintenanceTrainingStatus& GetMaintenanceTrainingStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
