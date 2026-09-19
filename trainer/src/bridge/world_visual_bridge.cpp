#include "bridge/world_visual_bridge.hpp"

#include <jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/world_visibility_render_bridge.hpp"

namespace pztrainer::bridge {
namespace {

constexpr std::size_t kDaylightFloatCount = 5;
constexpr float kLocalLightColor = 0.5f;
constexpr int kLocalLightRadius = 18;

struct Bindings {
    bool ready = false;
    jclass climate_manager = nullptr;
    jclass climate_float = nullptr;
    jclass climate_bool = nullptr;
    jclass climate_color = nullptr;
    jclass climate_color_info = nullptr;
    jclass iso_player = nullptr;
    jclass iso_world = nullptr;
    jclass iso_cell = nullptr;
    jclass iso_light_source = nullptr;
    jclass lighting_jni = nullptr;
    std::array<jfieldID, kDaylightFloatCount> daylight_float_ids{};
    jfieldID global_light_color_id = nullptr;
    jfieldID fog_intensity_id = nullptr;
    jfieldID precipitation_intensity_id = nullptr;
    jfieldID precipitation_is_snow_id = nullptr;
    jfieldID float_is_override_value = nullptr;
    jfieldID float_override_internal = nullptr;
    jfieldID iso_world_instance = nullptr;
    jmethodID get_climate_manager = nullptr;
    jmethodID get_climate_float = nullptr;
    jmethodID get_climate_color = nullptr;
    jmethodID get_climate_bool = nullptr;
    jmethodID float_get_override = nullptr;
    jmethodID float_get_override_interpolate = nullptr;
    jmethodID float_set_override = nullptr;
    jmethodID float_set_enable_override = nullptr;
    jmethodID float_is_enable_override = nullptr;
    jmethodID bool_get_override = nullptr;
    jmethodID bool_set_override = nullptr;
    jmethodID bool_set_enable_override = nullptr;
    jmethodID bool_is_enable_override = nullptr;
    jmethodID color_get_override = nullptr;
    jmethodID color_get_override_interpolate = nullptr;
    jmethodID color_set_override = nullptr;
    jmethodID color_set_enable_override = nullptr;
    jmethodID color_is_enable_override = nullptr;
    jmethodID color_info_constructor = nullptr;
    jmethodID color_info_set_to = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_player_x = nullptr;
    jmethodID get_player_y = nullptr;
    jmethodID get_player_z = nullptr;
    jmethodID get_world_cell = nullptr;
    jmethodID light_source_constructor = nullptr;
    jmethodID add_lamppost = nullptr;
    jmethodID remove_lamppost = nullptr;
    jfieldID vision_cone_lerp = nullptr;
};

struct ClimateFloatSnapshot {
    jobject value = nullptr;
    bool enabled = false;
    bool override_value_enabled = false;
    float override_value = 0.0f;
    float override_internal = 0.0f;
    float interpolate = 0.0f;
};

Bindings g_bindings;
WorldVisualStatus g_status;
bool g_fake_daylight_requested = false;
bool g_dark_area_requested = false;
bool g_force_360_vision_requested = false;
bool g_reveal_unexplored_requested = false;
WeatherVisualMode g_fog_mode = WeatherVisualMode::FollowGame;
PrecipitationVisualMode g_precipitation_mode =
    PrecipitationVisualMode::FollowGame;
float g_fog_intensity = 0.75f;
float g_precipitation_intensity = 0.75f;
bool g_daylight_applied = false;
bool g_vision_cone_applied = false;
float g_previous_vision_cone = 0.0f;
ClimateFloatSnapshot g_fog_snapshot{};
ClimateFloatSnapshot g_precipitation_snapshot{};
jobject g_snow_variable = nullptr;
bool g_snow_override_enabled = false;
bool g_snow_override_value = false;
std::array<ClimateFloatSnapshot, kDaylightFloatCount> g_float_snapshots{};
jobject g_color_variable = nullptr;
jobject g_color_snapshot = nullptr;
bool g_color_enabled = false;
float g_color_interpolate = 0.0f;
jobject g_local_light = nullptr;
jobject g_local_light_cell = nullptr;
int g_local_light_x = 0;
int g_local_light_y = 0;
int g_local_light_z = 0;

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
    g_bindings.climate_manager = LoadGlobalClass(
        env, "zombie/iso/weather/ClimateManager");
    g_bindings.climate_float = LoadGlobalClass(
        env, "zombie/iso/weather/ClimateManager$ClimateFloat");
    g_bindings.climate_bool = LoadGlobalClass(
        env, "zombie/iso/weather/ClimateManager$ClimateBool");
    g_bindings.climate_color = LoadGlobalClass(
        env, "zombie/iso/weather/ClimateManager$ClimateColor");
    g_bindings.climate_color_info = LoadGlobalClass(
        env, "zombie/iso/weather/ClimateColorInfo");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.iso_world = LoadGlobalClass(env, "zombie/iso/IsoWorld");
    g_bindings.iso_cell = LoadGlobalClass(env, "zombie/iso/IsoCell");
    g_bindings.iso_light_source = LoadGlobalClass(env, "zombie/iso/IsoLightSource");
    g_bindings.lighting_jni = LoadGlobalClass(env, "zombie/iso/LightingJNI");
    if (g_bindings.climate_manager == nullptr || g_bindings.climate_float == nullptr ||
        g_bindings.climate_bool == nullptr ||
        g_bindings.climate_color == nullptr ||
        g_bindings.climate_color_info == nullptr || g_bindings.iso_player == nullptr ||
        g_bindings.iso_world == nullptr || g_bindings.iso_cell == nullptr ||
        g_bindings.iso_light_source == nullptr ||
        g_bindings.lighting_jni == nullptr) {
        return false;
    }

