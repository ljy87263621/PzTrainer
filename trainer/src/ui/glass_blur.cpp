#include "ui/glass_blur.hpp"

#include <Windows.h>
#include <gl/GL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif

namespace pztrainer::ui {
namespace {

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA8 0x8058
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_ARRAY_BUFFER 0x8892
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_STATIC_DRAW 0x88E4
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif

using GenFramebuffersFn = void(APIENTRYP)(GLsizei, GLuint*);
using BindFramebufferFn = void(APIENTRYP)(GLenum, GLuint);
using FramebufferTexture2DFn = void(APIENTRYP)(GLenum, GLenum, GLenum, GLuint, GLint);
using CheckFramebufferStatusFn = GLenum(APIENTRYP)(GLenum);
using DeleteFramebuffersFn = void(APIENTRYP)(GLsizei, const GLuint*);
using CreateShaderFn = GLuint(APIENTRYP)(GLenum);
using ShaderSourceFn = void(APIENTRYP)(GLuint, GLsizei, const char* const*, const GLint*);
using CompileShaderFn = void(APIENTRYP)(GLuint);
using GetShaderivFn = void(APIENTRYP)(GLuint, GLenum, GLint*);
using GetShaderInfoLogFn = void(APIENTRYP)(GLuint, GLsizei, GLsizei*, char*);
using DeleteShaderFn = void(APIENTRYP)(GLuint);
using CreateProgramFn = GLuint(APIENTRYP)();
using AttachShaderFn = void(APIENTRYP)(GLuint, GLuint);
using LinkProgramFn = void(APIENTRYP)(GLuint);
using GetProgramivFn = void(APIENTRYP)(GLuint, GLenum, GLint*);
using GetProgramInfoLogFn = void(APIENTRYP)(GLuint, GLsizei, GLsizei*, char*);
using DeleteProgramFn = void(APIENTRYP)(GLuint);
using UseProgramFn = void(APIENTRYP)(GLuint);
using GetUniformLocationFn = GLint(APIENTRYP)(GLuint, const char*);
using Uniform1iFn = void(APIENTRYP)(GLint, GLint);
using Uniform2fFn = void(APIENTRYP)(GLint, GLfloat, GLfloat);
using ActiveTextureFn = void(APIENTRYP)(GLenum);
using BindAttribLocationFn = void(APIENTRYP)(GLuint, GLuint, const char*);
using GenVertexArraysFn = void(APIENTRYP)(GLsizei, GLuint*);
using BindVertexArrayFn = void(APIENTRYP)(GLuint);
using GenBuffersFn = void(APIENTRYP)(GLsizei, GLuint*);
using BindBufferFn = void(APIENTRYP)(GLenum, GLuint);
using BufferDataFn = void(APIENTRYP)(GLenum, std::ptrdiff_t, const void*, GLenum);
using EnableVertexAttribArrayFn = void(APIENTRYP)(GLuint);
using VertexAttribPointerFn = void(APIENTRYP)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

GenFramebuffersFn g_gen_framebuffers = nullptr;
BindFramebufferFn g_bind_framebuffer = nullptr;
FramebufferTexture2DFn g_framebuffer_texture_2d = nullptr;
CheckFramebufferStatusFn g_check_framebuffer_status = nullptr;
DeleteFramebuffersFn g_delete_framebuffers = nullptr;
CreateShaderFn g_create_shader = nullptr;
ShaderSourceFn g_shader_source = nullptr;
CompileShaderFn g_compile_shader = nullptr;
GetShaderivFn g_get_shader_iv = nullptr;
GetShaderInfoLogFn g_get_shader_info_log = nullptr;
DeleteShaderFn g_delete_shader = nullptr;
CreateProgramFn g_create_program = nullptr;
AttachShaderFn g_attach_shader = nullptr;
LinkProgramFn g_link_program = nullptr;
GetProgramivFn g_get_program_iv = nullptr;
GetProgramInfoLogFn g_get_program_info_log = nullptr;
DeleteProgramFn g_delete_program = nullptr;
UseProgramFn g_use_program = nullptr;
GetUniformLocationFn g_get_uniform_location = nullptr;
Uniform1iFn g_uniform_1i = nullptr;
Uniform2fFn g_uniform_2f = nullptr;
ActiveTextureFn g_active_texture = nullptr;
BindAttribLocationFn g_bind_attrib_location = nullptr;
GenVertexArraysFn g_gen_vertex_arrays = nullptr;
BindVertexArrayFn g_bind_vertex_array = nullptr;
GenBuffersFn g_gen_buffers = nullptr;
BindBufferFn g_bind_buffer = nullptr;
BufferDataFn g_buffer_data = nullptr;
EnableVertexAttribArrayFn g_enable_vertex_attrib_array = nullptr;
VertexAttribPointerFn g_vertex_attrib_pointer = nullptr;

GLuint g_program = 0;
GLuint g_vertex_array = 0;
GLuint g_vertex_buffer = 0;
GLuint g_source_texture = 0;
GLuint g_blur_textures[2]{};
GLuint g_framebuffers[2]{};
GLint g_texture_uniform = -1;
GLint g_direction_uniform = -1;
int g_source_width = 0;
int g_source_height = 0;
int g_blur_width = 0;
int g_blur_height = 0;
bool g_initialized = false;
std::string g_error;

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

template <typename Function>
bool Resolve(Function& function, const char* name) {
    function = reinterpret_cast<Function>(ResolveOpenGlFunction(name));
    if (function != nullptr) return true;
    g_error = std::string("missing OpenGL function: ") + name;
    return false;
}

GLuint CompileShader(GLenum type, const char* source) {
    const GLuint shader = g_create_shader(type);
    g_shader_source(shader, 1, &source, nullptr);
    g_compile_shader(shader);
    GLint compiled = GL_FALSE;
    g_get_shader_iv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;

    GLint length = 0;
    g_get_shader_iv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    g_get_shader_info_log(shader, length, nullptr, log.data());
    g_error = "glass blur shader compilation failed: " + log;
    g_delete_shader(shader);
    return 0;
}

bool CreateProgram() {
    constexpr const char* vertex_source = R"GLSL(
#version 130
varying vec2 TextureCoordinate;
attribute vec2 Position;
attribute vec2 TextureInput;
void main() {
    gl_Position = vec4(Position, 0.0, 1.0);
    TextureCoordinate = TextureInput;
}
)GLSL";
    constexpr const char* fragment_source = R"GLSL(
#version 130
uniform sampler2D SourceTexture;
uniform vec2 Direction;
varying vec2 TextureCoordinate;
void main() {
    vec4 color = texture2D(SourceTexture, TextureCoordinate) * 0.2270270270;
    color += texture2D(SourceTexture, TextureCoordinate + Direction * 1.3846153846) * 0.3162162162;
    color += texture2D(SourceTexture, TextureCoordinate - Direction * 1.3846153846) * 0.3162162162;
    color += texture2D(SourceTexture, TextureCoordinate + Direction * 3.2307692308) * 0.0702702703;
    color += texture2D(SourceTexture, TextureCoordinate - Direction * 3.2307692308) * 0.0702702703;
    gl_FragColor = color;
}
)GLSL";

