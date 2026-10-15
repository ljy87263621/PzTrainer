#pragma once
#include "features/visual/zombie_model_effect.hpp"
namespace pztrainer::features::visual {
constexpr bool ModelVisualsEnabled(ZombieModelEffect effect,bool edge) {
    return effect!=ZombieModelEffect::Disabled || edge;
}
inline void SetModelFillEnabled(ZombieModelEffect& effect,ZombieModelEffect& remembered,bool enabled) {
    if(enabled)effect=remembered==ZombieModelEffect::Disabled?ZombieModelEffect::Shaded:remembered;
    else {
        if(effect!=ZombieModelEffect::Disabled)remembered=effect;
        effect=ZombieModelEffect::Disabled;
    }
}
}