    constexpr std::array<const char*, kDaylightFloatCount> float_names{
        "FLOAT_DESATURATION",
        "FLOAT_GLOBAL_LIGHT_INTENSITY",
        "FLOAT_NIGHT_STRENGTH",
        "FLOAT_AMBIENT",
        "FLOAT_DAYLIGHT_STRENGTH",
    };
    for (std::size_t index = 0; index < float_names.size(); ++index) {
        g_bindings.daylight_float_ids[index] = env->GetStaticFieldID(
            g_bindings.climate_manager, float_names[index], "I");
    }
    g_bindings.global_light_color_id = env->GetStaticFieldID(
        g_bindings.climate_manager, "COLOR_GLOBAL_LIGHT", "I");
    g_bindings.fog_intensity_id = env->GetStaticFieldID(
        g_bindings.climate_manager, "FLOAT_FOG_INTENSITY", "I");
    g_bindings.precipitation_intensity_id = env->GetStaticFieldID(
        g_bindings.climate_manager, "FLOAT_PRECIPITATION_INTENSITY", "I");
    g_bindings.precipitation_is_snow_id = env->GetStaticFieldID(
        g_bindings.climate_manager, "BOOL_IS_SNOW", "I");
    g_bindings.float_is_override_value = env->GetFieldID(
        g_bindings.climate_float, "isOverrideValue", "Z");
    g_bindings.float_override_internal = env->GetFieldID(
        g_bindings.climate_float, "overrideInternal", "F");
    g_bindings.iso_world_instance = env->GetStaticFieldID(
        g_bindings.iso_world, "instance", "Lzombie/iso/IsoWorld;");
    g_bindings.get_climate_manager = env->GetStaticMethodID(
        g_bindings.climate_manager, "getInstance",
        "()Lzombie/iso/weather/ClimateManager;");
    g_bindings.get_climate_float = env->GetMethodID(
        g_bindings.climate_manager, "getClimateFloat",
        "(I)Lzombie/iso/weather/ClimateManager$ClimateFloat;");
    g_bindings.get_climate_color = env->GetMethodID(
        g_bindings.climate_manager, "getClimateColor",
        "(I)Lzombie/iso/weather/ClimateManager$ClimateColor;");
    g_bindings.get_climate_bool = env->GetMethodID(
        g_bindings.climate_manager, "getClimateBool",
        "(I)Lzombie/iso/weather/ClimateManager$ClimateBool;");
    g_bindings.float_get_override = env->GetMethodID(
        g_bindings.climate_float, "getOverride", "()F");
    g_bindings.float_get_override_interpolate = env->GetMethodID(
        g_bindings.climate_float, "getOverrideInterpolate", "()F");
    g_bindings.float_set_override = env->GetMethodID(
        g_bindings.climate_float, "setOverride", "(FF)V");
    g_bindings.float_set_enable_override = env->GetMethodID(
        g_bindings.climate_float, "setEnableOverride", "(Z)V");
    g_bindings.float_is_enable_override = env->GetMethodID(
        g_bindings.climate_float, "isEnableOverride", "()Z");
    g_bindings.bool_get_override = env->GetMethodID(
        g_bindings.climate_bool, "getOverride", "()Z");
    g_bindings.bool_set_override = env->GetMethodID(
        g_bindings.climate_bool, "setOverride", "(Z)V");
    g_bindings.bool_set_enable_override = env->GetMethodID(
        g_bindings.climate_bool, "setEnableOverride", "(Z)V");
    g_bindings.bool_is_enable_override = env->GetMethodID(
        g_bindings.climate_bool, "isEnableOverride", "()Z");
    g_bindings.color_get_override = env->GetMethodID(
        g_bindings.climate_color, "getOverride",
        "()Lzombie/iso/weather/ClimateColorInfo;");
    g_bindings.color_get_override_interpolate = env->GetMethodID(
        g_bindings.climate_color, "getOverrideInterpolate", "()F");
    g_bindings.color_set_override = env->GetMethodID(
        g_bindings.climate_color, "setOverride",
        "(Lzombie/iso/weather/ClimateColorInfo;F)V");
    g_bindings.color_set_enable_override = env->GetMethodID(
        g_bindings.climate_color, "setEnableOverride", "(Z)V");
    g_bindings.color_is_enable_override = env->GetMethodID(
        g_bindings.climate_color, "isEnableOverride", "()Z");
    g_bindings.color_info_constructor = env->GetMethodID(
        g_bindings.climate_color_info, "<init>", "(FFFFFFFF)V");
    g_bindings.color_info_set_to = env->GetMethodID(
        g_bindings.climate_color_info, "setTo",
        "(Lzombie/iso/weather/ClimateColorInfo;)V");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_player_x = env->GetMethodID(g_bindings.iso_player, "getX", "()F");
    g_bindings.get_player_y = env->GetMethodID(g_bindings.iso_player, "getY", "()F");
    g_bindings.get_player_z = env->GetMethodID(g_bindings.iso_player, "getZ", "()F");
    g_bindings.get_world_cell = env->GetMethodID(
        g_bindings.iso_world, "getCell", "()Lzombie/iso/IsoCell;");
    g_bindings.light_source_constructor = env->GetMethodID(
        g_bindings.iso_light_source, "<init>", "(IIIFFFI)V");
    g_bindings.add_lamppost = env->GetMethodID(
        g_bindings.iso_cell, "addLamppost", "(Lzombie/iso/IsoLightSource;)V");
    g_bindings.remove_lamppost = env->GetMethodID(
        g_bindings.iso_cell, "removeLamppost", "(Lzombie/iso/IsoLightSource;)V");
    g_bindings.vision_cone_lerp = env->GetStaticFieldID(
        g_bindings.lighting_jni, "visionConeLerp", "F");

