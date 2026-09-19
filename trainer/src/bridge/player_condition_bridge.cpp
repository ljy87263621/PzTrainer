#include "bridge/player_condition_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/player_effect_bridge.hpp"
#include "bridge/server_player_effect_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass iso_player = nullptr;
    jclass body_damage = nullptr;
    jclass body_part = nullptr;
    jclass array_list = nullptr;
    jfieldID client_flag = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_body_damage = nullptr;
    jmethodID get_body_parts = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID body_is_infected = nullptr;
    jmethodID body_set_infected = nullptr;
    jmethodID body_set_infection_time = nullptr;
    jmethodID body_set_mortality_duration = nullptr;
    jmethodID part_bitten = nullptr;
    jmethodID part_set_bitten = nullptr;
    jmethodID part_set_bite_time = nullptr;
    jmethodID part_set_bleeding = nullptr;
    jmethodID part_set_bleeding_time = nullptr;
    jmethodID part_is_infected = nullptr;
    jmethodID part_set_infected = nullptr;
    jmethodID part_is_fake_infected = nullptr;
    jmethodID part_set_fake_infected = nullptr;
    jmethodID part_is_infected_wound = nullptr;
    jmethodID part_get_wound_infection_level = nullptr;
    jmethodID part_set_wound_infection_level = nullptr;
    jmethodID send_player_damage = nullptr;
};

Bindings g_bindings;
PlayerConditionStatus g_status;
bool g_fatigue_requested = false;
bool g_panic_requested = false;
bool g_hunger_requested = false;
bool g_thirst_requested = false;
bool g_negative_moodles_requested = false;
bool g_infection_requested = false;
std::chrono::steady_clock::time_point g_last_body_sync{};

