#include <jvmti.h>
#include "features/visual/native_capture_frame_hook.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "runtime/lua_bridge_resource.hpp"
#include "features/visual/model_owner_policy.hpp"
#include "features/visual/native_model_chams.hpp"

#include <Windows.h>
#include <GL/gl.h>
#include <MinHook.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/world_visibility_render_bridge.hpp"
#include "bridge/viewpoint_bridge.hpp"
#include "features/visual/viewpoint_model_chams.hpp"
#include "features/visual/native_model_material.hpp"
#include "features/visual/native_model_outline.hpp"
#include "features/visual/model_visual_policy.hpp"

namespace pztrainer::features::visual {
namespace {

constexpr GLenum kCurrentProgram = 0x8B8D;
constexpr GLenum kVertexArrayBinding = 0x85B5;
constexpr GLenum kArrayBuffer = 0x8892;
constexpr GLenum kElementArrayBuffer = 0x8893;
constexpr GLenum kArrayBufferBinding = 0x8894;
constexpr GLenum kElementArrayBufferBinding = 0x8895;
constexpr GLenum kVertexAttribArrayEnabled = 0x8622;
constexpr GLenum kVertexAttribArraySize = 0x8623;
constexpr GLenum kVertexAttribArrayStride = 0x8624;
constexpr GLenum kVertexAttribArrayType = 0x8625;
constexpr GLenum kVertexAttribArrayNormalized = 0x886A;
constexpr GLenum kVertexAttribArrayPointer = 0x8645;
constexpr GLenum kVertexAttribArrayBufferBinding = 0x889F;
constexpr GLenum kActiveTexture = 0x84E0;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kBlendSrcRgb = 0x80C9;
constexpr GLenum kBlendDstRgb = 0x80C8;
constexpr GLenum kTexture2DArray = 0x8C1A;
constexpr GLenum kTextureBinding2DArray = 0x8C1D;
constexpr GLenum kMaxArrayTextureLayers = 0x88FF;
constexpr GLenum kShaderStorageBuffer = 0x90D2;
constexpr GLenum kShaderStorageBufferBinding = 0x90D3;
constexpr GLenum kMaxShaderStorageBufferBindings = 0x90DD;
constexpr GLenum kRgba8 = 0x8058;
constexpr GLenum kTextureWrapR = 0x8072;
constexpr GLenum kClampToEdge = 0x812F;
constexpr std::size_t kCapturedSsboBindingLimit = 16;
constexpr std::size_t kCapturedVertexAttributeLimit = 8;

using DrawElementsFn = void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);
using DrawArraysFn = void(APIENTRY*)(GLenum, GLint, GLsizei);
using DrawRangeElementsFn = void(APIENTRY*)(
    GLenum, GLuint, GLuint, GLsizei, GLenum, const void*);
using DrawElementsInstancedFn = void(APIENTRY*)(
    GLenum, GLsizei, GLenum, const void*, GLsizei);
using UniformMatrix4fvFn = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
using GetUniformLocationFn = GLint(APIENTRY*)(GLuint, const char*);
using GetUniformfvFn = void(APIENTRY*)(GLuint, GLint, GLfloat*);
using GetUniformivFn = void(APIENTRY*)(GLuint, GLint, GLint*);
using UseProgramFn = void(APIENTRY*)(GLuint);
using Uniform1fFn = void(APIENTRY*)(GLint, GLfloat);
using Uniform1iFn = void(APIENTRY*)(GLint, GLint);
using Uniform3fFn = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using Uniform4fFn = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using BindVertexArrayFn = void(APIENTRY*)(GLuint);
using GenVertexArraysFn = void(APIENTRY*)(GLsizei, GLuint*);
using BindBufferFn = void(APIENTRY*)(GLenum, GLuint);
using GetVertexAttribivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetVertexAttribPointervFn = void(APIENTRY*)(GLuint, GLenum, void**);
using VertexAttribPointerFn = void(APIENTRY*)(
    GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using EnableVertexAttribArrayFn = void(APIENTRY*)(GLuint);
using DisableVertexAttribArrayFn = void(APIENTRY*)(GLuint);
using ActiveTextureFn = void(APIENTRY*)(GLenum);
using GetIntegeriVFn = void(APIENTRY*)(GLenum, GLuint, GLint*);
using BindBufferBaseFn = void(APIENTRY*)(GLenum, GLuint, GLuint);
using TexImage3DFn = void(APIENTRY*)(
    GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum,
    const void*);
using TexSubImage3DFn = void(APIENTRY*)(
    GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum,
    GLenum, const void*);
struct ProgramState {
    GLint model_view_projection = -1;
    GLint matrix_palette = -1;
    GLint final_scale = -1;
    GLint transform = -1;
    GLint color = -1;
    GLint tint_color = -1;
    GLint paint_color = -1;
    GLint alpha = -1;
    GLint target_depth = -1;
    GLint texture = -1;
    GLint instanced_texture = -1;
    GLint texture_dimensions = -1;
    std::array<float, 16> model_view_projection_value{};
    std::array<float, 16> transform_value{};
    std::array<float, 60 * 16> matrix_palette_value{};
    int matrix_palette_count = 0;
    GLboolean model_view_projection_transpose = GL_FALSE;
    GLboolean matrix_palette_transpose = GL_FALSE;
    GLboolean transform_transpose = GL_FALSE;
    bool has_model_view_projection = false;
    bool has_transform = false;
    bool classified = false;
    bool outline_program = false;
    bool instanced_program = false;
    bool vehicle_program = false;
};

struct DrawCommand {
    struct VertexAttribute {
        GLuint buffer = 0;
        GLint size = 0;
        GLenum type = 0;
        GLboolean normalized = GL_FALSE;
        GLsizei stride = 0;
        const void* pointer = nullptr;
        bool enabled = false;
    };

    GLuint program = 0;
    GLuint vertex_array = 0;
    GLuint element_buffer = 0;
    std::array<VertexAttribute, kCapturedVertexAttributeLimit> attributes{};
    GLuint texture = 0;
    GLint texture_unit = 0;
    GLenum mode = GL_TRIANGLES;
    GLsizei count = 0;
    GLenum type = GL_UNSIGNED_INT;
    const void* indices = nullptr;
    GLsizei instance_count = 0;
    bool instanced = false;
    int target_color_index = -1;
    int owner_identity = 0;
    std::array<GLint, 4> viewport{};
    GLint model_view_projection = -1;
    GLint matrix_palette = -1;
    GLint final_scale = -1;
    GLint transform = -1;
    GLint color = -1;
    std::array<float, 4> color_value{{1.0f, 1.0f, 1.0f, 1.0f}};
    std::array<float, 16> model_view_projection_value{};
    std::array<float, 16> transform_value{};
    std::array<float, 60 * 16> matrix_palette_value{};
    int matrix_palette_count = 0;
    GLboolean model_view_projection_transpose = GL_FALSE;
    GLboolean matrix_palette_transpose = GL_FALSE;
    GLboolean transform_transpose = GL_FALSE;
    float final_scale_value = 1.0f;
    std::array<GLuint, kCapturedSsboBindingLimit> ssbo_bindings{};
};

DrawElementsFn g_draw_elements = nullptr;
DrawArraysFn g_draw_arrays = nullptr;
DrawRangeElementsFn g_draw_range_elements = nullptr;
DrawElementsInstancedFn g_draw_elements_instanced = nullptr;
UniformMatrix4fvFn g_uniform_matrix4fv = nullptr;
GetUniformLocationFn g_get_uniform_location = nullptr;
GetUniformfvFn g_get_uniform_fv = nullptr;
GetUniformivFn g_get_uniform_iv = nullptr;
UseProgramFn g_use_program = nullptr;
Uniform1fFn g_uniform_1f = nullptr;
Uniform1iFn g_uniform_1i = nullptr;
NativeModelMaterial g_character_materials;
NativeModelOutline g_model_outline;
NativeModelOutline g_model_fill;
NativeModelOutline g_model_self_mask;
Uniform3fFn g_uniform_3f = nullptr;
Uniform4fFn g_uniform_4f = nullptr;
BindVertexArrayFn g_bind_vertex_array = nullptr;
GenVertexArraysFn g_gen_vertex_arrays = nullptr;
BindBufferFn g_bind_buffer = nullptr;
GetVertexAttribivFn g_get_vertex_attrib_iv = nullptr;
GetVertexAttribPointervFn g_get_vertex_attrib_pointer_v = nullptr;
VertexAttribPointerFn g_vertex_attrib_pointer = nullptr;
EnableVertexAttribArrayFn g_enable_vertex_attrib_array = nullptr;
DisableVertexAttribArrayFn g_disable_vertex_attrib_array = nullptr;
ActiveTextureFn g_active_texture = nullptr;
GetIntegeriVFn g_get_integer_i_v = nullptr;
BindBufferBaseFn g_bind_buffer_base = nullptr;
TexImage3DFn g_tex_image_3d = nullptr;
TexSubImage3DFn g_tex_sub_image_3d = nullptr;
std::unordered_map<GLuint, ProgramState> g_programs;
std::vector<DrawCommand> g_commands;
std::mutex g_commands_mutex;
bool g_initialized = false;
std::atomic_bool g_capture_enabled{false};
std::atomic_uintptr_t g_capture_context{0};
int g_initialization_status = 0;
std::uint64_t g_observed_draw_calls = 0;
std::uint64_t g_observed_instanced_draw_calls = 0;
std::uint64_t g_observed_matrix_uploads = 0;
std::uint64_t g_observed_use_program_calls = 0;
std::uint64_t g_capture_draw_calls = 0;
std::uint64_t g_capture_draws_without_program = 0;
std::uint64_t g_capture_enable_calls = 0;
std::uint64_t g_capture_enable_true_calls = 0;
std::uint64_t g_capture_context_binds = 0;
std::uint64_t g_foreign_context_updates_ignored = 0;
std::uint64_t g_foreign_context_draws_ignored = 0;
std::size_t g_outline_program_count = 0;
std::size_t g_instanced_program_count = 0;
std::size_t g_vehicle_program_count = 0;
std::uint64_t g_vehicle_tinted_draw_calls = 0;
std::array<ImVec4, 12> g_capture_target_colors{};
std::array<ImVec4, 12> g_capture_fill_colors{};
bool g_capture_target_colors_ready = false;
bool g_capture_vehicle_tint_enabled = false;
std::vector<bridge::VehicleSnapshot> g_capture_vehicles;
GLuint g_solid_texture_array = 0;
GLuint g_replay_vertex_array = 0;
GLsizei g_solid_texture_layers = 0;
GLuint g_ssbo_binding_count = 0;
// The launcher manual-maps this DLL. Calling TLS callbacks alone does not
// register a PE image's static TLS slots with the Windows loader, so C++
// thread_local storage is not valid for this module. Keep the render state in
// atomics instead; the active OpenGL context still scopes which calls are used.
std::atomic_bool g_replaying{false};
std::atomic<GLuint> g_current_program{0};

std::uintptr_t CurrentRenderContext() {
    return reinterpret_cast<std::uintptr_t>(wglGetCurrentContext());
}

bool IsCaptureRenderContext() {
    const std::uintptr_t capture_context =
        g_capture_context.load(std::memory_order_acquire);
    return capture_context != 0 && CurrentRenderContext() == capture_context;
}

void* ResolveOpenGlFunction(const char* name) {
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || value <= 3 || value == static_cast<std::uintptr_t>(-1)) {
        HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
        address = opengl == nullptr
            ? nullptr
            : reinterpret_cast<void*>(GetProcAddress(opengl, name));
    }
    return address;
}

bool ClearJniException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadSystemClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearJniException(env)) return nullptr;
    jmethodID get_system_class_loader = env->GetStaticMethodID(
        class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_class_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(class_loader_class, get_system_class_loader);
    env->DeleteLocalRef(class_loader_class);
    if (loader == nullptr || load_class == nullptr || ClearJniException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass loaded = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (ClearJniException(env)) return nullptr;
    return loaded;
}

void* ResolveLwjglCapabilityFunction(JNIEnv* env, const char* field_name) {
    if (env == nullptr) return nullptr;

    jclass gl_class = LoadSystemClass(env, "org/lwjgl/opengl/GL");
    if (gl_class == nullptr) return nullptr;
    jmethodID get_capabilities = env->GetStaticMethodID(
        gl_class, "getCapabilities", "()Lorg/lwjgl/opengl/GLCapabilities;");
    jobject capabilities = get_capabilities == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(gl_class, get_capabilities);
    if (capabilities == nullptr || ClearJniException(env)) {
        env->DeleteLocalRef(gl_class);
        return nullptr;
    }

    jclass capabilities_class = env->GetObjectClass(capabilities);
    jfieldID function_field = capabilities_class == nullptr
        ? nullptr
        : env->GetFieldID(capabilities_class, field_name, "J");
    const jlong function_address = function_field == nullptr
        ? 0
        : env->GetLongField(capabilities, function_field);
    const bool failed = ClearJniException(env);
    if (capabilities_class != nullptr) env->DeleteLocalRef(capabilities_class);
    env->DeleteLocalRef(capabilities);
    env->DeleteLocalRef(gl_class);
    if (failed || function_address == 0) return nullptr;
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(function_address));
}

