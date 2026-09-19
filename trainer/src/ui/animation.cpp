#include "ui/animation.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace pztrainer::ui::animation {
namespace {

struct SpringState {
    float value = 0.0f;
    float velocity = 0.0f;
    bool initialized = false;
};

std::unordered_map<ImGuiID, SpringState> g_springs;

}  // namespace

float DeltaTime() {
    return std::min(ImGui::GetIO().DeltaTime, 1.0f / 20.0f);
}

void SpringValue(float& value, float& velocity, float target, float stiffness, float damping) {
    const float delta_time = DeltaTime();
    velocity += (target - value) * stiffness * delta_time;
    velocity *= std::exp(-damping * delta_time);
    value += velocity * delta_time;

    if (std::abs(target - value) < 0.0005f && std::abs(velocity) < 0.0005f) {
        value = target;
        velocity = 0.0f;
    }
}

float Spring(ImGuiID id, float target, float stiffness, float damping) {
    SpringState& state = g_springs[id];
    if (!state.initialized) {
        state.value = target;
        state.initialized = true;
    }
    SpringValue(state.value, state.velocity, target, stiffness, damping);
    return state.value;
}

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float from, float to, float amount) {
    return from + (to - from) * amount;
}

ImVec4 Lerp(const ImVec4& from, const ImVec4& to, float amount) {
    return ImVec4(
        Lerp(from.x, to.x, amount),
        Lerp(from.y, to.y, amount),
        Lerp(from.z, to.z, amount),
        Lerp(from.w, to.w, amount));
}

}  // namespace pztrainer::ui::animation