constexpr std::array<const char*, 12> kNegativeMoodleStats{
    "anger", "boredom", "discomfort", "foodsickness", "intoxication",
    "nicotinewithdrawal", "pain", "poison", "sickness", "stress",
    "unhappiness", "wetness"};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
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
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.body_damage = LoadGlobalClass(
        env, "zombie/characters/BodyDamage/BodyDamage");
    g_bindings.body_part = LoadGlobalClass(
        env, "zombie/characters/BodyDamage/BodyPart");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    if (g_bindings.game_client == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.body_damage == nullptr || g_bindings.body_part == nullptr ||
        g_bindings.array_list == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(
        g_bindings.game_client, "client", "Z");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_body_damage = env->GetMethodID(
        g_bindings.iso_player, "getBodyDamage",
        "()Lzombie/characters/BodyDamage/BodyDamage;");
    g_bindings.get_body_parts = env->GetMethodID(
        g_bindings.body_damage, "getBodyParts", "()Ljava/util/ArrayList;");
    g_bindings.list_size = env->GetMethodID(g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.body_is_infected = env->GetMethodID(
        g_bindings.body_damage, "isInfected", "()Z");
    g_bindings.body_set_infected = env->GetMethodID(
        g_bindings.body_damage, "setInfected", "(Z)V");
    g_bindings.body_set_infection_time = env->GetMethodID(
        g_bindings.body_damage, "setInfectionTime", "(F)V");
    g_bindings.body_set_mortality_duration = env->GetMethodID(
        g_bindings.body_damage, "setInfectionMortalityDuration", "(F)V");
    g_bindings.part_bitten = env->GetMethodID(
        g_bindings.body_part, "bitten", "()Z");
    g_bindings.part_set_bitten = env->GetMethodID(
        g_bindings.body_part, "SetBitten", "(Z)V");
    g_bindings.part_set_bite_time = env->GetMethodID(
        g_bindings.body_part, "setBiteTime", "(F)V");
    g_bindings.part_set_bleeding = env->GetMethodID(
        g_bindings.body_part, "setBleeding", "(Z)V");
    g_bindings.part_set_bleeding_time = env->GetMethodID(
        g_bindings.body_part, "setBleedingTime", "(F)V");
    g_bindings.part_is_infected = env->GetMethodID(
        g_bindings.body_part, "IsInfected", "()Z");
    g_bindings.part_set_infected = env->GetMethodID(
        g_bindings.body_part, "SetInfected", "(Z)V");
    g_bindings.part_is_fake_infected = env->GetMethodID(
        g_bindings.body_part, "IsFakeInfected", "()Z");
    g_bindings.part_set_fake_infected = env->GetMethodID(
        g_bindings.body_part, "SetFakeInfected", "(Z)V");
    g_bindings.part_is_infected_wound = env->GetMethodID(
        g_bindings.body_part, "isInfectedWound", "()Z");
    g_bindings.part_get_wound_infection_level = env->GetMethodID(
        g_bindings.body_part, "getWoundInfectionLevel", "()F");
    g_bindings.part_set_wound_infection_level = env->GetMethodID(
        g_bindings.body_part, "setWoundInfectionLevel", "(F)V");
    g_bindings.send_player_damage = env->GetStaticMethodID(
        g_bindings.game_client, "sendPlayerDamage",
        "(Lzombie/characters/IsoPlayer;)V");

    g_bindings.ready = !ClearException(env) &&
        g_bindings.client_flag != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_body_damage != nullptr && g_bindings.get_body_parts != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.body_is_infected != nullptr &&
        g_bindings.body_set_infected != nullptr &&
        g_bindings.body_set_infection_time != nullptr &&
        g_bindings.body_set_mortality_duration != nullptr &&
        g_bindings.part_bitten != nullptr && g_bindings.part_set_bitten != nullptr &&
        g_bindings.part_set_bite_time != nullptr &&
        g_bindings.part_set_bleeding != nullptr &&
        g_bindings.part_set_bleeding_time != nullptr &&
        g_bindings.part_is_infected != nullptr &&
        g_bindings.part_set_infected != nullptr &&
        g_bindings.part_is_fake_infected != nullptr &&
        g_bindings.part_set_fake_infected != nullptr &&
        g_bindings.part_is_infected_wound != nullptr &&
        g_bindings.part_get_wound_infection_level != nullptr &&
        g_bindings.part_set_wound_infection_level != nullptr &&
        g_bindings.send_player_damage != nullptr;
    return g_bindings.ready;
}

const PlayerEffectEntry* FindStat(const PlayerEffectStatus& effects,
                                  const char* id) {
    std::string requested_id(id);
    std::transform(
        requested_id.begin(), requested_id.end(), requested_id.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
    for (const PlayerEffectEntry& entry : effects.entries) {
        std::string entry_id = entry.id;
        std::transform(
            entry_id.begin(), entry_id.end(), entry_id.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        if (entry.kind == PlayerEffectKind::CharacterStat &&
            entry_id == requested_id) {
            return &entry;
        }
    }
    return nullptr;
}

bool ProtectStat(const char* id, bool requested) {
    if (!requested) return false;
    const PlayerEffectEntry* entry = FindStat(GetPlayerEffectStatus(), id);
    if (entry == nullptr || entry->maximum <= entry->minimum) return false;
    const float trigger = entry->minimum +
        (entry->maximum - entry->minimum) * 0.10f;
    const float reset = entry->minimum +
        (entry->maximum - entry->minimum) * 0.01f;
    if (entry->value < trigger) return false;
    if (std::string(id) == "thirst" &&
        !GetPlayerEffectStatus().server_stat_sync_available &&
        CanSetServerCharacterStat(entry->id)) {
        return SetServerCharacterStat(entry->id, reset);
    }
    return SetPlayerCharacterStat(entry->id, reset);
}

bool ClearInfection(JNIEnv* env, jobject player) {
    jobject body_damage = env->CallObjectMethod(
        player, g_bindings.get_body_damage);
    jobject parts = body_damage == nullptr ? nullptr : env->CallObjectMethod(
        body_damage, g_bindings.get_body_parts);
    if (body_damage == nullptr || parts == nullptr || ClearException(env)) {
        if (parts != nullptr) env->DeleteLocalRef(parts);
        if (body_damage != nullptr) env->DeleteLocalRef(body_damage);
        return false;
    }

    bool changed = env->CallBooleanMethod(
        body_damage, g_bindings.body_is_infected) == JNI_TRUE;
    if (changed) {
        env->CallVoidMethod(body_damage, g_bindings.body_set_infected, JNI_FALSE);
        env->CallVoidMethod(body_damage, g_bindings.body_set_infection_time, 0.0f);
        env->CallVoidMethod(
            body_damage, g_bindings.body_set_mortality_duration, 0.0f);
    }
    const jint count = env->CallIntMethod(parts, g_bindings.list_size);
    for (jint index = 0; index < count && !env->ExceptionCheck(); ++index) {
        jobject part = env->CallObjectMethod(parts, g_bindings.list_get, index);
        if (part == nullptr) continue;
        const bool bitten = env->CallBooleanMethod(part, g_bindings.part_bitten) == JNI_TRUE;
        const bool infected = env->CallBooleanMethod(
            part, g_bindings.part_is_infected) == JNI_TRUE;
        const bool fake_infected = env->CallBooleanMethod(
            part, g_bindings.part_is_fake_infected) == JNI_TRUE;
        const bool infected_wound = env->CallBooleanMethod(
            part, g_bindings.part_is_infected_wound) == JNI_TRUE;
        const float wound_level = env->CallFloatMethod(
            part, g_bindings.part_get_wound_infection_level);
        if (bitten || infected || fake_infected || infected_wound ||
            wound_level != 0.0f) {
            changed = true;
            env->CallVoidMethod(part, g_bindings.part_set_bitten, JNI_FALSE);
            env->CallVoidMethod(part, g_bindings.part_set_bite_time, 0.0f);
            env->CallVoidMethod(part, g_bindings.part_set_bleeding, JNI_FALSE);
            env->CallVoidMethod(part, g_bindings.part_set_bleeding_time, 0.0f);
            env->CallVoidMethod(part, g_bindings.part_set_infected, JNI_FALSE);
            env->CallVoidMethod(
                part, g_bindings.part_set_fake_infected, JNI_FALSE);
            env->CallVoidMethod(
                part, g_bindings.part_set_wound_infection_level, 0.0f);
        }
        env->DeleteLocalRef(part);
    }
    const bool failed = ClearException(env);
    env->DeleteLocalRef(parts);
    env->DeleteLocalRef(body_damage);
    return changed && !failed;
}

void SyncPlayerState(JNIEnv* env, jobject player) {
    if (!g_status.multiplayer ||
        std::chrono::steady_clock::now() - g_last_body_sync <
            std::chrono::milliseconds(100)) {
        return;
    }
    env->CallStaticVoidMethod(
        g_bindings.game_client, g_bindings.send_player_damage, player);
    if (!ClearException(env)) {
        ++g_status.server_sync_count;
        g_last_body_sync = std::chrono::steady_clock::now();
    }
}

}  // namespace

void UpdatePlayerConditionBridge() {
    g_status.fatigue_enabled = g_fatigue_requested;
    g_status.panic_enabled = g_panic_requested;
    g_status.hunger_enabled = g_hunger_requested;
    g_status.thirst_enabled = g_thirst_requested;
    g_status.negative_moodles_enabled = g_negative_moodles_requested;
    g_status.infection_immunity_enabled = g_infection_requested;

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.message = "玩家状态保护桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    g_status.multiplayer = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) g_status.multiplayer = false;

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.message = "等待进入存档并创建玩家";
        return;
    }
    g_status.player_ready = true;
    g_status.server_stat_sync_available =
        GetPlayerEffectStatus().server_stat_sync_available;
    const bool local = !g_status.multiplayer;
    g_status.fatigue_available = local || g_status.server_stat_sync_available;
    g_status.panic_available = local || g_status.server_stat_sync_available;
    g_status.hunger_available = local || g_status.server_stat_sync_available;
    g_status.thirst_available = local ||
        g_status.server_stat_sync_available || CanSetServerCharacterStat("thirst");
    g_status.negative_moodles_available =
        local || g_status.server_stat_sync_available;
    g_status.infection_immunity_available = true;
    g_status.fatigue_enabled =
        g_fatigue_requested && g_status.fatigue_available;
    g_status.panic_enabled = g_panic_requested && g_status.panic_available;
    g_status.hunger_enabled = g_hunger_requested && g_status.hunger_available;
    g_status.thirst_enabled = g_thirst_requested && g_status.thirst_available;
    g_status.negative_moodles_enabled =
        g_negative_moodles_requested && g_status.negative_moodles_available;
    g_status.infection_immunity_enabled =
        g_infection_requested && g_status.infection_immunity_available;

    const auto protect = [](const char* id, bool requested) {
        if (ProtectStat(id, requested)) {
            ++g_status.stat_restore_count;
        }
    };
    protect("fatigue", g_status.fatigue_enabled);
    protect("panic", g_status.panic_enabled);
    protect("hunger", g_status.hunger_enabled);
    protect("thirst", g_status.thirst_enabled);
    if (g_status.negative_moodles_enabled) {
        for (const char* id : kNegativeMoodleStats) protect(id, true);
    }
    bool body_changed = false;
    if (g_status.infection_immunity_enabled) {
        protect("zombieinfection", true);
        protect("zombiefever", true);
        body_changed = ClearInfection(env, player);
        if (body_changed) ++g_status.body_restore_count;
    }
    if (body_changed) SyncPlayerState(env, player);
    g_status.message = g_status.infection_immunity_enabled ||
            g_status.negative_moodles_enabled
        ? "玩家状态保护运行中"
        : "玩家状态保护待命";
    env->DeleteLocalRef(player);
}

const PlayerConditionStatus& GetPlayerConditionStatus() {
    return g_status;
}

void SetFatigueProtectionEnabled(bool enabled) { g_fatigue_requested = enabled; }
void SetPanicProtectionEnabled(bool enabled) { g_panic_requested = enabled; }
void SetHungerProtectionEnabled(bool enabled) { g_hunger_requested = enabled; }
void SetThirstProtectionEnabled(bool enabled) { g_thirst_requested = enabled; }
void SetNegativeMoodlesProtectionEnabled(bool enabled) {
    g_negative_moodles_requested = enabled;
}
void SetInfectionImmunityEnabled(bool enabled) { g_infection_requested = enabled; }

bool IsFatigueProtectionRequested() { return g_fatigue_requested; }
bool IsPanicProtectionRequested() { return g_panic_requested; }
bool IsHungerProtectionRequested() { return g_hunger_requested; }
bool IsThirstProtectionRequested() { return g_thirst_requested; }
bool IsNegativeMoodlesProtectionRequested() {
    return g_negative_moodles_requested;
}
bool IsInfectionImmunityRequested() { return g_infection_requested; }

}  // namespace pztrainer::bridge
