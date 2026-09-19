#include "bridge/experience_bridge.hpp"

#include "settings/safety_mode.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "bridge/maintenance_training_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct SkillDefinition {
    const char* id;
    const char* display_name;
    const char* category;
    const char* radio_code;
    SkillOnlineRoute online_route;
};

constexpr std::array<SkillDefinition, 35> kSkills{{
    {"Fitness", "健身", "体能", nullptr, SkillOnlineRoute::FitnessExercise},
    {"Strength", "力量", "体能", nullptr, SkillOnlineRoute::StrengthExercise},
    {"Sprinting", "冲刺", "敏捷", "SPR", SkillOnlineRoute::RadioTeaching},
    {"Lightfoot", "轻巧", "敏捷", "LFT", SkillOnlineRoute::RadioTeaching},
    {"Nimble", "灵活", "敏捷", "NIM", SkillOnlineRoute::RadioTeaching},
    {"Sneak", "潜行", "敏捷", "SNE", SkillOnlineRoute::RadioTeaching},
    {"Axe", "斧类", "战斗", "BAA", SkillOnlineRoute::RadioTeaching},
    {"Blunt", "长钝器", "战斗", "BUA", SkillOnlineRoute::RadioTeaching},
    {"SmallBlunt", "短钝器", "战斗", "SBU", SkillOnlineRoute::RadioTeaching},
    {"Spear", "长矛", "战斗", "SPE", SkillOnlineRoute::RadioTeaching},
    {"LongBlade", "长刃", "战斗", "LBA", SkillOnlineRoute::RadioTeaching},
    {"SmallBlade", "短刃", "战斗", "SBA", SkillOnlineRoute::RadioTeaching},
    {"Maintenance", "维护", "战斗", nullptr, SkillOnlineRoute::MaintenanceTraining},
    {"Aiming", "瞄准", "枪械", "AIM", SkillOnlineRoute::RadioTeaching},
    {"Reloading", "装填", "枪械", "REL", SkillOnlineRoute::RadioTeaching},
    {"Woodwork", "木工", "制作", "CRP", SkillOnlineRoute::RadioTeaching},
    {"Cooking", "烹饪", "制作", "COO", SkillOnlineRoute::RadioTeaching},
    {"Farming", "耕作", "制作", "FRM", SkillOnlineRoute::RadioTeaching},
    {"Doctor", "急救", "制作", "DOC", SkillOnlineRoute::RadioTeaching},
    {"Electricity", "电工", "制作", "ELC", SkillOnlineRoute::RadioTeaching},
    {"MetalWelding", "金属加工", "制作", "MTL", SkillOnlineRoute::RadioTeaching},
    {"Mechanics", "机械", "制作", "MEC", SkillOnlineRoute::RadioTeaching},
    {"Tailoring", "裁缝", "制作", "TAI", SkillOnlineRoute::RadioTeaching},
    {"Blacksmith", "锻造", "制作", "BLA", SkillOnlineRoute::RadioTeaching},
    {"FlintKnapping", "燧石打制", "制作", "FKN", SkillOnlineRoute::RadioTeaching},
    {"Masonry", "石工", "制作", "MAS", SkillOnlineRoute::RadioTeaching},
    {"Pottery", "陶艺", "制作", "POT", SkillOnlineRoute::RadioTeaching},
    {"Carving", "雕刻", "制作", "CRV", SkillOnlineRoute::RadioTeaching},
    {"Glassmaking", "玻璃制作", "制作", "GLA", SkillOnlineRoute::RadioTeaching},
    {"Butchering", "屠宰", "生存", "BUT", SkillOnlineRoute::RadioTeaching},
    {"Husbandry", "畜牧", "生存", "HUS", SkillOnlineRoute::RadioTeaching},
    {"Fishing", "钓鱼", "生存", "FIS", SkillOnlineRoute::RadioTeaching},
    {"Trapping", "捕猎", "生存", "TRA", SkillOnlineRoute::RadioTeaching},
    {"PlantScavenging", "搜寻", "生存", "FOR", SkillOnlineRoute::RadioTeaching},
    {"Tracking", "追踪", "生存", "TRK", SkillOnlineRoute::RadioTeaching},
}};

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass xp = nullptr;
    jclass perks = nullptr;
    jclass perk = nullptr;
    jclass zomboid_radio = nullptr;
    jclass wave_signal_device = nullptr;
    jclass device_data = nullptr;
    jclass array_list = nullptr;
    jclass lua_manager = nullptr;
    jclass kahlua_table = nullptr;
    jclass lua_caller = nullptr;
    jclass lua_return = nullptr;
    jclass object = nullptr;
    jclass number = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_current_square = nullptr;
    jmethodID get_xp = nullptr;
    jmethodID get_perk_level = nullptr;
    jmethodID get_x = nullptr;
    jmethodID get_y = nullptr;
    jmethodID get_z = nullptr;
    jmethodID xp_get = nullptr;
    jmethodID xp_get_multiplier = nullptr;
    jmethodID xp_add = nullptr;
    jmethodID perk_total_xp_for_level = nullptr;
    jmethodID radio_get_instance = nullptr;
    jmethodID radio_get_devices = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID device_get_data = nullptr;
    jmethodID device_get_x = nullptr;
    jmethodID device_get_y = nullptr;
    jmethodID device_get_z = nullptr;
    jmethodID data_is_on = nullptr;
    jmethodID data_get_channel = nullptr;
    jmethodID data_is_tv = nullptr;
    jmethodID data_get_volume = nullptr;
    jmethodID data_is_playing_media = nullptr;
    jmethodID data_is_no_transmit = nullptr;
    jmethodID send_wave_signal = nullptr;
    jmethodID lua_get_table_object = nullptr;
    jmethodID lua_get_function_object = nullptr;
    jmethodID table_rawget = nullptr;
    jfieldID lua_caller_field = nullptr;
    jfieldID lua_thread_field = nullptr;
    jmethodID lua_return_is_success = nullptr;
    jmethodID lua_return_get_first = nullptr;
    jmethodID number_int_value = nullptr;
};

