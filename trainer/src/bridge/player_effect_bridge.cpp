#include "bridge/player_effect_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <string>
#include <unordered_map>
#include <utility>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::bridge {
namespace {

constexpr float kEffectTicksPerSecond = 30.0f;

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass stats = nullptr;
    jclass character_stat = nullptr;
    jclass role = nullptr;
    jclass capability = nullptr;
    jclass packet_type = nullptr;
    jclass network_packet = nullptr;
    jclass sync_player_stats_packet = nullptr;
    jclass integer = nullptr;
    jclass object = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jfieldID ordered_stats = nullptr;
    jfieldID can_modify_body_stats = nullptr;
    jfieldID sync_player_stats = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_stats = nullptr;
    jmethodID get_role = nullptr;
    jmethodID role_has_capability = nullptr;
    jmethodID stats_get = nullptr;
    jmethodID stats_set = nullptr;
    jmethodID stats_reset = nullptr;
    jmethodID stat_get_id = nullptr;
    jmethodID stat_get_minimum = nullptr;
    jmethodID stat_get_maximum = nullptr;
    jmethodID stat_get_default = nullptr;
    jmethodID stat_bit_mask = nullptr;
    jmethodID integer_value_of = nullptr;
    jmethodID send_packet = nullptr;
    std::array<jmethodID, 4> effect_getters{};
    std::array<jmethodID, 4> delta_getters{};
    std::array<jmethodID, 4> effect_setters{};
    std::array<jmethodID, 4> delta_setters{};
    std::array<jmethodID, 4> native_effect_methods{};
};

struct InternalEntry {
    PlayerEffectKind kind = PlayerEffectKind::CharacterStat;
    jobject handle = nullptr;
    int timed_index = -1;
};

struct TimedDefinition {
    const char* id;
    const char* display_name;
    const char* source_item_id;
};

constexpr std::array<TimedDefinition, 4> kTimedDefinitions{{
    {"SleepingTablet", "安眠药效", "Base.PillsSleepingTablets"},
    {"BetaBlockers", "β 阻滞剂", "Base.PillsBeta"},
    {"Antidepressant", "抗抑郁药", "Base.PillsAntiDep"},
    {"PainMeds", "止痛药效", "Base.Pills"},
}};

Bindings g_bindings;
PlayerEffectStatus g_status;
std::vector<InternalEntry> g_internal_entries;
std::chrono::steady_clock::time_point g_next_refresh{};

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

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (utf == nullptr || ClearException(env)) return {};
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