ProgramState& GetProgramState(GLuint program) {
    ProgramState& state = g_programs[program];
    if (state.classified) return state;

    state.classified = true;
    state.model_view_projection = g_get_uniform_location(program, "ModelViewProjection");
    state.matrix_palette = g_get_uniform_location(program, "MatrixPalette[0]");
    if (state.matrix_palette < 0) {
        state.matrix_palette = g_get_uniform_location(program, "MatrixPalette");
    }
    state.final_scale = g_get_uniform_location(program, "FinalScale");
    state.transform = g_get_uniform_location(program, "transform");
    state.color = g_get_uniform_location(program, "u_color");
    state.tint_color = g_get_uniform_location(program, "TintColour");
    state.paint_color = g_get_uniform_location(program, "TexturePainColor");
    state.alpha = g_get_uniform_location(program, "Alpha");
    state.target_depth = g_get_uniform_location(program, "targetDepth");
    state.texture = g_get_uniform_location(program, "Texture");
    state.instanced_texture = g_get_uniform_location(program, "InstTexArr");
    state.texture_dimensions = g_get_uniform_location(program, "TextureDimensions");
    state.outline_program = state.model_view_projection >= 0 && state.color >= 0 &&
        (state.matrix_palette >= 0 || state.transform >= 0);
    if (state.outline_program) ++g_outline_program_count;
    state.instanced_program =
        state.instanced_texture >= 0 && state.texture_dimensions >= 0;
    if (state.instanced_program) ++g_instanced_program_count;
    state.vehicle_program = state.model_view_projection >= 0 &&
        state.tint_color >= 0 && state.target_depth >= 0 &&
        (state.paint_color >= 0 || state.alpha >= 0) &&
        (state.matrix_palette >= 0 || state.transform >= 0);
    if (state.vehicle_program) ++g_vehicle_program_count;
    return state;
}

GLuint CurrentProgram() {
    GLint queried_program = 0;
    glGetIntegerv(kCurrentProgram, &queried_program);
    if (queried_program > 0) {
        g_current_program = static_cast<GLuint>(queried_program);
    }
    return g_current_program;
}

void APIENTRY HookedUseProgram(GLuint program) {
    g_use_program(program);
    g_current_program = program;
    ++g_observed_use_program_calls;
}

bool CaptureVertexInput(DrawCommand& command) {
    glGetIntegerv(kVertexArrayBinding, reinterpret_cast<GLint*>(&command.vertex_array));
    glGetIntegerv(
        kElementArrayBufferBinding,
        reinterpret_cast<GLint*>(&command.element_buffer));
    if (command.element_buffer == 0) return false;

    bool has_enabled_attribute = false;
    for (GLuint index = 0; index < command.attributes.size(); ++index) {
        DrawCommand::VertexAttribute& attribute = command.attributes[index];
        GLint enabled = 0;
        GLint normalized = 0;
        GLint type = 0;
        GLint stride = 0;
        g_get_vertex_attrib_iv(index, kVertexAttribArrayEnabled, &enabled);
        attribute.enabled = enabled != 0;
        if (!attribute.enabled) continue;

        g_get_vertex_attrib_iv(index, kVertexAttribArrayBufferBinding,
                               reinterpret_cast<GLint*>(&attribute.buffer));
        g_get_vertex_attrib_iv(index, kVertexAttribArraySize, &attribute.size);
        g_get_vertex_attrib_iv(index, kVertexAttribArrayType, &type);
        g_get_vertex_attrib_iv(index, kVertexAttribArrayNormalized, &normalized);
        g_get_vertex_attrib_iv(index, kVertexAttribArrayStride, &stride);
        void* pointer = nullptr;
        g_get_vertex_attrib_pointer_v(index, kVertexAttribArrayPointer, &pointer);
        attribute.type = static_cast<GLenum>(type);
        attribute.normalized = normalized != 0 ? GL_TRUE : GL_FALSE;
        attribute.stride = static_cast<GLsizei>(stride);
        attribute.pointer = pointer;
        if (attribute.buffer == 0) return false;
        has_enabled_attribute = true;
    }
    return has_enabled_attribute;
}

void BindCapturedVertexInput(const DrawCommand& command) {
    if (g_replay_vertex_array == 0) {
        g_gen_vertex_arrays(1, &g_replay_vertex_array);
    }
    g_bind_vertex_array(g_replay_vertex_array);
    for (GLuint index = 0; index < command.attributes.size(); ++index) {
        const DrawCommand::VertexAttribute& attribute = command.attributes[index];
        if (!attribute.enabled) {
            g_disable_vertex_attrib_array(index);
            continue;
        }
        g_bind_buffer(kArrayBuffer, attribute.buffer);
        g_vertex_attrib_pointer(
            index, attribute.size, attribute.type, attribute.normalized,
            attribute.stride, attribute.pointer);
        g_enable_vertex_attrib_array(index);
    }
    g_bind_buffer(kElementArrayBuffer, command.element_buffer);
}

