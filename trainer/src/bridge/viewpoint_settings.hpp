#pragma once

#include <string>

namespace pztrainer::bridge {

struct ViewpointSettings {
    bool esp_3d = false;
    bool legit_3d = false;
    bool rage_3d = false;
    // ViewPoint changes the game's projection from 2.5D to 3D. Keep the
    // compatibility bridge enabled by default when the mod is detected.
    bool compatibility_mode = true;
};

struct ViewpointStatus {
    bool detected = false;
    bool ready = false;
    bool mod_enabled = false;
    bool compatibility_mode = true;
    bool camera_valid = false;
    std::string message = "正在检测 ViewPoint Mod";
};

inline bool ViewpointBridgeReady(const ViewpointSettings& settings,
                                 const ViewpointStatus& status) {
    return settings.compatibility_mode && status.detected && status.ready &&
        status.mod_enabled;
}

inline bool CanUseViewpoint3D(const ViewpointSettings& settings,
                              const ViewpointStatus& status) {
    return ViewpointBridgeReady(settings, status) && status.camera_valid;
}

inline bool UseViewpointEsp(const ViewpointSettings& settings,
                            const ViewpointStatus& status) {
    return settings.esp_3d && CanUseViewpoint3D(settings, status);
}

inline bool ShouldDrawEsp(const ViewpointSettings& settings,
                          const ViewpointStatus& status) {
    // A requested 3D overlay must wait for its camera, rather than showing
    // vanilla coordinates on top of an active ViewPoint scene.
    return !settings.esp_3d || !status.mod_enabled ||
        CanUseViewpoint3D(settings, status);
}

ViewpointSettings& GetViewpointSettings();
const ViewpointStatus& GetViewpointStatus();
void SetViewpointCompatibilityMode(bool enabled);

}  // namespace pztrainer::bridge