    const GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertex_source);
    if (vertex == 0) return false;
    const GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment == 0) {
        g_delete_shader(vertex);
        return false;
    }

    g_program = g_create_program();
    g_attach_shader(g_program, vertex);
    g_attach_shader(g_program, fragment);
    g_bind_attrib_location(g_program, 0, "Position");
    g_bind_attrib_location(g_program, 1, "TextureInput");
    g_link_program(g_program);
    g_delete_shader(vertex);
    g_delete_shader(fragment);

    GLint linked = GL_FALSE;
    g_get_program_iv(g_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        g_get_program_iv(g_program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        g_get_program_info_log(g_program, length, nullptr, log.data());
        g_error = "glass blur program link failed: " + log;
        g_delete_program(g_program);
        g_program = 0;
        return false;
    }

    g_texture_uniform = g_get_uniform_location(g_program, "SourceTexture");
    g_direction_uniform = g_get_uniform_location(g_program, "Direction");
    return g_texture_uniform >= 0 && g_direction_uniform >= 0;
}

void CreateGeometry() {
    constexpr float vertices[]{
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
    };
    GLint previous_vertex_array = 0;
    GLint previous_vertex_buffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vertex_array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_vertex_buffer);

    g_gen_vertex_arrays(1, &g_vertex_array);
    g_bind_vertex_array(g_vertex_array);
    g_gen_buffers(1, &g_vertex_buffer);
    g_bind_buffer(GL_ARRAY_BUFFER, g_vertex_buffer);
    g_buffer_data(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    g_enable_vertex_attrib_array(0);
    g_vertex_attrib_pointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
    g_enable_vertex_attrib_array(1);
    g_vertex_attrib_pointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                            reinterpret_cast<const void*>(sizeof(float) * 2));

    g_bind_buffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_vertex_buffer));
    g_bind_vertex_array(static_cast<GLuint>(previous_vertex_array));
}

