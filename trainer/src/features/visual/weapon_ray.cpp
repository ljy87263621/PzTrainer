#include "features/visual/weapon_ray.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

#include "bridge/jni_game_bridge.hpp"

namespace pztrainer::features::visual {
namespace {

struct Bindings {
    bool ready = false;
    jclass iso_player = nullptr;
    jclass hand_weapon = nullptr;
    jclass ballistics_controller = nullptr;
    jclass vector3 = nullptr;
    jclass iso_utils = nullptr;
    jclass core = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_primary_item = nullptr;
    jmethodID is_aiming = nullptr;
    jmethodID is_ranged = nullptr;
    jmethodID get_max_range = nullptr;
    jmethodID get_ballistics_controller = nullptr;
    jmethodID get_muzzle_position = nullptr;
    jmethodID get_muzzle_direction = nullptr;
    jmethodID get_camera_targets = nullptr;
    jmethodID get_camera_target_count = nullptr;
    jmethodID get_core = nullptr;
    jmethodID get_zoom = nullptr;
    jmethodID x_to_screen = nullptr;
    jmethodID y_to_screen = nullptr;
    jfieldID vector3_x = nullptr;
    jfieldID vector3_y = nullptr;
    jfieldID vector3_z = nullptr;
};

Bindings g_bindings;
WeaponRaySettings g_settings;
WeaponRayStatus g_status;

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
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.hand_weapon = LoadGlobalClass(env, "zombie/inventory/types/HandWeapon");
    g_bindings.ballistics_controller = LoadGlobalClass(
        env, "zombie/core/physics/BallisticsController");
    g_bindings.vector3 = LoadGlobalClass(env, "zombie/iso/Vector3");
    g_bindings.iso_utils = LoadGlobalClass(env, "zombie/iso/IsoUtils");
    g_bindings.core = LoadGlobalClass(env, "zombie/core/Core");
    if (g_bindings.iso_player == nullptr || g_bindings.hand_weapon == nullptr ||
        g_bindings.ballistics_controller == nullptr || g_bindings.vector3 == nullptr ||
        g_bindings.iso_utils == nullptr || g_bindings.core == nullptr) {
        return false;
    }

    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_primary_item = env->GetMethodID(
        g_bindings.iso_player, "getPrimaryHandItem",
        "()Lzombie/inventory/InventoryItem;");
    g_bindings.is_aiming = env->GetMethodID(g_bindings.iso_player, "isAiming", "()Z");
    g_bindings.is_ranged = env->GetMethodID(g_bindings.hand_weapon, "isRanged", "()Z");
    g_bindings.get_max_range = env->GetMethodID(
        g_bindings.hand_weapon, "getMaxRange", "()F");
    g_bindings.get_ballistics_controller = env->GetMethodID(
        g_bindings.iso_player, "getBallisticsController",
        "()Lzombie/core/physics/BallisticsController;");
    g_bindings.get_muzzle_position = env->GetMethodID(
        g_bindings.ballistics_controller, "getMuzzlePosition", "()Lzombie/iso/Vector3;");
    g_bindings.get_muzzle_direction = env->GetMethodID(
        g_bindings.ballistics_controller, "getMuzzleDirection", "()Lzombie/iso/Vector3;");
    g_bindings.get_camera_targets = env->GetMethodID(
        g_bindings.ballistics_controller, "getCameraTargets", "()[F");
    g_bindings.get_camera_target_count = env->GetMethodID(
        g_bindings.ballistics_controller, "getNumberOfCameraTargets", "()I");
    g_bindings.get_core = env->GetStaticMethodID(
        g_bindings.core, "getInstance", "()Lzombie/core/Core;");
    g_bindings.get_zoom = env->GetMethodID(g_bindings.core, "getZoom", "(I)F");
    g_bindings.x_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "XToScreenExact", "(FFFI)F");
    g_bindings.y_to_screen = env->GetStaticMethodID(
        g_bindings.iso_utils, "YToScreenExact", "(FFFI)F");
    g_bindings.vector3_x = env->GetFieldID(g_bindings.vector3, "x", "F");
    g_bindings.vector3_y = env->GetFieldID(g_bindings.vector3, "y", "F");
    g_bindings.vector3_z = env->GetFieldID(g_bindings.vector3, "z", "F");