int CaptureTargetColorIndex(const std::array<float, 4>& color) {
    if (!g_capture_target_colors_ready) return -1;
    constexpr float kTolerance = 0.004f;
    for (int index = 0; index < static_cast<int>(g_capture_target_colors.size());
         ++index) {
        const ImVec4& target = g_capture_target_colors[index];
        if (std::abs(color[0] - target.x) <= kTolerance &&
            std::abs(color[1] - target.y) <= kTolerance &&
            std::abs(color[2] - target.z) <= kTolerance) {
            return index;
        }
    }
    return -1;
}

float MatrixElement(const std::array<float, 16>& matrix, int row, int column,
                    GLboolean transpose) {
    return transpose == GL_TRUE
        ? matrix[static_cast<std::size_t>(row * 4 + column)]
        : matrix[static_cast<std::size_t>(column * 4 + row)];
}

std::array<float, 4> TransformOrigin(
        const std::array<float, 16>& matrix, GLboolean transpose) {
    return {
        MatrixElement(matrix, 0, 3, transpose),
        MatrixElement(matrix, 1, 3, transpose),
        MatrixElement(matrix, 2, 3, transpose),
        MatrixElement(matrix, 3, 3, transpose),
    };
}

std::array<float, 4> TransformPoint(
        const std::array<float, 16>& matrix, GLboolean transpose,
        const std::array<float, 4>& point) {
    std::array<float, 4> result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            result[static_cast<std::size_t>(row)] +=
                MatrixElement(matrix, row, column, transpose) *
                point[static_cast<std::size_t>(column)];
        }
    }
    return result;
}

int FindVehicleColorIndex(const ProgramState& state) {
    if (!g_capture_vehicle_tint_enabled || g_capture_vehicles.empty() ||
        !state.has_model_view_projection) {
        return -1;
    }

    std::array<std::array<float, 4>, 3> clip_origins{};
    std::size_t origin_count = 0;
    clip_origins[origin_count++] = TransformOrigin(
        state.model_view_projection_value,
        state.model_view_projection_transpose);
    if (state.has_transform) {
        clip_origins[origin_count++] = TransformPoint(
            state.model_view_projection_value,
            state.model_view_projection_transpose,
            TransformOrigin(state.transform_value, state.transform_transpose));
    }
    if (state.matrix_palette_count > 0) {
        std::array<float, 16> root_matrix{};
        std::copy_n(state.matrix_palette_value.begin(), 16, root_matrix.begin());
        clip_origins[origin_count++] = TransformPoint(
            state.model_view_projection_value,
            state.model_view_projection_transpose,
            TransformOrigin(root_matrix, state.matrix_palette_transpose));
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const ImGuiIO& io = ImGui::GetIO();
    const float framebuffer_scale_x = std::max(
        io.DisplayFramebufferScale.x, 0.0001f);
    const float framebuffer_scale_y = std::max(
        io.DisplayFramebufferScale.y, 0.0001f);
    float best_score = 1000000.0f;
    const bridge::VehicleSnapshot* best_vehicle = nullptr;
    for (std::size_t origin_index = 0; origin_index < origin_count;
         ++origin_index) {
        const std::array<float, 4>& clip = clip_origins[origin_index];
        if (std::abs(clip[3]) <= 0.0001f) continue;
        const float framebuffer_x = static_cast<float>(viewport[0]) +
            (clip[0] / clip[3] * 0.5f + 0.5f) *
                static_cast<float>(viewport[2]);
        const float framebuffer_y = static_cast<float>(viewport[1]) +
            (clip[1] / clip[3] * 0.5f + 0.5f) *
                static_cast<float>(viewport[3]);
        const float screen_x = framebuffer_x / framebuffer_scale_x;
        const float screen_y = io.DisplaySize.y -
            framebuffer_y / framebuffer_scale_y;

        for (const bridge::VehicleSnapshot& vehicle : g_capture_vehicles) {
            if(!GetVisibilityOptions().Allows(VisualCategory::Vehicles,VisualFeature::Fill,vehicle.game_visible))continue;
            const float height = std::max(
                20.0f, vehicle.screen_y - vehicle.screen_top_y);
            const float half_width = height * 1.125f;
            const float margin = std::max(12.0f, height * 0.35f);
            const float x_extent = half_width + margin;
            const float y_min = vehicle.screen_top_y - margin;
            const float y_max = vehicle.screen_y + margin;
            if (screen_x < vehicle.screen_x - x_extent ||
                screen_x > vehicle.screen_x + x_extent ||
                screen_y < y_min || screen_y > y_max) {
                continue;
            }
            const float center_y =
                (vehicle.screen_top_y + vehicle.screen_y) * 0.5f;
            const float score = std::abs(screen_x - vehicle.screen_x) / x_extent +
                std::abs(screen_y - center_y) /
                    std::max((y_max - y_min) * 0.5f, 1.0f);
            if (score < best_score) {
                best_score = score;
                best_vehicle = &vehicle;
            }
        }
    }
    if (best_vehicle == nullptr) return -1;
    return 9 + (best_vehicle->behind_wall ? 1 : best_vehicle->in_view ? 2 : 0);
}

struct VehicleUniformOverride {
    ProgramState* state = nullptr;
    std::array<float, 3> tint{};
    std::array<float, 4> paint{};
    bool active = false;
};

VehicleUniformOverride BeginVehicleUniformOverride() {
    VehicleUniformOverride result{};
    if (!g_capture_enabled.load(std::memory_order_acquire) ||
        !IsCaptureRenderContext() || g_replaying ||
        !g_capture_target_colors_ready) {
        return result;
    }
    g_current_program = CurrentProgram();
    if (g_current_program == 0) return result;
    ProgramState& state = GetProgramState(g_current_program);
    if (!state.vehicle_program) return result;

    const int color_index = FindVehicleColorIndex(state);
    if (color_index < 9 || color_index >= 12) return result;
    const ImVec4& color =
        g_capture_fill_colors[static_cast<std::size_t>(color_index)];
    result.state = &state;
    g_get_uniform_fv(g_current_program, state.tint_color, result.tint.data());
    g_uniform_3f(state.tint_color, color.x, color.y, color.z);
    if (state.paint_color >= 0) {
        g_get_uniform_fv(g_current_program, state.paint_color, result.paint.data());
        float hue = 0.0f;
        float saturation = 0.0f;
        float value = 0.0f;
        ImGui::ColorConvertRGBtoHSV(
            color.x, color.y, color.z, hue, saturation, value);
        g_uniform_4f(
            state.paint_color, hue, saturation, value, result.paint[3]);
    }
    result.active = true;
    ++g_vehicle_tinted_draw_calls;
    return result;
}

void EndVehicleUniformOverride(const VehicleUniformOverride& override_state) {
    if (!override_state.active || override_state.state == nullptr) return;
    const ProgramState& state = *override_state.state;
    g_uniform_3f(
        state.tint_color, override_state.tint[0], override_state.tint[1],
        override_state.tint[2]);
    if (state.paint_color >= 0) {
        g_uniform_4f(
            state.paint_color, override_state.paint[0], override_state.paint[1],
            override_state.paint[2], override_state.paint[3]);
    }
}

int ResolveTrackedEntityColorIndex(const DrawCommand& command,
                                    const ModelOwnerMap& owners) {
    return command.instanced ? -1 : ResolveModelOwner(command.owner_identity,owners).group;
}

jclass g_owned_capture_helper=nullptr;
jmethodID g_current_owner=nullptr;
jfieldID g_owned_capture_error_field=nullptr;
jfieldID g_owned_frame_count=nullptr,g_owned_rendered_count=nullptr,g_owned_queued_count=nullptr;
std::string g_owned_capture_error;
jmethodID g_publish_capture=nullptr,g_clear_capture=nullptr;
bool g_owned_capture_ready=false;
std::chrono::steady_clock::time_point g_next_owned_capture{};

bool InitializeOwnedCapture(JNIEnv* env) {
    if(g_owned_capture_ready)return true;
    JavaVM* vm=nullptr; jvmtiEnv* tool=nullptr; std::string error;
    const auto jar=runtime::EnsurePlayerOverridesJar(error);
    if(jar.empty() || env->GetJavaVM(&vm)!=JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&tool),JVMTI_VERSION_1_2)!=JNI_OK) {
        g_owned_capture_error=error.empty()?"Native capture JVM interface unavailable":error;return false;
    }
    const bool appended=tool->AddToSystemClassLoaderSearch(jar.u8string().c_str())==JVMTI_ERROR_NONE;
    tool->DisposeEnvironment(); if(!appended) {g_owned_capture_error="Native capture helper search-path append failed";return false;}
    jclass helper=bridge::LoadEmbeddedJavaClass(env,"pztrainer.player.visual.NativeModelCapture",error);
    if(!helper) {g_owned_capture_error=error;return false;}
    g_current_owner=env->GetStaticMethodID(helper,"currentIdentity","()I");
    g_owned_capture_error_field=env->GetStaticFieldID(helper,"error","Ljava/lang/String;");
    g_owned_frame_count=env->GetStaticFieldID(helper,"worldCallbacks","J");
    g_owned_rendered_count=env->GetStaticFieldID(helper,"renderedModels","J");
    g_owned_queued_count=env->GetStaticFieldID(helper,"queuedModels","I");
    if(env->ExceptionCheck() || !g_current_owner || !g_owned_capture_error_field ||
        !g_owned_frame_count || !g_owned_rendered_count || !g_owned_queued_count) {
        if(env->ExceptionCheck())env->ExceptionClear();env->DeleteLocalRef(helper);
        g_owned_capture_error="Native capture helper is outdated; restart the game with the current launcher";
        return false;
    }
    if(!env->ExceptionCheck() && g_current_owner && !g_owned_capture_helper)g_owned_capture_helper=static_cast<jclass>(env->NewGlobalRef(helper));
    if(env->ExceptionCheck())env->ExceptionClear();
    g_publish_capture=env->GetStaticMethodID(helper,"publish","([I[I[F)V");
    g_clear_capture=env->GetStaticMethodID(helper,"clearPublished","()V");
    if(env->ExceptionCheck()) {
        env->ExceptionClear();env->DeleteLocalRef(helper);
        g_owned_capture_error="Native capture publish interface is missing; restart the game";return false;
    }
    g_owned_capture_ready=g_owned_capture_helper && g_publish_capture && g_clear_capture &&
        native_capture_frame::Install(env,g_owned_capture_error);
    env->DeleteLocalRef(helper);
    return g_owned_capture_ready;
}

