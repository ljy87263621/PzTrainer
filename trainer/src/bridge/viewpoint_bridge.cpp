#include "bridge/viewpoint_bridge.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <string>

#include "bridge/viewpoint_projection.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    jclass view = nullptr;
    jclass look = nullptr;
    jclass renderer = nullptr;
    jfieldID enabled = nullptr;
    jfieldID yaw = nullptr;
    jfieldID pitch = nullptr;
    jfieldID captured = nullptr;
    jfieldID free_camera = nullptr;
    jfieldID frame = nullptr;
    jfieldID scene = nullptr;
    jfieldID matrix = nullptr;
    std::array<jfieldID, 3> origin{};
    std::array<jfieldID, 3> eye{};
    std::array<jfieldID, 16> elements{};
};

Bindings g_bindings;
ViewpointCamera g_camera;
ViewpointSettings g_settings;
bool g_active = false;
bool g_loaded = false;
std::chrono::steady_clock::time_point g_next_probe{};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.view != nullptr) return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_next_probe) return false;
    g_next_probe = now + std::chrono::seconds(2);
    if (env->PushLocalFrame(16) != JNI_OK) {
        ClearException(env);
        return false;
    }
    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    const jmethodID get_loader = loader_class == nullptr ? nullptr : env->GetStaticMethodID(
        loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = loader_class == nullptr ? nullptr : env->GetMethodID(
        loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_loader == nullptr ? nullptr :
        env->CallStaticObjectMethod(loader_class, get_loader);
    if (ClearException(env) || loader == nullptr || load_class == nullptr) {
        env->PopLocalFrame(nullptr);
        return false;
    }
    const auto load = [&](const char* name) {
        jstring class_name = env->NewStringUTF(name);
        jclass type = class_name == nullptr ? nullptr :
            static_cast<jclass>(env->CallObjectMethod(loader, load_class, class_name));
        if (class_name != nullptr) env->DeleteLocalRef(class_name);
        if (ClearException(env)) return static_cast<jclass>(nullptr);
        return type;
    };
    jclass view = load("viewpoint.core.View");
    if (view == nullptr) {
        env->PopLocalFrame(nullptr);
        return false;
    }
    g_loaded = true;
    jclass look = load("viewpoint.input.Look");
    jclass renderer = load("viewpoint.render.WorldRenderer");
    jclass context = load("viewpoint.render.FrameContext");
    jclass scene = load("viewpoint.render.SceneData");
    jclass matrix = load("org.joml.Matrix4f");
    if (look == nullptr || renderer == nullptr || context == nullptr ||
        scene == nullptr || matrix == nullptr) {
        env->PopLocalFrame(nullptr);
        return false;
    }
    Bindings bindings;
    bindings.enabled = env->GetStaticFieldID(view, "enabled", "Z");
    bindings.yaw = env->GetStaticFieldID(look, "yaw", "F");
    bindings.pitch = env->GetStaticFieldID(look, "pitch", "F");
    bindings.captured = env->GetStaticFieldID(look, "captured", "Z");
    bindings.free_camera = env->GetStaticFieldID(renderer, "freeCamera", "Z");
    bindings.frame = env->GetStaticFieldID(renderer, "frame", "Lviewpoint/render/FrameContext;");
    bindings.scene = env->GetFieldID(context, "scene", "Lviewpoint/render/SceneData;");
    bindings.matrix = env->GetFieldID(context, "plainViewProjection", "Lorg/joml/Matrix4f;");
    constexpr const char* origins[]{"originX", "originY", "originZ"};
    constexpr const char* eyes[]{"eyeX", "eyeY", "eyeZ"};
    for (std::size_t index = 0; index < 3; ++index) {
        bindings.origin[index] = env->GetFieldID(scene, origins[index], "D");
        bindings.eye[index] = env->GetFieldID(context, eyes[index], "F");
    }
    for (std::size_t index = 0; index < 16; ++index) {
        const std::string name = "m" + std::to_string(index / 4) + std::to_string(index % 4);
        bindings.elements[index] = env->GetFieldID(matrix, name.c_str(), "F");
    }
    if (ClearException(env)) {
        env->PopLocalFrame(nullptr);
        return false;
    }
    bindings.view = static_cast<jclass>(env->NewGlobalRef(view));
    bindings.look = static_cast<jclass>(env->NewGlobalRef(look));
    bindings.renderer = static_cast<jclass>(env->NewGlobalRef(renderer));
    g_bindings = bindings;
    env->PopLocalFrame(nullptr);
    return true;
}

template <typename Snapshot>
void ProjectCharacter(Snapshot& target, float height) {
    ScreenPoint feet;
    ScreenPoint top;
    ProjectViewpointWorldPoint({target.world_x, target.world_y, target.world_z}, feet);
    ProjectViewpointWorldPoint({target.world_x, target.world_y, target.world_z + height}, top);
    target.screen_x = feet.x;
    target.screen_y = feet.y;
    target.screen_top_y = top.y;
    target.in_view = !target.behind_wall && feet.x >= 0.0f && feet.x <= g_camera.width &&
        feet.y >= 0.0f && feet.y <= g_camera.height;
    if (target.has_bones) {
        for (std::size_t index = 0; index < target.bones.size(); ++index) {
            ProjectViewpointWorldPoint(target.bone_world_positions[index], target.bones[index]);
        }
    }
}

}  // namespace