void DeleteRenderTargets() {
    if (g_source_texture != 0) glDeleteTextures(1, &g_source_texture);
    if (g_blur_textures[0] != 0) glDeleteTextures(2, g_blur_textures);
    if (g_framebuffers[0] != 0) g_delete_framebuffers(2, g_framebuffers);
    g_source_texture = 0;
    g_blur_textures[0] = 0;
    g_blur_textures[1] = 0;
    g_framebuffers[0] = 0;
    g_framebuffers[1] = 0;
}

void ConfigureTexture(GLuint texture, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
}

bool EnsureRenderTargets(int source_width, int source_height) {
    const int blur_width = std::max(source_width / 6, 1);
    const int blur_height = std::max(source_height / 6, 1);
    if (g_source_texture != 0 && source_width == g_source_width &&
        source_height == g_source_height && blur_width == g_blur_width &&
        blur_height == g_blur_height) {
        return true;
    }

    DeleteRenderTargets();
    g_source_width = source_width;
    g_source_height = source_height;
    g_blur_width = blur_width;
    g_blur_height = blur_height;

    glGenTextures(1, &g_source_texture);
    ConfigureTexture(g_source_texture, source_width, source_height);
    glGenTextures(2, g_blur_textures);
    ConfigureTexture(g_blur_textures[0], blur_width, blur_height);
    ConfigureTexture(g_blur_textures[1], blur_width, blur_height);

    g_gen_framebuffers(2, g_framebuffers);
    for (int index = 0; index < 2; ++index) {
        g_bind_framebuffer(GL_FRAMEBUFFER, g_framebuffers[index]);
        g_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, g_blur_textures[index], 0);
        if (g_check_framebuffer_status(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            g_error = "glass blur framebuffer is incomplete";
            g_bind_framebuffer(GL_FRAMEBUFFER, 0);
            DeleteRenderTargets();
            return false;
        }
    }
    return true;
}

void SetEnabled(GLenum capability, bool enabled) {
    if (enabled) glEnable(capability);
    else glDisable(capability);
}