int CurrentCapturedOwner() {
    JNIEnv* env=bridge::GetCurrentJniEnvironment();
    if(!env || !g_owned_capture_helper || !g_current_owner)return 0;
    const int owner=env->CallStaticIntMethod(g_owned_capture_helper,g_current_owner);
    if(env->ExceptionCheck()) {env->ExceptionClear();return 0;}
    return owner;
}

void QueueOwnedCapture(const bridge::FrameSnapshot& frame,const std::array<bool,4>& enabled) {
    JNIEnv* env=bridge::GetCurrentJniEnvironment(); if(!env)return;
    const auto now=std::chrono::steady_clock::now(); if(now<g_next_owned_capture)return;
    g_next_owned_capture=now+std::chrono::milliseconds(16);
    if(!InitializeOwnedCapture(env)) {g_next_owned_capture=now+std::chrono::seconds(2);return;}
    if(g_owned_capture_error_field) {
        auto detail=static_cast<jstring>(env->GetStaticObjectField(g_owned_capture_helper,g_owned_capture_error_field));
        if(detail && !env->ExceptionCheck()) {
            const char* text=env->GetStringUTFChars(detail,nullptr);
            if(text) {g_owned_capture_error=text;env->ReleaseStringUTFChars(detail,text);}
        }
        if(detail)env->DeleteLocalRef(detail);
        if(env->ExceptionCheck())env->ExceptionClear();
    }
    const auto owner_map=BuildModelOwnerMap(frame);
    std::vector<jint> owners,groups;
    const auto append=[&](const auto& objects,int category) {
        if(!enabled[category])return;
        for(const auto& object:objects) {
            const auto owner=ResolveModelOwner(object.identity,owner_map);
            if(owner.group<0 || (!ModelLayerVisible(owner,VisualFeature::Fill) && !ModelLayerVisible(owner,VisualFeature::Rim)))continue;
            owners.push_back(object.identity);groups.push_back(owner.group);
        }
    };
    append(frame.zombies,0);append(frame.players,1);append(frame.animals,2);
    if(owners.empty()) {env->CallStaticVoidMethod(g_owned_capture_helper,g_clear_capture);return;}
    const auto size=static_cast<jsize>(owners.size());
    jintArray owner_array=env->NewIntArray(size),group_array=env->NewIntArray(size);
    jfloatArray marker_array=env->NewFloatArray(48); float markers[48]{};
    for(unsigned i=0;i<12;i++) {markers[i*4]=g_capture_target_colors[i].x;markers[i*4+1]=g_capture_target_colors[i].y;markers[i*4+2]=g_capture_target_colors[i].z;}
    if(owner_array && group_array && marker_array) {
        env->SetIntArrayRegion(owner_array,0,size,owners.data());env->SetIntArrayRegion(group_array,0,size,groups.data());
        env->SetFloatArrayRegion(marker_array,0,48,markers);
        env->CallStaticVoidMethod(g_owned_capture_helper,g_publish_capture,owner_array,group_array,marker_array);
        if(env->ExceptionCheck())g_owned_capture_error="Unable to publish native model selection";
    }
    if(env->ExceptionCheck())env->ExceptionClear();
    if(owner_array)env->DeleteLocalRef(owner_array);if(group_array)env->DeleteLocalRef(group_array);if(marker_array)env->DeleteLocalRef(marker_array);
}

void APIENTRY HookedUniformMatrix4fv(GLint location, GLsizei count,
                                     GLboolean transpose, const GLfloat* value) {
    g_uniform_matrix4fv(location, count, transpose, value);
    ++g_observed_matrix_uploads;
    if (!g_capture_enabled.load(std::memory_order_acquire) ||
        g_replaying || value == nullptr || count <= 0 ||
        !IsCaptureRenderContext()) {
        return;
    }

    g_current_program = CurrentProgram();
    if (g_current_program == 0) return;
    ProgramState& state = GetProgramState(g_current_program);
    if (!state.outline_program && !state.vehicle_program) return;

    if (location == state.model_view_projection) {
        std::copy_n(value, 16, state.model_view_projection_value.begin());
        state.model_view_projection_transpose = transpose;
        state.has_model_view_projection = true;
    } else if (location == state.transform) {
        std::copy_n(value, 16, state.transform_value.begin());
        state.transform_transpose = transpose;
        state.has_transform = true;
    } else if (state.matrix_palette >= 0 && location >= state.matrix_palette &&
               location < state.matrix_palette + 60) {
        const int first_matrix = location - state.matrix_palette;
        const int copied_matrices = std::min<int>(count, 60 - first_matrix);
        std::copy_n(value, copied_matrices * 16,
                    state.matrix_palette_value.begin() + first_matrix * 16);
        state.matrix_palette_count = std::max(
            state.matrix_palette_count, first_matrix + copied_matrices);
        state.matrix_palette_transpose = transpose;
    }
}

void CaptureDrawElements(GLenum mode, GLsizei count, GLenum element_type,
                         const void* indices) {
    if (!g_capture_enabled.load(std::memory_order_acquire) ||
        g_replaying || count <= 0) {
        return;
    }
    if (!IsCaptureRenderContext()) {
        ++g_foreign_context_draws_ignored;
        return;
    }
    std::lock_guard<std::mutex> command_lock(g_commands_mutex);
    if (g_commands.size() >= 2048) return;
    ++g_capture_draw_calls;

    g_current_program = CurrentProgram();
    if (g_current_program == 0) {
        ++g_capture_draws_without_program;
        return;
    }
    ProgramState& state = GetProgramState(g_current_program);
    if (!state.outline_program || !state.has_model_view_projection ||
        (state.matrix_palette >= 0 && state.matrix_palette_count <= 0) ||
        (state.transform >= 0 && !state.has_transform)) {
        return;
    }

    DrawCommand command{};
    command.program = g_current_program;
    command.mode = mode;
    command.count = count;
    command.type = element_type;
    command.indices = indices;
    command.model_view_projection = state.model_view_projection;
    command.matrix_palette = state.matrix_palette;
    command.final_scale = state.final_scale;
    command.transform = state.transform;
    command.color = state.color;
    command.model_view_projection_value = state.model_view_projection_value;
    command.transform_value = state.transform_value;
    command.matrix_palette_value = state.matrix_palette_value;
    command.matrix_palette_count = state.matrix_palette_count;
    command.model_view_projection_transpose =
        state.model_view_projection_transpose;
    command.matrix_palette_transpose = state.matrix_palette_transpose;
    command.transform_transpose = state.transform_transpose;
    glGetIntegerv(GL_VIEWPORT, command.viewport.data());
    if (state.final_scale >= 0) {
        g_get_uniform_fv(command.program, state.final_scale, &command.final_scale_value);
    }
    g_get_uniform_fv(command.program, state.color, command.color_value.data());
    command.owner_identity=CurrentCapturedOwner();
    if(command.owner_identity==0)return;
    const int target_color_index = CaptureTargetColorIndex(command.color_value);
    if(target_color_index<0)return;
    command.target_color_index = target_color_index;
    if (target_color_index >= 0) {
        const ImVec4& fill_color = g_capture_fill_colors[target_color_index];
        command.color_value = {
            fill_color.x, fill_color.y, fill_color.z, fill_color.w,
        };
    }

    GLint active_texture = 0;
    glGetIntegerv(kActiveTexture, &active_texture);
    command.texture_unit = 0;
    if (state.texture >= 0) {
        g_get_uniform_iv(command.program, state.texture, &command.texture_unit);
    }
    g_active_texture(kTexture0 + command.texture_unit);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, reinterpret_cast<GLint*>(&command.texture));
    g_active_texture(static_cast<GLenum>(active_texture));
    if (command.texture != 0 && CaptureVertexInput(command)) {
        g_commands.push_back(command);
    }
}

void APIENTRY HookedDrawElements(GLenum mode, GLsizei count, GLenum element_type,
                                 const void* indices) {
    const VehicleUniformOverride vehicle_override =
        BeginVehicleUniformOverride();
    g_draw_elements(mode, count, element_type, indices);
    EndVehicleUniformOverride(vehicle_override);
    ++g_observed_draw_calls;
    CaptureDrawElements(mode, count, element_type, indices);
}

