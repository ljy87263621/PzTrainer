#include "features/aim/magic_bullet_hit_list.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace pztrainer::features::aim {
namespace {

struct Bindings {
    bool ready = false;
    jclass iso_player = nullptr;
    jclass iso_zombie = nullptr;
    jclass iso_world = nullptr;
    jclass iso_moving_object = nullptr;
    jclass iso_game_character = nullptr;
    jclass network_player_ai = nullptr;
    jclass pz_array_list = nullptr;
    jclass hit_info = nullptr;
    jfieldID world_instance = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_id = nullptr;
    jmethodID get_x = nullptr;
    jmethodID get_y = nullptr;
    jmethodID get_z = nullptr;
    jmethodID is_dead = nullptr;
    jmethodID is_attack_started = nullptr;
    jmethodID get_network_ai = nullptr;
    jmethodID get_shot_id = nullptr;
    jmethodID get_cell = nullptr;
    jmethodID get_object_list = nullptr;
    jmethodID set_iterator = nullptr;
    jmethodID iterator_has_next = nullptr;
    jmethodID iterator_next = nullptr;
    jmethodID clear_hit_info = nullptr;
    jmethodID get_hit_info_list = nullptr;
    jmethodID list_add = nullptr;
    jmethodID set_variable = nullptr;
    jmethodID hit_info_constructor = nullptr;
    jmethodID hit_info_init = nullptr;
    jfieldID hit_chance = nullptr;
};

Bindings g_bindings;
int g_last_character_id = -1;
jbyte g_last_shot_id = 0;
bool g_has_last_shot = false;

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
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.iso_zombie = LoadGlobalClass(env, "zombie/characters/IsoZombie");
    g_bindings.iso_world = LoadGlobalClass(env, "zombie/iso/IsoWorld");
    g_bindings.iso_moving_object = LoadGlobalClass(
        env, "zombie/iso/IsoMovingObject");
    g_bindings.iso_game_character = LoadGlobalClass(
        env, "zombie/characters/IsoGameCharacter");
    g_bindings.network_player_ai = LoadGlobalClass(
        env, "zombie/characters/NetworkPlayerAI");
    g_bindings.pz_array_list = LoadGlobalClass(
        env, "zombie/util/list/PZArrayList");
    g_bindings.hit_info = LoadGlobalClass(
        env, "zombie/network/fields/hit/HitInfo");
    if (g_bindings.iso_player == nullptr || g_bindings.iso_zombie == nullptr ||
        g_bindings.iso_world == nullptr ||
        g_bindings.iso_moving_object == nullptr ||
        g_bindings.iso_game_character == nullptr ||
        g_bindings.network_player_ai == nullptr ||
        g_bindings.pz_array_list == nullptr || g_bindings.hit_info == nullptr) {
        return false;
    }

    g_bindings.world_instance = env->GetStaticFieldID(
        g_bindings.iso_world, "instance", "Lzombie/iso/IsoWorld;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_id = env->GetMethodID(
        g_bindings.iso_moving_object, "getID", "()I");
    g_bindings.get_x = env->GetMethodID(
        g_bindings.iso_moving_object, "getX", "()F");
    g_bindings.get_y = env->GetMethodID(
        g_bindings.iso_moving_object, "getY", "()F");
    g_bindings.get_z = env->GetMethodID(
        g_bindings.iso_moving_object, "getZ", "()F");
    g_bindings.is_dead = env->GetMethodID(
        g_bindings.iso_game_character, "isDead", "()Z");
    g_bindings.is_attack_started = env->GetMethodID(
        g_bindings.iso_player, "isAttackStarted", "()Z");
    g_bindings.get_network_ai = env->GetMethodID(
        g_bindings.iso_game_character, "getNetworkCharacterAI",
        "()Lzombie/characters/NetworkCharacterAI;");
    g_bindings.get_shot_id = env->GetMethodID(
        g_bindings.network_player_ai, "getShotID", "()B");
    g_bindings.get_cell = env->GetMethodID(
        g_bindings.iso_world, "getCell", "()Lzombie/iso/IsoCell;");

    jclass iso_cell = LoadGlobalClass(env, "zombie/iso/IsoCell");
    jclass set = LoadGlobalClass(env, "java/util/Set");
    jclass iterator = LoadGlobalClass(env, "java/util/Iterator");
    if (iso_cell == nullptr || set == nullptr || iterator == nullptr) {
        return false;
    }
    g_bindings.get_object_list = env->GetMethodID(
        iso_cell, "getObjectList", "()Ljava/util/Set;");
    g_bindings.set_iterator = env->GetMethodID(
        set, "iterator", "()Ljava/util/Iterator;");
    g_bindings.iterator_has_next = env->GetMethodID(
        iterator, "hasNext", "()Z");
    g_bindings.iterator_next = env->GetMethodID(
        iterator, "next", "()Ljava/lang/Object;");

