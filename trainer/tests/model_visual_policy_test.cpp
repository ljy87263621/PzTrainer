#include <cassert>
#include <cstdlib>
#include "features/visual/model_visual_policy.hpp"
using namespace pztrainer::features::visual;
int main() {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    assert(!ModelVisualsEnabled(ZombieModelEffect::Disabled,false));
    assert(ModelVisualsEnabled(ZombieModelEffect::Disabled,true));
    assert(ModelVisualsEnabled(ZombieModelEffect::Thermal,false));
    assert(ModelVisualsEnabled(ZombieModelEffect::Thermal,true));
    ZombieModelEffect effect=ZombieModelEffect::Thermal, remembered=ZombieModelEffect::Shaded;
    SetModelFillEnabled(effect,remembered,false);
    assert(effect==ZombieModelEffect::Disabled && remembered==ZombieModelEffect::Thermal);
    SetModelFillEnabled(effect,remembered,true);
    assert(effect==ZombieModelEffect::Thermal);
}