void RenderPass(GLuint source, GLuint target_framebuffer, float direction_x,
                float direction_y) {
    g_bind_framebuffer(GL_FRAMEBUFFER, target_framebuffer);
    glViewport(0, 0, g_blur_width, g_blur_height);
    g_use_program(g_program);
    g_active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    g_uniform_1i(g_texture_uniform, 0);
    g_uniform_2f(g_direction_uniform, direction_x, direction_y);

    g_bind_vertex_array(g_vertex_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

}  // namespace

bool InitializeGlassBlur() {
    if (g_initialized) return true;
    g_error.clear();
    if (!Resolve(g_gen_framebuffers, "glGenFramebuffers") ||
        !Resolve(g_bind_framebuffer, "glBindFramebuffer") ||
        !Resolve(g_framebuffer_texture_2d, "glFramebufferTexture2D") ||
        !Resolve(g_check_framebuffer_status, "glCheckFramebufferStatus") ||
        !Resolve(g_delete_framebuffers, "glDeleteFramebuffers") ||
        !Resolve(g_create_shader, "glCreateShader") ||
        !Resolve(g_shader_source, "glShaderSource") ||
        !Resolve(g_compile_shader, "glCompileShader") ||
        !Resolve(g_get_shader_iv, "glGetShaderiv") ||
        !Resolve(g_get_shader_info_log, "glGetShaderInfoLog") ||
        !Resolve(g_delete_shader, "glDeleteShader") ||
        !Resolve(g_create_program, "glCreateProgram") ||
        !Resolve(g_attach_shader, "glAttachShader") ||
        !Resolve(g_link_program, "glLinkProgram") ||
        !Resolve(g_get_program_iv, "glGetProgramiv") ||
        !Resolve(g_get_program_info_log, "glGetProgramInfoLog") ||
        !Resolve(g_delete_program, "glDeleteProgram") ||
        !Resolve(g_use_program, "glUseProgram") ||
        !Resolve(g_get_uniform_location, "glGetUniformLocation") ||
        !Resolve(g_uniform_1i, "glUniform1i") ||
        !Resolve(g_uniform_2f, "glUniform2f") ||
        !Resolve(g_active_texture, "glActiveTexture") ||
        !Resolve(g_bind_attrib_location, "glBindAttribLocation") ||
        !Resolve(g_gen_vertex_arrays, "glGenVertexArrays") ||
        !Resolve(g_bind_vertex_array, "glBindVertexArray") ||
        !Resolve(g_gen_buffers, "glGenBuffers") ||
        !Resolve(g_bind_buffer, "glBindBuffer") ||
        !Resolve(g_buffer_data, "glBufferData") ||
        !Resolve(g_enable_vertex_attrib_array, "glEnableVertexAttribArray") ||
        !Resolve(g_vertex_attrib_pointer, "glVertexAttribPointer")) {
        return false;
    }
    g_initialized = CreateProgram();
    if (!g_initialized && g_error.empty()) {
        g_error = "glass blur uniforms were unavailable";
    }
    if (g_initialized) CreateGeometry();
    return g_initialized;
}

void PrepareGlassBlur() {
    if (!g_initialized) return;

    GLint viewport[4]{};
    GLint previous_framebuffer = 0;
    GLint previous_program = 0;
    GLint previous_active_texture = 0;
    GLint previous_texture = 0;
    GLint previous_vertex_array = 0;
    GLint previous_vertex_buffer = 0;
    GLint polygon_mode[2]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    g_active_texture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_POLYGON_MODE, polygon_mode);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vertex_array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_vertex_buffer);
    const bool blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;
    const bool cull_enabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    const bool depth_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    const bool scissor_enabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    const bool srgb_enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;

    if (!EnsureRenderTargets(viewport[2], viewport[3])) {
        g_bind_framebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
        g_use_program(static_cast<GLuint>(previous_program));
        g_bind_vertex_array(static_cast<GLuint>(previous_vertex_array));
        g_bind_buffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_vertex_buffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        g_active_texture(static_cast<GLenum>(previous_active_texture));
        return;
    }

    g_bind_framebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    glBindTexture(GL_TEXTURE_2D, g_source_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1],
                        viewport[2], viewport[3]);

    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const float texel_x = 1.0f / static_cast<float>(g_blur_width);
    const float texel_y = 1.0f / static_cast<float>(g_blur_height);
    RenderPass(g_source_texture, g_framebuffers[0], texel_x, 0.0f);
    RenderPass(g_blur_textures[0], g_framebuffers[1], 0.0f, texel_y);
    RenderPass(g_blur_textures[1], g_framebuffers[0], texel_x, 0.0f);
    RenderPass(g_blur_textures[0], g_framebuffers[1], 0.0f, texel_y);

    g_bind_framebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    g_use_program(static_cast<GLuint>(previous_program));
    g_bind_vertex_array(static_cast<GLuint>(previous_vertex_array));
    g_bind_buffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_vertex_buffer));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    g_active_texture(static_cast<GLenum>(previous_active_texture));
    glPolygonMode(GL_FRONT, static_cast<GLenum>(polygon_mode[0]));
    glPolygonMode(GL_BACK, static_cast<GLenum>(polygon_mode[1]));
    SetEnabled(GL_BLEND, blend_enabled);
    SetEnabled(GL_CULL_FACE, cull_enabled);
    SetEnabled(GL_DEPTH_TEST, depth_enabled);
    SetEnabled(GL_SCISSOR_TEST, scissor_enabled);
    SetEnabled(GL_FRAMEBUFFER_SRGB, srgb_enabled);
}

void DrawGlassPanel(ImDrawList* draw_list, const ImVec2& minimum,
                    const ImVec2& maximum, float rounding) {
    if (!g_initialized || g_blur_textures[1] == 0 || draw_list == nullptr) return;

    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    if (display_size.x <= 0.0f || display_size.y <= 0.0f) return;
    const ImVec2 uv_minimum(
        minimum.x / display_size.x,
        1.0f - minimum.y / display_size.y);
    const ImVec2 uv_maximum(
        maximum.x / display_size.x,
        1.0f - maximum.y / display_size.y);
    const float alpha = ImGui::GetStyle().Alpha;
    draw_list->AddImageRounded(
        ImTextureRef(static_cast<ImTextureID>(g_blur_textures[1])),
        minimum, maximum, uv_minimum, uv_maximum,
        ImGui::GetColorU32(ImVec4(0.62f, 0.76f, 0.92f, 0.50f * alpha)),
        rounding);
    draw_list->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(0.010f, 0.034f, 0.058f, 0.64f * alpha)),
        rounding);
}

const char* GlassBlurError() {
    return g_error.c_str();
}

}  // namespace pztrainer::ui