    g_bindings.clear_hit_info = env->GetMethodID(
        g_bindings.iso_game_character, "clearHitInfo", "()V");
    g_bindings.get_hit_info_list = env->GetMethodID(
        g_bindings.iso_game_character, "getHitInfoList",
        "()Lzombie/util/list/PZArrayList;");
    g_bindings.list_add = env->GetMethodID(
        g_bindings.pz_array_list, "add", "(Ljava/lang/Object;)Z");
    g_bindings.set_variable = env->GetMethodID(
        g_bindings.iso_game_character, "setVariable",
        "(Ljava/lang/String;Ljava/lang/String;)"
        "Lzombie/core/skinnedmodel/advancedanimation/IAnimationVariableSlot;");
    g_bindings.hit_info_constructor = env->GetMethodID(
        g_bindings.hit_info, "<init>", "()V");
    g_bindings.hit_info_init = env->GetMethodID(
        g_bindings.hit_info, "init",
        "(Lzombie/iso/IsoMovingObject;FFFFF)Lzombie/network/fields/hit/HitInfo;");
    g_bindings.hit_chance = env->GetFieldID(
        g_bindings.hit_info, "chance", "I");

    g_bindings.ready = g_bindings.world_instance != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.get_id != nullptr &&
        g_bindings.get_x != nullptr && g_bindings.get_y != nullptr &&
        g_bindings.get_z != nullptr && g_bindings.is_dead != nullptr &&
        g_bindings.is_attack_started != nullptr &&
        g_bindings.get_network_ai != nullptr &&
        g_bindings.get_shot_id != nullptr && g_bindings.get_cell != nullptr &&
        g_bindings.get_object_list != nullptr &&
        g_bindings.set_iterator != nullptr &&
        g_bindings.iterator_has_next != nullptr &&
        g_bindings.iterator_next != nullptr &&
        g_bindings.clear_hit_info != nullptr &&
        g_bindings.get_hit_info_list != nullptr &&
        g_bindings.list_add != nullptr &&
        g_bindings.set_variable != nullptr &&
        g_bindings.hit_info_constructor != nullptr &&
        g_bindings.hit_info_init != nullptr &&
        g_bindings.hit_chance != nullptr && !ClearException(env);
    return g_bindings.ready;
}

const char* HitReactionForBodyPart(int body_part) {
    switch (body_part) {
        case 0: return "ShotBellyStep";
        case 1: return "ShotChest";
        case 2: return "ShotHeadFwd";
        case 3:
        case 4: return "ShotLegL";
        case 5:
        case 6: return "ShotLegR";
        case 7:
        case 8: return "ShotShoulderL";
        case 9:
        case 10: return "ShotShoulderR";
        default: return nullptr;
    }
}

bool SetTargetedHitReaction(JNIEnv* env, jobject player, int body_part) {
    const char* reaction = HitReactionForBodyPart(body_part);
    if (reaction == nullptr) return false;
    jstring key = env->NewStringUTF("ZombieHitReaction");
    jstring value = env->NewStringUTF(reaction);
    jobject slot = key == nullptr || value == nullptr
        ? nullptr
        : env->CallObjectMethod(
              player, g_bindings.set_variable, key, value);
    const bool succeeded = slot != nullptr && !ClearException(env);
    if (slot != nullptr) env->DeleteLocalRef(slot);
    if (value != nullptr) env->DeleteLocalRef(value);
    if (key != nullptr) env->DeleteLocalRef(key);
    return succeeded;
}

jobject FindTarget(JNIEnv* env, int target_id, bool target_is_player) {
    jobject world = env->GetStaticObjectField(
        g_bindings.iso_world, g_bindings.world_instance);
    jobject cell = world == nullptr
        ? nullptr : env->CallObjectMethod(world, g_bindings.get_cell);
    jobject objects = cell == nullptr
        ? nullptr : env->CallObjectMethod(cell, g_bindings.get_object_list);
    jobject iterator = objects == nullptr
        ? nullptr : env->CallObjectMethod(objects, g_bindings.set_iterator);
    if (iterator == nullptr || ClearException(env)) {
        if (iterator != nullptr) env->DeleteLocalRef(iterator);
        if (objects != nullptr) env->DeleteLocalRef(objects);
        if (cell != nullptr) env->DeleteLocalRef(cell);
        if (world != nullptr) env->DeleteLocalRef(world);
        return nullptr;
    }

    jobject found = nullptr;
    while (env->CallBooleanMethod(
               iterator, g_bindings.iterator_has_next) == JNI_TRUE &&
           !ClearException(env)) {
        jobject object = env->CallObjectMethod(iterator, g_bindings.iterator_next);
        if (object == nullptr || ClearException(env)) {
            if (object != nullptr) env->DeleteLocalRef(object);
            break;
        }
        const bool expected_type = target_is_player
            ? env->IsInstanceOf(object, g_bindings.iso_player) == JNI_TRUE
            : env->IsInstanceOf(object, g_bindings.iso_zombie) == JNI_TRUE;
        const int object_id = expected_type
            ? env->CallIntMethod(object, g_bindings.get_id) : -1;
        if (expected_type && object_id == target_id && !ClearException(env)) {
            found = object;
            break;
        }
        env->DeleteLocalRef(object);
    }
    env->DeleteLocalRef(iterator);
    env->DeleteLocalRef(objects);
    env->DeleteLocalRef(cell);
    env->DeleteLocalRef(world);
    return found;
}