struct InternalSkill {
    jobject perk = nullptr;
    const SkillDefinition* definition = nullptr;
};

struct Receiver {
    bool ready = false;
    int x = 0;
    int y = 0;
    int channel = 0;
    bool television = false;
};

Bindings g_bindings;
ExperienceStatus g_status;
std::vector<InternalSkill> g_internal_skills;
Receiver g_receiver;
jobject g_bound_player = nullptr;
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

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.game_server = LoadGlobalClass(env, "zombie/network/GameServer");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.xp = LoadGlobalClass(env, "zombie/characters/IsoGameCharacter$XP");
    g_bindings.perks = LoadGlobalClass(env, "zombie/characters/skills/PerkFactory$Perks");
    g_bindings.perk = LoadGlobalClass(env, "zombie/characters/skills/PerkFactory$Perk");
    g_bindings.zomboid_radio = LoadGlobalClass(env, "zombie/radio/ZomboidRadio");
    g_bindings.wave_signal_device = LoadGlobalClass(env, "zombie/radio/devices/WaveSignalDevice");
    g_bindings.device_data = LoadGlobalClass(env, "zombie/radio/devices/DeviceData");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    g_bindings.lua_manager = LoadGlobalClass(env, "zombie/Lua/LuaManager");
    g_bindings.kahlua_table = LoadGlobalClass(env, "se/krka/kahlua/vm/KahluaTable");
    g_bindings.lua_caller = LoadGlobalClass(
        env, "se/krka/kahlua/integration/LuaCaller");
    g_bindings.lua_return = LoadGlobalClass(
        env, "se/krka/kahlua/integration/LuaReturn");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.number = LoadGlobalClass(env, "java/lang/Number");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.xp == nullptr ||
        g_bindings.perks == nullptr || g_bindings.perk == nullptr ||
        g_bindings.zomboid_radio == nullptr || g_bindings.wave_signal_device == nullptr ||
        g_bindings.device_data == nullptr || g_bindings.array_list == nullptr ||
        g_bindings.lua_manager == nullptr || g_bindings.kahlua_table == nullptr ||
        g_bindings.lua_caller == nullptr || g_bindings.lua_return == nullptr ||
        g_bindings.object == nullptr || g_bindings.number == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_current_square = env->GetMethodID(
        g_bindings.iso_player, "getCurrentSquare", "()Lzombie/iso/IsoGridSquare;");
    g_bindings.get_xp = env->GetMethodID(
        g_bindings.iso_player, "getXp", "()Lzombie/characters/IsoGameCharacter$XP;");
    g_bindings.get_perk_level = env->GetMethodID(
        g_bindings.iso_player, "getPerkLevel", "(Lzombie/characters/skills/PerkFactory$Perk;)I");
    g_bindings.get_x = env->GetMethodID(g_bindings.iso_player, "getX", "()F");
    g_bindings.get_y = env->GetMethodID(g_bindings.iso_player, "getY", "()F");
    g_bindings.get_z = env->GetMethodID(g_bindings.iso_player, "getZ", "()F");
    g_bindings.xp_get = env->GetMethodID(
        g_bindings.xp, "getXP", "(Lzombie/characters/skills/PerkFactory$Perk;)F");
    g_bindings.xp_get_multiplier = env->GetMethodID(
        g_bindings.xp, "getMultiplier", "(Lzombie/characters/skills/PerkFactory$Perk;)F");
    g_bindings.xp_add = env->GetMethodID(
        g_bindings.xp, "AddXP", "(Lzombie/characters/skills/PerkFactory$Perk;F)V");
    g_bindings.perk_total_xp_for_level = env->GetMethodID(
        g_bindings.perk, "getTotalXpForLevel", "(I)F");
    g_bindings.radio_get_instance = env->GetStaticMethodID(
        g_bindings.zomboid_radio, "getInstance", "()Lzombie/radio/ZomboidRadio;");
    g_bindings.radio_get_devices = env->GetMethodID(
        g_bindings.zomboid_radio, "getDevices", "()Ljava/util/ArrayList;");
    g_bindings.list_size = env->GetMethodID(g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.device_get_data = env->GetMethodID(
        g_bindings.wave_signal_device, "getDeviceData", "()Lzombie/radio/devices/DeviceData;");
    g_bindings.device_get_x = env->GetMethodID(g_bindings.wave_signal_device, "getX", "()F");
    g_bindings.device_get_y = env->GetMethodID(g_bindings.wave_signal_device, "getY", "()F");
    g_bindings.device_get_z = env->GetMethodID(g_bindings.wave_signal_device, "getZ", "()F");
    g_bindings.data_is_on = env->GetMethodID(g_bindings.device_data, "getIsTurnedOn", "()Z");
    g_bindings.data_get_channel = env->GetMethodID(g_bindings.device_data, "getChannel", "()I");
    g_bindings.data_is_tv = env->GetMethodID(g_bindings.device_data, "getIsTelevision", "()Z");
    g_bindings.data_get_volume = env->GetMethodID(g_bindings.device_data, "getDeviceVolume", "()F");
    g_bindings.data_is_playing_media = env->GetMethodID(
        g_bindings.device_data, "isPlayingMedia", "()Z");
    g_bindings.data_is_no_transmit = env->GetMethodID(
        g_bindings.device_data, "isNoTransmit", "()Z");
    g_bindings.send_wave_signal = env->GetStaticMethodID(
        g_bindings.game_client, "sendIsoWaveSignal",
        "(IIILjava/lang/String;Ljava/lang/String;Ljava/lang/String;FFFIZ)V");
    g_bindings.lua_get_table_object = env->GetStaticMethodID(
        g_bindings.lua_manager, "getTableObject", "(Ljava/lang/String;)Ljava/lang/Object;");
    g_bindings.lua_get_function_object = env->GetStaticMethodID(
        g_bindings.lua_manager, "getFunctionObject",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    g_bindings.table_rawget = env->GetMethodID(
        g_bindings.kahlua_table, "rawget", "(Ljava/lang/Object;)Ljava/lang/Object;");
    g_bindings.lua_caller_field = env->GetStaticFieldID(
        g_bindings.lua_manager, "caller", "Lse/krka/kahlua/integration/LuaCaller;");
    g_bindings.lua_thread_field = env->GetStaticFieldID(
        g_bindings.lua_manager, "thread", "Lse/krka/kahlua/vm/KahluaThread;");
    g_bindings.lua_return_is_success = env->GetMethodID(
        g_bindings.lua_return, "isSuccess", "()Z");
    g_bindings.lua_return_get_first = env->GetMethodID(
        g_bindings.lua_return, "getFirst", "()Ljava/lang/Object;");
    g_bindings.number_int_value = env->GetMethodID(
        g_bindings.number, "intValue", "()I");

    g_bindings.ready = !ClearException(env) && g_bindings.client_flag != nullptr &&
        g_bindings.server_flag != nullptr && g_bindings.get_player != nullptr &&
        g_bindings.get_current_square != nullptr && g_bindings.get_xp != nullptr &&
        g_bindings.get_perk_level != nullptr &&
        g_bindings.get_x != nullptr && g_bindings.get_y != nullptr &&
        g_bindings.get_z != nullptr && g_bindings.xp_get != nullptr &&
        g_bindings.xp_get_multiplier != nullptr && g_bindings.xp_add != nullptr &&
        g_bindings.perk_total_xp_for_level != nullptr &&
        g_bindings.radio_get_instance != nullptr && g_bindings.radio_get_devices != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.device_get_data != nullptr && g_bindings.device_get_x != nullptr &&
        g_bindings.device_get_y != nullptr && g_bindings.device_get_z != nullptr &&
        g_bindings.data_is_on != nullptr && g_bindings.data_get_channel != nullptr &&
        g_bindings.data_is_tv != nullptr && g_bindings.data_get_volume != nullptr &&
        g_bindings.data_is_playing_media != nullptr &&
        g_bindings.data_is_no_transmit != nullptr && g_bindings.send_wave_signal != nullptr &&
        g_bindings.lua_get_table_object != nullptr &&
        g_bindings.lua_get_function_object != nullptr && g_bindings.table_rawget != nullptr &&
        g_bindings.lua_caller_field != nullptr && g_bindings.lua_thread_field != nullptr &&
        g_bindings.lua_return_is_success != nullptr &&
        g_bindings.lua_return_get_first != nullptr &&
        g_bindings.number_int_value != nullptr;
    return g_bindings.ready;
}

ExperienceSessionMode DetectSessionMode(JNIEnv* env) {
    const bool client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool server = env->GetStaticBooleanField(
        g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (server) return ExperienceSessionMode::DedicatedServer;
    if (client) return ExperienceSessionMode::MultiplayerClient;
    return ExperienceSessionMode::Local;
}

void ReleaseCatalog(JNIEnv* env) {
    for (InternalSkill& skill : g_internal_skills) {
        if (skill.perk != nullptr) env->DeleteGlobalRef(skill.perk);
    }
    g_internal_skills.clear();
    g_status.entries.clear();
}

void ResetPlayerSession(JNIEnv* env, const char* message) {
    const bool had_session = g_bound_player != nullptr ||
        !g_internal_skills.empty() || g_status.player_ready;
    if (GetMaintenanceTrainingStatus().active) {
        StopMaintenanceTraining("维修训练已随人物会话停止");
    }
    if (g_bound_player != nullptr) {
        env->DeleteGlobalRef(g_bound_player);
        g_bound_player = nullptr;
    }
    ReleaseCatalog(env);
    g_receiver = {};
    g_status.player_ready = false;
    g_status.nearby_receiver_ready = false;
    g_status.maintenance_training_active = false;
    g_status.maintenance_packets_sent = 0;
    g_status.media_level_cutoff = 0;
    g_status.receiver_name.clear();
    g_status.message = message;
    if (had_session) ++g_status.session_generation;
}

bool BuildCatalog(JNIEnv* env) {
    ReleaseCatalog(env);
    for (const SkillDefinition& definition : kSkills) {
        const jfieldID field = env->GetStaticFieldID(
            g_bindings.perks, definition.id,
            "Lzombie/characters/skills/PerkFactory$Perk;");
        jobject perk = field == nullptr
            ? nullptr
            : env->GetStaticObjectField(g_bindings.perks, field);
        if (perk == nullptr || ClearException(env)) {
            if (perk != nullptr) env->DeleteLocalRef(perk);
            ReleaseCatalog(env);
            return false;
        }

        SkillExperienceEntry entry;
        entry.id = definition.id;
        entry.display_name = definition.display_name;
        entry.category = definition.category;
        entry.online_route = definition.online_route;
        entry.online_supported = definition.online_route != SkillOnlineRoute::None;
        g_status.entries.push_back(std::move(entry));
        g_internal_skills.push_back({env->NewGlobalRef(perk), &definition});
        env->DeleteLocalRef(perk);
    }
    return !ClearException(env);
}

Receiver FindNearbyReceiver(JNIEnv* env, jobject player) {
    Receiver result;
    jobject radio = env->CallStaticObjectMethod(
        g_bindings.zomboid_radio, g_bindings.radio_get_instance);
    jobject devices = radio == nullptr
        ? nullptr
        : env->CallObjectMethod(radio, g_bindings.radio_get_devices);
    if (radio == nullptr || devices == nullptr || ClearException(env)) {
        if (devices != nullptr) env->DeleteLocalRef(devices);
        if (radio != nullptr) env->DeleteLocalRef(radio);
        return result;
    }

    const float player_x = env->CallFloatMethod(player, g_bindings.get_x);
    const float player_y = env->CallFloatMethod(player, g_bindings.get_y);
    const float player_z = env->CallFloatMethod(player, g_bindings.get_z);
    const jint count = env->CallIntMethod(devices, g_bindings.list_size);
    float best_distance = 1000000.0f;
    for (jint index = 0; index < count; ++index) {
        jobject device = env->CallObjectMethod(devices, g_bindings.list_get, index);
        jobject data = device == nullptr
            ? nullptr
            : env->CallObjectMethod(device, g_bindings.device_get_data);
        if (device == nullptr || data == nullptr || ClearException(env)) {
            if (data != nullptr) env->DeleteLocalRef(data);
            if (device != nullptr) env->DeleteLocalRef(device);
            continue;
        }

        const bool usable = env->CallBooleanMethod(data, g_bindings.data_is_on) == JNI_TRUE &&
            env->CallFloatMethod(data, g_bindings.data_get_volume) > 0.0f &&
            env->CallBooleanMethod(data, g_bindings.data_is_playing_media) != JNI_TRUE &&
            env->CallBooleanMethod(data, g_bindings.data_is_no_transmit) != JNI_TRUE;
        const float x = env->CallFloatMethod(device, g_bindings.device_get_x);
        const float y = env->CallFloatMethod(device, g_bindings.device_get_y);
        const float z = env->CallFloatMethod(device, g_bindings.device_get_z);
        const float dx = std::fabs(player_x - x);
        const float dy = std::fabs(player_y - y);
        const float distance = dx + dy;
        if (usable && dx <= 5.0f && dy <= 5.0f &&
            std::floor(player_z) == std::floor(z) && distance < best_distance) {
            result.ready = true;
            result.x = static_cast<int>(std::floor(x));
            result.y = static_cast<int>(std::floor(y));
            result.channel = env->CallIntMethod(data, g_bindings.data_get_channel);
            result.television = env->CallBooleanMethod(
                data, g_bindings.data_is_tv) == JNI_TRUE;
            best_distance = distance;
        }
        env->DeleteLocalRef(data);
        env->DeleteLocalRef(device);
    }
    env->DeleteLocalRef(devices);
    env->DeleteLocalRef(radio);
    ClearException(env);
    return result;
}

std::size_t FindSkill(const std::string& id) {
    for (std::size_t index = 0; index < g_status.entries.size(); ++index) {
        if (g_status.entries[index].id == id) return index;
    }
    return g_status.entries.size();
}

void RefreshValues(JNIEnv* env, jobject player) {
    jobject xp = env->CallObjectMethod(player, g_bindings.get_xp);
    if (xp == nullptr || ClearException(env)) {
        if (xp != nullptr) env->DeleteLocalRef(xp);
        g_status.player_ready = false;
        g_status.message = "无法读取玩家经验数据";
        return;
    }

    for (std::size_t index = 0; index < g_status.entries.size(); ++index) {
        SkillExperienceEntry& entry = g_status.entries[index];
        jobject perk = g_internal_skills[index].perk;
        entry.level = env->CallIntMethod(player, g_bindings.get_perk_level, perk);
        entry.total_xp = env->CallFloatMethod(xp, g_bindings.xp_get, perk);
        entry.multiplier = env->CallFloatMethod(
            xp, g_bindings.xp_get_multiplier, perk);
        entry.level_start_xp = entry.level <= 0 ? 0.0f : env->CallFloatMethod(
            perk, g_bindings.perk_total_xp_for_level, entry.level);
        entry.next_level_xp = entry.level >= 10 ? entry.total_xp : env->CallFloatMethod(
            perk, g_bindings.perk_total_xp_for_level, entry.level + 1);
        entry.max_level_xp = env->CallFloatMethod(
            perk, g_bindings.perk_total_xp_for_level, 10);
    }
    env->DeleteLocalRef(xp);
    if (ClearException(env)) {
        g_status.player_ready = false;
        g_status.message = "刷新技能经验失败";
        return;
    }

    g_status.player_ready = true;
    if (g_status.session_mode == ExperienceSessionMode::MultiplayerClient) {
        jstring sandbox_name = env->NewStringUTF("SandboxVars");
        jobject sandbox_vars = sandbox_name == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                g_bindings.lua_manager, g_bindings.lua_get_table_object, sandbox_name);
        jstring cutoff_key = env->NewStringUTF("LevelForMediaXPCutoff");
        jobject cutoff = sandbox_vars == nullptr || cutoff_key == nullptr
            ? nullptr
            : env->CallObjectMethod(
                sandbox_vars, g_bindings.table_rawget, cutoff_key);
        if (cutoff != nullptr && env->IsInstanceOf(cutoff, g_bindings.number)) {
            g_status.media_level_cutoff = env->CallIntMethod(
                cutoff, g_bindings.number_int_value);
        } else {
            g_status.media_level_cutoff = 0;
        }
        if (cutoff != nullptr) env->DeleteLocalRef(cutoff);
        if (cutoff_key != nullptr) env->DeleteLocalRef(cutoff_key);
        if (sandbox_vars != nullptr) env->DeleteLocalRef(sandbox_vars);
        if (sandbox_name != nullptr) env->DeleteLocalRef(sandbox_name);
        ClearException(env);
        g_receiver = FindNearbyReceiver(env, player);
        g_status.nearby_receiver_ready = g_receiver.ready;
        g_status.receiver_name = g_receiver.television ? "电视" : "收音机";
        g_status.message = g_receiver.ready
            ? "已找到附近开启的接收设备"
            : "联机训练需要站在开启且有音量的收音机或电视旁";
    } else if (g_status.session_mode == ExperienceSessionMode::Local) {
        g_status.media_level_cutoff = 0;
        g_status.nearby_receiver_ready = false;
        g_status.receiver_name.clear();
        g_status.message = "单机使用游戏原生经验系统";
    } else {
        g_status.message = "当前会话不允许编辑技能经验";
    }
}

bool SendTrainingBroadcast(JNIEnv* env, jobject player,
                           const InternalSkill& skill, float amount) {
    g_receiver = FindNearbyReceiver(env, player);
    if (!g_receiver.ready || skill.definition->radio_code == nullptr) return false;

    const float media_amount = amount / 50.0f;
    char code[64]{};
    std::snprintf(code, sizeof(code), "%s+%.3f",
                  skill.definition->radio_code, media_amount);
    jstring message = env->NewStringUTF("技能训练");
    jstring codes = env->NewStringUTF(code);
    if (message == nullptr || codes == nullptr || ClearException(env)) {
        if (message != nullptr) env->DeleteLocalRef(message);
        if (codes != nullptr) env->DeleteLocalRef(codes);
        return false;
    }

    env->CallStaticVoidMethod(
        g_bindings.game_client, g_bindings.send_wave_signal,
        g_receiver.x + 1, g_receiver.y + 1, g_receiver.channel,
        message, nullptr, codes, 0.82f, 0.88f, 1.0f, -1,
        g_receiver.television ? JNI_TRUE : JNI_FALSE);
    const bool succeeded = !ClearException(env);
    env->DeleteLocalRef(codes);
    env->DeleteLocalRef(message);
    return succeeded;
}

bool CallLuaFunctionOnMainThread(JNIEnv* env, const char* name,
                                 std::initializer_list<jobject> arguments,
                                 jobject* first_result) {
    if (first_result != nullptr) *first_result = nullptr;
    jstring function_name = env->NewStringUTF(name);
    jobject function = function_name == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.lua_manager, g_bindings.lua_get_function_object,
            function_name);
    jobject caller = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_caller_field);
    jobject thread = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_thread_field);
    jobjectArray java_arguments = env->NewObjectArray(
        static_cast<jsize>(arguments.size()), g_bindings.object, nullptr);
    jsize index = 0;
    for (jobject argument : arguments) {
        env->SetObjectArrayElement(java_arguments, index++, argument);
    }
    if (function == nullptr || caller == nullptr || thread == nullptr ||
        java_arguments == nullptr || ClearException(env)) {
        if (java_arguments != nullptr) env->DeleteLocalRef(java_arguments);
        if (thread != nullptr) env->DeleteLocalRef(thread);
        if (caller != nullptr) env->DeleteLocalRef(caller);
        if (function != nullptr) env->DeleteLocalRef(function);
        if (function_name != nullptr) env->DeleteLocalRef(function_name);
        return false;
    }

    bool invoked = false;
    jobject lua_result = InvokeObjectMethodOnMainThread(
        env, caller, "protectedCall", {thread, function, java_arguments}, &invoked);
    const bool succeeded = invoked && lua_result != nullptr &&
        env->IsInstanceOf(lua_result, g_bindings.lua_return) &&
        env->CallBooleanMethod(
            lua_result, g_bindings.lua_return_is_success) == JNI_TRUE &&
        !ClearException(env);
    if (succeeded && first_result != nullptr) {
        *first_result = env->CallObjectMethod(
            lua_result, g_bindings.lua_return_get_first);
        ClearException(env);
    }
    if (lua_result != nullptr) env->DeleteLocalRef(lua_result);
    env->DeleteLocalRef(java_arguments);
    env->DeleteLocalRef(thread);
    env->DeleteLocalRef(caller);
    env->DeleteLocalRef(function);
    env->DeleteLocalRef(function_name);
    return succeeded;
}

}  // namespace

