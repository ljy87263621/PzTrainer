#pragma once

#include <jni.h>

#include "bridge/game_snapshot.hpp"

namespace pztrainer::bridge {

struct ZombieModelVisualColor {
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;
};

struct ZombieModelVisualRequest {
    bool enabled = false;
    ZombieModelVisualColor normal;
    ZombieModelVisualColor behind_wall;
    ZombieModelVisualColor in_view;
    bool behind_wall_enabled = true;
    bool in_view_enabled = true;
    bool force_visible = false;
};

JNIEnv* GetCurrentJniEnvironment();
const FrameSnapshot& CollectFrameSnapshot(bool collect_zombies, float max_distance,
                                          bool collect_bones,
                                          const ZombieModelVisualRequest& model_visual,
                                          bool collect_players,
                                          float player_max_distance,
                                          bool collect_player_bones,
                                          const ZombieModelVisualRequest& player_model_visual,
                                          bool collect_animals,
                                          float animal_max_distance,
                                          bool collect_animal_bones,
                                          const ZombieModelVisualRequest& animal_model_visual,
                                          bool collect_vehicles,
                                          float vehicle_max_distance,
                                          const ZombieModelVisualRequest& vehicle_model_visual);
const char* GateStatusLabel(GateStatus status);

}  // namespace pztrainer::bridge