bool IsNewShot(JNIEnv* env, jobject player, int character_id) {
    jobject network_ai = env->CallObjectMethod(
        player, g_bindings.get_network_ai);
    if (network_ai == nullptr ||
        env->IsInstanceOf(network_ai, g_bindings.network_player_ai) != JNI_TRUE ||
        ClearException(env)) {
        if (network_ai != nullptr) env->DeleteLocalRef(network_ai);
        return false;
    }
    const jbyte shot_id = env->CallByteMethod(
        network_ai, g_bindings.get_shot_id);
    const bool attack_started = env->CallBooleanMethod(
        player, g_bindings.is_attack_started) == JNI_TRUE;
    env->DeleteLocalRef(network_ai);
    if (ClearException(env)) return false;

    if (!g_has_last_shot || g_last_character_id != character_id) {
        g_last_character_id = character_id;
        g_last_shot_id = shot_id;
        g_has_last_shot = true;
        return attack_started;
    }
    if (shot_id == g_last_shot_id) return false;
    g_last_shot_id = shot_id;
    return true;
}

}  // namespace

bool TryReplaceMagicBulletHitList(JNIEnv* env, int character_id,
                                  int target_id, int body_part,
                                  bool target_is_player) {
    if (env == nullptr || target_id < 0 || !Initialize(env)) return false;
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }
    const int player_id = env->CallIntMethod(player, g_bindings.get_id);
    if (ClearException(env) || player_id != character_id ||
        !IsNewShot(env, player, character_id)) {
        env->DeleteLocalRef(player);
        return false;
    }

    jobject target = FindTarget(env, target_id, target_is_player);
    if (target == nullptr || env->CallBooleanMethod(
            target, g_bindings.is_dead) == JNI_TRUE || ClearException(env)) {
        if (target != nullptr) env->DeleteLocalRef(target);
        env->DeleteLocalRef(player);
        return false;
    }

    const float player_x = env->CallFloatMethod(player, g_bindings.get_x);
    const float player_y = env->CallFloatMethod(player, g_bindings.get_y);
    const float player_z = env->CallFloatMethod(player, g_bindings.get_z);
    const float target_x = env->CallFloatMethod(target, g_bindings.get_x);
    const float target_y = env->CallFloatMethod(target, g_bindings.get_y);
    const float target_z = env->CallFloatMethod(target, g_bindings.get_z);
    if (ClearException(env)) {
        env->DeleteLocalRef(target);
        env->DeleteLocalRef(player);
        return false;
    }
    const float delta_x = target_x - player_x;
    const float delta_y = target_y - player_y;
    const float delta_z = (std::floor(target_z) - std::floor(player_z)) * 3.0f;
    const float distance_squared = delta_x * delta_x + delta_y * delta_y +
        delta_z * delta_z;

    jobject hit_info = env->NewObject(
        g_bindings.hit_info, g_bindings.hit_info_constructor);
    jobject initialized = hit_info == nullptr
        ? nullptr
        : env->CallObjectMethod(
              hit_info, g_bindings.hit_info_init, target, 1.0f,
              distance_squared, target_x, target_y, target_z);
    if (initialized != nullptr) env->DeleteLocalRef(initialized);
    if (hit_info == nullptr || ClearException(env)) {
        if (hit_info != nullptr) env->DeleteLocalRef(hit_info);
        env->DeleteLocalRef(target);
        env->DeleteLocalRef(player);
        return false;
    }
    env->SetIntField(hit_info, g_bindings.hit_chance, 100);
    env->CallVoidMethod(player, g_bindings.clear_hit_info);
    jobject hit_list = env->CallObjectMethod(
        player, g_bindings.get_hit_info_list);
    const bool added = hit_list != nullptr &&
        env->CallBooleanMethod(hit_list, g_bindings.list_add, hit_info) == JNI_TRUE &&
        !ClearException(env);
    const bool reaction_set = added &&
        SetTargetedHitReaction(env, player, body_part);
    if (hit_list != nullptr) env->DeleteLocalRef(hit_list);
    env->DeleteLocalRef(hit_info);
    env->DeleteLocalRef(target);
    env->DeleteLocalRef(player);
    return added && reaction_set;
}

void ResetMagicBulletShotTracking() {
    g_last_character_id = -1;
    g_last_shot_id = 0;
    g_has_last_shot = false;
}

}  // namespace pztrainer::features::aim
