#pragma once

#include <algorithm>
#include <cmath>

namespace launcher::ui {

inline float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float EaseOutCubic(float value) {
    const float inverse = 1.0f - Clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

inline float EaseOutBack(float value) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float x = Clamp01(value) - 1.0f;
    return 1.0f + c3 * x * x * x + c1 * x * x;
}

inline float Approach(float current, float target, float speed, float delta_seconds) {
    const float weight = 1.0f - std::exp(-speed * delta_seconds);
    return current + (target - current) * weight;
}

inline float Stagger(float elapsed, float delay, float duration = 0.52f) {
    return EaseOutCubic((elapsed - delay) / duration);
}

}  // namespace launcher::ui
