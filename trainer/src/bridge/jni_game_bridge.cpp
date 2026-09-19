#include "bridge/jni_game_bridge.hpp"
#include "bridge/model_outline_bridge.hpp"

#include "vmprotect.hpp"

#include <Windows.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pztrainer::bridge {
namespace {

using GetCreatedJavaVMsFn = jint(JNICALL*)(JavaVM**, jsize, jsize*);

struct Bindings {
    JavaVM* vm = nullptr;
    bool initialized = false;
    jobject class_loader = nullptr;
    jmethodID load_class = nullptr;

    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass iso_animal = nullptr;
    jclass base_vehicle = nullptr;
    jclass item_container = nullptr;
    jclass iso_world = nullptr;
    jclass iso_cell = nullptr;
    jclass array_list = nullptr;
    jclass hash_map = nullptr;
    jclass collection = nullptr;
    jclass set = nullptr;
    jclass iterator = nullptr;
    jclass moving_object = nullptr;
    jclass iso_utils = nullptr;
    jclass core = nullptr;
    jclass animation_player = nullptr;
    jclass matrix4f = nullptr;
    jclass skinning_data = nullptr;
    jclass skinning_bone = nullptr;
    jclass skeleton_bone_class = nullptr;
    jclass iso_game_character = nullptr;
    jclass safety = nullptr;
    jclass combat_manager = nullptr;
    jclass los_util = nullptr;
    jclass los_test_results = nullptr;
    jclass system = nullptr;

    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jfieldID client_player_map = nullptr;
    jfieldID world_instance = nullptr;
    jobject los_blocked = nullptr;
    jobject los_closed_door = nullptr;

    jmethodID get_player = nullptr;
    jmethodID get_cell = nullptr;
    jmethodID get_zombie_list = nullptr;
    jmethodID get_object_list = nullptr;
    jmethodID get_vehicles = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID map_values = nullptr;
    jmethodID collection_iterator = nullptr;
    jmethodID set_iterator = nullptr;
    jmethodID iterator_has_next = nullptr;
    jmethodID iterator_next = nullptr;
    jmethodID get_x = nullptr;
    jmethodID get_y = nullptr;
    jmethodID get_z = nullptr;
    jmethodID get_id = nullptr;
    jmethodID get_online_id = nullptr;
    jmethodID player_get_online_id = nullptr;
    jmethodID player_get_username = nullptr;
    jmethodID player_get_access_level = nullptr;
    jmethodID character_is_invisible = nullptr;
    jmethodID player_get_safety = nullptr;
    jmethodID safety_is_enabled = nullptr;
    jmethodID check_pvp = nullptr;
    jmethodID animal_get_custom_name = nullptr;
    jmethodID animal_get_full_name = nullptr;
    jmethodID animal_get_type = nullptr;
    jmethodID animal_get_size = nullptr;
    jmethodID animal_is_baby = nullptr;
    jmethodID vehicle_get_id = nullptr;
    jmethodID vehicle_get_script_name = nullptr;
    jmethodID vehicle_get_engine_condition = nullptr;
    jmethodID vehicle_is_engine_running = nullptr;
    jmethodID vehicle_all_doors_locked = nullptr;
    jmethodID vehicle_any_door_locked = nullptr;
    jmethodID vehicle_is_hotwired = nullptr;
    jmethodID vehicle_is_hotwire_broken = nullptr;
    jmethodID vehicle_keys_in_ignition = nullptr;
    jmethodID vehicle_get_key_id = nullptr;
    jmethodID player_get_inventory = nullptr;
    jmethodID inventory_have_key_id = nullptr;
    jmethodID get_health = nullptr;
    jmethodID is_alive = nullptr;
    jmethodID is_crawling = nullptr;
    jmethodID is_prone = nullptr;
    jmethodID is_falling = nullptr;
    jmethodID is_knocked_down = nullptr;
    jmethodID is_fall_on_front = nullptr;
    jmethodID set_alpha_and_target = nullptr;
    jmethodID x_to_screen = nullptr;
    jmethodID y_to_screen = nullptr;
    jmethodID get_core = nullptr;
    jmethodID get_zoom = nullptr;
    jmethodID get_animation_player = nullptr;
    jmethodID animation_player_ready = nullptr;
    jmethodID get_rendered_angle = nullptr;
    jmethodID get_bone_index = nullptr;
    jmethodID get_bone_model_transform = nullptr;
    jmethodID get_model_transform_at = nullptr;
    jmethodID get_skinning_data = nullptr;
    jmethodID skinning_get_bone = nullptr;
    jmethodID skeleton_bone_index = nullptr;
    jmethodID matrix_constructor = nullptr;
    jmethodID get_forward_x = nullptr;
    jmethodID get_forward_y = nullptr;
    jmethodID character_forward_x = nullptr;
    jmethodID character_forward_y = nullptr;
    jmethodID line_clear = nullptr;
    jmethodID identity_hash_code = nullptr;
    jfieldID matrix_m30 = nullptr;
    jfieldID matrix_m31 = nullptr;
    jfieldID matrix_m32 = nullptr;
    jfieldID matrix_m03 = nullptr;
    jfieldID matrix_m13 = nullptr;
    jfieldID matrix_m23 = nullptr;
    std::array<jfieldID, kZombieBoneCount> skeleton_bones{};
    std::array<jint, kZombieBoneCount> skeleton_bone_indices{};
    bool bones_available = false;
    bool visual_state_available = false;
};

Bindings g_bindings;
FrameSnapshot g_frame;
ModelOutlineBridge g_model_outline_bridge;
std::unordered_map<jint, float> g_zombie_max_health;
std::unordered_map<jint, float> g_animal_max_health;
constexpr std::array<const char*, kZombieBoneCount> kBoneNames{{
    "Bip01_Head", "Bip01_Neck", "Bip01_Spine1", "Bip01_Spine", "Bip01_Pelvis",
    "Bip01_L_Clavicle", "Bip01_L_UpperArm", "Bip01_L_Forearm", "Bip01_L_Hand",
    "Bip01_R_Clavicle", "Bip01_R_UpperArm", "Bip01_R_Forearm", "Bip01_R_Hand",
    "Bip01_L_Thigh", "Bip01_L_Calf", "Bip01_L_Foot",
    "Bip01_R_Thigh", "Bip01_R_Calf", "Bip01_R_Foot",
}};
constexpr float kModelHorizontalScale = 1.5f;
constexpr float kModelVerticalScale = 0.61237234f;
const char* g_binding_error = "未知 JNI 绑定";

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) {
        return false;
    }
    env->ExceptionClear();
    return true;
}

