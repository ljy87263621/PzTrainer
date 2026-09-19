#pragma once

#include <string>

namespace pztrainer::bridge {

enum class WeatherVisualMode {
    FollowGame,
    ForceOff,
    ForceOn,
};

enum class PrecipitationVisualMode {
    FollowGame,
    ForceOff,
    Rain,
    Snow,
};

struct WorldVisualStatus {
    bool initialized = false;
    bool world_ready = false;
    bool fake_daylight_enabled = false;
    bool dark_area_brightness_enabled = false;
    bool force_360_vision_enabled = false;
    bool reveal_unexplored_enabled = false;
    WeatherVisualMode fog_mode = WeatherVisualMode::FollowGame;
    PrecipitationVisualMode precipitation_mode =
        PrecipitationVisualMode::FollowGame;
    float fog_intensity = 0.75f;
    float precipitation_intensity = 0.75f;
    std::string message = "等待进入存档";
};

void UpdateWorldVisualBridge(bool world_ready);
const WorldVisualStatus& GetWorldVisualStatus();
void SetFakeDaylightEnabled(bool enabled);
void SetDarkAreaBrightnessEnabled(bool enabled);
void SetForce360VisionEnabled(bool enabled);
void SetRevealUnexploredEnabled(bool enabled);
void SetFogVisualMode(WeatherVisualMode mode);
void SetPrecipitationVisualMode(PrecipitationVisualMode mode);
void SetFogVisualIntensity(float intensity);
void SetPrecipitationVisualIntensity(float intensity);

}  // namespace pztrainer::bridge