void APIENTRY HookedDrawArrays(GLenum mode, GLint first, GLsizei count) {
    const GLuint current_program = CurrentProgram();
    if (current_program > 0 &&
        bridge::ShouldSuppressVisibilityPolygon(
            current_program)) {
        return;
    }
    g_draw_arrays(mode, first, count);
}

void APIENTRY HookedDrawRangeElements(
    GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum element_type,
    const void* indices) {
    const VehicleUniformOverride vehicle_override =
        BeginVehicleUniformOverride();
    g_draw_range_elements(mode, start, end, count, element_type, indices);
    EndVehicleUniformOverride(vehicle_override);
    ++g_observed_draw_calls;
    CaptureDrawElements(mode, count, element_type, indices);
}

void APIENTRY HookedDrawElementsInstanced(
    GLenum mode, GLsizei count, GLenum element_type, const void* indices,
    GLsizei instance_count) {
    g_draw_elements_instanced(mode, count, element_type, indices, instance_count);
    ++g_observed_instanced_draw_calls;
    if (!g_capture_enabled.load(std::memory_order_acquire) ||
        g_replaying || count <= 0 || instance_count <= 0) {
        return;
    }
    if (!IsCaptureRenderContext()) {
        ++g_foreign_context_draws_ignored;
        return;
    }
    std::lock_guard<std::mutex> command_lock(g_commands_mutex);
    if (g_commands.size() >= 2048) return;
    ++g_capture_draw_calls;

    g_current_program = CurrentProgram();
    if (g_current_program == 0) {
        ++g_capture_draws_without_program;
        return;
    }
    ProgramState& state = GetProgramState(g_current_program);
    if (!state.instanced_program) return;

    DrawCommand command{};
    command.program = g_current_program;
    command.mode = mode;
    command.count = count;
    command.type = element_type;
    command.indices = indices;
    command.instance_count = instance_count;
    command.instanced = true;
    glGetIntegerv(GL_VIEWPORT, command.viewport.data());

    GLint active_texture = 0;
    glGetIntegerv(kActiveTexture, &active_texture);
    g_get_uniform_iv(command.program, state.instanced_texture, &command.texture_unit);
    g_active_texture(kTexture0 + command.texture_unit);
    glGetIntegerv(kTextureBinding2DArray, reinterpret_cast<GLint*>(&command.texture));
    g_active_texture(static_cast<GLenum>(active_texture));

    bool has_ssbo = false;
    for (GLuint binding = 0; binding < g_ssbo_binding_count; ++binding) {
        GLint buffer = 0;
        g_get_integer_i_v(kShaderStorageBufferBinding, binding, &buffer);
        command.ssbo_bindings[binding] =
            buffer > 0 ? static_cast<GLuint>(buffer) : 0;
        has_ssbo = has_ssbo || buffer > 0;
    }
    if (command.texture != 0 && has_ssbo && CaptureVertexInput(command)) {
        g_commands.push_back(command);
    }
}

