#include <Windows.h>
#include <jni.h>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

#include "bridge/viewpoint_bridge.hpp"
#include "bridge/viewpoint_projection.hpp"

using namespace pztrainer::bridge;

namespace {

bool Near(float actual, float expected) {
    return std::abs(actual - expected) < 0.01f;
}

void TestProjection() {
    ViewpointCamera camera;
    camera.valid = true;
    camera.width = 1920.0f;
    camera.height = 1080.0f;
    camera.origin = {-100.0, 2.0 * kViewpointHeightScale, -200.0};
    // Perspective matrix looking along -Z, with a 90 degree vertical FOV.
    camera.view_projection = {1080.0f / 1920.0f, 0, 0, 0,
                              0, 1, 0, 0, 0, 0, -1.002f, -1,
                              0, 0, -0.2002f, 0};
    ScreenPoint screen;
    assert(ProjectViewpointPoint(camera, {100, 205, 2}, screen));
    assert(Near(screen.x, 960) && Near(screen.y, 540));
    assert(ProjectViewpointPoint(camera, {99, 205, 2}, screen));
    assert(Near(screen.x, 1068));
    assert(ProjectViewpointPoint(camera, {100, 205, 3}, screen));
    assert(Near(screen.y, 540.0f - kViewpointHeightScale * 108.0f));
    assert(!ProjectViewpointPoint(camera, {100, 195, 2}, screen));
    assert(screen.x < 0 || screen.y < 0 || screen.x > 1920 || screen.y > 1080);
    assert(!ProjectViewpointPoint(camera, {100, 200.01f, 2}, screen));
    assert(!ProjectViewpointPoint(camera, {std::numeric_limits<float>::quiet_NaN(), 205, 2}, screen));
    float yaw;
    float pitch;
    assert(ViewpointTargetAngles(camera, {100, 205, 2}, yaw, pitch));
    assert(Near(yaw, 1.5707963f) && Near(pitch, 0));
    camera.eye[1] = 1.0f;
    assert(ViewpointTargetAngles(camera, {101, 205, 3}, yaw, pitch));
    assert(Near(yaw, std::atan2(5.0f, 1.0f)));
    assert(Near(pitch, std::atan2(kViewpointHeightScale - 1.0f, std::sqrt(26.0f))));
    camera.valid = false;
    assert(!ProjectViewpointPoint(camera, {100, 205, 2}, screen));
    assert(!ViewpointTargetAngles(camera, {100, 205, 2}, yaw, pitch));
}

void CheckJni(JNIEnv* env) {
    if (env->ExceptionCheck()) env->ExceptionDescribe();
    assert(!env->ExceptionCheck());
}

void TestActualMod(JNIEnv* env) {
    RefreshViewpointCamera(env, 1920, 1080);
    CheckJni(env);
    assert(IsViewpointLoaded() && !IsViewpointActive());
    jclass view = env->FindClass("viewpoint/core/View");
    jclass look = env->FindClass("viewpoint/input/Look");
    jclass renderer = env->FindClass("viewpoint/render/WorldRenderer");
    jclass context = env->FindClass("viewpoint/render/FrameContext");
    jclass scene_type = env->FindClass("viewpoint/render/SceneData");
    jclass matrix_type = env->FindClass("org/joml/Matrix4f");
    CheckJni(env);
    env->SetStaticBooleanField(view, env->GetStaticFieldID(view, "enabled", "Z"), JNI_TRUE);
    jobject frame = env->GetStaticObjectField(renderer,
        env->GetStaticFieldID(renderer, "frame", "Lviewpoint/render/FrameContext;"));
    jobject scene = env->NewObject(scene_type, env->GetMethodID(scene_type, "<init>", "()V"));
    env->SetObjectField(frame, env->GetFieldID(context, "scene", "Lviewpoint/render/SceneData;"), scene);
    env->SetDoubleField(scene, env->GetFieldID(scene_type, "originX", "D"), -100);
    env->SetDoubleField(scene, env->GetFieldID(scene_type, "originY", "D"), 2.0 * kViewpointHeightScale);
    env->SetDoubleField(scene, env->GetFieldID(scene_type, "originZ", "D"), -200);
    jobject matrix = env->GetObjectField(frame,
        env->GetFieldID(context, "plainViewProjection", "Lorg/joml/Matrix4f;"));
    env->CallObjectMethod(matrix, env->GetMethodID(matrix_type, "setPerspective", "(FFFF)Lorg/joml/Matrix4f;"),
        1.5707963f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    RefreshViewpointCamera(env, 1920, 1080);
    CheckJni(env);
    assert(IsViewpointActive());
    ScreenPoint screen;
    assert(ProjectViewpointWorldPoint({100, 205, 2}, screen));
    assert(Near(screen.x, 960) && Near(screen.y, 540));

    FrameSnapshot original;
    original.zombies.emplace_back();
    original.zombies[0].world_x = 100;
    original.zombies[0].world_y = 205;
    original.zombies[0].world_z = 2;
    original.zombies[0].screen_x = 321;
    original.zombies[0].has_bones = true;
    original.zombies[0].bone_world_positions.fill({100, 205, 2.5f});
    original.animals.emplace_back();
    original.animals[0].world_x = 100;
    original.animals[0].world_y = 205;
    original.animals[0].world_z = 2;
    original.vehicles.emplace_back();
    original.vehicles[0].world_x = 100;
    original.vehicles[0].world_y = 205;
    original.vehicles[0].world_z = 2;
    const auto projected = MakeViewpointFrame(original);
    assert(!original.viewpoint_3d && original.zombies[0].screen_x == 321);
    assert(projected.viewpoint_3d && Near(projected.zombies[0].screen_x, 960));
    assert(projected.zombies[0].screen_top_y < projected.zombies[0].screen_y);
    assert(projected.zombies[0].bones[0].y < 540);
    assert(projected.has_local_screen_position && Near(projected.local_screen_x, 960));
    assert(Near(projected.animals[0].screen_x, 960));
    assert(Near(projected.vehicles[0].screen_x, 960));

    const jfieldID yaw_field = env->GetStaticFieldID(look, "yaw", "F");
    const jfieldID pitch_field = env->GetStaticFieldID(look, "pitch", "F");
    const jfieldID captured_field = env->GetStaticFieldID(look, "captured", "Z");
    const jfieldID free_field = env->GetStaticFieldID(renderer, "freeCamera", "Z");
    env->SetStaticBooleanField(look, captured_field, JNI_TRUE);
    env->SetStaticFloatField(look, yaw_field, 1.5707963f);
    env->SetStaticFloatField(look, pitch_field, 0);
    float error = 0;
    assert(AimViewpointAt(env, {101, 205, 2.5f}, 0.5f, &error));
    assert(error > 0);
    assert(Near(env->GetStaticFloatField(look, yaw_field),
        (1.5707963f + std::atan2(5.0f, 1.0f)) * 0.5f));
    assert(env->GetStaticFloatField(look, pitch_field) > 0);
    env->SetStaticBooleanField(look, captured_field, JNI_FALSE);
    assert(!AimViewpointAt(env, {101, 205, 2.5f}, 1));
    env->SetStaticBooleanField(look, captured_field, JNI_TRUE);
    env->SetStaticBooleanField(renderer, free_field, JNI_TRUE);
    assert(!AimViewpointAt(env, {101, 205, 2.5f}, 1));
    jobject camera_view = env->NewObject(matrix_type, env->GetMethodID(matrix_type, "<init>", "()V"));
    const jmethodID perspective = env->GetMethodID(matrix_type, "setPerspective", "(FFFF)Lorg/joml/Matrix4f;");
    const jmethodID look_at = env->GetMethodID(matrix_type, "setLookAt", "(FFFFFFFFF)Lorg/joml/Matrix4f;");
    const jmethodID multiply = env->GetMethodID(matrix_type, "mul", "(Lorg/joml/Matrix4fc;)Lorg/joml/Matrix4f;");
    env->CallObjectMethod(matrix, perspective, 1.5707963f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    env->CallObjectMethod(camera_view, look_at, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    env->CallObjectMethod(matrix, multiply, camera_view);
    RefreshViewpointCamera(env, 1920, 1080);
    assert(ProjectViewpointWorldPoint({105, 200, 2}, screen));
    assert(Near(screen.x, 960) && Near(screen.y, 540));
    env->CallObjectMethod(matrix, perspective, 1.5707963f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
    env->CallObjectMethod(camera_view, look_at, 0.5f, 1.0f, -2.0f, 0.5f, 1.0f, -3.0f, 0.0f, 1.0f, 0.0f);
    env->CallObjectMethod(matrix, multiply, camera_view);
    env->SetFloatField(frame, env->GetFieldID(context, "eyeX", "F"), 0.5f);
    env->SetFloatField(frame, env->GetFieldID(context, "eyeY", "F"), 1.0f);
    env->SetFloatField(frame, env->GetFieldID(context, "eyeZ", "F"), -2.0f);
    RefreshViewpointCamera(env, 1920, 1080);
    assert(ProjectViewpointWorldPoint({99.5f, 207, 2.0f + 1.0f / kViewpointHeightScale}, screen));
    assert(Near(screen.x, 960) && Near(screen.y, 540));
    env->SetStaticBooleanField(view, env->GetStaticFieldID(view, "enabled", "Z"), JNI_FALSE);
    RefreshViewpointCamera(env, 1920, 1080);
    assert(IsViewpointLoaded() && !IsViewpointActive());
    assert(!ProjectViewpointWorldPoint({100, 205, 2}, screen));
    CheckJni(env);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    TestProjection();
    assert(argc >= 2);
    const std::filesystem::path game_root(argv[1]);
    const auto java_bin = game_root / "jre64" / "bin";
    SetDllDirectoryW(java_bin.c_str());
    HMODULE jvm_module = LoadLibraryW((java_bin / "server" / "jvm.dll").c_str());
    assert(jvm_module != nullptr);
    using CreateVm = jint(JNICALL*)(JavaVM**, void**, void*);
    auto create_vm = reinterpret_cast<CreateVm>(GetProcAddress(jvm_module, "JNI_CreateJavaVM"));
    assert(create_vm != nullptr);
    std::string classpath = "-Djava.class.path=" + (game_root / "projectzomboid.jar").u8string();
    if (argc >= 3) classpath += ";" + std::filesystem::path(argv[2]).u8string();
    JavaVMOption options[]{{classpath.data(), nullptr}};
    JavaVMInitArgs arguments{};
    arguments.version = JNI_VERSION_1_8;
    arguments.nOptions = 1;
    arguments.options = options;
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    assert(create_vm(&vm, reinterpret_cast<void**>(&env), &arguments) == JNI_OK);
    if (argc >= 3) {
        TestActualMod(env);
    } else {
        RefreshViewpointCamera(env, 1920, 1080);
        assert(!IsViewpointLoaded() && !IsViewpointActive());
        CheckJni(env);
    }
    assert(vm->DestroyJavaVM() == JNI_OK);
    return 0;
}