    g_bindings.ready = !ClearException(env) && g_bindings.get_player != nullptr &&
        g_bindings.get_primary_item != nullptr && g_bindings.is_aiming != nullptr &&
        g_bindings.is_ranged != nullptr &&
        g_bindings.get_max_range != nullptr &&
        g_bindings.get_ballistics_controller != nullptr &&
        g_bindings.get_muzzle_position != nullptr &&
        g_bindings.get_muzzle_direction != nullptr &&
        g_bindings.get_camera_targets != nullptr &&
        g_bindings.get_camera_target_count != nullptr && g_bindings.get_core != nullptr &&
        g_bindings.get_zoom != nullptr && g_bindings.x_to_screen != nullptr &&
        g_bindings.y_to_screen != nullptr && g_bindings.vector3_x != nullptr &&
        g_bindings.vector3_y != nullptr && g_bindings.vector3_z != nullptr;
    return g_bindings.ready;
}

void ResetStatus() {
    g_status.visible = false;
    g_status.hit = false;
    g_status.muzzle = ImVec2();
    g_status.endpoint = ImVec2();
}

bool Project(JNIEnv* env, float x, float y, float z, float zoom, ImVec2& point) {
    jvalue arguments[4]{};
    arguments[0].f = x;
    arguments[1].f = y;
    arguments[2].f = z;
    arguments[3].i = 0;
    point.x = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.x_to_screen, arguments) / zoom;
    point.y = env->CallStaticFloatMethodA(
        g_bindings.iso_utils, g_bindings.y_to_screen, arguments) / zoom;
    return !ClearException(env) && std::isfinite(point.x) && std::isfinite(point.y);
}

}  // namespace

WeaponRaySettings& GetWeaponRaySettings() {
    return g_settings;
}

const WeaponRayStatus& GetWeaponRayStatus() {
    return g_status;
}

