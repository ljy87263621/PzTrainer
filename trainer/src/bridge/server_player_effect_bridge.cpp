#include "bridge/server_player_effect_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "bridge/pallet_item_bridge.hpp"

namespace pztrainer::bridge {
namespace {

enum class MedicationPhase {
    Idle,
    WaitingForItem,
    WaitingForEffect,
};

struct MedicationDefinition {
    const char* id;
    const char* item_type;
    const char* effect_getter;
    const char* delta_getter;
};

constexpr std::array<MedicationDefinition, 4> kMedicationDefinitions{{
    {"SleepingTablet", "Base.PillsSleepingTablets",
     "getSleepingTabletEffect", "getSleepingTabletDelta"},
    {"BetaBlockers", "Base.PillsBeta", "getBetaEffect", "getBetaDelta"},
    {"Antidepressant", "Base.PillsAntiDep",
     "getDepressEffect", "getDepressDelta"},
    {"PainMeds", "Base.Pills", "getPainEffect", "getPainDelta"},
}};

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jclass stats = nullptr;
    jclass character_stat = nullptr;
    jclass inventory_item = nullptr;
    jclass object = nullptr;
    jclass net_timed_action_packet = nullptr;
    jfieldID game_client_instance = nullptr;
    jfieldID thirst_stat = nullptr;
    jfieldID access_level = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_stats = nullptr;
    jmethodID stats_get = nullptr;
    jmethodID stats_set = nullptr;
    jmethodID is_timed_action_instant_cheat = nullptr;
    jmethodID set_timed_action_instant_cheat = nullptr;
    std::array<jmethodID, 4> effect_getters{};
    std::array<jmethodID, 4> delta_getters{};
};

struct PendingMedication {
    MedicationPhase phase = MedicationPhase::Idle;
    std::size_t definition_index = 0;
    float baseline_effect = 0.0f;
    float baseline_delta = 0.0f;
    std::chrono::steady_clock::time_point next_step{};
    std::chrono::steady_clock::time_point deadline{};
};

Bindings g_bindings;
PendingMedication g_pending;
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

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.stats = LoadGlobalClass(env, "zombie/characters/Stats");
    g_bindings.character_stat = LoadGlobalClass(
        env, "zombie/characters/CharacterStat");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.net_timed_action_packet = LoadGlobalClass(
        env, "zombie/network/packets/NetTimedActionPacket");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.stats == nullptr || g_bindings.character_stat == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.object == nullptr || g_bindings.net_timed_action_packet == nullptr) {
        return false;
    }

    g_bindings.game_client_instance = env->GetStaticFieldID(
        g_bindings.game_client, "instance", "Lzombie/network/GameClient;");
    g_bindings.thirst_stat = env->GetStaticFieldID(
        g_bindings.character_stat, "THIRST",
        "Lzombie/characters/CharacterStat;");
    g_bindings.access_level = env->GetFieldID(
        g_bindings.iso_player, "accessLevel", "Ljava/lang/String;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_stats = env->GetMethodID(
        g_bindings.iso_player, "getStats", "()Lzombie/characters/Stats;");
    g_bindings.stats_get = env->GetMethodID(
        g_bindings.stats, "get", "(Lzombie/characters/CharacterStat;)F");
    g_bindings.stats_set = env->GetMethodID(
        g_bindings.stats, "set", "(Lzombie/characters/CharacterStat;F)Z");
    g_bindings.is_timed_action_instant_cheat = env->GetMethodID(
        g_bindings.iso_player, "isTimedActionInstantCheat", "()Z");
    g_bindings.set_timed_action_instant_cheat = env->GetMethodID(
        g_bindings.iso_player, "setTimedActionInstantCheat", "(Z)V");
    for (std::size_t index = 0; index < kMedicationDefinitions.size(); ++index) {
        g_bindings.effect_getters[index] = env->GetMethodID(
            g_bindings.iso_player,
            kMedicationDefinitions[index].effect_getter, "()F");
        g_bindings.delta_getters[index] = env->GetMethodID(
            g_bindings.iso_player,
            kMedicationDefinitions[index].delta_getter, "()F");
    }

    bool medication_ready = true;
    for (std::size_t index = 0; index < kMedicationDefinitions.size(); ++index) {
        medication_ready = medication_ready &&
            g_bindings.effect_getters[index] != nullptr &&
            g_bindings.delta_getters[index] != nullptr;
    }
    g_bindings.ready = !ClearException(env) && medication_ready &&
        g_bindings.game_client_instance != nullptr &&
        g_bindings.thirst_stat != nullptr && g_bindings.access_level != nullptr &&
        g_bindings.get_player != nullptr &&
        g_bindings.get_stats != nullptr && g_bindings.stats_get != nullptr &&
        g_bindings.stats_set != nullptr &&
        g_bindings.is_timed_action_instant_cheat != nullptr &&
        g_bindings.set_timed_action_instant_cheat != nullptr;
    return g_bindings.ready;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