bool UpdateSolidTextureArray(const ImVec4& color) {
    GLint saved_active_texture = 0;
    GLint saved_texture = 0;
    glGetIntegerv(kActiveTexture, &saved_active_texture);
    g_active_texture(kTexture0);
    glGetIntegerv(kTextureBinding2DArray, &saved_texture);

    if (g_solid_texture_array == 0) {
        GLint maximum_layers = 0;
        glGetIntegerv(kMaxArrayTextureLayers, &maximum_layers);
        g_solid_texture_layers = static_cast<GLsizei>(
            std::clamp(maximum_layers, 1, 2048));
        glGenTextures(1, &g_solid_texture_array);
        glBindTexture(kTexture2DArray, g_solid_texture_array);
        glTexParameteri(kTexture2DArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(kTexture2DArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(kTexture2DArray, GL_TEXTURE_WRAP_S, kClampToEdge);
        glTexParameteri(kTexture2DArray, GL_TEXTURE_WRAP_T, kClampToEdge);
        glTexParameteri(kTexture2DArray, kTextureWrapR, kClampToEdge);
        g_tex_image_3d(
            kTexture2DArray, 0, kRgba8, 1, 1, g_solid_texture_layers, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    } else {
        glBindTexture(kTexture2DArray, g_solid_texture_array);
    }

    const auto channel = [](float value) {
        return static_cast<unsigned char>(
            std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    };
    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(g_solid_texture_layers) * 4);
    for (GLsizei layer = 0; layer < g_solid_texture_layers; ++layer) {
        const std::size_t offset = static_cast<std::size_t>(layer) * 4;
        pixels[offset] = channel(color.x);
        pixels[offset + 1] = channel(color.y);
        pixels[offset + 2] = channel(color.z);
        pixels[offset + 3] = channel(color.w);
    }
    g_tex_sub_image_3d(
        kTexture2DArray, 0, 0, 0, 0, 1, 1, g_solid_texture_layers,
        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    glBindTexture(kTexture2DArray, static_cast<GLuint>(saved_texture));
    g_active_texture(static_cast<GLenum>(saved_active_texture));
    return g_solid_texture_array != 0;
}

void ReplayCapturedModel(const DrawCommand& command,const ImVec4& colour,
                         ZombieModelEffect effect,const NativeModelMaterial::Program* material) {
    // Begin() set the output mask's viewport. Captured MVP coordinates are NDC
    // in the zoomed world FBO; replaying its larger viewport into the UI-sized
    // mask would scale/clip both fill and rim a second time.
    g_use_program(material?material->id:command.program);
    BindCapturedVertexInput(command);
    g_active_texture(kTexture0+command.texture_unit); glBindTexture(GL_TEXTURE_2D,command.texture);
    const GLint projection=material?material->projection:command.model_view_projection;
    const GLint palette=material?material->palette:command.matrix_palette;
    const GLint transform=material?material->transform:command.transform;
    const GLint scale=material?material->scale:command.final_scale;
    const GLint color=material?material->colour:command.color;
    g_uniform_matrix4fv(projection,1,command.model_view_projection_transpose,command.model_view_projection_value.data());
    if(palette>=0 && command.matrix_palette_count>0)
        g_uniform_matrix4fv(palette,command.matrix_palette_count,command.matrix_palette_transpose,command.matrix_palette_value.data());
    if(transform>=0)g_uniform_matrix4fv(transform,1,command.transform_transpose,command.transform_value.data());
    if(scale>=0)g_uniform_1f(scale,command.final_scale_value);
    g_uniform_4f(color,colour.x,colour.y,colour.z,colour.w);
    if(material) {
        g_uniform_1i(material->texture,command.texture_unit);
        g_uniform_1i(material->mode,effect==ZombieModelEffect::WhiteHot?2:1);
    }
    g_draw_elements(command.mode,command.count,command.type,command.indices);
}

ImVec4 EffectColor(const ImVec4& base, ZombieModelEffect effect, std::size_t index) {
    ImVec4 color = base;
    if (effect == ZombieModelEffect::Shaded) color.w *= 0.62f;
    if (effect == ZombieModelEffect::Solid) color.w = std::max(color.w, 0.92f);
    if (effect == ZombieModelEffect::Glow || effect == ZombieModelEffect::GlowOutline) {
        color.w = std::max(color.w, 0.82f);
    }
    if (effect == ZombieModelEffect::Glossy) {
        color.x = std::min(1.0f, color.x * 1.18f);
        color.y = std::min(1.0f, color.y * 1.18f);
        color.z = std::min(1.0f, color.z * 1.18f);
        color.w = std::max(color.w, 0.88f);
    }
    if (effect == ZombieModelEffect::Iridescent) {
        const float hue = std::fmod(
            static_cast<float>(ImGui::GetTime()) * 0.18f + index * 0.11f, 1.0f);
        ImGui::ColorConvertHSVtoRGB(hue, 0.82f, 1.0f, color.x, color.y, color.z);
        color.w = 0.90f;
    }
    if (effect == ZombieModelEffect::WaterFlow) {
        const float wave = 0.5f + 0.5f * std::sin(
            static_cast<float>(ImGui::GetTime()) * 2.0f + index * 0.7f);
        color = ImVec4(0.05f, 0.55f + wave * 0.25f, 1.0f, 0.86f);
    }
    return color;
}

}  // namespace

bool NativeModelChams::Initialize() {
    if (g_initialized) return true;

    JNIEnv* env = bridge::GetCurrentJniEnvironment();
    if (env == nullptr) {
        g_initialization_status = -2;
        return false;
    }

    void* draw_elements = ResolveLwjglCapabilityFunction(env, "glDrawElements");
    void* draw_arrays = ResolveLwjglCapabilityFunction(env, "glDrawArrays");
    void* draw_range_elements = ResolveLwjglCapabilityFunction(
        env, "glDrawRangeElements");
    void* draw_elements_instanced = ResolveLwjglCapabilityFunction(
        env, "glDrawElementsInstanced");
    void* uniform_matrix4fv = ResolveLwjglCapabilityFunction(
        env, "glUniformMatrix4fv");
    void* use_program = ResolveLwjglCapabilityFunction(env, "glUseProgram");
    if (draw_elements == nullptr || draw_arrays == nullptr ||
        draw_range_elements == nullptr ||
        draw_elements_instanced == nullptr ||
        uniform_matrix4fv == nullptr || use_program == nullptr) {
        g_initialization_status = -3;
        return false;
    }

    g_draw_elements = reinterpret_cast<DrawElementsFn>(draw_elements);
    g_draw_arrays = reinterpret_cast<DrawArraysFn>(draw_arrays);
    g_draw_range_elements = reinterpret_cast<DrawRangeElementsFn>(
        draw_range_elements);
    g_draw_elements_instanced = reinterpret_cast<DrawElementsInstancedFn>(
        draw_elements_instanced);
    g_uniform_matrix4fv = reinterpret_cast<UniformMatrix4fvFn>(uniform_matrix4fv);
    g_use_program = reinterpret_cast<UseProgramFn>(use_program);
    g_get_uniform_location = reinterpret_cast<GetUniformLocationFn>(
        ResolveOpenGlFunction("glGetUniformLocation"));
    g_get_uniform_fv = reinterpret_cast<GetUniformfvFn>(
        ResolveOpenGlFunction("glGetUniformfv"));
    g_get_uniform_iv = reinterpret_cast<GetUniformivFn>(
        ResolveOpenGlFunction("glGetUniformiv"));
    g_uniform_1f = reinterpret_cast<Uniform1fFn>(ResolveOpenGlFunction("glUniform1f"));
    g_uniform_1i = reinterpret_cast<Uniform1iFn>(ResolveOpenGlFunction("glUniform1i"));
    g_uniform_3f = reinterpret_cast<Uniform3fFn>(ResolveOpenGlFunction("glUniform3f"));
    g_uniform_4f = reinterpret_cast<Uniform4fFn>(ResolveOpenGlFunction("glUniform4f"));
    g_bind_vertex_array = reinterpret_cast<BindVertexArrayFn>(
        ResolveOpenGlFunction("glBindVertexArray"));
    g_gen_vertex_arrays = reinterpret_cast<GenVertexArraysFn>(
        ResolveOpenGlFunction("glGenVertexArrays"));
    g_bind_buffer = reinterpret_cast<BindBufferFn>(
        ResolveOpenGlFunction("glBindBuffer"));
    g_get_vertex_attrib_iv = reinterpret_cast<GetVertexAttribivFn>(
        ResolveOpenGlFunction("glGetVertexAttribiv"));
    g_get_vertex_attrib_pointer_v = reinterpret_cast<GetVertexAttribPointervFn>(
        ResolveOpenGlFunction("glGetVertexAttribPointerv"));
    g_vertex_attrib_pointer = reinterpret_cast<VertexAttribPointerFn>(
        ResolveOpenGlFunction("glVertexAttribPointer"));
    g_enable_vertex_attrib_array = reinterpret_cast<EnableVertexAttribArrayFn>(
        ResolveOpenGlFunction("glEnableVertexAttribArray"));
    g_disable_vertex_attrib_array = reinterpret_cast<DisableVertexAttribArrayFn>(
        ResolveOpenGlFunction("glDisableVertexAttribArray"));
    g_active_texture = reinterpret_cast<ActiveTextureFn>(
        ResolveOpenGlFunction("glActiveTexture"));
    g_get_integer_i_v = reinterpret_cast<GetIntegeriVFn>(
        ResolveOpenGlFunction("glGetIntegeri_v"));
    g_bind_buffer_base = reinterpret_cast<BindBufferBaseFn>(
        ResolveOpenGlFunction("glBindBufferBase"));
    g_tex_image_3d = reinterpret_cast<TexImage3DFn>(
        ResolveOpenGlFunction("glTexImage3D"));
    g_tex_sub_image_3d = reinterpret_cast<TexSubImage3DFn>(
        ResolveOpenGlFunction("glTexSubImage3D"));
    if (g_draw_elements == nullptr || g_draw_arrays == nullptr ||
        g_draw_range_elements == nullptr ||
        g_draw_elements_instanced == nullptr ||
        g_uniform_matrix4fv == nullptr ||
        g_use_program == nullptr ||
        g_get_uniform_location == nullptr || g_get_uniform_fv == nullptr ||
        g_get_uniform_iv == nullptr ||
        g_uniform_1f == nullptr || g_uniform_1i == nullptr || g_uniform_3f == nullptr ||
        g_uniform_4f == nullptr ||
        g_bind_vertex_array == nullptr || g_gen_vertex_arrays == nullptr ||
        g_bind_buffer == nullptr || g_get_vertex_attrib_iv == nullptr ||
        g_get_vertex_attrib_pointer_v == nullptr ||
        g_vertex_attrib_pointer == nullptr ||
        g_enable_vertex_attrib_array == nullptr ||
        g_disable_vertex_attrib_array == nullptr ||
        g_active_texture == nullptr ||
        g_get_integer_i_v == nullptr || g_bind_buffer_base == nullptr ||
        g_tex_image_3d == nullptr || g_tex_sub_image_3d == nullptr) {
        g_initialization_status = -1;
        return false;
    }

    MH_STATUS hook_status = MH_CreateHook(
        draw_elements,
        reinterpret_cast<void*>(&HookedDrawElements),
        reinterpret_cast<void**>(&g_draw_elements));
    if (hook_status != MH_OK) {
        g_initialization_status = 100 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_CreateHook(
        draw_arrays,
        reinterpret_cast<void*>(&HookedDrawArrays),
        reinterpret_cast<void**>(&g_draw_arrays));
    if (hook_status != MH_OK) {
        g_initialization_status = 125 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_CreateHook(
        draw_range_elements,
        reinterpret_cast<void*>(&HookedDrawRangeElements),
        reinterpret_cast<void**>(&g_draw_range_elements));
    if (hook_status != MH_OK) {
        g_initialization_status = 150 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_CreateHook(
        uniform_matrix4fv,
        reinterpret_cast<void*>(&HookedUniformMatrix4fv),
        reinterpret_cast<void**>(&g_uniform_matrix4fv));
    if (hook_status != MH_OK) {
        g_initialization_status = 200 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_CreateHook(
        use_program,
        reinterpret_cast<void*>(&HookedUseProgram),
        reinterpret_cast<void**>(&g_use_program));
    if (hook_status != MH_OK) {
        g_initialization_status = 225 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_CreateHook(
        draw_elements_instanced,
        reinterpret_cast<void*>(&HookedDrawElementsInstanced),
        reinterpret_cast<void**>(&g_draw_elements_instanced));
    if (hook_status != MH_OK) {
        g_initialization_status = 250 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(draw_elements);
    if (hook_status != MH_OK) {
        g_initialization_status = 300 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(draw_arrays);
    if (hook_status != MH_OK) {
        g_initialization_status = 325 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(draw_range_elements);
    if (hook_status != MH_OK) {
        g_initialization_status = 350 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(uniform_matrix4fv);
    if (hook_status != MH_OK) {
        g_initialization_status = 400 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(use_program);
    if (hook_status != MH_OK) {
        g_initialization_status = 425 + static_cast<int>(hook_status);
        return false;
    }
    hook_status = MH_EnableHook(draw_elements_instanced);
    if (hook_status != MH_OK) {
        g_initialization_status = 450 + static_cast<int>(hook_status);
        return false;
    }

    g_current_program = CurrentProgram();

    GLint maximum_ssbo_bindings = 0;
    glGetIntegerv(kMaxShaderStorageBufferBindings, &maximum_ssbo_bindings);
    g_ssbo_binding_count = static_cast<GLuint>(std::clamp(
        maximum_ssbo_bindings, 0,
        static_cast<GLint>(kCapturedSsboBindingLimit)));

    g_commands.reserve(512);
    g_initialized = true;
    g_initialization_status = 1;
    return true;
}

void NativeModelChams::SetCaptureEnabled(
        bool enabled, const bridge::FrameSnapshot& frame,
        const VisualSettings& settings,
        const PlayerVisualSettings& player_settings,
        const AnimalVisualSettings& animal_settings,
        const VehicleVisualSettings& vehicle_settings) {
    ++g_capture_enable_calls;
    const std::uintptr_t current_context = CurrentRenderContext();
    std::uintptr_t capture_context =
        g_capture_context.load(std::memory_order_acquire);
    if (capture_context == 0) {
        if (!enabled || current_context == 0 ||
            frame.gate_status != bridge::GateStatus::SinglePlayerAllowed ||
            frame.model_chams_status != 2) {
            return;
        }
        g_capture_context.store(current_context, std::memory_order_release);
        capture_context = current_context;
        ++g_capture_context_binds;
    } else if (current_context != capture_context) {
        if (!enabled || current_context == 0 ||
            frame.gate_status != bridge::GateStatus::SinglePlayerAllowed ||
            frame.model_chams_status != 2) {
            ++g_foreign_context_updates_ignored;
            return;
        }
        g_capture_enabled.store(false, std::memory_order_release);
        {
            std::lock_guard<std::mutex> command_lock(g_commands_mutex);
            g_commands.clear();
        }
        g_capture_context.store(current_context, std::memory_order_release);
        capture_context = current_context;
        ++g_capture_context_binds;
    }

    g_capture_target_colors = {
        ModelCaptureMarker(ZombieVisualState::Default),
        ModelCaptureMarker(ZombieVisualState::BehindWall),
        ModelCaptureMarker(ZombieVisualState::InView),
        PlayerModelCaptureMarker(ZombieVisualState::Default),
        PlayerModelCaptureMarker(ZombieVisualState::BehindWall),
        PlayerModelCaptureMarker(ZombieVisualState::InView),
        AnimalModelCaptureMarker(ZombieVisualState::Default),
        AnimalModelCaptureMarker(ZombieVisualState::BehindWall),
        AnimalModelCaptureMarker(ZombieVisualState::InView),
        VehicleModelCaptureMarker(ZombieVisualState::Default),
        VehicleModelCaptureMarker(ZombieVisualState::BehindWall),
        VehicleModelCaptureMarker(ZombieVisualState::InView),
    };
    g_capture_fill_colors = {
        settings.model_colors.normal,
        settings.model_colors.behind_wall_enabled
            ? settings.model_colors.behind_wall : settings.model_colors.normal,
        settings.model_colors.in_view_enabled
            ? settings.model_colors.in_view : settings.model_colors.normal,
        player_settings.model_colors.normal,
        player_settings.model_colors.behind_wall_enabled
            ? player_settings.model_colors.behind_wall
            : player_settings.model_colors.normal,
        player_settings.model_colors.in_view_enabled
            ? player_settings.model_colors.in_view
            : player_settings.model_colors.normal,
        animal_settings.model_colors.normal,
        animal_settings.model_colors.behind_wall_enabled
            ? animal_settings.model_colors.behind_wall
            : animal_settings.model_colors.normal,
        animal_settings.model_colors.in_view_enabled
            ? animal_settings.model_colors.in_view
            : animal_settings.model_colors.normal,
        vehicle_settings.model_colors.normal,
        vehicle_settings.model_colors.behind_wall_enabled
            ? vehicle_settings.model_colors.behind_wall
            : vehicle_settings.model_colors.normal,
        vehicle_settings.model_colors.in_view_enabled
            ? vehicle_settings.model_colors.in_view
            : vehicle_settings.model_colors.normal,
    };
    g_capture_target_colors_ready = true;
    const bool capture_enabled = enabled && g_initialized && !bridge::IsViewpointActive();
    g_capture_enabled.store(capture_enabled, std::memory_order_release);
    if (capture_enabled) ++g_capture_enable_true_calls;
    if(capture_enabled)QueueOwnedCapture(frame,{{settings.zombie_esp && ModelVisualsEnabled(settings.model_effect,settings.model_edge_glow),
        player_settings.player_esp && ModelVisualsEnabled(player_settings.model_effect,player_settings.model_edge_glow),
        animal_settings.animal_esp && ModelVisualsEnabled(animal_settings.model_effect,animal_settings.model_edge_glow),false}});
    g_capture_vehicle_tint_enabled = capture_enabled &&
        vehicle_settings.vehicle_esp &&
        vehicle_settings.model_effect != ZombieModelEffect::Disabled;
    g_capture_vehicles = g_capture_vehicle_tint_enabled
        ? frame.vehicles : std::vector<bridge::VehicleSnapshot>{};
    if (!capture_enabled) {
        JNIEnv* env=bridge::GetCurrentJniEnvironment();
        if(env && g_owned_capture_helper && g_clear_capture) {
            env->CallStaticVoidMethod(g_owned_capture_helper,g_clear_capture);
            if(env->ExceptionCheck())env->ExceptionClear();
        }
        std::lock_guard<std::mutex> command_lock(g_commands_mutex);
        g_commands.clear();
    }
}

void NativeModelChams::Replay(
        const bridge::FrameSnapshot& frame, const VisualSettings& settings,
        const PlayerVisualSettings& player_settings,
        const AnimalVisualSettings& animal_settings,
        const VehicleVisualSettings& vehicle_settings) {
    const auto& viewpoint_settings = bridge::GetViewpointSettings();
    const auto& viewpoint_status = bridge::GetViewpointStatus();
    const bool viewpoint_active = bridge::IsViewpointActive();
    const bool viewpoint_esp = bridge::UseViewpointEsp(
        viewpoint_settings, viewpoint_status);
    if (!bridge::CanUseViewpoint3D(viewpoint_settings, viewpoint_status)) {
        ResetViewpointModelChamsStatus(viewpoint_status.message);
    } else if (!viewpoint_esp) {
        ResetViewpointModelChamsStatus("ViewPoint 3D ESP 已关闭");
    } else if (frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        ResetViewpointModelChamsStatus("3D 模型上色仅在单人模式可用");
    }

    // ViewPoint uses its own model batches. It must not wait for a vanilla
    // outline draw or the vanilla capture context to be armed.
    if(viewpoint_active) {
        g_capture_enabled.store(false,std::memory_order_release);
        if(!viewpoint_esp || frame.gate_status!=bridge::GateStatus::SinglePlayerAllowed)return;
        std::array<ImVec4,12> markers{},colours{},edges{};
        std::array<int,12> effects{};
        const auto append=[&](int group,bool enabled,const auto& options) {
            if(!enabled || !ModelVisualsEnabled(options.model_effect,options.model_edge_glow))return;
            for(int state=0;state<3;state++) {
                const int at=group*3+state;
                colours[at]=EffectColor(ResolveColor(options.model_colors,static_cast<ZombieVisualState>(state)),options.model_effect,at);
                effects[at]=static_cast<int>(options.model_effect);
                if(options.model_edge_glow)edges[at]=options.model_edge_glow_color;
            }
        };
        append(0,settings.zombie_esp,settings); append(1,player_settings.player_esp,player_settings);
        append(2,animal_settings.animal_esp,animal_settings); append(3,vehicle_settings.vehicle_esp,vehicle_settings);
        DrawViewpointModelChams(markers,colours,edges,effects,frame);
        return;
    }
    const std::uintptr_t capture_context =
        g_capture_context.load(std::memory_order_acquire);
    if (capture_context == 0 || CurrentRenderContext() != capture_context) {
        if (viewpoint_esp && frame.gate_status == bridge::GateStatus::SinglePlayerAllowed) {
            ResetViewpointModelChamsStatus("等待 ViewPoint 3D 模型渲染接口");
        }
        return;
    }
    g_capture_enabled.store(false, std::memory_order_release);
    const bool zombie_models = settings.zombie_esp &&
        ModelVisualsEnabled(settings.model_effect, settings.model_edge_glow);
    const bool player_models = player_settings.player_esp &&
        ModelVisualsEnabled(player_settings.model_effect, player_settings.model_edge_glow);
    const bool animal_models = animal_settings.animal_esp &&
        ModelVisualsEnabled(animal_settings.model_effect, animal_settings.model_edge_glow);
    const bool vehicle_models = vehicle_settings.vehicle_esp &&
        ModelVisualsEnabled(vehicle_settings.model_effect, vehicle_settings.model_edge_glow);
    if (viewpoint_esp &&
        !zombie_models && !player_models && !animal_models && !vehicle_models) {
        ResetViewpointModelChamsStatus("请启用实体透视并选择模型效果");
    }
    if (!g_initialized ||
        (!zombie_models && !player_models && !animal_models && !vehicle_models) ||
        frame.gate_status != bridge::GateStatus::SinglePlayerAllowed) {
        if (viewpoint_esp && !g_initialized &&
            frame.gate_status == bridge::GateStatus::SinglePlayerAllowed) {
            ResetViewpointModelChamsStatus("等待 ViewPoint 3D 模型渲染接口");
        }
        std::lock_guard<std::mutex> command_lock(g_commands_mutex);
        g_commands.clear();
        return;
    }

    std::vector<DrawCommand> replay_commands;
    {
        std::lock_guard<std::mutex> command_lock(g_commands_mutex);
        replay_commands.swap(g_commands);
    }
    if (replay_commands.empty()) return;
    const auto owner_map=BuildModelOwnerMap(frame);

    GLint saved_program = 0;
    GLint saved_vertex_array = 0;
    GLint saved_array_buffer = 0;
    GLint saved_active_texture = 0;
    GLint saved_texture = 0;
    GLint saved_viewport[4]{};
    GLint saved_scissor[4]{};
    GLint saved_blend_src = GL_SRC_ALPHA;
    GLint saved_blend_dst = GL_ONE_MINUS_SRC_ALPHA;
    std::array<GLint, kCapturedSsboBindingLimit> saved_ssbo_bindings{};
    GLboolean saved_depth_mask = GL_TRUE;
    const GLboolean blend_enabled = glIsEnabled(GL_BLEND);
    const GLboolean depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean cull_enabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean alpha_enabled=glIsEnabled(GL_ALPHA_TEST),stencil_enabled=glIsEnabled(GL_STENCIL_TEST),logic_enabled=glIsEnabled(GL_COLOR_LOGIC_OP);
    glGetIntegerv(kCurrentProgram, &saved_program);
    glGetIntegerv(kVertexArrayBinding, &saved_vertex_array);
    glGetIntegerv(kArrayBufferBinding, &saved_array_buffer);
    glGetIntegerv(kActiveTexture, &saved_active_texture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_texture);
    glGetIntegerv(GL_VIEWPORT, saved_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, saved_scissor);
    glGetIntegerv(kBlendSrcRgb, &saved_blend_src);
    glGetIntegerv(kBlendDstRgb, &saved_blend_dst);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &saved_depth_mask);
    for (GLuint binding = 0; binding < g_ssbo_binding_count; ++binding) {
        g_get_integer_i_v(
            kShaderStorageBufferBinding, binding, &saved_ssbo_bindings[binding]);
    }

    struct TextureUnitState {
        GLint unit = 0;
        GLint texture_2d = 0;
        GLint texture_array = 0;
    };
    std::vector<TextureUnitState> saved_texture_units;
    for (const DrawCommand& command : replay_commands) {
        const auto found = std::find_if(
            saved_texture_units.begin(), saved_texture_units.end(),
            [&command](const TextureUnitState& state) {
                return state.unit == command.texture_unit;
            });
        if (found != saved_texture_units.end()) continue;
        TextureUnitState state{};
        state.unit = command.texture_unit;
        g_active_texture(kTexture0 + state.unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture_2d);
        glGetIntegerv(kTextureBinding2DArray, &state.texture_array);
        saved_texture_units.push_back(state);
    }
    g_active_texture(static_cast<GLenum>(saved_active_texture));

    g_replaying = true;
    glDisable(GL_ALPHA_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_COLOR_LOGIC_OP);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);


    const std::array<bool,4> enabled{{zombie_models,player_models,animal_models,vehicle_models}};
    const std::array<ZombieModelEffect,4> effects{{settings.model_effect,player_settings.model_effect,
        animal_settings.model_effect,vehicle_settings.model_effect}};
    const std::array<bool,4> edges{{settings.model_edge_glow,player_settings.model_edge_glow,
        animal_settings.model_edge_glow,vehicle_settings.model_edge_glow}};
    const std::array<ImVec4,4> edge_colours{{settings.model_edge_glow_color,player_settings.model_edge_glow_color,
        animal_settings.model_edge_glow_color,vehicle_settings.model_edge_glow_color}};
    const ImGuiIO& io=ImGui::GetIO();
    const int width=static_cast<int>(std::lround(io.DisplaySize.x*io.DisplayFramebufferScale.x));
    const int height=static_cast<int>(std::lround(io.DisplaySize.y*io.DisplayFramebufferScale.y));
    GLuint self_mask=0;
    const bool has_self=std::any_of(replay_commands.begin(),replay_commands.end(),[](const DrawCommand& command){return command.owner_identity==-1;});
    if(has_self && g_model_self_mask.Begin(width,height)) {
        for(const auto& command:replay_commands)if(command.owner_identity==-1)
            ReplayCapturedModel(command,ImVec4(1,1,1,1),ZombieModelEffect::Solid,nullptr);
        g_model_self_mask.End();self_mask=g_model_self_mask.Texture();
    }
    for(int target=0;target<12;target++) {
        const int group=target/3;
        if(!enabled[group] || effects[group]==ZombieModelEffect::Disabled)continue;
        const auto selected=[&](const DrawCommand& command) {
            return ResolveTrackedEntityColorIndex(command,owner_map)==target &&
                ModelLayerVisible(ResolveModelOwner(command.owner_identity,owner_map),VisualFeature::Fill);
        };
        if(!std::any_of(replay_commands.begin(),replay_commands.end(),selected) || !g_model_fill.Begin(width,height))continue;
        for(std::size_t index=0;index<replay_commands.size();index++) {
            const auto& command=replay_commands[index];if(!selected(command))continue;
            const auto effect=effects[group];
            const NativeModelMaterial::Program* material=nullptr;
            if(target<6 && IsThermalMaterial(effect)) {
                material=g_character_materials.Get(command.program);if(!material)continue;
            }
            ReplayCapturedModel(command,EffectColor(g_capture_fill_colors[target],effect,index),effect,material);
        }
        g_model_fill.End();g_model_fill.Composite(ImVec4(0,0,0,0),true,self_mask);
    }
    for(int target=0;target<12;target++) {
        const int group=target/3;
        if(!enabled[group] || !edges[group] || edge_colours[group].w<=0)continue;
        bool present=false;
        for(const DrawCommand& command:replay_commands)if(ResolveTrackedEntityColorIndex(command,owner_map)==target && ModelLayerVisible(ResolveModelOwner(command.owner_identity,owner_map),VisualFeature::Rim)) { present=true;break; }
        if(!present || !g_model_outline.Begin(width,height))continue;
        for(const DrawCommand& command:replay_commands) {
            if(ResolveTrackedEntityColorIndex(command,owner_map)==target && ModelLayerVisible(ResolveModelOwner(command.owner_identity,owner_map),VisualFeature::Rim))
                ReplayCapturedModel(command,ImVec4(1,1,1,1),ZombieModelEffect::Solid,nullptr);
        }
        g_model_outline.End();
        g_model_outline.Composite(edge_colours[group],false,self_mask);
    }

    for (GLuint binding = 0; binding < g_ssbo_binding_count; ++binding) {
        g_bind_buffer_base(
            kShaderStorageBuffer, binding,
            static_cast<GLuint>(saved_ssbo_bindings[binding]));
    }
    for (const TextureUnitState& state : saved_texture_units) {
        g_active_texture(kTexture0 + state.unit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture_2d));
        glBindTexture(kTexture2DArray, static_cast<GLuint>(state.texture_array));
    }
    g_active_texture(static_cast<GLenum>(saved_active_texture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(saved_texture));
    g_bind_vertex_array(static_cast<GLuint>(saved_vertex_array));
    g_bind_buffer(kArrayBuffer, static_cast<GLuint>(saved_array_buffer));
    g_use_program(static_cast<GLuint>(saved_program));
    g_current_program = saved_program > 0 ? static_cast<GLuint>(saved_program) : 0;
    glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
    glScissor(saved_scissor[0], saved_scissor[1], saved_scissor[2], saved_scissor[3]);
    glBlendFunc(saved_blend_src, saved_blend_dst);
    glDepthMask(saved_depth_mask);
    if (blend_enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depth_enabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (scissor_enabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (cull_enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if(alpha_enabled)glEnable(GL_ALPHA_TEST);else glDisable(GL_ALPHA_TEST);
    if(stencil_enabled)glEnable(GL_STENCIL_TEST);else glDisable(GL_STENCIL_TEST);
    if(logic_enabled)glEnable(GL_COLOR_LOGIC_OP);else glDisable(GL_COLOR_LOGIC_OP);
    g_replaying = false;
}

std::size_t NativeModelChams::CapturedDrawCount() {
    std::lock_guard<std::mutex> command_lock(g_commands_mutex);
    return g_commands.size();
}

NativeModelChamsDiagnostics NativeModelChams::Diagnostics() {
    NativeModelChamsDiagnostics diagnostics{};
    diagnostics.initialization_status = g_initialization_status;
    diagnostics.draw_calls = g_observed_draw_calls;
    diagnostics.instanced_draw_calls = g_observed_instanced_draw_calls;
    diagnostics.matrix_uploads = g_observed_matrix_uploads;
    diagnostics.use_program_calls = g_observed_use_program_calls;
    diagnostics.capture_draw_calls = g_capture_draw_calls;
    diagnostics.capture_draws_without_program =
        g_capture_draws_without_program;
    diagnostics.capture_enable_calls = g_capture_enable_calls;
    diagnostics.capture_enable_true_calls = g_capture_enable_true_calls;
    diagnostics.capture_context_binds = g_capture_context_binds;
    diagnostics.foreign_context_updates_ignored =
        g_foreign_context_updates_ignored;
    diagnostics.foreign_context_draws_ignored =
        g_foreign_context_draws_ignored;
    diagnostics.classified_programs = g_programs.size();
    diagnostics.outline_programs = g_outline_program_count;
    diagnostics.instanced_programs = g_instanced_program_count;
    diagnostics.vehicle_programs = g_vehicle_program_count;
    diagnostics.vehicle_tinted_draw_calls = g_vehicle_tinted_draw_calls;
    JNIEnv* env=bridge::GetCurrentJniEnvironment();
    if(env && g_owned_capture_ready) {
        diagnostics.world_frame_callbacks=env->GetStaticLongField(g_owned_capture_helper,g_owned_frame_count);
        diagnostics.rendered_models=env->GetStaticLongField(g_owned_capture_helper,g_owned_rendered_count);
        diagnostics.queued_models=env->GetStaticIntField(g_owned_capture_helper,g_owned_queued_count);
        if(env->ExceptionCheck())env->ExceptionClear();
    }
    diagnostics.material_error = g_character_materials.error;
    if(!g_owned_capture_error.empty())diagnostics.material_error += " Capture: "+g_owned_capture_error;
    if(!g_model_outline.error.empty())diagnostics.material_error += " Edge: " + g_model_outline.error;
    if(!g_model_fill.error.empty())diagnostics.material_error += " Fill: "+g_model_fill.error;
    if(!g_model_self_mask.error.empty())diagnostics.material_error += " Self mask: "+g_model_self_mask.error;
    return diagnostics;
}

}  // namespace pztrainer::features::visual