void UpdateExperienceBridge() {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_next_refresh) return;
    g_next_refresh = now + std::chrono::milliseconds(250);

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.message = "等待 JVM";
        return;
    }
    if (!Initialize(env)) {
        g_status.initialized = false;
        g_status.player_ready = false;
        g_status.message = "初始化技能经验接口失败";
        return;
    }

    g_status.initialized = true;
    const ExperienceSessionMode mode = DetectSessionMode(env);
    const bool mode_changed = mode != g_status.session_mode;
    g_status.session_mode = mode;

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    jobject square = player == nullptr
        ? nullptr
        : env->CallObjectMethod(player, g_bindings.get_current_square);
    const bool player_lookup_failed = ClearException(env);
    if (player == nullptr || square == nullptr || player_lookup_failed) {
        if (square != nullptr) env->DeleteLocalRef(square);
        if (player != nullptr) env->DeleteLocalRef(player);
        ResetPlayerSession(env, "等待进入游戏角色");
        return;
    }
    env->DeleteLocalRef(square);

    const bool player_changed = g_bound_player == nullptr ||
        env->IsSameObject(g_bound_player, player) != JNI_TRUE;
    if (mode_changed || player_changed) {
        if (GetMaintenanceTrainingStatus().active) {
            StopMaintenanceTraining("维修训练已随人物切换停止");
        }
        if (g_bound_player != nullptr) env->DeleteGlobalRef(g_bound_player);
        g_bound_player = env->NewGlobalRef(player);
        ReleaseCatalog(env);
        g_receiver = {};
        g_status.player_ready = false;
        g_status.nearby_receiver_ready = false;
        g_status.maintenance_training_active = false;
        g_status.maintenance_packets_sent = 0;
        g_status.receiver_name.clear();
        ++g_status.session_generation;
    }
    if (g_internal_skills.empty() && !BuildCatalog(env)) {
        env->DeleteLocalRef(player);
        g_status.player_ready = false;
        g_status.message = "读取技能目录失败";
        return;
    }
    RefreshValues(env, player);
    const std::size_t maintenance_index = FindSkill("Maintenance");
    const bool was_training = GetMaintenanceTrainingStatus().active;
    if (mode == ExperienceSessionMode::MultiplayerClient &&
        maintenance_index < g_status.entries.size()) {
        UpdateMaintenanceTraining(
            env, player, g_status.entries[maintenance_index].total_xp);
    } else if (was_training) {
        StopMaintenanceTraining("维修训练已停止：当前不是联机会话");
    }
    const MaintenanceTrainingStatus& training = GetMaintenanceTrainingStatus();
    g_status.maintenance_training_active = training.active;
    g_status.maintenance_packets_sent = training.packets_sent;
    if (training.active || was_training != training.active) {
        g_status.message = training.message;
    }
    env->DeleteLocalRef(player);
}

