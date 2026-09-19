#pragma once

#include <cstdint>
#include <jni.h>

namespace pztrainer::features::aim {

struct RageBallisticsDiagnostics {
    bool initialized = false;
    std::uint64_t calls = 0;
    std::uint64_t overrides = 0;
    std::uint64_t reticle_overrides = 0;
    std::uint64_t target_overrides = 0;
    int last_character_id = -1;
    int published_character_id = -1;
};

bool InitializeRageBallisticsHook();
void SetRageBallisticsOverride(
    int character_id, int target_id, int body_part,
    float target_x, float target_y, float target_z,
    bool target_is_player, bool replace_hit_list);
void ClearRageBallisticsOverride();
RageBallisticsDiagnostics GetRageBallisticsDiagnostics();

}  // namespace pztrainer::features::aim
