#include "bridge/world_visibility_render_bridge.hpp"

#include <Windows.h>
#include <GL/gl.h>

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace pztrainer::bridge {
namespace {

using GetUniformLocationFn = GLint(APIENTRY*)(GLuint, const char*);
using GetAttribLocationFn = GLint(APIENTRY*)(GLuint, const char*);

std::atomic_bool g_suppress_unexplored_mask{false};
GetUniformLocationFn g_get_uniform_location = nullptr;
GetAttribLocationFn g_get_attrib_location = nullptr;
std::unordered_map<GLuint, bool> g_visibility_programs;

void* ResolveOpenGlFunction(const char* name) {
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || value <= 3 ||
        value == static_cast<std::uintptr_t>(-1)) {
        const HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
        address = opengl == nullptr
            ? nullptr
            : reinterpret_cast<void*>(GetProcAddress(opengl, name));
    }
    return address;
}

bool IsVisibilityPolygonProgram(GLuint program) {
    const auto found = g_visibility_programs.find(program);
    if (found != g_visibility_programs.end()) return found->second;
    if (g_get_uniform_location == nullptr) {
        g_get_uniform_location = reinterpret_cast<GetUniformLocationFn>(
            ResolveOpenGlFunction("glGetUniformLocation"));
        g_get_attrib_location = reinterpret_cast<GetAttribLocationFn>(
            ResolveOpenGlFunction("glGetAttribLocation"));
    }
    const bool result = g_get_uniform_location != nullptr &&
        g_get_attrib_location != nullptr &&
        g_get_uniform_location(program, "ModelViewProjection") >= 0 &&
        g_get_attrib_location(program, "aPosition") >= 0 &&
        g_get_attrib_location(program, "aDist") >= 0 &&
        g_get_attrib_location(program, "aDepth") >= 0;
    g_visibility_programs.emplace(program, result);
    return result;
}

}  // namespace

void SetUnexploredMaskSuppressed(bool enabled) {
    g_suppress_unexplored_mask.store(enabled, std::memory_order_relaxed);
}

bool ShouldSuppressVisibilityPolygon(unsigned int program) {
    return program != 0 &&
        g_suppress_unexplored_mask.load(std::memory_order_relaxed) &&
        IsVisibilityPolygonProgram(static_cast<GLuint>(program));
}

}  // namespace pztrainer::bridge