const char* LocalizedStatName(const std::string& id) {
    static const std::unordered_map<std::string, const char*> names{
        {"anger", "愤怒"}, {"boredom", "无聊"}, {"discomfort", "不适"},
        {"endurance", "耐力"}, {"fatigue", "疲劳"}, {"fitness", "体能"},
        {"foodsickness", "食物中毒"}, {"hunger", "饥饿"},
        {"idleness", "闲置"}, {"intoxication", "醉酒度"},
        {"morale", "士气"}, {"nicotinewithdrawal", "尼古丁戒断"},
        {"pain", "疼痛"}, {"panic", "恐慌"}, {"poison", "中毒"},
        {"sanity", "理智"}, {"sickness", "疾病"}, {"stress", "压力"},
        {"temperature", "体温"}, {"thirst", "口渴"},
        {"unhappiness", "不开心"}, {"wetness", "潮湿"},
        {"zombiefever", "尸变发热"}, {"zombieinfection", "尸毒感染"},
    };
    const auto found = names.find(LowerAscii(id));
    return found == names.end() ? nullptr : found->second;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.game_server = LoadGlobalClass(env, "zombie/network/GameServer");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.stats = LoadGlobalClass(env, "zombie/characters/Stats");
    g_bindings.character_stat = LoadGlobalClass(env, "zombie/characters/CharacterStat");
    g_bindings.role = LoadGlobalClass(env, "zombie/characters/Role");
    g_bindings.capability = LoadGlobalClass(env, "zombie/characters/Capability");
    g_bindings.packet_type = LoadGlobalClass(env, "zombie/network/PacketTypes$PacketType");
    g_bindings.network_packet = LoadGlobalClass(env, "zombie/network/packets/INetworkPacket");
    g_bindings.sync_player_stats_packet = LoadGlobalClass(
        env, "zombie/network/packets/SyncPlayerStatsPacket");
    g_bindings.integer = LoadGlobalClass(env, "java/lang/Integer");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.stats == nullptr ||
        g_bindings.character_stat == nullptr || g_bindings.role == nullptr ||
        g_bindings.capability == nullptr || g_bindings.packet_type == nullptr ||
        g_bindings.network_packet == nullptr ||
        g_bindings.sync_player_stats_packet == nullptr ||
        g_bindings.integer == nullptr || g_bindings.object == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.ordered_stats = env->GetStaticFieldID(
        g_bindings.character_stat, "ORDERED_STATS", "[Lzombie/characters/CharacterStat;");
    g_bindings.can_modify_body_stats = env->GetStaticFieldID(
        g_bindings.capability, "CanModifyBodyStats", "Lzombie/characters/Capability;");
    g_bindings.sync_player_stats = env->GetStaticFieldID(
        g_bindings.packet_type, "SyncPlayerStats", "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_stats = env->GetMethodID(
        g_bindings.iso_player, "getStats", "()Lzombie/characters/Stats;");
    g_bindings.get_role = env->GetMethodID(
        g_bindings.iso_player, "getRole", "()Lzombie/characters/Role;");
    g_bindings.role_has_capability = env->GetMethodID(
        g_bindings.role, "hasCapability", "(Lzombie/characters/Capability;)Z");
    g_bindings.stats_get = env->GetMethodID(
        g_bindings.stats, "get", "(Lzombie/characters/CharacterStat;)F");
    g_bindings.stats_set = env->GetMethodID(
        g_bindings.stats, "set", "(Lzombie/characters/CharacterStat;F)Z");
    g_bindings.stats_reset = env->GetMethodID(
        g_bindings.stats, "reset", "(Lzombie/characters/CharacterStat;)Z");
    g_bindings.stat_get_id = env->GetMethodID(
        g_bindings.character_stat, "getId", "()Ljava/lang/String;");
    g_bindings.stat_get_minimum = env->GetMethodID(
        g_bindings.character_stat, "getMinimumValue", "()F");
    g_bindings.stat_get_maximum = env->GetMethodID(
        g_bindings.character_stat, "getMaximumValue", "()F");
    g_bindings.stat_get_default = env->GetMethodID(
        g_bindings.character_stat, "getDefaultValue", "()F");
    g_bindings.stat_bit_mask = env->GetStaticMethodID(
        g_bindings.sync_player_stats_packet, "getBitMaskForStat",
        "(Lzombie/characters/CharacterStat;)I");
    g_bindings.integer_value_of = env->GetStaticMethodID(
        g_bindings.integer, "valueOf", "(I)Ljava/lang/Integer;");
    g_bindings.send_packet = env->GetStaticMethodID(
        g_bindings.network_packet, "send",
        "(Lzombie/network/PacketTypes$PacketType;[Ljava/lang/Object;)V");

    constexpr std::array<const char*, 4> effect_getters{
        "getSleepingTabletEffect", "getBetaEffect", "getDepressEffect", "getPainEffect"};
    constexpr std::array<const char*, 4> delta_getters{
        "getSleepingTabletDelta", "getBetaDelta", "getDepressDelta", "getPainDelta"};
    constexpr std::array<const char*, 4> effect_setters{
        "setSleepingTabletEffect", "setBetaEffect", "setDepressEffect", "setPainEffect"};
    constexpr std::array<const char*, 4> delta_setters{
        "setSleepingTabletDelta", "setBetaDelta", "setDepressDelta", "setPainDelta"};
    constexpr std::array<const char*, 4> native_methods{
        "SleepingTablet", "BetaBlockers", "BetaAntiDepress", "PainMeds"};
    for (std::size_t index = 0; index < kTimedDefinitions.size(); ++index) {
        g_bindings.effect_getters[index] = env->GetMethodID(
            g_bindings.iso_player, effect_getters[index], "()F");
        g_bindings.delta_getters[index] = env->GetMethodID(
            g_bindings.iso_player, delta_getters[index], "()F");
        g_bindings.effect_setters[index] = env->GetMethodID(
            g_bindings.iso_player, effect_setters[index], "(F)V");
        g_bindings.delta_setters[index] = env->GetMethodID(
            g_bindings.iso_player, delta_setters[index], "(F)V");
        g_bindings.native_effect_methods[index] = env->GetMethodID(
            g_bindings.iso_player, native_methods[index], "(F)V");
    }

    bool timed_ready = true;
    for (std::size_t index = 0; index < kTimedDefinitions.size(); ++index) {
        timed_ready = timed_ready && g_bindings.effect_getters[index] != nullptr &&
            g_bindings.delta_getters[index] != nullptr &&
            g_bindings.effect_setters[index] != nullptr &&
            g_bindings.delta_setters[index] != nullptr &&
            g_bindings.native_effect_methods[index] != nullptr;
    }
    g_bindings.ready = !ClearException(env) && timed_ready &&
        g_bindings.client_flag != nullptr && g_bindings.server_flag != nullptr &&
        g_bindings.ordered_stats != nullptr &&
        g_bindings.can_modify_body_stats != nullptr &&
        g_bindings.sync_player_stats != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.get_stats != nullptr &&
        g_bindings.get_role != nullptr && g_bindings.role_has_capability != nullptr &&
        g_bindings.stats_get != nullptr &&
        g_bindings.stats_set != nullptr && g_bindings.stats_reset != nullptr &&
        g_bindings.stat_get_id != nullptr && g_bindings.stat_get_minimum != nullptr &&
        g_bindings.stat_get_maximum != nullptr && g_bindings.stat_get_default != nullptr &&
        g_bindings.stat_bit_mask != nullptr && g_bindings.integer_value_of != nullptr &&
        g_bindings.send_packet != nullptr;
    return g_bindings.ready;
}

