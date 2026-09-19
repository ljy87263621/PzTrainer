#include "features/visual/zombie_model_effect.hpp"

namespace pztrainer::features::visual {

const char* ZombieModelEffectLabel(ZombieModelEffect effect) {
    switch (effect) {
        case ZombieModelEffect::Disabled: return "Disabled";
        case ZombieModelEffect::Shaded: return "Shaded";
        case ZombieModelEffect::Solid: return "Solid";
        case ZombieModelEffect::Glow: return "Glow";
        case ZombieModelEffect::GlowOutline: return "Glow Outline";
        case ZombieModelEffect::Iridescent: return "Iridescent";
        case ZombieModelEffect::WaterFlow: return "Water Flow";
        case ZombieModelEffect::Glossy: return "Glossy";
    }
    return "Disabled";
}

}  // namespace pztrainer::features::visual