void RefreshViewpointCamera(JNIEnv* env, float viewport_width, float viewport_height) {
    g_active = false;
    g_camera.valid = false;
    if (env == nullptr || !Initialize(env)) return;
    g_active = env->GetStaticBooleanField(g_bindings.view, g_bindings.enabled) == JNI_TRUE;
    if (ClearException(env) || !g_active) return;
    if (env->PushLocalFrame(3) != JNI_OK) {
        ClearException(env);
        return;
    }
    jobject frame = env->GetStaticObjectField(g_bindings.renderer, g_bindings.frame);
    jobject scene = frame == nullptr ? nullptr : env->GetObjectField(frame, g_bindings.scene);
    jobject matrix = frame == nullptr ? nullptr : env->GetObjectField(frame, g_bindings.matrix);
    if (ClearException(env) || scene == nullptr || matrix == nullptr) {
        env->PopLocalFrame(nullptr);
        return;
    }
    g_camera.width = viewport_width;
    g_camera.height = viewport_height;
    bool finite = viewport_width > 0.0f && viewport_height > 0.0f;
    for (std::size_t index = 0; index < 3; ++index) {
        g_camera.origin[index] = env->GetDoubleField(scene, g_bindings.origin[index]);
        g_camera.eye[index] = env->GetFloatField(frame, g_bindings.eye[index]);
        finite = finite && std::isfinite(g_camera.origin[index]) && std::isfinite(g_camera.eye[index]);
    }
    for (std::size_t index = 0; index < 16; ++index) {
        g_camera.view_projection[index] = env->GetFloatField(matrix, g_bindings.elements[index]);
        finite = finite && std::isfinite(g_camera.view_projection[index]);
    }
    g_camera.valid = !ClearException(env) && finite;
    env->PopLocalFrame(nullptr);
}

bool IsViewpointLoaded() { return g_loaded; }
bool IsViewpointActive() { return g_active; }
ViewpointSettings& GetViewpointSettings() { return g_settings; }

bool ProjectViewpointWorldPoint(const WorldPoint& point, ScreenPoint& screen) {
    return ProjectViewpointPoint(g_camera, point, screen);
}

FrameSnapshot MakeViewpointFrame(const FrameSnapshot& frame) {
    FrameSnapshot result = frame;
    result.viewpoint_3d = true;
    result.local_screen_x = g_camera.width * 0.5f;
    result.local_screen_y = g_camera.height * 0.5f;
    result.has_local_screen_position = g_camera.valid;
    for (auto& zombie : result.zombies) {
        ProjectCharacter(zombie, zombie.prone ? 0.20f : 0.58f);
    }
    for (auto& player : result.players) {
        ProjectCharacter(player, player.prone ? 0.20f : 0.58f);
    }
    for (auto& animal : result.animals) {
        ProjectCharacter(animal, std::clamp(animal.size * 0.52f, 0.28f, 0.82f));
    }
    for (auto& vehicle : result.vehicles) {
        ScreenPoint feet;
        ScreenPoint top;
        ProjectViewpointWorldPoint({vehicle.world_x, vehicle.world_y, vehicle.world_z}, feet);
        ProjectViewpointWorldPoint({vehicle.world_x, vehicle.world_y, vehicle.world_z + 0.72f}, top);
        vehicle.screen_x = feet.x;
        vehicle.screen_y = feet.y;
        vehicle.screen_top_y = top.y;
        vehicle.in_view = !vehicle.behind_wall && feet.x >= 0.0f && feet.x <= g_camera.width &&
            feet.y >= 0.0f && feet.y <= g_camera.height;
    }
    return result;
}

bool AimViewpointAt(JNIEnv* env, const WorldPoint& point, float response,
                    float* angle_error_degrees) {
    if (env == nullptr || !g_active || !g_camera.valid ||
        env->GetStaticBooleanField(g_bindings.look, g_bindings.captured) != JNI_TRUE ||
        env->GetStaticBooleanField(g_bindings.renderer, g_bindings.free_camera) == JNI_TRUE ||
        ClearException(env)) return false;
    float desired_yaw;
    float desired_pitch;
    if (!ViewpointTargetAngles(g_camera, point, desired_yaw, desired_pitch)) return false;
    const float yaw = env->GetStaticFloatField(g_bindings.look, g_bindings.yaw);
    const float pitch = env->GetStaticFloatField(g_bindings.look, g_bindings.pitch);
    if (ClearException(env) || !std::isfinite(yaw) || !std::isfinite(pitch)) return false;
    const float yaw_delta = std::remainder(desired_yaw - yaw, 6.2831853f);
    const float pitch_delta = desired_pitch - pitch;
    if (angle_error_degrees != nullptr) {
        *angle_error_degrees = std::hypot(yaw_delta, pitch_delta) * 57.2957795f;
    }
    response = std::clamp(response, 0.0f, 1.0f);
    env->SetStaticFloatField(g_bindings.look, g_bindings.yaw,
        std::remainder(yaw + yaw_delta * response, 6.2831853f));
    env->SetStaticFloatField(g_bindings.look, g_bindings.pitch, pitch + pitch_delta * response);
    return !ClearException(env);
}

}  // namespace pztrainer::bridge