PlayerEffectSessionMode DetectSessionMode(JNIEnv* env) {
    const bool client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool server = env->GetStaticBooleanField(
        g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (server) return PlayerEffectSessionMode::DedicatedServer;
    if (client) return PlayerEffectSessionMode::MultiplayerClient;
    return PlayerEffectSessionMode::Local;
}

void ReleaseCatalog(JNIEnv* env) {
    for (InternalEntry& entry : g_internal_entries) {
        if (entry.handle != nullptr) env->DeleteGlobalRef(entry.handle);
    }
    g_internal_entries.clear();
    g_status.entries.clear();
}

bool BuildCatalog(JNIEnv* env) {
    ReleaseCatalog(env);
    for (std::size_t index = 0; index < kTimedDefinitions.size(); ++index) {
        const TimedDefinition& definition = kTimedDefinitions[index];
        PlayerEffectEntry entry;
        entry.kind = PlayerEffectKind::TimedMedication;
        entry.id = definition.id;
        entry.display_name = definition.display_name;
        entry.source_item_id = definition.source_item_id;
        entry.minimum = 0.0f;
        entry.maximum = 900.0f;
        entry.editable = g_status.session_mode == PlayerEffectSessionMode::Local ||
            g_status.session_mode == PlayerEffectSessionMode::MultiplayerClient;
        g_status.entries.push_back(std::move(entry));
        g_internal_entries.push_back(
            {PlayerEffectKind::TimedMedication, nullptr, static_cast<int>(index)});
    }

    jobjectArray ordered_stats = static_cast<jobjectArray>(env->GetStaticObjectField(
        g_bindings.character_stat, g_bindings.ordered_stats));
    if (ordered_stats == nullptr || ClearException(env)) return false;
    const jsize stat_count = env->GetArrayLength(ordered_stats);
    for (jsize index = 0; index < stat_count; ++index) {
        jobject stat = env->GetObjectArrayElement(ordered_stats, index);
        jstring id_value = stat == nullptr ? nullptr : static_cast<jstring>(
            env->CallObjectMethod(stat, g_bindings.stat_get_id));
        const std::string id = JavaString(env, id_value);
        if (id_value != nullptr) env->DeleteLocalRef(id_value);
        if (stat == nullptr || id.empty() || ClearException(env)) {
            if (stat != nullptr) env->DeleteLocalRef(stat);
            env->DeleteLocalRef(ordered_stats);
            return false;
        }
        PlayerEffectEntry entry;
        entry.kind = PlayerEffectKind::CharacterStat;
        entry.id = id;
        const char* localized = LocalizedStatName(id);
        entry.display_name = localized == nullptr ? id : localized;
        entry.minimum = env->CallFloatMethod(stat, g_bindings.stat_get_minimum);
        entry.maximum = env->CallFloatMethod(stat, g_bindings.stat_get_maximum);
        entry.default_value = env->CallFloatMethod(stat, g_bindings.stat_get_default);
        entry.editable = g_status.session_mode == PlayerEffectSessionMode::Local ||
            g_status.session_mode == PlayerEffectSessionMode::MultiplayerClient;
        g_status.entries.push_back(std::move(entry));
        g_internal_entries.push_back(
            {PlayerEffectKind::CharacterStat, env->NewGlobalRef(stat), -1});
        env->DeleteLocalRef(stat);
    }
    env->DeleteLocalRef(ordered_stats);

    return !ClearException(env) && g_status.entries.size() == g_internal_entries.size();
}

void RefreshValues(JNIEnv* env, jobject player) {
    jobject stats = env->CallObjectMethod(player, g_bindings.get_stats);
    if (stats == nullptr || ClearException(env)) {
        if (stats != nullptr) env->DeleteLocalRef(stats);
        g_status.player_ready = false;
        return;
    }
    for (std::size_t index = 0; index < g_status.entries.size(); ++index) {
        PlayerEffectEntry& entry = g_status.entries[index];
        const InternalEntry& internal = g_internal_entries[index];
        entry.editable = g_status.session_mode == PlayerEffectSessionMode::Local ||
            g_status.session_mode == PlayerEffectSessionMode::MultiplayerClient;
        if (entry.kind == PlayerEffectKind::TimedMedication) {
            const std::size_t timed_index = static_cast<std::size_t>(internal.timed_index);
            const float ticks = env->CallFloatMethod(
                player, g_bindings.effect_getters[timed_index]);
            entry.duration_seconds = std::max(0.0f, ticks / kEffectTicksPerSecond);
            entry.strength = env->CallFloatMethod(
                player, g_bindings.delta_getters[timed_index]);
            entry.value = entry.duration_seconds;
            entry.active = ticks > 0.0f;
        } else {
            entry.value = env->CallFloatMethod(
                stats, g_bindings.stats_get, internal.handle);
            entry.active = entry.value != entry.default_value;
        }
    }
    ClearException(env);
    env->DeleteLocalRef(stats);
    g_status.player_ready = true;
}

std::size_t FindEntry(PlayerEffectKind kind, const std::string& id) {
    for (std::size_t index = 0; index < g_status.entries.size(); ++index) {
        if (g_status.entries[index].kind == kind && g_status.entries[index].id == id) {
            return index;
        }
    }
    return g_status.entries.size();
}

jobject GetPlayer(JNIEnv* env) {
    return env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
}

bool HasServerStatSyncCapability(JNIEnv* env, jobject player) {
    if (g_status.session_mode != PlayerEffectSessionMode::MultiplayerClient) {
        return false;
    }
    jobject role = env->CallObjectMethod(player, g_bindings.get_role);
    jobject capability = env->GetStaticObjectField(
        g_bindings.capability, g_bindings.can_modify_body_stats);
    if (role == nullptr || capability == nullptr || ClearException(env)) {
        if (role != nullptr) env->DeleteLocalRef(role);
        if (capability != nullptr) env->DeleteLocalRef(capability);
        return false;
    }
    const bool available = env->CallBooleanMethod(
        role, g_bindings.role_has_capability, capability) == JNI_TRUE;
    const bool failed = ClearException(env);
    env->DeleteLocalRef(capability);
    env->DeleteLocalRef(role);
    return available && !failed;
}

bool SendPlayerStatSync(JNIEnv* env, jobject player, jobject stat) {
    const jint bit_mask = env->CallStaticIntMethod(
        g_bindings.sync_player_stats_packet, g_bindings.stat_bit_mask, stat);
    jobject mask_value = env->CallStaticObjectMethod(
        g_bindings.integer, g_bindings.integer_value_of, bit_mask);
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, g_bindings.sync_player_stats);
    jobjectArray arguments = env->NewObjectArray(2, g_bindings.object, nullptr);
    if (mask_value == nullptr || packet_type == nullptr || arguments == nullptr ||
        ClearException(env)) {
        if (mask_value != nullptr) env->DeleteLocalRef(mask_value);
        if (packet_type != nullptr) env->DeleteLocalRef(packet_type);
        if (arguments != nullptr) env->DeleteLocalRef(arguments);
        return false;
    }
    env->SetObjectArrayElement(arguments, 0, player);
    env->SetObjectArrayElement(arguments, 1, mask_value);
    env->CallStaticVoidMethod(
        g_bindings.network_packet, g_bindings.send_packet, packet_type, arguments);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(arguments);
    env->DeleteLocalRef(packet_type);
    env->DeleteLocalRef(mask_value);
    return succeeded;
}

}  // namespace