jclass MakeGlobalClass(JNIEnv* env, const char* name) {
    if (g_bindings.class_loader == nullptr || g_bindings.load_class == nullptr) {
        return nullptr;
    }

    std::string dotted_name(name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring java_name = env->NewStringUTF(dotted_name.c_str());
    if (java_name == nullptr || ClearException(env)) {
        return nullptr;
    }
    jclass local = static_cast<jclass>(
        env->CallObjectMethod(g_bindings.class_loader, g_bindings.load_class, java_name));
    env->DeleteLocalRef(java_name);
    if (local == nullptr || ClearException(env)) {
        return nullptr;
    }
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

JNIEnv* GetEnvironment() {
    if (g_bindings.vm == nullptr) {
        HMODULE jvm_module = GetModuleHandleW(L"jvm.dll");
        if (jvm_module == nullptr) {
            return nullptr;
        }
        const auto get_vms = reinterpret_cast<GetCreatedJavaVMsFn>(
            GetProcAddress(jvm_module, "JNI_GetCreatedJavaVMs"));
        if (get_vms == nullptr) {
            return nullptr;
        }

        jsize count = 0;
        if (get_vms(&g_bindings.vm, 1, &count) != JNI_OK || count != 1) {
            g_bindings.vm = nullptr;
            return nullptr;
        }
    }

    JNIEnv* env = nullptr;
    if (g_bindings.vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return nullptr;
    }
    return env;
}

PZ_VMP_NOINLINE bool InitializeBindings(JNIEnv* env) {
    if (g_bindings.initialized) {
        return true;
    }
    PZ_VMP_BEGIN_MUTATION("PZ.DLL.JniCoreBindings");

    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearException(env)) {
        g_binding_error = "引导类 ClassLoader";
        return false;
    }
    jmethodID get_system_class_loader = env->GetStaticMethodID(
        class_loader_class,
        "getSystemClassLoader",
        "()Ljava/lang/ClassLoader;");
    g_bindings.load_class = env->GetMethodID(
        class_loader_class,
        "loadClass",
        "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject local_loader = get_system_class_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(class_loader_class, get_system_class_loader);
    if (local_loader != nullptr && !ClearException(env)) {
        g_bindings.class_loader = env->NewGlobalRef(local_loader);
        env->DeleteLocalRef(local_loader);
    }
    env->DeleteLocalRef(class_loader_class);
    if (g_bindings.class_loader == nullptr || g_bindings.load_class == nullptr) {
        ClearException(env);
        g_binding_error = "JVM system class loader";
        return false;
    }

    g_bindings.game_client = MakeGlobalClass(env, "zombie/network/GameClient");
    g_bindings.game_server = MakeGlobalClass(env, "zombie/network/GameServer");
    g_bindings.iso_player = MakeGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.iso_animal = MakeGlobalClass(
        env, "zombie/characters/animals/IsoAnimal");
    g_bindings.base_vehicle = MakeGlobalClass(env, "zombie/vehicles/BaseVehicle");
    g_bindings.item_container = MakeGlobalClass(env, "zombie/inventory/ItemContainer");
    g_bindings.iso_world = MakeGlobalClass(env, "zombie/iso/IsoWorld");
    g_bindings.iso_cell = MakeGlobalClass(env, "zombie/iso/IsoCell");
    g_bindings.array_list = MakeGlobalClass(env, "java/util/ArrayList");
    g_bindings.hash_map = MakeGlobalClass(env, "java/util/HashMap");
    g_bindings.collection = MakeGlobalClass(env, "java/util/Collection");
    g_bindings.set = MakeGlobalClass(env, "java/util/Set");
    g_bindings.iterator = MakeGlobalClass(env, "java/util/Iterator");
    g_bindings.moving_object = MakeGlobalClass(env, "zombie/iso/IsoMovingObject");
    g_bindings.iso_utils = MakeGlobalClass(env, "zombie/iso/IsoUtils");
    g_bindings.core = MakeGlobalClass(env, "zombie/core/Core");
    g_bindings.animation_player = MakeGlobalClass(
        env, "zombie/core/skinnedmodel/animation/AnimationPlayer");
    g_bindings.matrix4f = MakeGlobalClass(env, "org/lwjgl/util/vector/Matrix4f");
    g_bindings.skinning_data = MakeGlobalClass(
        env, "zombie/core/skinnedmodel/model/SkinningData");
    g_bindings.skinning_bone = MakeGlobalClass(
        env, "zombie/core/skinnedmodel/model/SkinningBone");
    g_bindings.skeleton_bone_class = MakeGlobalClass(
        env, "zombie/core/skinnedmodel/model/SkeletonBone");
    g_bindings.iso_game_character = MakeGlobalClass(env, "zombie/characters/IsoGameCharacter");
    g_bindings.safety = MakeGlobalClass(env, "zombie/characters/Safety");
    g_bindings.combat_manager = MakeGlobalClass(env, "zombie/CombatManager");
    g_bindings.los_util = MakeGlobalClass(env, "zombie/iso/LosUtil");
    g_bindings.los_test_results = MakeGlobalClass(env, "zombie/iso/LosUtil$TestResults");
    g_bindings.system = MakeGlobalClass(env, "java/lang/System");

    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.iso_animal == nullptr ||
        g_bindings.base_vehicle == nullptr || g_bindings.item_container == nullptr ||
        g_bindings.iso_world == nullptr ||
        g_bindings.iso_cell == nullptr || g_bindings.array_list == nullptr ||
        g_bindings.hash_map == nullptr || g_bindings.collection == nullptr ||
        g_bindings.set == nullptr || g_bindings.iterator == nullptr ||
        g_bindings.moving_object == nullptr || g_bindings.iso_utils == nullptr ||
        g_bindings.core == nullptr || g_bindings.animation_player == nullptr ||
        g_bindings.matrix4f == nullptr || g_bindings.skinning_data == nullptr ||
        g_bindings.skinning_bone == nullptr ||
        g_bindings.skeleton_bone_class == nullptr ||
        g_bindings.iso_game_character == nullptr || g_bindings.safety == nullptr ||
        g_bindings.combat_manager == nullptr) {
        if (g_bindings.game_client == nullptr) g_binding_error = "类 GameClient";
        else if (g_bindings.game_server == nullptr) g_binding_error = "类 GameServer";
        else if (g_bindings.iso_player == nullptr) g_binding_error = "类 IsoPlayer";
        else if (g_bindings.iso_animal == nullptr) g_binding_error = "类 IsoAnimal";
        else if (g_bindings.base_vehicle == nullptr) g_binding_error = "类 BaseVehicle";
        else if (g_bindings.iso_world == nullptr) g_binding_error = "类 IsoWorld";
        else if (g_bindings.iso_cell == nullptr) g_binding_error = "类 IsoCell";
        else if (g_bindings.safety == nullptr) g_binding_error = "类 Safety";
        else if (g_bindings.combat_manager == nullptr) g_binding_error = "类 CombatManager";
        else if (g_bindings.array_list == nullptr) g_binding_error = "类 ArrayList";
        else if (g_bindings.hash_map == nullptr) g_binding_error = "类 HashMap";
        else if (g_bindings.collection == nullptr) g_binding_error = "类 Collection";
        else if (g_bindings.moving_object == nullptr) g_binding_error = "类 IsoMovingObject";
        else if (g_bindings.iso_utils == nullptr) g_binding_error = "类 IsoUtils";
        else g_binding_error = "类 Core";
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.client_player_map = env->GetStaticFieldID(
        g_bindings.game_client, "IDToPlayerMap", "Ljava/util/HashMap;");
    g_bindings.world_instance = env->GetStaticFieldID(
        g_bindings.iso_world, "instance", "Lzombie/iso/IsoWorld;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_cell = env->GetMethodID(
        g_bindings.iso_world, "getCell", "()Lzombie/iso/IsoCell;");
    g_bindings.get_zombie_list = env->GetMethodID(
        g_bindings.iso_cell, "getZombieList", "()Ljava/util/ArrayList;");
    g_bindings.get_object_list = env->GetMethodID(
        g_bindings.iso_cell, "getObjectList", "()Ljava/util/Set;");
    g_bindings.get_vehicles = env->GetMethodID(
        g_bindings.iso_cell, "getVehicles", "()Ljava/util/Set;");
    g_bindings.list_size = env->GetMethodID(g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.map_values = env->GetMethodID(
        g_bindings.hash_map, "values", "()Ljava/util/Collection;");
    g_bindings.collection_iterator = env->GetMethodID(
        g_bindings.collection, "iterator", "()Ljava/util/Iterator;");
    g_bindings.set_iterator = env->GetMethodID(
        g_bindings.set, "iterator", "()Ljava/util/Iterator;");
    g_bindings.iterator_has_next = env->GetMethodID(
        g_bindings.iterator, "hasNext", "()Z");
    g_bindings.iterator_next = env->GetMethodID(
        g_bindings.iterator, "next", "()Ljava/lang/Object;");
    g_bindings.get_x = env->GetMethodID(g_bindings.moving_object, "getX", "()F");
    g_bindings.get_y = env->GetMethodID(g_bindings.moving_object, "getY", "()F");
    g_bindings.get_z = env->GetMethodID(g_bindings.moving_object, "getZ", "()F");
    g_bindings.get_id = env->GetMethodID(
        g_bindings.iso_game_character, "getID", "()I");
    if (g_bindings.system != nullptr) {
        g_bindings.identity_hash_code = env->GetStaticMethodID(
            g_bindings.system, "identityHashCode", "(Ljava/lang/Object;)I");
    }

    if (g_bindings.iso_game_character != nullptr) {
        g_bindings.get_health = env->GetMethodID(
            g_bindings.iso_game_character, "getHealth", "()F");
        g_bindings.is_alive = env->GetMethodID(
            g_bindings.iso_game_character, "isAlive", "()Z");
        g_bindings.get_animation_player = env->GetMethodID(
            g_bindings.iso_game_character, "getAnimationPlayer",
            "()Lzombie/core/skinnedmodel/animation/AnimationPlayer;");
        g_bindings.get_forward_x = env->GetMethodID(
            g_bindings.iso_game_character, "getForwardDirectionX", "()F");
        g_bindings.get_forward_y = env->GetMethodID(
            g_bindings.iso_game_character, "getForwardDirectionY", "()F");
        g_bindings.is_fall_on_front = env->GetMethodID(
            g_bindings.iso_game_character, "isFallOnFront", "()Z");
        g_bindings.is_falling = env->GetMethodID(
            g_bindings.iso_game_character, "isFalling", "()Z");
        g_bindings.is_knocked_down = env->GetMethodID(
            g_bindings.iso_game_character, "isKnockedDown", "()Z");
        g_bindings.character_is_invisible = env->GetMethodID(
            g_bindings.iso_game_character, "isInvisible", "()Z");
        g_bindings.set_alpha_and_target = env->GetMethodID(
            g_bindings.iso_game_character, "setAlphaAndTarget", "(F)V");
    }
    g_bindings.player_get_online_id = env->GetMethodID(
        g_bindings.iso_player, "getOnlineID", "()S");
    g_bindings.player_get_username = env->GetMethodID(
        g_bindings.iso_player, "getUsername", "()Ljava/lang/String;");
    g_bindings.player_get_access_level = env->GetMethodID(
        g_bindings.iso_player, "getAccessLevel", "()Ljava/lang/String;");
    g_bindings.player_get_safety = env->GetMethodID(
        g_bindings.iso_player, "getSafety", "()Lzombie/characters/Safety;");
    if (g_bindings.safety != nullptr) {
        g_bindings.safety_is_enabled = env->GetMethodID(
            g_bindings.safety, "isEnabled", "()Z");
    }
    if (g_bindings.combat_manager != nullptr) {
        g_bindings.check_pvp = env->GetStaticMethodID(
            g_bindings.combat_manager, "checkPVP",
            "(Lzombie/iso/IsoMovingObject;Lzombie/iso/IsoMovingObject;Z)Z");
    }
    g_bindings.animal_get_custom_name = env->GetMethodID(
        g_bindings.iso_animal, "getCustomName", "()Ljava/lang/String;");
    g_bindings.animal_get_full_name = env->GetMethodID(
        g_bindings.iso_animal, "getFullName", "()Ljava/lang/String;");
    g_bindings.animal_get_type = env->GetMethodID(
        g_bindings.iso_animal, "getAnimalType", "()Ljava/lang/String;");
    g_bindings.animal_get_size = env->GetMethodID(
        g_bindings.iso_animal, "getAnimalSize", "()F");
    g_bindings.animal_is_baby = env->GetMethodID(
        g_bindings.iso_animal, "isBaby", "()Z");
    g_bindings.vehicle_get_id = env->GetMethodID(
        g_bindings.base_vehicle, "getId", "()S");
    g_bindings.vehicle_get_script_name = env->GetMethodID(
        g_bindings.base_vehicle, "getScriptName", "()Ljava/lang/String;");
    g_bindings.vehicle_get_engine_condition = env->GetMethodID(
        g_bindings.base_vehicle, "getEngineCondition", "()I");
    g_bindings.vehicle_is_engine_running = env->GetMethodID(
        g_bindings.base_vehicle, "isEngineRunning", "()Z");
    g_bindings.vehicle_all_doors_locked = env->GetMethodID(
        g_bindings.base_vehicle, "areAllDoorsLocked", "()Z");
    g_bindings.vehicle_any_door_locked = env->GetMethodID(
        g_bindings.base_vehicle, "isAnyDoorLocked", "()Z");
    g_bindings.vehicle_is_hotwired = env->GetMethodID(
        g_bindings.base_vehicle, "isHotwired", "()Z");
    g_bindings.vehicle_is_hotwire_broken = env->GetMethodID(
        g_bindings.base_vehicle, "isHotwiredBroken", "()Z");
    g_bindings.vehicle_keys_in_ignition = env->GetMethodID(
        g_bindings.base_vehicle, "isKeysInIgnition", "()Z");
    g_bindings.vehicle_get_key_id = env->GetMethodID(
        g_bindings.base_vehicle, "getKeyId", "()I");
    g_bindings.player_get_inventory = env->GetMethodID(
        g_bindings.iso_player, "getInventory", "()Lzombie/inventory/ItemContainer;");
    g_bindings.inventory_have_key_id = env->GetMethodID(
        g_bindings.item_container, "haveThisKeyId",
        "(I)Lzombie/inventory/InventoryItem;");

    jclass zombie_class = MakeGlobalClass(env, "zombie/characters/IsoZombie");
    if (zombie_class != nullptr) {
        g_bindings.get_online_id = env->GetMethodID(zombie_class, "getOnlineID", "()S");
        g_bindings.is_crawling = env->GetMethodID(zombie_class, "isCrawling", "()Z");
        g_bindings.is_prone = env->GetMethodID(zombie_class, "isProne", "()Z");
        env->DeleteGlobalRef(zombie_class);
    }

    g_bindings.x_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "XToScreenExact", "(FFFI)F");
    g_bindings.y_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "YToScreenExact", "(FFFI)F");
    g_bindings.get_core = env->GetStaticMethodID(
        g_bindings.core, "getInstance", "()Lzombie/core/Core;");
    g_bindings.get_zoom = env->GetMethodID(g_bindings.core, "getZoom", "(I)F");
    if (g_bindings.iso_game_character != nullptr && g_bindings.los_util != nullptr &&
        g_bindings.los_test_results != nullptr) {
        g_bindings.character_forward_x = env->GetMethodID(
            g_bindings.iso_game_character, "getForwardDirectionX", "()F");
        g_bindings.character_forward_y = env->GetMethodID(
            g_bindings.iso_game_character, "getForwardDirectionY", "()F");
        g_bindings.line_clear = env->GetStaticMethodID(
            g_bindings.los_util, "lineClear",
            "(Lzombie/iso/IsoCell;IIIIIIZ)Lzombie/iso/LosUtil$TestResults;");
        const jfieldID blocked_field = env->GetStaticFieldID(
            g_bindings.los_test_results, "Blocked", "Lzombie/iso/LosUtil$TestResults;");
        const jfieldID closed_door_field = env->GetStaticFieldID(
            g_bindings.los_test_results, "ClearThroughClosedDoor",
            "Lzombie/iso/LosUtil$TestResults;");
        jobject blocked = blocked_field == nullptr
            ? nullptr : env->GetStaticObjectField(g_bindings.los_test_results, blocked_field);
        jobject closed_door = closed_door_field == nullptr
            ? nullptr : env->GetStaticObjectField(g_bindings.los_test_results, closed_door_field);
        if (!ClearException(env) && g_bindings.character_forward_x != nullptr &&
            g_bindings.character_forward_y != nullptr &&
            g_bindings.line_clear != nullptr &&
            blocked != nullptr && closed_door != nullptr) {
            g_bindings.los_blocked = env->NewGlobalRef(blocked);
            g_bindings.los_closed_door = env->NewGlobalRef(closed_door);
            g_bindings.visual_state_available =
                g_bindings.los_blocked != nullptr && g_bindings.los_closed_door != nullptr;
        }
        if (blocked != nullptr) env->DeleteLocalRef(blocked);
        if (closed_door != nullptr) env->DeleteLocalRef(closed_door);
        ClearException(env);
    }
    g_bindings.animation_player_ready = env->GetMethodID(
        g_bindings.animation_player, "isReady", "()Z");
    g_bindings.get_rendered_angle = env->GetMethodID(
        g_bindings.animation_player, "getRenderedAngle", "()F");
    g_bindings.get_bone_index = env->GetMethodID(
        g_bindings.animation_player, "getSkinningBoneIndex", "(Ljava/lang/String;I)I");
    g_bindings.get_bone_model_transform = env->GetMethodID(
        g_bindings.animation_player, "getBoneModelTransform",
        "(ILorg/lwjgl/util/vector/Matrix4f;)Lorg/lwjgl/util/vector/Matrix4f;");
    g_bindings.get_model_transform_at = env->GetMethodID(
        g_bindings.animation_player, "getModelTransformAt",
        "(I)Lorg/lwjgl/util/vector/Matrix4f;");
    g_bindings.get_skinning_data = env->GetMethodID(
        g_bindings.animation_player, "getSkinningData",
        "()Lzombie/core/skinnedmodel/model/SkinningData;");
    g_bindings.skinning_get_bone = env->GetMethodID(
        g_bindings.skinning_data, "getBone",
        "(Ljava/lang/String;)Lzombie/core/skinnedmodel/model/SkinningBone;");
    g_bindings.skeleton_bone_index = env->GetMethodID(
        g_bindings.skeleton_bone_class, "index", "()I");
    bool bone_indices_available = g_bindings.skeleton_bone_index != nullptr;
    for (std::size_t index = 0;
         bone_indices_available && index < kBoneNames.size(); ++index) {
        g_bindings.skeleton_bones[index] = env->GetStaticFieldID(
            g_bindings.skeleton_bone_class, kBoneNames[index],
            "Lzombie/core/skinnedmodel/model/SkeletonBone;");
        if (g_bindings.skeleton_bones[index] == nullptr || ClearException(env)) {
            bone_indices_available = false;
            break;
        }
        jobject skeleton = env->GetStaticObjectField(
            g_bindings.skeleton_bone_class, g_bindings.skeleton_bones[index]);
        if (skeleton == nullptr || ClearException(env)) {
            if (skeleton != nullptr) env->DeleteLocalRef(skeleton);
            bone_indices_available = false;
            break;
        }
        g_bindings.skeleton_bone_indices[index] = env->CallIntMethod(
            skeleton, g_bindings.skeleton_bone_index);
        env->DeleteLocalRef(skeleton);
        if (g_bindings.skeleton_bone_indices[index] < 0 || ClearException(env)) {
            bone_indices_available = false;
        }
    }
    ClearException(env);
    g_bindings.matrix_constructor = env->GetMethodID(g_bindings.matrix4f, "<init>", "()V");
    g_bindings.matrix_m30 = env->GetFieldID(g_bindings.matrix4f, "m30", "F");
    g_bindings.matrix_m31 = env->GetFieldID(g_bindings.matrix4f, "m31", "F");
    g_bindings.matrix_m32 = env->GetFieldID(g_bindings.matrix4f, "m32", "F");
    g_bindings.matrix_m03 = env->GetFieldID(g_bindings.matrix4f, "m03", "F");
    g_bindings.matrix_m13 = env->GetFieldID(g_bindings.matrix4f, "m13", "F");
    g_bindings.matrix_m23 = env->GetFieldID(g_bindings.matrix4f, "m23", "F");
    g_bindings.bones_available = bone_indices_available &&
        g_bindings.get_model_transform_at != nullptr &&
        g_bindings.get_rendered_angle != nullptr &&
        g_bindings.matrix_m03 != nullptr && g_bindings.matrix_m13 != nullptr &&
        g_bindings.matrix_m23 != nullptr;
    if (ClearException(env) || g_bindings.client_flag == nullptr ||
        g_bindings.server_flag == nullptr || g_bindings.client_player_map == nullptr ||
        g_bindings.world_instance == nullptr ||
        g_bindings.get_player == nullptr || g_bindings.get_cell == nullptr ||
        g_bindings.get_zombie_list == nullptr || g_bindings.get_object_list == nullptr ||
        g_bindings.get_vehicles == nullptr ||
        g_bindings.list_size == nullptr || g_bindings.list_get == nullptr ||
        g_bindings.map_values == nullptr || g_bindings.collection_iterator == nullptr ||
        g_bindings.set_iterator == nullptr || g_bindings.iterator_has_next == nullptr ||
        g_bindings.iterator_next == nullptr || g_bindings.get_x == nullptr ||
        g_bindings.get_y == nullptr || g_bindings.get_z == nullptr ||
        g_bindings.get_id == nullptr ||
        g_bindings.get_online_id == nullptr || g_bindings.player_get_online_id == nullptr ||
        g_bindings.player_get_username == nullptr ||
        g_bindings.player_get_access_level == nullptr ||
        g_bindings.character_is_invisible == nullptr ||
        g_bindings.player_get_safety == nullptr ||
        g_bindings.safety_is_enabled == nullptr ||
        g_bindings.check_pvp == nullptr ||
        g_bindings.animal_get_custom_name == nullptr ||
        g_bindings.animal_get_full_name == nullptr ||
        g_bindings.animal_get_type == nullptr ||
        g_bindings.animal_get_size == nullptr || g_bindings.animal_is_baby == nullptr ||
        g_bindings.vehicle_get_id == nullptr ||
        g_bindings.vehicle_get_script_name == nullptr ||
        g_bindings.vehicle_get_engine_condition == nullptr ||
        g_bindings.vehicle_is_engine_running == nullptr ||
        g_bindings.vehicle_all_doors_locked == nullptr ||
        g_bindings.vehicle_any_door_locked == nullptr ||
        g_bindings.vehicle_is_hotwired == nullptr ||
        g_bindings.vehicle_is_hotwire_broken == nullptr ||
        g_bindings.vehicle_keys_in_ignition == nullptr ||
        g_bindings.vehicle_get_key_id == nullptr ||
        g_bindings.player_get_inventory == nullptr ||
        g_bindings.inventory_have_key_id == nullptr ||
        g_bindings.get_health == nullptr ||
        g_bindings.is_alive == nullptr ||
        g_bindings.identity_hash_code == nullptr ||
        g_bindings.is_crawling == nullptr ||
        g_bindings.is_prone == nullptr || g_bindings.is_falling == nullptr ||
        g_bindings.is_knocked_down == nullptr ||
        g_bindings.is_fall_on_front == nullptr ||
        g_bindings.x_to_screen == nullptr ||
        g_bindings.y_to_screen == nullptr || g_bindings.get_core == nullptr ||
        g_bindings.get_zoom == nullptr || g_bindings.get_animation_player == nullptr ||
        g_bindings.get_forward_x == nullptr || g_bindings.get_forward_y == nullptr ||
        g_bindings.animation_player_ready == nullptr ||
        g_bindings.get_rendered_angle == nullptr || g_bindings.get_bone_index == nullptr ||
        g_bindings.get_bone_model_transform == nullptr || g_bindings.matrix_constructor == nullptr ||
        g_bindings.get_model_transform_at == nullptr ||
        g_bindings.matrix_m30 == nullptr || g_bindings.matrix_m31 == nullptr ||
        g_bindings.matrix_m32 == nullptr || g_bindings.matrix_m03 == nullptr ||
        g_bindings.matrix_m13 == nullptr || g_bindings.matrix_m23 == nullptr) {
        if (g_bindings.client_flag == nullptr) g_binding_error = "字段 GameClient.client";
        else if (g_bindings.server_flag == nullptr) g_binding_error = "字段 GameServer.server";
        else if (g_bindings.client_player_map == nullptr) g_binding_error = "字段 GameClient.IDToPlayerMap";
        else if (g_bindings.world_instance == nullptr) g_binding_error = "字段 IsoWorld.instance";
        else if (g_bindings.get_player == nullptr) g_binding_error = "方法 IsoPlayer.getInstance";
        else if (g_bindings.get_cell == nullptr) g_binding_error = "方法 IsoWorld.getCell";
        else if (g_bindings.get_zombie_list == nullptr) g_binding_error = "方法 IsoCell.getZombieList";
        else if (g_bindings.list_size == nullptr) g_binding_error = "方法 ArrayList.size";
        else if (g_bindings.list_get == nullptr) g_binding_error = "方法 ArrayList.get";
        else if (g_bindings.map_values == nullptr) g_binding_error = "方法 HashMap.values";
        else if (g_bindings.collection_iterator == nullptr) g_binding_error = "方法 Collection.iterator";
        else if (g_bindings.get_x == nullptr) g_binding_error = "方法 IsoMovingObject.getX";
        else if (g_bindings.get_y == nullptr) g_binding_error = "方法 IsoMovingObject.getY";
        else if (g_bindings.get_z == nullptr) g_binding_error = "方法 IsoMovingObject.getZ";
        else if (g_bindings.get_id == nullptr) g_binding_error = "方法 IsoGameCharacter.getID";
        else if (g_bindings.player_get_access_level == nullptr) g_binding_error = "方法 IsoPlayer.getAccessLevel";
        else if (g_bindings.character_is_invisible == nullptr) g_binding_error = "方法 IsoGameCharacter.isInvisible";
        else if (g_bindings.get_online_id == nullptr) g_binding_error = "方法 IsoZombie.getOnlineID";
        else if (g_bindings.get_health == nullptr) g_binding_error = "方法 IsoZombie.getHealth";
        else if (g_bindings.identity_hash_code == nullptr) g_binding_error = "方法 System.identityHashCode";
        else if (g_bindings.is_crawling == nullptr) g_binding_error = "方法 IsoZombie.isCrawling";
        else if (g_bindings.is_prone == nullptr) g_binding_error = "方法 IsoZombie.isProne";
        else if (g_bindings.is_falling == nullptr) g_binding_error = "方法 IsoGameCharacter.isFalling";
        else if (g_bindings.is_knocked_down == nullptr) g_binding_error = "方法 IsoGameCharacter.isKnockedDown";
        else if (g_bindings.x_to_screen == nullptr) g_binding_error = "方法 IsoUtils.XToScreenExact";
        else if (g_bindings.y_to_screen == nullptr) g_binding_error = "方法 IsoUtils.YToScreenExact";
        else if (g_bindings.get_core == nullptr) g_binding_error = "方法 Core.getInstance";
        else g_binding_error = "方法 Core.getZoom";
        return false;
    }

    g_bindings.initialized = true;
    PZ_VMP_END();
    return true;
}

void SetGate(GateStatus status, const char* message) {
    g_frame.gate_status = status;
    g_frame.gate_message = message;
    if (status != GateStatus::SinglePlayerAllowed) {
        g_frame.camera_zoom = 1.0f;
        g_frame.has_local_screen_position = false;
        g_frame.zombies.clear();
        g_frame.players.clear();
        g_frame.animals.clear();
        g_frame.vehicles.clear();
        g_zombie_max_health.clear();
        g_animal_max_health.clear();
    }
}

float TrackZombieHealthFraction(jint identity, float current_health) {
    if (!std::isfinite(current_health) || current_health <= 0.0f) return 0.0f;
    auto [entry, inserted] = g_zombie_max_health.try_emplace(identity, current_health);
    if (!inserted && current_health > entry->second) {
        entry->second = current_health;
    }
    return entry->second > 0.0f
        ? std::clamp(current_health / entry->second, 0.0f, 1.0f)
        : 0.0f;
}

float TrackAnimalHealthFraction(jint identity, float current_health) {
    if (!std::isfinite(current_health) || current_health <= 0.0f) return 0.0f;
    auto [entry, inserted] = g_animal_max_health.try_emplace(identity, current_health);
    if (!inserted && current_health > entry->second) {
        entry->second = current_health;
    }
    return entry->second > 0.0f
        ? std::clamp(current_health / entry->second, 0.0f, 1.0f)
        : 0.0f;
}

std::string ReadJavaString(JNIEnv* env, jobject object, jmethodID method) {
    if (object == nullptr || method == nullptr) return {};
    jstring value = static_cast<jstring>(env->CallObjectMethod(object, method));
    if (value == nullptr || ClearException(env)) {
        if (value != nullptr) env->DeleteLocalRef(value);
        return {};
    }
    std::string result;
    const char* utf8 = env->GetStringUTFChars(value, nullptr);
    if (utf8 != nullptr) {
        result = utf8;
        env->ReleaseStringUTFChars(value, utf8);
    }
    ClearException(env);
    env->DeleteLocalRef(value);
    return result;
}

template <typename Snapshot>
bool CollectCharacterBones(JNIEnv* env, jobject character, float world_x, float world_y,
                           float world_z, float camera_zoom, Snapshot& snapshot) {
    if (!g_bindings.bones_available) {
        snapshot.bone_failure = 4;
        return false;
    }
    jobject player = env->CallObjectMethod(character, g_bindings.get_animation_player);
    if (player == nullptr || ClearException(env)) {
        snapshot.bone_failure = 1;
        if (player != nullptr) env->DeleteLocalRef(player);
        return false;
    }
    if (env->CallBooleanMethod(player, g_bindings.animation_player_ready) != JNI_TRUE ||
        ClearException(env)) {
        snapshot.bone_failure = 2;
        env->DeleteLocalRef(player);
        return false;
    }
    const float rendered_angle = env->CallFloatMethod(
        player, g_bindings.get_rendered_angle);
    const bool rendered_angle_failed = ClearException(env);
    if (rendered_angle_failed || !std::isfinite(rendered_angle)) {
        snapshot.bone_failure = 3;
        env->DeleteLocalRef(player);
        return false;
    }
    const float angle_cosine = std::cos(rendered_angle);
    const float angle_sine = std::sin(rendered_angle);

    bool succeeded = true;
    // getBoneWorldPosition reaches getBoneModelTransform, which mutates static
    // Matrix4f scratch objects. Reading the precomputed transforms avoids racing
    // the game's animation update from the overlay render thread.
    for (std::size_t index = 0; index < kBoneNames.size(); ++index) {
        jobject transform = env->CallObjectMethod(
            player, g_bindings.get_model_transform_at,
            g_bindings.skeleton_bone_indices[index]);
        if (transform == nullptr || ClearException(env)) {
            if (transform != nullptr) env->DeleteLocalRef(transform);
            snapshot.bone_failure = 100 + static_cast<int>(index);
            succeeded = false;
            break;
        }
        const float model_x = env->GetFloatField(transform, g_bindings.matrix_m03);
        const float model_y = env->GetFloatField(transform, g_bindings.matrix_m13);
        const float model_z = env->GetFloatField(transform, g_bindings.matrix_m23);
        env->DeleteLocalRef(transform);
        const bool transform_read_failed = ClearException(env);
        if (transform_read_failed || !std::isfinite(model_x) ||
            !std::isfinite(model_y) || !std::isfinite(model_z)) {
            snapshot.bone_failure = 200 + static_cast<int>(index);
            succeeded = false;
            break;
        }
        const float negated_x = -model_x;
        const float rotated_x =
            negated_x * angle_cosine - model_z * angle_sine;
        const float rotated_z =
            negated_x * angle_sine + model_z * angle_cosine;
        const float bone_world_x =
            world_x + rotated_x * kModelHorizontalScale;
        const float bone_world_y =
            world_y + rotated_z * kModelHorizontalScale;
        const float bone_world_z =
            world_z + model_y * kModelVerticalScale;
        snapshot.bone_world_positions[index].x = bone_world_x;
        snapshot.bone_world_positions[index].y = bone_world_y;
        snapshot.bone_world_positions[index].z = bone_world_z;
        if (index == 0) {
            snapshot.bone_debug_row[0] = bone_world_x;
            snapshot.bone_debug_row[1] = bone_world_y;
            snapshot.bone_debug_row[2] = bone_world_z;
        }
        jvalue projection[4]{};
        projection[0].f = bone_world_x;
        projection[1].f = bone_world_y;
        projection[2].f = bone_world_z;
        projection[3].i = 0;
        snapshot.bones[index].x = env->CallStaticFloatMethodA(
            g_bindings.iso_utils, g_bindings.x_to_screen, projection) / camera_zoom;
        snapshot.bones[index].y = env->CallStaticFloatMethodA(
            g_bindings.iso_utils, g_bindings.y_to_screen, projection) / camera_zoom;
        if (ClearException(env)) {
            snapshot.bone_failure = 300 + static_cast<int>(index);
            succeeded = false;
            break;
        }
    }
    env->DeleteLocalRef(player);
    snapshot.has_bones = succeeded;
    return succeeded;
}

template <typename Snapshot>
void CollectCharacterVisualState(JNIEnv* env, jobject cell,
                                 float player_x, float player_y, float player_z,
                                 float player_forward_x, float player_forward_y,
                                 float target_x, float target_y, float target_z,
                                 Snapshot& snapshot) {
    if (!g_bindings.visual_state_available) return;

    jobject result = env->CallStaticObjectMethod(
        g_bindings.los_util, g_bindings.line_clear, cell,
        static_cast<jint>(std::floor(player_x)),
        static_cast<jint>(std::floor(player_y)),
        static_cast<jint>(std::floor(player_z)),
        static_cast<jint>(std::floor(target_x)),
        static_cast<jint>(std::floor(target_y)),
        static_cast<jint>(std::floor(target_z)), JNI_FALSE);
    if (result != nullptr && !ClearException(env)) {
        snapshot.behind_wall =
            env->IsSameObject(result, g_bindings.los_blocked) == JNI_TRUE ||
            env->IsSameObject(result, g_bindings.los_closed_door) == JNI_TRUE;
    } else {
        ClearException(env);
    }
    if (result != nullptr) env->DeleteLocalRef(result);

    const float delta_x = target_x - player_x;
    const float delta_y = target_y - player_y;
    const float delta_length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
    const float forward_length = std::sqrt(
        player_forward_x * player_forward_x + player_forward_y * player_forward_y);
    if (!snapshot.behind_wall && delta_length > 0.001f && forward_length > 0.001f) {
        const float facing_dot =
            (delta_x * player_forward_x + delta_y * player_forward_y) /
            (delta_length * forward_length);
        snapshot.in_view = facing_dot >= 0.0f;
    }
}

bool PlayerAlreadyCollected(jshort online_id, jint identity) {
    return std::any_of(
        g_frame.players.begin(), g_frame.players.end(),
        [online_id, identity](const PlayerSnapshot& snapshot) {
            if (online_id >= 0 && snapshot.online_id >= 0) {
                return snapshot.online_id == online_id;
            }
            return snapshot.identity == identity;
        });
}

void CollectPlayerObject(
        JNIEnv* env, jobject object, jobject local_player, jobject cell,
        float local_x, float local_y, float local_z,
        float local_forward_x, float local_forward_y, float camera_zoom,
        float maximum_distance, bool collect_bones, bool model_chams_armed,
        const ZombieModelVisualRequest& model_visual) {
    if (object == nullptr ||
        env->IsInstanceOf(object, g_bindings.iso_player) != JNI_TRUE ||
        env->IsSameObject(object, local_player) == JNI_TRUE) {
        return;
    }

    const float x = env->CallFloatMethod(object, g_bindings.get_x);
    const float y = env->CallFloatMethod(object, g_bindings.get_y);
    const float z = env->CallFloatMethod(object, g_bindings.get_z);
    const float delta_x = x - local_x;
    const float delta_y = y - local_y;
    const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
    if (ClearException(env) || distance > maximum_distance) return;

    const jint identity = env->CallStaticIntMethod(
        g_bindings.system, g_bindings.identity_hash_code, object);
    const jshort online_id = env->CallShortMethod(
        object, g_bindings.player_get_online_id);
    if (ClearException(env) || PlayerAlreadyCollected(online_id, identity)) return;

    jvalue projection[4]{};
    projection[0].f = x;
    projection[1].f = y;
    projection[2].f = z;
    projection[3].i = 0;
    const float screen_x = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.x_to_screen, projection) / camera_zoom;
    const float screen_y = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.y_to_screen, projection) / camera_zoom;
    const bool prone = env->CallBooleanMethod(
        object, g_bindings.is_fall_on_front) == JNI_TRUE;
    projection[2].f = z + (prone ? 0.20f : 0.58f);
    const float screen_top_y = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.y_to_screen, projection) / camera_zoom;
    const float health = env->CallFloatMethod(object, g_bindings.get_health);
    const bool alive = env->CallBooleanMethod(
        object, g_bindings.is_alive) == JNI_TRUE;
    const jint game_id = env->CallIntMethod(object, g_bindings.get_id);
    const bool invisible = env->CallBooleanMethod(
        object, g_bindings.character_is_invisible) == JNI_TRUE;
    jobject safety = env->CallObjectMethod(
        object, g_bindings.player_get_safety);
    const bool pvp_enabled = safety != nullptr &&
        env->CallBooleanMethod(safety, g_bindings.safety_is_enabled) != JNI_TRUE;
    const bool pvp_eligible = env->CallStaticBooleanMethod(
        g_bindings.combat_manager, g_bindings.check_pvp,
        local_player, object, JNI_FALSE) == JNI_TRUE;
    if (ClearException(env)) {
        if (safety != nullptr) env->DeleteLocalRef(safety);
        return;
    }

    PlayerSnapshot snapshot{};
    snapshot.world_x = x;
    snapshot.world_y = y;
    snapshot.world_z = z;
    snapshot.screen_x = screen_x;
    snapshot.screen_y = screen_y;
    snapshot.screen_top_y = screen_top_y;
    snapshot.distance = distance;
    snapshot.current_health = health;
    snapshot.health_fraction = std::clamp(health / 100.0f, 0.0f, 1.0f);
    snapshot.game_id = game_id;
    snapshot.identity = identity;
    snapshot.online_id = online_id;
    snapshot.prone = prone;
    snapshot.dead = !alive || health <= 0.0f;
    snapshot.pvp_enabled = alive && pvp_enabled && pvp_eligible;
    snapshot.name = ReadJavaString(
        env, object, g_bindings.player_get_username);
    snapshot.admin = ReadJavaString(
        env, object, g_bindings.player_get_access_level) == "admin";
    snapshot.invisible = invisible;
    CollectCharacterVisualState(
        env, cell, local_x, local_y, local_z,
        local_forward_x, local_forward_y, x, y, z, snapshot);
    if (collect_bones) {
        CollectCharacterBones(
            env, object, x, y, z, camera_zoom, snapshot);
    }
    if (model_chams_armed && model_visual.enabled) {
        if (model_visual.force_visible &&
            g_bindings.set_alpha_and_target != nullptr) {
            env->CallVoidMethod(
                object, g_bindings.set_alpha_and_target, 1.0f);
        }
        const ZombieModelVisualColor* color = &model_visual.normal;
        if (snapshot.behind_wall && model_visual.behind_wall_enabled) {
            color = &model_visual.behind_wall;
        } else if (snapshot.in_view && model_visual.in_view_enabled) {
            color = &model_visual.in_view;
        }
        g_model_outline_bridge.ApplyToObject(
            env, object, color->red, color->green,
            color->blue, color->alpha);
    }
    if (safety != nullptr) env->DeleteLocalRef(safety);
    g_frame.players.push_back(std::move(snapshot));
}

}  // namespace