void UpdateWeaponRay(bool world_ready) {
    ResetStatus();
    if (!g_settings.enabled || !world_ready) return;
    JNIEnv* env = bridge::GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        return;
    }
    g_status.initialized = true;

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    jobject weapon = player == nullptr
        ? nullptr : env->CallObjectMethod(player, g_bindings.get_primary_item);
    if (player == nullptr || weapon == nullptr || ClearException(env) ||
        env->IsInstanceOf(weapon, g_bindings.hand_weapon) != JNI_TRUE ||
        env->CallBooleanMethod(weapon, g_bindings.is_ranged) != JNI_TRUE ||
        env->CallBooleanMethod(player, g_bindings.is_aiming) != JNI_TRUE ||
        ClearException(env)) {
        if (weapon != nullptr) env->DeleteLocalRef(weapon);
        if (player != nullptr) env->DeleteLocalRef(player);
        return;
    }

    jobject controller = env->CallObjectMethod(
        player, g_bindings.get_ballistics_controller);
    jobject muzzle = controller == nullptr
        ? nullptr : env->CallObjectMethod(controller, g_bindings.get_muzzle_position);
    jobject direction = controller == nullptr
        ? nullptr : env->CallObjectMethod(controller, g_bindings.get_muzzle_direction);
    jobject core = env->CallStaticObjectMethod(g_bindings.core, g_bindings.get_core);
    if (controller == nullptr || muzzle == nullptr || direction == nullptr ||
        core == nullptr || ClearException(env)) {
        if (core != nullptr) env->DeleteLocalRef(core);
        if (direction != nullptr) env->DeleteLocalRef(direction);
        if (muzzle != nullptr) env->DeleteLocalRef(muzzle);
        if (controller != nullptr) env->DeleteLocalRef(controller);
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }

    const float start_x = env->GetFloatField(muzzle, g_bindings.vector3_x);
    const float start_y = env->GetFloatField(muzzle, g_bindings.vector3_y);
    const float start_z = env->GetFloatField(muzzle, g_bindings.vector3_z);
    float direction_x = env->GetFloatField(direction, g_bindings.vector3_x);
    float direction_y = env->GetFloatField(direction, g_bindings.vector3_y);
    float direction_z = env->GetFloatField(direction, g_bindings.vector3_z);
    const float maximum_range = std::max(
        1.0f, env->CallFloatMethod(weapon, g_bindings.get_max_range));
    const float zoom = env->CallFloatMethod(core, g_bindings.get_zoom, 0);
    if (ClearException(env) || zoom <= 0.01f) {
        env->DeleteLocalRef(core);
        env->DeleteLocalRef(direction);
        env->DeleteLocalRef(muzzle);
        env->DeleteLocalRef(controller);
        env->DeleteLocalRef(weapon);
        env->DeleteLocalRef(player);
        return;
    }

    jint target_count = env->CallIntMethod(
        controller, g_bindings.get_camera_target_count);
    jfloatArray targets = static_cast<jfloatArray>(env->CallObjectMethod(
        controller, g_bindings.get_camera_targets));
    float end_x = start_x;
    float end_y = start_y;
    float end_z = start_z;
    if (!ClearException(env) && targets != nullptr && target_count > 0) {
        const jsize target_values = env->GetArrayLength(targets);
        target_count = std::min({
            target_count, static_cast<jint>(10),
            static_cast<jint>(target_values / 5)});
        std::array<jfloat, 50> values{};
        env->GetFloatArrayRegion(targets, 0, target_count * 5, values.data());
        float nearest_distance_squared = std::numeric_limits<float>::max();
        const float maximum_distance_squared =
            (maximum_range + 1.0f) * (maximum_range + 1.0f);
        for (jint index = 0; index < target_count; ++index) {
            const int offset = index * 5;
            const float x = values[offset + 1];
            const float y = values[offset + 3];
            const float z = values[offset + 2] / 2.44949f;
            const float delta_x = x - start_x;
            const float delta_y = y - start_y;
            const float delta_z = z - start_z;
            const float distance_squared = delta_x * delta_x +
                delta_y * delta_y + delta_z * delta_z;
            if (std::isfinite(distance_squared) && distance_squared > 0.0001f &&
                distance_squared <= maximum_distance_squared &&
                distance_squared < nearest_distance_squared) {
                nearest_distance_squared = distance_squared;
                end_x = x;
                end_y = y;
                end_z = z;
            }
        }
        g_status.hit = nearest_distance_squared < std::numeric_limits<float>::max();
    }
    if (targets != nullptr) env->DeleteLocalRef(targets);

    if (!g_status.hit) {
        const float direction_length = std::sqrt(
            direction_x * direction_x + direction_y * direction_y +
            direction_z * direction_z);
        if (direction_length > 0.0001f) {
            direction_x /= direction_length;
            direction_y /= direction_length;
            direction_z /= direction_length;
            end_x += direction_x * maximum_range;
            end_y += direction_y * maximum_range;
            end_z += direction_z * maximum_range;
        }
    }

    g_status.visible = !ClearException(env) &&
        Project(env, start_x, start_y, start_z, zoom, g_status.muzzle) &&
        Project(env, end_x, end_y, end_z, zoom, g_status.endpoint);
    env->DeleteLocalRef(core);
    env->DeleteLocalRef(direction);
    env->DeleteLocalRef(muzzle);
    env->DeleteLocalRef(controller);
    env->DeleteLocalRef(weapon);
    env->DeleteLocalRef(player);
}

void DrawWeaponRay() {
    if (!g_settings.enabled || !g_status.visible) return;
    ImGui::GetBackgroundDrawList()->AddLine(
        g_status.muzzle, g_status.endpoint,
        ImGui::GetColorU32(g_settings.color), 1.0f);
}

}  // namespace pztrainer::features::visual
