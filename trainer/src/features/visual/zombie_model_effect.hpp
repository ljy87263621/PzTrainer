#pragma once

namespace pztrainer::features::visual {

enum class ZombieModelEffect {
    Disabled,
    Shaded,
    Solid,
    Glow,
    GlowOutline,
    Iridescent,
    WaterFlow,
    Glossy,
};

const char* ZombieModelEffectLabel(ZombieModelEffect effect);

}  // namespace pztrainer::features::visual