JNIEnv* GetCurrentJniEnvironment() {
    return GetEnvironment();
}

const FrameSnapshot& CollectFrameSnapshot(
        bool collect_zombies, float max_distance, bool collect_bones,
        const ZombieModelVisualRequest& model_visual,
        bool collect_players, float player_max_distance,
        bool collect_player_bones,
        const ZombieModelVisualRequest& player_model_visual,
        bool collect_animals, float animal_max_distance,
        bool collect_animal_bones,
        const ZombieModelVisualRequest& animal_model_visual,
        bool collect_vehicles, float vehicle_max_distance,
        const ZombieModelVisualRequest& vehicle_model_visual) {
    g_frame.zombies.clear();
    g_frame.players.clear();
    g_frame.animals.clear();
    g_frame.vehicles.clear();
    g_frame.has_local_screen_position = false;
    g_frame.local_pvp_enabled = false;
    g_frame.model_chams_status = 0;
    JNIEnv* env = GetEnvironment();
    if (env == nullptr) {
        SetGate(GateStatus::Initializing, "等待已附加 JVM 的 OpenGL 线程");
        return g_frame;
    }
    if (!InitializeBindings(env)) {
        ClearException(env);
        g_frame.gate_status = GateStatus::Error;
        g_frame.gate_message = std::string("Build 42.20 JNI 签名解析失败：") + g_binding_error;
        g_frame.zombies.clear();
        g_frame.players.clear();
        g_frame.animals.clear();
        g_frame.vehicles.clear();
        return g_frame;
    }
    const bool network_client = env->GetStaticBooleanField(
        g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    if (ClearException(env)) {
        SetGate(GateStatus::Error, "读取世界状态失败");
        return g_frame;
    }

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    jobject world = env->GetStaticObjectField(g_bindings.iso_world, g_bindings.world_instance);
    if (ClearException(env) || player == nullptr || world == nullptr) {
        if (player != nullptr) env->DeleteLocalRef(player);
        if (world != nullptr) env->DeleteLocalRef(world);
        SetGate(GateStatus::WaitingForWorld, "等待进入游戏世界");
        return g_frame;
    }

    SetGate(GateStatus::SinglePlayerAllowed, "游戏世界已连接，只读 ESP 可用");
    if (!collect_zombies && !collect_players && !collect_animals && !collect_vehicles) {
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        return g_frame;
    }

    jobject cell = env->CallObjectMethod(world, g_bindings.get_cell);
    jobject zombies = cell == nullptr
        ? nullptr
        : env->CallObjectMethod(cell, g_bindings.get_zombie_list);
    jobject objects = cell == nullptr
        ? nullptr
        : env->CallObjectMethod(cell, g_bindings.get_object_list);
    jobject vehicles = cell == nullptr || !collect_vehicles
        ? nullptr
        : env->CallObjectMethod(cell, g_bindings.get_vehicles);
    if (ClearException(env) || cell == nullptr ||
        (collect_zombies && zombies == nullptr) ||
        ((collect_players || collect_animals) && objects == nullptr) ||
        (collect_vehicles && vehicles == nullptr)) {
        if (vehicles != nullptr) env->DeleteLocalRef(vehicles);
        if (objects != nullptr) env->DeleteLocalRef(objects);
        if (zombies != nullptr) env->DeleteLocalRef(zombies);
        if (cell != nullptr) env->DeleteLocalRef(cell);
        env->DeleteLocalRef(world);
        env->DeleteLocalRef(player);
        SetGate(GateStatus::WaitingForWorld, "游戏世界尚未完成加载");
        return g_frame;
    }

    const float player_x = env->CallFloatMethod(player, g_bindings.get_x);
    const float player_y = env->CallFloatMethod(player, g_bindings.get_y);
    const float player_z = env->CallFloatMethod(player, g_bindings.get_z);
    jobject local_safety = env->CallObjectMethod(
        player, g_bindings.player_get_safety);
    if (local_safety != nullptr && !ClearException(env)) {
        g_frame.local_pvp_enabled = env->CallBooleanMethod(
            local_safety, g_bindings.safety_is_enabled) != JNI_TRUE;
        ClearException(env);
    }
    if (local_safety != nullptr) env->DeleteLocalRef(local_safety);
    float player_forward_x = 0.0f;
    float player_forward_y = 0.0f;
    if (g_bindings.character_forward_x != nullptr &&
        g_bindings.character_forward_y != nullptr) {
        player_forward_x = env->CallFloatMethod(player, g_bindings.character_forward_x);
        player_forward_y = env->CallFloatMethod(player, g_bindings.character_forward_y);
    }
    jobject core = env->CallStaticObjectMethod(g_bindings.core, g_bindings.get_core);
    const float camera_zoom = core == nullptr
        ? 0.0f
        : env->CallFloatMethod(core, g_bindings.get_zoom, 0);
    const jint count = zombies == nullptr
        ? 0 : env->CallIntMethod(zombies, g_bindings.list_size);
    if (ClearException(env) || core == nullptr || camera_zoom <= 0.01f) {
        SetGate(GateStatus::Error, "读取游戏实体列表失败");
    } else {
        g_frame.camera_zoom = camera_zoom;
        jvalue local_projection[4]{};
        local_projection[0].f = player_x;
        local_projection[1].f = player_y;
        local_projection[2].f = player_z;
        local_projection[3].i = 0;
        g_frame.local_screen_x = env->CallStaticFloatMethodA(
            g_bindings.iso_utils, g_bindings.x_to_screen, local_projection) / camera_zoom;
        g_frame.local_screen_y = env->CallStaticFloatMethodA(
            g_bindings.iso_utils, g_bindings.y_to_screen, local_projection) / camera_zoom;
        g_frame.has_local_screen_position = !ClearException(env);
        const bool model_bridge_ready = g_model_outline_bridge.Initialize(
            env, g_bindings.class_loader, g_bindings.load_class);
        const bool model_chams_armed = model_bridge_ready &&
            g_model_outline_bridge.BeginFrame(
                env, model_visual.enabled || player_model_visual.enabled ||
                    animal_model_visual.enabled || vehicle_model_visual.enabled);
        if (model_visual.enabled || player_model_visual.enabled ||
            animal_model_visual.enabled || vehicle_model_visual.enabled) {
            g_frame.model_chams_status = model_chams_armed ? 2 : 1;
        }
        const jint limited_count = std::min(count, static_cast<jint>(2048));
        g_frame.zombies.reserve(static_cast<std::size_t>(limited_count));
        for (jint index = 0; collect_zombies && index < limited_count; ++index) {
            jobject zombie = env->CallObjectMethod(zombies, g_bindings.list_get, index);
            if (zombie == nullptr || ClearException(env)) {
                if (zombie != nullptr) env->DeleteLocalRef(zombie);
                continue;
            }

            const float x = env->CallFloatMethod(zombie, g_bindings.get_x);
            const float y = env->CallFloatMethod(zombie, g_bindings.get_y);
            const float z = env->CallFloatMethod(zombie, g_bindings.get_z);
            const float delta_x = x - player_x;
            const float delta_y = y - player_y;
            const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (!ClearException(env) && distance <= max_distance) {
                jvalue projection_args[4]{};
                projection_args[0].f = x;
                projection_args[1].f = y;
                projection_args[2].f = z;
                projection_args[3].i = 0;
                const float screen_x = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.x_to_screen, projection_args) / camera_zoom;
                const float screen_y = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.y_to_screen, projection_args) / camera_zoom;
                const jshort online_id = env->CallShortMethod(zombie, g_bindings.get_online_id);
                const jint game_id = env->CallIntMethod(zombie, g_bindings.get_id);
                const jint identity = env->CallStaticIntMethod(
                    g_bindings.system, g_bindings.identity_hash_code, zombie);
                const float health = env->CallFloatMethod(zombie, g_bindings.get_health);
                const bool alive = env->CallBooleanMethod(
                    zombie, g_bindings.is_alive) == JNI_TRUE;
                const bool crawling =
                    env->CallBooleanMethod(zombie, g_bindings.is_crawling) == JNI_TRUE;
                const bool prone = crawling ||
                    env->CallBooleanMethod(zombie, g_bindings.is_prone) == JNI_TRUE;
                const bool unstable_pose = prone ||
                    env->CallBooleanMethod(zombie, g_bindings.is_falling) == JNI_TRUE ||
                    env->CallBooleanMethod(zombie, g_bindings.is_knocked_down) == JNI_TRUE;
                const float forward_x = env->CallFloatMethod(zombie, g_bindings.get_forward_x);
                const float forward_y = env->CallFloatMethod(zombie, g_bindings.get_forward_y);
                projection_args[2].f = z + (prone ? 0.20f : 0.58f);
                const float screen_top_y = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.y_to_screen, projection_args) / camera_zoom;
                if (!ClearException(env)) {
                    ZombieSnapshot snapshot{};
                    snapshot.world_x = x;
                    snapshot.world_y = y;
                    snapshot.world_z = z;
                    snapshot.screen_x = screen_x;
                    snapshot.screen_y = screen_y;
                    snapshot.screen_top_y = screen_top_y;
                    snapshot.distance = distance;
                    snapshot.current_health = health;
                    snapshot.health_fraction = TrackZombieHealthFraction(identity, health);
                    snapshot.game_id = game_id;
                    snapshot.dead = !alive;
                    snapshot.identity = identity;
                    snapshot.online_id = online_id;
                    snapshot.prone = prone;
                    snapshot.crawling = crawling;
                    snapshot.unstable_pose = unstable_pose;
                    snapshot.forward_x = forward_x;
                    snapshot.forward_y = forward_y;
                    CollectCharacterVisualState(
                        env, cell, player_x, player_y, player_z,
                        player_forward_x, player_forward_y, x, y, z, snapshot);
                    if (collect_bones) {
                        CollectCharacterBones(
                            env, zombie, x, y, z, camera_zoom, snapshot);
                    }
                    if (model_chams_armed) {
                        if (model_visual.force_visible &&
                            g_bindings.set_alpha_and_target != nullptr) {
                            env->CallVoidMethod(
                                zombie, g_bindings.set_alpha_and_target, 1.0f);
                        }
                        const ZombieModelVisualColor* color = &model_visual.normal;
                        if (snapshot.behind_wall && model_visual.behind_wall_enabled) {
                            color = &model_visual.behind_wall;
                        } else if (snapshot.in_view && model_visual.in_view_enabled) {
                            color = &model_visual.in_view;
                        }
                        g_model_outline_bridge.ApplyToObject(
                            env, zombie, color->red, color->green,
                            color->blue, color->alpha);
                    }
                    g_frame.zombies.push_back(snapshot);
                }
            }
            env->DeleteLocalRef(zombie);
        }

        jobject iterator = (collect_players || collect_animals) && objects != nullptr
            ? env->CallObjectMethod(objects, g_bindings.set_iterator)
            : nullptr;
        g_frame.players.reserve(64);
        g_frame.animals.reserve(64);
        int inspected = 0;
        while (iterator != nullptr && inspected < 2048 &&
               env->CallBooleanMethod(iterator, g_bindings.iterator_has_next) == JNI_TRUE) {
            ++inspected;
            jobject object = env->CallObjectMethod(iterator, g_bindings.iterator_next);
            if (object == nullptr || ClearException(env)) {
                if (object != nullptr) env->DeleteLocalRef(object);
                continue;
            }
            const bool is_animal =
                env->IsInstanceOf(object, g_bindings.iso_animal) == JNI_TRUE;
            if (is_animal) {
                if (collect_animals) {
                    const float x = env->CallFloatMethod(object, g_bindings.get_x);
                    const float y = env->CallFloatMethod(object, g_bindings.get_y);
                    const float z = env->CallFloatMethod(object, g_bindings.get_z);
                    const float delta_x = x - player_x;
                    const float delta_y = y - player_y;
                    const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
                    if (!ClearException(env) && distance <= animal_max_distance) {
                        const float size = std::max(0.15f, env->CallFloatMethod(
                            object, g_bindings.animal_get_size));
                        const bool baby = env->CallBooleanMethod(
                            object, g_bindings.animal_is_baby) == JNI_TRUE;
                        const float health = env->CallFloatMethod(object, g_bindings.get_health);
                        const jint identity = env->CallStaticIntMethod(
                            g_bindings.system, g_bindings.identity_hash_code, object);
                        jvalue projection[4]{};
                        projection[0].f = x;
                        projection[1].f = y;
                        projection[2].f = z;
                        projection[3].i = 0;
                        const float screen_x = env->CallStaticFloatMethodA(
                            g_bindings.iso_utils, g_bindings.x_to_screen, projection) /
                            camera_zoom;
                        const float screen_y = env->CallStaticFloatMethodA(
                            g_bindings.iso_utils, g_bindings.y_to_screen, projection) /
                            camera_zoom;
                        projection[2].f = z + std::clamp(size * 0.52f, 0.28f, 0.82f);
                        const float screen_top_y = env->CallStaticFloatMethodA(
                            g_bindings.iso_utils, g_bindings.y_to_screen, projection) /
                            camera_zoom;
                        if (!ClearException(env)) {
                            AnimalSnapshot snapshot{};
                            snapshot.screen_x = screen_x;
                            snapshot.screen_y = screen_y;
                            snapshot.screen_top_y = screen_top_y;
                            snapshot.distance = distance;
                            snapshot.health_fraction =
                                TrackAnimalHealthFraction(identity, health);
                            snapshot.baby = baby;
                            snapshot.size = size;
                            snapshot.animal_type = ReadJavaString(
                                env, object, g_bindings.animal_get_type);
                            snapshot.name = ReadJavaString(
                                env, object, g_bindings.animal_get_custom_name);
                            if (snapshot.name.empty()) {
                                snapshot.name = ReadJavaString(
                                    env, object, g_bindings.animal_get_full_name);
                            }
                            if (snapshot.name.empty()) snapshot.name = snapshot.animal_type;
                            CollectCharacterVisualState(
                                env, cell, player_x, player_y, player_z,
                                player_forward_x, player_forward_y, x, y, z, snapshot);
                            if (collect_animal_bones) {
                                CollectCharacterBones(
                                    env, object, x, y, z, camera_zoom, snapshot);
                            }
                            if (model_chams_armed && animal_model_visual.enabled) {
                                if (animal_model_visual.force_visible &&
                                    g_bindings.set_alpha_and_target != nullptr) {
                                    env->CallVoidMethod(
                                        object, g_bindings.set_alpha_and_target, 1.0f);
                                }
                                const ZombieModelVisualColor* color =
                                    &animal_model_visual.normal;
                                if (snapshot.behind_wall &&
                                    animal_model_visual.behind_wall_enabled) {
                                    color = &animal_model_visual.behind_wall;
                                } else if (snapshot.in_view &&
                                           animal_model_visual.in_view_enabled) {
                                    color = &animal_model_visual.in_view;
                                }
                                g_model_outline_bridge.ApplyToObject(
                                    env, object, color->red, color->green,
                                    color->blue, color->alpha);
                            }
                            g_frame.animals.push_back(std::move(snapshot));
                        }
                    }
                }
                env->DeleteLocalRef(object);
                continue;
            }
            if (collect_players) {
                CollectPlayerObject(
                    env, object, player, cell,
                    player_x, player_y, player_z,
                    player_forward_x, player_forward_y, camera_zoom,
                    player_max_distance, collect_player_bones,
                    model_chams_armed, player_model_visual);
            }
            env->DeleteLocalRef(object);
        }
        if (iterator != nullptr) env->DeleteLocalRef(iterator);

        jobject player_map = collect_players && network_client
            ? env->GetStaticObjectField(
                g_bindings.game_client, g_bindings.client_player_map)
            : nullptr;
        jobject player_values = player_map == nullptr
            ? nullptr
            : env->CallObjectMethod(player_map, g_bindings.map_values);
        jobject online_player_iterator = player_values == nullptr
            ? nullptr
            : env->CallObjectMethod(
                player_values, g_bindings.collection_iterator);
        if (ClearException(env)) {
            if (online_player_iterator != nullptr) {
                env->DeleteLocalRef(online_player_iterator);
                online_player_iterator = nullptr;
            }
        }
        int online_players_inspected = 0;
        while (online_player_iterator != nullptr &&
               online_players_inspected < 512 &&
               env->CallBooleanMethod(
                   online_player_iterator,
                   g_bindings.iterator_has_next) == JNI_TRUE) {
            ++online_players_inspected;
            jobject online_player = env->CallObjectMethod(
                online_player_iterator, g_bindings.iterator_next);
            if (online_player == nullptr || ClearException(env)) {
                if (online_player != nullptr) env->DeleteLocalRef(online_player);
                continue;
            }
            CollectPlayerObject(
                env, online_player, player, cell,
                player_x, player_y, player_z,
                player_forward_x, player_forward_y, camera_zoom,
                player_max_distance, collect_player_bones,
                model_chams_armed, player_model_visual);
            env->DeleteLocalRef(online_player);
        }
        if (online_player_iterator != nullptr) {
            env->DeleteLocalRef(online_player_iterator);
        }
        if (player_values != nullptr) env->DeleteLocalRef(player_values);
        if (player_map != nullptr) env->DeleteLocalRef(player_map);

        jobject inventory = collect_vehicles
            ? env->CallObjectMethod(player, g_bindings.player_get_inventory)
            : nullptr;
        jobject vehicle_iterator = collect_vehicles && vehicles != nullptr
            ? env->CallObjectMethod(vehicles, g_bindings.set_iterator)
            : nullptr;
        g_frame.vehicles.reserve(64);
        inspected = 0;
        while (vehicle_iterator != nullptr && inspected < 1024 &&
               env->CallBooleanMethod(
                   vehicle_iterator, g_bindings.iterator_has_next) == JNI_TRUE) {
            ++inspected;
            jobject vehicle = env->CallObjectMethod(
                vehicle_iterator, g_bindings.iterator_next);
            if (vehicle == nullptr || ClearException(env)) {
                if (vehicle != nullptr) env->DeleteLocalRef(vehicle);
                continue;
            }
            const float x = env->CallFloatMethod(vehicle, g_bindings.get_x);
            const float y = env->CallFloatMethod(vehicle, g_bindings.get_y);
            const float z = env->CallFloatMethod(vehicle, g_bindings.get_z);
            const float delta_x = x - player_x;
            const float delta_y = y - player_y;
            const float distance = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (!ClearException(env) && distance <= vehicle_max_distance) {
                VehicleSnapshot snapshot{};
                snapshot.distance = distance;
                snapshot.vehicle_id = env->CallShortMethod(
                    vehicle, g_bindings.vehicle_get_id);
                snapshot.name = ReadJavaString(
                    env, vehicle, g_bindings.vehicle_get_script_name);
                const int engine_condition = env->CallIntMethod(
                    vehicle, g_bindings.vehicle_get_engine_condition);
                snapshot.engine_health_fraction = std::clamp(
                    static_cast<float>(engine_condition) / 100.0f, 0.0f, 1.0f);
                snapshot.engine_running = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_is_engine_running) == JNI_TRUE;
                // These two BaseVehicle methods call Lua internally. The ESP snapshot is
                // collected from the render thread, where B42 rejects Lua calls and can
                // flood the log until the game stops responding. Use the already-read,
                // thread-safe engine state for the display-only approximation instead.
                snapshot.engine_working = engine_condition > 0;
                snapshot.operational = engine_condition > 0;
                snapshot.all_doors_locked = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_all_doors_locked) == JNI_TRUE;
                snapshot.any_door_locked = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_any_door_locked) == JNI_TRUE;
                snapshot.hotwired = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_is_hotwired) == JNI_TRUE;
                snapshot.hotwire_broken = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_is_hotwire_broken) == JNI_TRUE;
                snapshot.keys_in_ignition = env->CallBooleanMethod(
                    vehicle, g_bindings.vehicle_keys_in_ignition) == JNI_TRUE;
                const jint key_id = env->CallIntMethod(
                    vehicle, g_bindings.vehicle_get_key_id);
                if (inventory != nullptr && key_id >= 0 && !ClearException(env)) {
                    jobject key = env->CallObjectMethod(
                        inventory, g_bindings.inventory_have_key_id, key_id);
                    snapshot.has_key = key != nullptr && !ClearException(env);
                    if (key != nullptr) env->DeleteLocalRef(key);
                }
                snapshot.can_drive_now = snapshot.operational &&
                    snapshot.engine_working && engine_condition > 0 &&
                    !snapshot.all_doors_locked &&
                    (snapshot.engine_running || snapshot.keys_in_ignition ||
                     snapshot.has_key ||
                     (snapshot.hotwired && !snapshot.hotwire_broken));
                jvalue projection[4]{};
                projection[0].f = x;
                projection[1].f = y;
                projection[2].f = z;
                projection[3].i = 0;
                snapshot.screen_x = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.x_to_screen, projection) /
                    camera_zoom;
                snapshot.screen_y = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.y_to_screen, projection) /
                    camera_zoom;
                projection[2].f = z + 0.72f;
                snapshot.screen_top_y = env->CallStaticFloatMethodA(
                    g_bindings.iso_utils, g_bindings.y_to_screen, projection) /
                    camera_zoom;
                CollectCharacterVisualState(
                    env, cell, player_x, player_y, player_z,
                    player_forward_x, player_forward_y, x, y, z, snapshot);
                if (!ClearException(env)) {
                    if (model_chams_armed && vehicle_model_visual.enabled) {
                        const ZombieModelVisualColor* color =
                            &vehicle_model_visual.normal;
                        if (snapshot.behind_wall &&
                            vehicle_model_visual.behind_wall_enabled) {
                            color = &vehicle_model_visual.behind_wall;
                        } else if (snapshot.in_view &&
                                   vehicle_model_visual.in_view_enabled) {
                            color = &vehicle_model_visual.in_view;
                        }
                        g_model_outline_bridge.ApplyToObject(
                            env, vehicle, color->red, color->green,
                            color->blue, color->alpha);
                    }
                    g_frame.vehicles.push_back(std::move(snapshot));
                }
            }
            env->DeleteLocalRef(vehicle);
        }
        if (vehicle_iterator != nullptr) env->DeleteLocalRef(vehicle_iterator);
        if (inventory != nullptr) env->DeleteLocalRef(inventory);
        ClearException(env);
    }

    if (vehicles != nullptr) env->DeleteLocalRef(vehicles);
    if (objects != nullptr) env->DeleteLocalRef(objects);
    if (zombies != nullptr) env->DeleteLocalRef(zombies);
    env->DeleteLocalRef(cell);
    if (core != nullptr) env->DeleteLocalRef(core);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(player);
    return g_frame;
}

const char* GateStatusLabel(GateStatus status) {
    switch (status) {
        case GateStatus::Initializing: return "初始化";
        case GateStatus::WaitingForWorld: return "等待游戏世界";
        case GateStatus::SinglePlayerAllowed: return "游戏中：已允许";
        case GateStatus::BlockedMultiplayer: return "联机：已阻止";
        case GateStatus::Error: return "接口错误：已禁用";
    }
    return "未知";
}

}  // namespace pztrainer::bridge