const MedicationDefinition* FindMedication(const std::string& id,
                                            std::size_t* index_out) {
    for (std::size_t index = 0; index < kMedicationDefinitions.size(); ++index) {
        if (id == kMedicationDefinitions[index].id) {
            if (index_out != nullptr) *index_out = index;
            return &kMedicationDefinitions[index];
        }
    }
    return nullptr;
}

jobject GetPlayer(JNIEnv* env) {
    return env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
}

jobject FindInventoryItem(JNIEnv* env, jobject player, const char* full_type) {
    bool invoked = false;
    jobject inventory = InvokeObjectMethodOnMainThread(
        env, player, "getInventory", {}, &invoked);
    jstring item_type = env->NewStringUTF(full_type);
    jobject item = invoked && inventory != nullptr && item_type != nullptr
        ? InvokeObjectMethodOnMainThread(
              env, inventory, "getFirstTypeRecurse", {item_type}, &invoked)
        : nullptr;
    DeleteLocalRef(env, item_type);
    DeleteLocalRef(env, inventory);
    if (!invoked || ClearException(env)) {
        DeleteLocalRef(env, item);
        return nullptr;
    }
    return item;
}

bool StartTakePillAction(JNIEnv* env, jobject player, jobject item,
                         std::size_t definition_index) {
    g_pending.baseline_effect = env->CallFloatMethod(
        player, g_bindings.effect_getters[definition_index]);
    g_pending.baseline_delta = env->CallFloatMethod(
        player, g_bindings.delta_getters[definition_index]);
    if (ClearException(env)) return false;

    jstring action_type = env->NewStringUTF("ISTakePillAction");
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (arguments != nullptr) {
        env->SetObjectArrayElement(arguments, 0, player);
        env->SetObjectArrayElement(arguments, 1, item);
    }

    // Build 42.20.2's vanilla ISTakePillAction asks DrainableComboItem for the
    // Food-only getEatTime() method unless this local construction is instant.
    // The server consumes the serialized action table and does not reconstruct it.
    jobject previous_access = env->GetObjectField(player, g_bindings.access_level);
    const bool previous_instant = env->CallBooleanMethod(
        player, g_bindings.is_timed_action_instant_cheat) == JNI_TRUE;
    jstring temporary_access = env->NewStringUTF("pztrainer-local");
    if (temporary_access != nullptr) {
        env->SetObjectField(player, g_bindings.access_level, temporary_access);
    }
    env->CallVoidMethod(
        player, g_bindings.set_timed_action_instant_cheat, JNI_TRUE);
    bool invoked = false;
    jobject result = action_type != nullptr && arguments != nullptr &&
            !ClearException(env)
        ? InvokeObjectMethodOnMainThread(
              env, g_bindings.net_timed_action_packet, "createNewAndSend",
              {action_type, player, arguments}, &invoked)
        : nullptr;
    const bool invocation_failed = ClearException(env);
    env->CallVoidMethod(
        player, g_bindings.set_timed_action_instant_cheat,
        previous_instant ? JNI_TRUE : JNI_FALSE);
    env->SetObjectField(player, g_bindings.access_level, previous_access);
    const bool restore_failed = ClearException(env);
    DeleteLocalRef(env, temporary_access);
    DeleteLocalRef(env, previous_access);
    DeleteLocalRef(env, result);
    DeleteLocalRef(env, arguments);
    DeleteLocalRef(env, action_type);
    if (!invoked || invocation_failed || restore_failed) return false;

    g_pending.phase = MedicationPhase::WaitingForEffect;
    g_pending.next_step =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    g_pending.deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(15);
    g_status = "已提交原版服药动作，等待服务端回发药效";
    return true;
}

}  // namespace

void UpdateServerPlayerEffectBridge() {
    if (g_pending.phase == MedicationPhase::Idle ||
        std::chrono::steady_clock::now() < g_pending.next_step) {
        return;
    }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return;
    jobject player = GetPlayer(env);
    if (player == nullptr || ClearException(env)) {
        DeleteLocalRef(env, player);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= g_pending.deadline) {
        g_status = g_pending.phase == MedicationPhase::WaitingForItem
            ? "服务端物品未在限定时间内写入背包，请求取消"
            : "原版服务端动作已提交，但未在限定时间内完成";
        g_pending = PendingMedication{};
        DeleteLocalRef(env, player);
        return;
    }

    if (g_pending.phase == MedicationPhase::WaitingForItem) {
        const char* item_type =
            kMedicationDefinitions[g_pending.definition_index].item_type;
        jobject item = FindInventoryItem(env, player, item_type);
        if (item != nullptr) {
            const bool started = StartTakePillAction(
                env, player, item, g_pending.definition_index);
            if (!started) {
                g_status = "物品已进入背包，但原版服务端动作提交失败";
                g_pending = PendingMedication{};
            }
            DeleteLocalRef(env, item);
        } else {
            g_pending.next_step = now + std::chrono::milliseconds(150);
        }
        DeleteLocalRef(env, player);
        return;
    }

    const float effect = env->CallFloatMethod(
        player, g_bindings.effect_getters[g_pending.definition_index]);
    const float delta = env->CallFloatMethod(
        player, g_bindings.delta_getters[g_pending.definition_index]);
    if (!ClearException(env) &&
        (std::fabs(effect - g_pending.baseline_effect) > 0.5f ||
         std::fabs(delta - g_pending.baseline_delta) > 0.0001f)) {
        g_status = "服务端已确认并广播原版药效";
        g_pending = PendingMedication{};
    } else {
        g_pending.next_step = now + std::chrono::milliseconds(150);
    }
    DeleteLocalRef(env, player);
}