void UpdatePlayerEffectBridge() {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.server_stat_sync_available = false;
        g_status.message = "状态编辑桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    const PlayerEffectSessionMode mode = DetectSessionMode(env);
    const bool mode_changed = mode != g_status.session_mode;
    g_status.session_mode = mode;

    jobject player = GetPlayer(env);
    if (player == nullptr || ClearException(env)) {
        g_status.player_ready = false;
        g_status.server_stat_sync_available = false;
        g_status.message = "等待玩家对象";
        return;
    }
    g_status.server_stat_sync_available = HasServerStatSyncCapability(env, player);
    if (g_status.entries.empty() || mode_changed) {
        if (!BuildCatalog(env)) {
            g_status.message = "读取角色属性注册表失败";
            env->DeleteLocalRef(player);
            return;
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (now >= g_next_refresh) {
        RefreshValues(env, player);
        g_next_refresh = now + std::chrono::milliseconds(150);
    }
    if (g_status.player_ready) {
        if (mode == PlayerEffectSessionMode::Local) {
            g_status.message = "单机原生状态编辑已启用";
        } else if (mode == PlayerEffectSessionMode::MultiplayerClient &&
                   g_status.server_stat_sync_available) {
            g_status.message = "联机属性可通过原生 SyncPlayerStats 同步";
        } else if (mode == PlayerEffectSessionMode::MultiplayerClient) {
            g_status.message = "联机属性仅在本地客户端生效；当前角色无服务器同步权限";
        } else {
            g_status.message = "专用服务器状态只读";
        }
    }
    env->DeleteLocalRef(player);
}

const PlayerEffectStatus& GetPlayerEffectStatus() {
    return g_status;
}

bool SetTimedPlayerEffect(const std::string& id, float duration_seconds,
                          float strength) {
    if (g_status.session_mode != PlayerEffectSessionMode::Local &&
        g_status.session_mode != PlayerEffectSessionMode::MultiplayerClient) {
        g_status.message = "当前会话不允许编辑药效";
        return false;
    }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    const std::size_t entry_index = FindEntry(PlayerEffectKind::TimedMedication, id);
    if (entry_index >= g_internal_entries.size()) return false;
    const std::size_t timed_index = static_cast<std::size_t>(
        g_internal_entries[entry_index].timed_index);
    jobject player = GetPlayer(env);
    if (player == nullptr || ClearException(env)) return false;

    const float clamped_duration = std::clamp(duration_seconds, 0.0f, 900.0f);
    const float clamped_strength = std::clamp(strength, 0.0f, 2.0f);
    if (clamped_duration > 0.0f && clamped_strength > 0.0f) {
        const float current_ticks = env->CallFloatMethod(
            player, g_bindings.effect_getters[timed_index]);
        if (current_ticks <= 0.0f) {
            env->CallVoidMethod(
                player, g_bindings.native_effect_methods[timed_index], clamped_strength);
        }
        env->CallVoidMethod(
            player, g_bindings.effect_setters[timed_index],
            clamped_duration * kEffectTicksPerSecond);
        env->CallVoidMethod(
            player, g_bindings.delta_setters[timed_index], clamped_strength);
    } else {
        env->CallVoidMethod(player, g_bindings.effect_setters[timed_index], 0.0f);
        env->CallVoidMethod(player, g_bindings.delta_setters[timed_index], 0.0f);
    }
    const bool succeeded = !ClearException(env);
    if (succeeded) {
        g_next_refresh = {};
        const bool active = clamped_duration > 0.0f && clamped_strength > 0.0f;
        if (g_status.session_mode == PlayerEffectSessionMode::Local) {
            g_status.message = active
                ? "已应用原生计时药效"
                : "已移除原生计时药效";
        } else {
            g_status.message = active
                ? "已为普通联机玩家在客户端应用原生药效"
                : "已在客户端移除原生药效";
        }
    }
    env->DeleteLocalRef(player);
    return succeeded;
}

bool SetPlayerCharacterStat(const std::string& id, float value) {
    if (g_status.session_mode != PlayerEffectSessionMode::Local &&
        g_status.session_mode != PlayerEffectSessionMode::MultiplayerClient) {
        g_status.message = "当前会话不允许编辑角色属性";
        return false;
    }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    const std::size_t entry_index = FindEntry(PlayerEffectKind::CharacterStat, id);
    if (entry_index >= g_internal_entries.size()) return false;
    jobject player = GetPlayer(env);
    jobject stats = player == nullptr ? nullptr : env->CallObjectMethod(
        player, g_bindings.get_stats);
    if (stats == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }
    const PlayerEffectEntry& entry = g_status.entries[entry_index];
    env->CallBooleanMethod(
        stats, g_bindings.stats_set, g_internal_entries[entry_index].handle,
        std::clamp(value, entry.minimum, entry.maximum));
    const bool local_succeeded = !ClearException(env);
    const bool should_sync = local_succeeded &&
        g_status.session_mode == PlayerEffectSessionMode::MultiplayerClient &&
        g_status.server_stat_sync_available;
    const bool sync_succeeded = !should_sync || SendPlayerStatSync(
        env, player, g_internal_entries[entry_index].handle);
    const bool succeeded = local_succeeded && sync_succeeded;
    if (local_succeeded) {
        g_next_refresh = {};
        if (g_status.session_mode == PlayerEffectSessionMode::Local) {
            g_status.message = "已更新原生角色属性";
        } else if (should_sync && sync_succeeded) {
            g_status.message = "已更新并通过原生 SyncPlayerStats 提交服务器";
        } else if (should_sync) {
            g_status.message = "客户端属性已更新，但服务器同步失败";
        } else {
            g_status.message =
                "已在客户端应用；当前角色无 CanModifyBodyStats 权限，未发送受限包";
        }
    }
    env->DeleteLocalRef(stats);
    env->DeleteLocalRef(player);
    return succeeded;
}

bool ResetPlayerCharacterStat(const std::string& id) {
    if (g_status.session_mode != PlayerEffectSessionMode::Local &&
        g_status.session_mode != PlayerEffectSessionMode::MultiplayerClient) {
        g_status.message = "当前会话不允许编辑角色属性";
        return false;
    }
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    const std::size_t entry_index = FindEntry(PlayerEffectKind::CharacterStat, id);
    if (entry_index >= g_internal_entries.size()) return false;
    jobject player = GetPlayer(env);
    jobject stats = player == nullptr ? nullptr : env->CallObjectMethod(
        player, g_bindings.get_stats);
    if (stats == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }
    env->CallBooleanMethod(
        stats, g_bindings.stats_reset, g_internal_entries[entry_index].handle);
    const bool local_succeeded = !ClearException(env);
    const bool should_sync = local_succeeded &&
        g_status.session_mode == PlayerEffectSessionMode::MultiplayerClient &&
        g_status.server_stat_sync_available;
    const bool sync_succeeded = !should_sync || SendPlayerStatSync(
        env, player, g_internal_entries[entry_index].handle);
    const bool succeeded = local_succeeded && sync_succeeded;
    if (local_succeeded) {
        g_next_refresh = {};
        if (g_status.session_mode == PlayerEffectSessionMode::Local) {
            g_status.message = "已重置原生角色属性";
        } else if (should_sync && sync_succeeded) {
            g_status.message = "已重置并通过原生 SyncPlayerStats 提交服务器";
        } else if (should_sync) {
            g_status.message = "客户端属性已重置，但服务器同步失败";
        } else {
            g_status.message =
                "已在客户端重置；当前角色无 CanModifyBodyStats 权限，未发送受限包";
        }
    }
    env->DeleteLocalRef(stats);
    env->DeleteLocalRef(player);
    return succeeded;
}

}  // namespace pztrainer::bridge