const ExperienceStatus& GetExperienceStatus() {
    return g_status;
}

bool AddSkillExperience(const std::string& id, float amount) {
    if (!g_status.player_ready || amount <= 0.0f) return false;
    const std::size_t index = FindSkill(id);
    if (index >= g_internal_skills.size()) return false;

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    jobject xp = player == nullptr
        ? nullptr
        : env->CallObjectMethod(player, g_bindings.get_xp);
    if (player == nullptr || xp == nullptr || ClearException(env)) {
        if (xp != nullptr) env->DeleteLocalRef(xp);
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }

    bool succeeded = false;
    if (g_status.session_mode == ExperienceSessionMode::Local) {
        env->CallVoidMethod(
            xp, g_bindings.xp_add, g_internal_skills[index].perk, amount);
        succeeded = !ClearException(env);
        g_status.message = succeeded ? "已通过原生经验系统添加经验" : "添加经验失败";
    } else if (g_status.session_mode == ExperienceSessionMode::MultiplayerClient) {
        const SkillOnlineRoute route =
            g_internal_skills[index].definition->online_route;
        if (route == SkillOnlineRoute::RadioTeaching) {
            succeeded = SendTrainingBroadcast(env, player, g_internal_skills[index], amount);
            g_status.message = succeeded
                ? "已提交服务器广播训练事件；以服务器返回的经验为准"
                : "提交失败：请靠近开启且有音量的收音机或电视";
        } else if (route == SkillOnlineRoute::FitnessExercise ||
                   route == SkillOnlineRoute::StrengthExercise) {
            g_status.message = "联机直接添加健身或力量经验会被检测，等待后续更新";
        } else if (route == SkillOnlineRoute::MaintenanceTraining) {
            if (GetMaintenanceTrainingStatus().active) {
                StopMaintenanceTraining("维修训练已由用户停止");
                succeeded = true;
            } else if (g_status.entries[index].level >= 10) {
                g_status.message = "维修技能已达到最高等级";
            } else {
                succeeded = StartMaintenanceTraining(
                    g_status.entries[index].total_xp, amount);
                g_status.message = succeeded
                    ? "低频维修训练已开始；训练期间不要普通攻击"
                    : "启动维修训练失败";
            }
            const MaintenanceTrainingStatus& training =
                GetMaintenanceTrainingStatus();
            g_status.maintenance_training_active = training.active;
            g_status.maintenance_packets_sent = training.packets_sent;
        }
    }

    env->DeleteLocalRef(xp);
    env->DeleteLocalRef(player);
    g_next_refresh = std::chrono::steady_clock::now();
    return succeeded;
}