bool RequestServerTimedMedication(const std::string& id) {
    if (g_pending.phase != MedicationPhase::Idle) {
        g_status = "上一项服务端服药请求仍在执行";
        return false;
    }
    std::size_t definition_index = 0;
    const MedicationDefinition* definition = FindMedication(id, &definition_index);
    if (definition == nullptr) {
        g_status = "该药效没有对应的原版服药动作";
        return false;
    }

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status = "服务端状态桥接尚未初始化";
        return false;
    }
    jobject player = GetPlayer(env);
    if (player == nullptr || ClearException(env)) {
        DeleteLocalRef(env, player);
        g_status = "联机玩家对象尚未就绪";
        return false;
    }

    g_pending = PendingMedication{};
    g_pending.definition_index = definition_index;
    jobject item = FindInventoryItem(env, player, definition->item_type);
    if (item != nullptr) {
        const bool started = StartTakePillAction(
            env, player, item, definition_index);
        DeleteLocalRef(env, item);
        DeleteLocalRef(env, player);
        if (!started) {
            g_status = "背包中已有对应药物，但原版服药动作提交失败";
            g_pending = PendingMedication{};
        }
        return started;
    }

    std::string detail;
    const int queued = QueuePalletItemRequests(
        env, player, definition->item_type, 1, detail);
    DeleteLocalRef(env, player);
    if (queued != 1) {
        g_status = detail;
        g_pending = PendingMedication{};
        return false;
    }
    g_pending.phase = MedicationPhase::WaitingForItem;
    g_pending.next_step =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(150);
    g_pending.deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(20);
    g_status = "正在取得服务端登记的原版药物";
    return true;
}

bool IsServerTimedMedicationBusy() {
    return g_pending.phase != MedicationPhase::Idle;
}

bool CanSetServerCharacterStat(const std::string& id) {
    return LowerAscii(id) == "thirst";
}

bool SetServerCharacterStat(const std::string& id, float value) {
    if (!CanSetServerCharacterStat(id)) {
        g_status = "该属性尚未找到普通玩家可用的服务端提交路径";
        return false;
    }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status = "服务端状态桥接尚未初始化";
        return false;
    }
    jobject player = GetPlayer(env);
    jobject stats = player == nullptr
        ? nullptr
        : env->CallObjectMethod(player, g_bindings.get_stats);
    jobject thirst = env->GetStaticObjectField(
        g_bindings.character_stat, g_bindings.thirst_stat);
    jobject client = env->GetStaticObjectField(
        g_bindings.game_client, g_bindings.game_client_instance);
    if (player == nullptr || stats == nullptr || thirst == nullptr ||
        client == nullptr || ClearException(env)) {
        DeleteLocalRef(env, client);
        DeleteLocalRef(env, thirst);
        DeleteLocalRef(env, stats);
        DeleteLocalRef(env, player);
        g_status = "无法读取联机玩家的口渴属性";
        return false;
    }

    const float current = env->CallFloatMethod(stats, g_bindings.stats_get, thirst);
    const float target = std::clamp(value, 0.0f, 1.0f);
    jobject amount = BoxFloat(env, current - target);
    bool invoked = false;
    jobject result = amount == nullptr
        ? nullptr
        : InvokeObjectMethodOnMainThread(
              env, client, "drink", {player, amount}, &invoked);
    DeleteLocalRef(env, result);
    DeleteLocalRef(env, amount);
    if (invoked && !ClearException(env)) {
        env->CallBooleanMethod(stats, g_bindings.stats_set, thirst, target);
    }
    const bool succeeded = invoked && !ClearException(env);
    g_status = succeeded
        ? "已通过普通 Drink 包提交口渴值，并同步当前客户端显示"
        : "口渴值的服务端提交失败";
    DeleteLocalRef(env, client);
    DeleteLocalRef(env, thirst);
    DeleteLocalRef(env, stats);
    DeleteLocalRef(env, player);
    return succeeded;
}

bool ResetServerCharacterStat(const std::string& id, float default_value) {
    return SetServerCharacterStat(id, default_value);
}

const std::string& GetServerPlayerEffectStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
