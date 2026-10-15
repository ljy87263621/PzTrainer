#pragma once
#include <algorithm>
#include <cmath>
#include <regex>
#include <stdexcept>
#include <string>
#include <imgui.h>
#include "features/visual/zombie_model_effect.hpp"

namespace pztrainer::features::visual {
inline bool IsThermalMaterial(ZombieModelEffect effect) {
    return effect == ZombieModelEffect::Thermal || effect == ZombieModelEffect::WhiteHot;
}

// A visual heat distribution in bind-pose space. It follows skinning and does
// not claim to measure the game's character temperature.
inline float ThermalSignal(float x, float y, float z) {
    // The game's MaleBody bind mesh is approximately 0..1 high.
    const float core = (x * x + z * z) * 9.0f + (y - 0.55f) * (y - 0.55f) * 6.0f;
    const float head = (x * x + z * z) * 25.0f + (y - 0.9f) * (y - 0.9f) * 30.0f;
    return std::clamp(0.18f + 0.70f * std::exp(-core * 1.7f) +
        0.45f * std::exp(-head), 0.0f, 1.0f);
}
inline ImVec4 ThermalPalette(float heat, bool white, float alpha) {
    heat = std::clamp(heat, 0.0f, 1.0f);
    if (white) return {heat, heat, heat, alpha};
    const ImVec4 stops[]{{0.04f,0.01f,0.18f,alpha}, {0.95f,0.06f,0.02f,alpha},
        {1.0f,0.85f,0.04f,alpha}, {1.0f,1.0f,0.93f,alpha}};
    const float scaled = heat * 3.0f;
    const int index = std::min(2, static_cast<int>(scaled));
    const float t = scaled - index;
    return {stops[index].x + (stops[index+1].x-stops[index].x)*t,
        stops[index].y + (stops[index+1].y-stops[index].y)*t,
        stops[index].z + (stops[index+1].z-stops[index].z)*t, alpha};
}

// build-viewpoint-chams.ps1 packages this exact body for the Java path.
inline constexpr const char* kThermalGlsl = R"PZTHERMAL(
float pzsaHeat(vec3 p) {
    float core = dot(p.xz,p.xz)*9.0 + (p.y-0.55)*(p.y-0.55)*6.0;
    float head = dot(p.xz,p.xz)*25.0 + (p.y-0.9)*(p.y-0.9)*30.0;
    return clamp(0.18 + 0.70*exp(-core*1.7) + 0.45*exp(-head),0.0,1.0);
}
vec3 pzsaHeatColour(vec3 p, int mode) {
    float heat = pzsaHeat(p);
    if (mode == 2) return vec3(heat);
    vec3 cold = vec3(0.04,0.01,0.18), red = vec3(0.95,0.06,0.02);
    vec3 yellow = vec3(1.0,0.85,0.04), white = vec3(1.0,1.0,0.93);
    float t = heat*3.0;
    if (t < 1.0) return mix(cold,red,t);
    if (t < 2.0) return mix(red,yellow,t-1.0);
    return mix(yellow,white,t-2.0);
}
)PZTHERMAL";

struct ThermalSources { std::string vertex, fragment; };
inline std::string RenameMaterialMain(const std::string& source) {
    static const std::regex main_pattern(R"(void\s+main\s*\(\s*(?:void\s*)?\))");
    if (!std::regex_search(source, main_pattern)) throw std::runtime_error("Model shader has no main function");
    return std::regex_replace(source, main_pattern, "void pzsaOriginalMain()",
        std::regex_constants::format_first_only);
}
inline ThermalSources BuildThermalSources(const std::string& vertex,
        const std::string& fragment, const std::string& position) {
    std::string output;
    if (fragment.find("gl_FragColor") != std::string::npos) output = "gl_FragColor";
    else {
        std::smatch match;
        if (!std::regex_search(fragment, match, std::regex(R"(out\s+vec4\s+(\w+)\s*;)")))
            throw std::runtime_error("Unsupported model fragment output");
        output = match[1].str();
    }
    return {RenameMaterialMain(vertex) + "\nout vec3 pzsaSurfacePosition;\nvoid main(){pzsaOriginalMain();pzsaSurfacePosition=" + position + ".xyz;}\n",
        RenameMaterialMain(fragment) + "\nin vec3 pzsaSurfacePosition;\nuniform int pzsaThermalMode;\n" + kThermalGlsl +
        "\nvoid main(){pzsaOriginalMain();" + output + ".rgb=pzsaHeatColour(pzsaSurfacePosition,pzsaThermalMode);}\n"};
}
}
