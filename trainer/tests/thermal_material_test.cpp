#include <cassert>
#include <cmath>
#include <string>
#include "features/visual/thermal_material.hpp"

using namespace pztrainer::features::visual;
int main() {
    static_assert(static_cast<int>(ZombieModelEffect::Glossy) == 7);
    static_assert(static_cast<int>(ZombieModelEffect::Thermal) == 8);
    static_assert(static_cast<int>(ZombieModelEffect::WhiteHot) == 9);
    assert(IsThermalMaterial(ZombieModelEffect::Thermal));
    assert(!IsThermalMaterial(ZombieModelEffect::Solid));
    const float core = ThermalSignal(0, 0.9f, 0);
    const float extremity = ThermalSignal(0.8f, 0.1f, 0);
    assert(core > extremity + 0.2f);
    assert(ThermalSignal(0,0.55f,0) > ThermalSignal(0,0.1f,0) + 0.25f);
    const auto hot = ThermalPalette(core, false, 0.4f);
    const auto cold = ThermalPalette(extremity, false, 0.4f);
    assert(hot.y > cold.y && hot.w == 0.4f);
    const auto white = ThermalPalette(core, true, 0.3f);
    assert(white.x == white.y && white.y == white.z && white.w == 0.3f);
    const std::string vertex = "#version 330\nlayout(location=0) in vec4 vertex; void main() { gl_Position=vertex; }";
    const std::string fragment = "#version 330\nuniform vec4 u_color; void main(){if(u_color.a<0.1)discard; gl_FragColor=u_color;}";
    const auto sources = BuildThermalSources(vertex, fragment, "vertex");
    assert(sources.vertex.find("pzsaOriginalMain") != std::string::npos);
    assert(sources.vertex.find("pzsaSurfacePosition=vertex.xyz") != std::string::npos);
    assert(sources.fragment.find("discard") != std::string::npos);
    assert(sources.fragment.find("gl_FragColor.rgb=") != std::string::npos);
    assert(sources.fragment.find("pzsaThermalMode") != std::string::npos);
    bool rejected = false;
    try { BuildThermalSources("invalid", fragment, "vertex"); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
}