    bool fields_ready = g_bindings.global_light_color_id != nullptr &&
        g_bindings.fog_intensity_id != nullptr &&
        g_bindings.precipitation_intensity_id != nullptr &&
        g_bindings.precipitation_is_snow_id != nullptr;
    for (jfieldID field : g_bindings.daylight_float_ids) {
        fields_ready = fields_ready && field != nullptr;
    }
    g_bindings.ready = !ClearException(env) && fields_ready &&
        g_bindings.float_is_override_value != nullptr &&
        g_bindings.float_override_internal != nullptr &&
        g_bindings.iso_world_instance != nullptr &&
        g_bindings.get_climate_manager != nullptr &&
        g_bindings.get_climate_float != nullptr &&
        g_bindings.get_climate_color != nullptr &&
        g_bindings.get_climate_bool != nullptr &&
        g_bindings.float_get_override != nullptr &&
        g_bindings.float_get_override_interpolate != nullptr &&
        g_bindings.float_set_override != nullptr &&
        g_bindings.float_set_enable_override != nullptr &&
        g_bindings.float_is_enable_override != nullptr &&
        g_bindings.bool_get_override != nullptr &&
        g_bindings.bool_set_override != nullptr &&
        g_bindings.bool_set_enable_override != nullptr &&
        g_bindings.bool_is_enable_override != nullptr &&
        g_bindings.color_get_override != nullptr &&
        g_bindings.color_get_override_interpolate != nullptr &&
        g_bindings.color_set_override != nullptr &&
        g_bindings.color_set_enable_override != nullptr &&
        g_bindings.color_is_enable_override != nullptr &&
        g_bindings.color_info_constructor != nullptr &&
        g_bindings.color_info_set_to != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.get_player_x != nullptr &&
        g_bindings.get_player_y != nullptr && g_bindings.get_player_z != nullptr &&
        g_bindings.get_world_cell != nullptr &&
        g_bindings.light_source_constructor != nullptr &&
        g_bindings.add_lamppost != nullptr && g_bindings.remove_lamppost != nullptr &&
        g_bindings.vision_cone_lerp != nullptr;
    return g_bindings.ready;
}

void ReleaseDaylightRefs(JNIEnv* env) {
    for (ClimateFloatSnapshot& snapshot : g_float_snapshots) {
        if (snapshot.value != nullptr) env->DeleteGlobalRef(snapshot.value);
        snapshot = ClimateFloatSnapshot{};
    }
    if (g_color_variable != nullptr) env->DeleteGlobalRef(g_color_variable);
    if (g_color_snapshot != nullptr) env->DeleteGlobalRef(g_color_snapshot);
    g_color_variable = nullptr;
    g_color_snapshot = nullptr;
}

void RestoreCapturedDaylight(JNIEnv* env) {
    for (ClimateFloatSnapshot& snapshot : g_float_snapshots) {
        if (snapshot.value == nullptr) continue;
        env->CallVoidMethod(
            snapshot.value, g_bindings.float_set_override,
            snapshot.override_value, snapshot.interpolate);
        env->SetBooleanField(
            snapshot.value, g_bindings.float_is_override_value,
            snapshot.override_value_enabled ? JNI_TRUE : JNI_FALSE);
        env->SetFloatField(
            snapshot.value, g_bindings.float_override_internal,
            snapshot.override_internal);
        env->CallVoidMethod(
            snapshot.value, g_bindings.float_set_enable_override,
            snapshot.enabled ? JNI_TRUE : JNI_FALSE);
        ClearException(env);
    }
    if (g_color_variable != nullptr && g_color_snapshot != nullptr) {
        env->CallVoidMethod(
            g_color_variable, g_bindings.color_set_override,
            g_color_snapshot, g_color_interpolate);
        env->CallVoidMethod(
            g_color_variable, g_bindings.color_set_enable_override,
            g_color_enabled ? JNI_TRUE : JNI_FALSE);
        ClearException(env);
    }
    ReleaseDaylightRefs(env);
    g_daylight_applied = false;
}

void RestoreDaylight(JNIEnv* env) {
    if (g_daylight_applied) RestoreCapturedDaylight(env);
}

void RestoreVisionCone(JNIEnv* env) {
    if (!g_vision_cone_applied) return;
    env->SetStaticFloatField(
        g_bindings.lighting_jni, g_bindings.vision_cone_lerp,
        g_previous_vision_cone);
    ClearException(env);
    g_vision_cone_applied = false;
}

bool ApplyForce360Vision(JNIEnv* env) {
    if (!g_force_360_vision_requested) {
        RestoreVisionCone(env);
        return true;
    }
    if (!g_vision_cone_applied) {
        g_previous_vision_cone = env->GetStaticFloatField(
            g_bindings.lighting_jni, g_bindings.vision_cone_lerp);
        if (ClearException(env)) return false;
        g_vision_cone_applied = true;
    }
    // The game lerps this value toward the normal cone before submitting it to
    // native lighting. 372 degrees keeps the submitted cone at or above 360.
    env->SetStaticFloatField(
        g_bindings.lighting_jni, g_bindings.vision_cone_lerp, 372.0f);
    return !ClearException(env);
}

void ReleaseWeatherSnapshot(JNIEnv* env, ClimateFloatSnapshot& snapshot) {
    if (snapshot.value != nullptr) env->DeleteGlobalRef(snapshot.value);
    snapshot = ClimateFloatSnapshot{};
}

void RestoreWeatherOverride(JNIEnv* env, ClimateFloatSnapshot& snapshot) {
    if (snapshot.value == nullptr) return;
    env->CallVoidMethod(
        snapshot.value, g_bindings.float_set_override,
        snapshot.override_value, snapshot.interpolate);
    env->SetBooleanField(
        snapshot.value, g_bindings.float_is_override_value,
        snapshot.override_value_enabled ? JNI_TRUE : JNI_FALSE);
    env->SetFloatField(
        snapshot.value, g_bindings.float_override_internal,
        snapshot.override_internal);
    env->CallVoidMethod(
        snapshot.value, g_bindings.float_set_enable_override,
        snapshot.enabled ? JNI_TRUE : JNI_FALSE);
    ClearException(env);
    ReleaseWeatherSnapshot(env, snapshot);
}

bool ApplyWeatherOverride(JNIEnv* env, jobject manager, jfieldID climate_id_field,
                          WeatherVisualMode mode,
                          float forced_intensity,
                          ClimateFloatSnapshot& snapshot) {
    if (mode == WeatherVisualMode::FollowGame) {
        RestoreWeatherOverride(env, snapshot);
        return true;
    }
    if (snapshot.value == nullptr) {
        const jint climate_id = env->GetStaticIntField(
            g_bindings.climate_manager, climate_id_field);
        jobject value = env->CallObjectMethod(
            manager, g_bindings.get_climate_float, climate_id);
        if (value == nullptr || ClearException(env)) {
            if (value != nullptr) env->DeleteLocalRef(value);
            return false;
        }
        snapshot.value = env->NewGlobalRef(value);
        snapshot.enabled = env->CallBooleanMethod(
            value, g_bindings.float_is_enable_override) == JNI_TRUE;
        snapshot.override_value_enabled = env->GetBooleanField(
            value, g_bindings.float_is_override_value) == JNI_TRUE;
        snapshot.override_value = env->CallFloatMethod(
            value, g_bindings.float_get_override);
        snapshot.override_internal = env->GetFloatField(
            value, g_bindings.float_override_internal);
        snapshot.interpolate = env->CallFloatMethod(
            value, g_bindings.float_get_override_interpolate);
        env->DeleteLocalRef(value);
        if (snapshot.value == nullptr || ClearException(env)) {
            ReleaseWeatherSnapshot(env, snapshot);
            return false;
        }
    }
    const float target = mode == WeatherVisualMode::ForceOn
        ? std::clamp(forced_intensity, 0.0f, 1.0f) : 0.0f;
    env->CallVoidMethod(
        snapshot.value, g_bindings.float_set_override, target, 1.0f);
    env->SetBooleanField(
        snapshot.value, g_bindings.float_is_override_value, JNI_FALSE);
    return !ClearException(env);
}

void RestoreSnowOverride(JNIEnv* env) {
    if (g_snow_variable == nullptr) return;
    env->CallVoidMethod(
        g_snow_variable, g_bindings.bool_set_override,
        g_snow_override_value ? JNI_TRUE : JNI_FALSE);
    env->CallVoidMethod(
        g_snow_variable, g_bindings.bool_set_enable_override,
        g_snow_override_enabled ? JNI_TRUE : JNI_FALSE);
    ClearException(env);
    env->DeleteGlobalRef(g_snow_variable);
    g_snow_variable = nullptr;
}

bool ApplyPrecipitationOverride(JNIEnv* env, jobject manager) {
    if (g_precipitation_mode == PrecipitationVisualMode::FollowGame) {
        RestoreWeatherOverride(env, g_precipitation_snapshot);
        RestoreSnowOverride(env);
        return true;
    }
    const WeatherVisualMode intensity_mode =
        g_precipitation_mode == PrecipitationVisualMode::ForceOff
            ? WeatherVisualMode::ForceOff : WeatherVisualMode::ForceOn;
    if (!ApplyWeatherOverride(
            env, manager, g_bindings.precipitation_intensity_id,
            intensity_mode, g_precipitation_intensity,
            g_precipitation_snapshot)) {
        return false;
    }
    if (g_precipitation_mode == PrecipitationVisualMode::ForceOff) {
        RestoreSnowOverride(env);
        return true;
    }
    if (g_snow_variable == nullptr) {
        const jint climate_id = env->GetStaticIntField(
            g_bindings.climate_manager, g_bindings.precipitation_is_snow_id);
        jobject value = env->CallObjectMethod(
            manager, g_bindings.get_climate_bool, climate_id);
        if (value == nullptr || ClearException(env)) {
            if (value != nullptr) env->DeleteLocalRef(value);
            return false;
        }
        g_snow_variable = env->NewGlobalRef(value);
        g_snow_override_enabled = env->CallBooleanMethod(
            value, g_bindings.bool_is_enable_override) == JNI_TRUE;
        g_snow_override_value = env->CallBooleanMethod(
            value, g_bindings.bool_get_override) == JNI_TRUE;
        env->DeleteLocalRef(value);
        if (g_snow_variable == nullptr || ClearException(env)) {
            RestoreSnowOverride(env);
            return false;
        }
    }
    env->CallVoidMethod(
        g_snow_variable, g_bindings.bool_set_override,
        g_precipitation_mode == PrecipitationVisualMode::Snow
            ? JNI_TRUE : JNI_FALSE);
    return !ClearException(env);
}

bool SnapshotDaylight(JNIEnv* env, jobject manager) {
    for (std::size_t index = 0; index < g_float_snapshots.size(); ++index) {
        const jint climate_id = env->GetStaticIntField(
            g_bindings.climate_manager, g_bindings.daylight_float_ids[index]);
        jobject value = env->CallObjectMethod(
            manager, g_bindings.get_climate_float, climate_id);
        if (value == nullptr || ClearException(env)) {
            if (value != nullptr) env->DeleteLocalRef(value);
            ReleaseDaylightRefs(env);
            return false;
        }
        ClimateFloatSnapshot& snapshot = g_float_snapshots[index];
        snapshot.value = env->NewGlobalRef(value);
        snapshot.enabled = env->CallBooleanMethod(
            value, g_bindings.float_is_enable_override) == JNI_TRUE;
        snapshot.override_value_enabled = env->GetBooleanField(
            value, g_bindings.float_is_override_value) == JNI_TRUE;
        snapshot.override_value = env->CallFloatMethod(
            value, g_bindings.float_get_override);
        snapshot.override_internal = env->GetFloatField(
            value, g_bindings.float_override_internal);
        snapshot.interpolate = env->CallFloatMethod(
            value, g_bindings.float_get_override_interpolate);
        env->DeleteLocalRef(value);
        if (snapshot.value == nullptr || ClearException(env)) {
            ReleaseDaylightRefs(env);
            return false;
        }
    }

    const jint color_id = env->GetStaticIntField(
        g_bindings.climate_manager, g_bindings.global_light_color_id);
    jobject color = env->CallObjectMethod(manager, g_bindings.get_climate_color, color_id);
    jobject previous = color == nullptr ? nullptr : env->CallObjectMethod(
        color, g_bindings.color_get_override);
    jobject snapshot = previous == nullptr ? nullptr : env->NewObject(
        g_bindings.climate_color_info, g_bindings.color_info_constructor,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    if (snapshot != nullptr) {
        env->CallVoidMethod(snapshot, g_bindings.color_info_set_to, previous);
    }
    if (color == nullptr || previous == nullptr || snapshot == nullptr ||
        ClearException(env)) {
        if (color != nullptr) env->DeleteLocalRef(color);
        if (previous != nullptr) env->DeleteLocalRef(previous);
        if (snapshot != nullptr) env->DeleteLocalRef(snapshot);
        ReleaseDaylightRefs(env);
        return false;
    }

    g_color_variable = env->NewGlobalRef(color);
    g_color_snapshot = env->NewGlobalRef(snapshot);
    g_color_enabled = env->CallBooleanMethod(
        color, g_bindings.color_is_enable_override) == JNI_TRUE;
    g_color_interpolate = env->CallFloatMethod(
        color, g_bindings.color_get_override_interpolate);
    env->DeleteLocalRef(snapshot);
    env->DeleteLocalRef(previous);
    env->DeleteLocalRef(color);
    if (g_color_variable == nullptr || g_color_snapshot == nullptr || ClearException(env)) {
        ReleaseDaylightRefs(env);
        return false;
    }
    g_daylight_applied = true;
    return true;
}

bool ApplyDaylight(JNIEnv* env) {
    constexpr std::array<float, kDaylightFloatCount> target_values{
        0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    };
    for (std::size_t index = 0; index < g_float_snapshots.size(); ++index) {
        jobject value = g_float_snapshots[index].value;
        if (value == nullptr) return false;
        env->CallVoidMethod(
            value, g_bindings.float_set_override, target_values[index], 1.0f);
        env->SetBooleanField(
            value, g_bindings.float_is_override_value, JNI_FALSE);
    }
    jobject white = env->NewObject(
        g_bindings.climate_color_info, g_bindings.color_info_constructor,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    if (white == nullptr || g_color_variable == nullptr) {
        if (white != nullptr) env->DeleteLocalRef(white);
        ClearException(env);
        return false;
    }
    env->CallVoidMethod(
        g_color_variable, g_bindings.color_set_override, white, 1.0f);
    env->DeleteLocalRef(white);
    return !ClearException(env);
}

void RemoveLocalLight(JNIEnv* env) {
    if (g_local_light != nullptr && g_local_light_cell != nullptr) {
        env->CallVoidMethod(
            g_local_light_cell, g_bindings.remove_lamppost, g_local_light);
        ClearException(env);
    }
    if (g_local_light != nullptr) env->DeleteGlobalRef(g_local_light);
    if (g_local_light_cell != nullptr) env->DeleteGlobalRef(g_local_light_cell);
    g_local_light = nullptr;
    g_local_light_cell = nullptr;
}

bool UpdateLocalLight(JNIEnv* env, jobject player) {
    if (!g_dark_area_requested || player == nullptr) {
        RemoveLocalLight(env);
        return true;
    }

    jobject world = env->GetStaticObjectField(
        g_bindings.iso_world, g_bindings.iso_world_instance);
    jobject cell = world == nullptr ? nullptr : env->CallObjectMethod(
        world, g_bindings.get_world_cell);
    const int x = static_cast<int>(std::floor(
        env->CallFloatMethod(player, g_bindings.get_player_x)));
    const int y = static_cast<int>(std::floor(
        env->CallFloatMethod(player, g_bindings.get_player_y)));
    const int z = static_cast<int>(std::floor(
        env->CallFloatMethod(player, g_bindings.get_player_z)));
    if (world != nullptr) env->DeleteLocalRef(world);
    if (cell == nullptr || ClearException(env)) {
        if (cell != nullptr) env->DeleteLocalRef(cell);
        return false;
    }

    const bool same_cell = g_local_light_cell != nullptr &&
        env->IsSameObject(g_local_light_cell, cell) == JNI_TRUE;
    if (g_local_light != nullptr && same_cell &&
        x == g_local_light_x && y == g_local_light_y && z == g_local_light_z) {
        env->DeleteLocalRef(cell);
        return true;
    }

    RemoveLocalLight(env);
    jobject light = env->NewObject(
        g_bindings.iso_light_source, g_bindings.light_source_constructor,
        x, y, z, kLocalLightColor, kLocalLightColor, kLocalLightColor,
        kLocalLightRadius);
    if (light == nullptr || ClearException(env)) {
        if (light != nullptr) env->DeleteLocalRef(light);
        env->DeleteLocalRef(cell);
        return false;
    }
    env->CallVoidMethod(cell, g_bindings.add_lamppost, light);
    if (ClearException(env)) {
        env->DeleteLocalRef(light);
        env->DeleteLocalRef(cell);
        return false;
    }
    g_local_light = env->NewGlobalRef(light);
    g_local_light_cell = env->NewGlobalRef(cell);
    g_local_light_x = x;
    g_local_light_y = y;
    g_local_light_z = z;
    env->DeleteLocalRef(light);
    env->DeleteLocalRef(cell);
    return g_local_light != nullptr && g_local_light_cell != nullptr;
}

}  // namespace

void UpdateWorldVisualBridge(bool world_ready) {
    g_status.fake_daylight_enabled = g_fake_daylight_requested;
    g_status.dark_area_brightness_enabled = g_dark_area_requested;
    g_status.force_360_vision_enabled = g_force_360_vision_requested;
    g_status.reveal_unexplored_enabled = g_reveal_unexplored_requested;
    g_status.fog_mode = g_fog_mode;
    g_status.precipitation_mode = g_precipitation_mode;
    g_status.fog_intensity = g_fog_intensity;
    g_status.precipitation_intensity = g_precipitation_intensity;
    g_status.world_ready = world_ready;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (!world_ready) {
        SetUnexploredMaskSuppressed(false);
        if (env != nullptr && g_bindings.ready) {
            RestoreDaylight(env);
            RestoreVisionCone(env);
            RestoreWeatherOverride(env, g_fog_snapshot);
            RestoreWeatherOverride(env, g_precipitation_snapshot);
            RestoreSnowOverride(env);
            RemoveLocalLight(env);
        }
        g_status.message = "等待进入存档";
        return;
    }
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.message = "世界视觉桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    SetUnexploredMaskSuppressed(g_reveal_unexplored_requested);

    jobject manager = env->CallStaticObjectMethod(
        g_bindings.climate_manager, g_bindings.get_climate_manager);
    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (manager == nullptr || player == nullptr || ClearException(env)) {
        g_status.message = "等待气候与玩家渲染对象";
        if (manager != nullptr) env->DeleteLocalRef(manager);
        if (player != nullptr) env->DeleteLocalRef(player);
        return;
    }

    bool daylight_ready = true;
    if (g_fake_daylight_requested) {
        daylight_ready = g_daylight_applied || SnapshotDaylight(env, manager);
        if (daylight_ready) daylight_ready = ApplyDaylight(env);
        if (!daylight_ready) RestoreDaylight(env);
    } else {
        RestoreDaylight(env);
    }
    const bool local_light_ready = UpdateLocalLight(env, player);
    const bool vision_ready = ApplyForce360Vision(env);
    const bool fog_ready = ApplyWeatherOverride(
        env, manager, g_bindings.fog_intensity_id, g_fog_mode,
        g_fog_intensity, g_fog_snapshot);
    const bool precipitation_ready = ApplyPrecipitationOverride(env, manager);

    if (!daylight_ready) {
        g_status.message = "保存或覆盖当前昼光状态失败";
    } else if (!local_light_ready) {
        g_status.message = "创建本地白色光源失败";
    } else if (!vision_ready) {
        g_status.message = "覆盖本地视野角度失败";
    } else if (!fog_ready || !precipitation_ready) {
        g_status.message = "覆盖本地天气渲染失败";
    } else if (g_fake_daylight_requested || g_dark_area_requested ||
               g_force_360_vision_requested || g_reveal_unexplored_requested ||
               g_fog_mode != WeatherVisualMode::FollowGame ||
               g_precipitation_mode != PrecipitationVisualMode::FollowGame) {
        g_status.message = "客户端世界视觉覆盖已启用";
    } else {
        g_status.message = "使用游戏原始世界光照";
    }

    env->DeleteLocalRef(player);
    env->DeleteLocalRef(manager);
}

const WorldVisualStatus& GetWorldVisualStatus() {
    return g_status;
}

void SetFakeDaylightEnabled(bool enabled) {
    g_fake_daylight_requested = enabled;
    g_status.fake_daylight_enabled = enabled;
}

void SetDarkAreaBrightnessEnabled(bool enabled) {
    g_dark_area_requested = enabled;
    g_status.dark_area_brightness_enabled = enabled;
}

void SetForce360VisionEnabled(bool enabled) {
    g_force_360_vision_requested = enabled;
    g_status.force_360_vision_enabled = enabled;
}

void SetRevealUnexploredEnabled(bool enabled) {
    g_reveal_unexplored_requested = enabled;
    g_status.reveal_unexplored_enabled = enabled;
    SetUnexploredMaskSuppressed(enabled && g_status.world_ready);
}

void SetFogVisualMode(WeatherVisualMode mode) {
    g_fog_mode = mode;
    g_status.fog_mode = mode;
}

void SetPrecipitationVisualMode(PrecipitationVisualMode mode) {
    g_precipitation_mode = mode;
    g_status.precipitation_mode = mode;
}

void SetFogVisualIntensity(float intensity) {
    g_fog_intensity = std::clamp(intensity, 0.0f, 1.0f);
    g_status.fog_intensity = g_fog_intensity;
}

void SetPrecipitationVisualIntensity(float intensity) {
    g_precipitation_intensity = std::clamp(intensity, 0.0f, 1.0f);
    g_status.precipitation_intensity = g_precipitation_intensity;
}

}  // namespace pztrainer::bridge