bool AddSkillExperienceVanilla(const std::string& id, float amount) {
    if (settings::IsSafeModeEnabled() || !g_status.player_ready ||
        g_status.session_mode == ExperienceSessionMode::Unknown || amount <= 0.0f) {
        return false;
    }
    const std::size_t index = FindSkill(id);
    if (index >= g_internal_skills.size()) return false;

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    jobject xp = player == nullptr
        ? nullptr
        : env->CallObjectMethod(player, g_bindings.get_xp);
    if (player == nullptr || xp == nullptr || ClearException(env)) {
        if (xp != nullptr) env->DeleteLocalRef(xp);
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }

    env->CallVoidMethod(
        xp, g_bindings.xp_add, g_internal_skills[index].perk, amount);
    const bool succeeded = !ClearException(env);
    g_status.message = succeeded
        ? "已通过原版 API 添加经验"
        : "原版 API 添加经验失败";
    env->DeleteLocalRef(xp);
    env->DeleteLocalRef(player);
    g_next_refresh = std::chrono::steady_clock::now();
    return succeeded;
}

bool AddSkillLevel(const std::string& id) {
    if (!g_status.player_ready ||
        g_status.session_mode != ExperienceSessionMode::Local) return false;
    const std::size_t index = FindSkill(id);
    if (index >= g_internal_skills.size()) return false;
    if (g_status.entries[index].level >= 10) return false;
    const float amount = std::max(
        1.0f, g_status.entries[index].next_level_xp -
            g_status.entries[index].total_xp + 0.01f);

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    jobject xp = player == nullptr
        ? nullptr
        : env->CallObjectMethod(player, g_bindings.get_xp);
    if (player == nullptr || xp == nullptr || ClearException(env)) {
        if (xp != nullptr) env->DeleteLocalRef(xp);
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }
    env->CallVoidMethod(
        xp, g_bindings.xp_add, g_internal_skills[index].perk, amount);
    const bool succeeded = !ClearException(env);
    g_status.message = succeeded ? "已通过原生经验系统提升熟练度" : "提升熟练度失败";
    env->DeleteLocalRef(xp);
    env->DeleteLocalRef(player);
    g_next_refresh = std::chrono::steady_clock::now();
    return succeeded;
}

}  // namespace pztrainer::bridge
