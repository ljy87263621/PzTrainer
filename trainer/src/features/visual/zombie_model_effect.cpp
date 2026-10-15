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
        case ZombieModelEffect::Thermal: return "彩色热能";
        case ZombieModelEffect::WhiteHot: return "白热";
        case ZombieModelEffect::Count: return "Disabled";
    }
    return "Disabled";
}

}  // namespace pztrainer::features::visual
