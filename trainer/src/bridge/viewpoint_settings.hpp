#pragma once

#include <string>

namespace pztrainer::bridge {

struct ViewpointSettings {
    // Legacy serialized slots. Keep their order/size for existing profiles;
    // projection is now chosen from the active renderer, not these flags.
    bool esp_3d = false;
    bool legit_3d = false;
    bool rage_3d = false;
    // Former opt-in flag; adaptation is unconditional for an active renderer.
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
    (void)settings;
    return status.detected && status.ready &&
        status.mod_enabled;
}

inline bool CanUseViewpoint3D(const ViewpointSettings& settings,
                              const ViewpointStatus& status) {
    return ViewpointBridgeReady(settings, status) && status.camera_valid;
}

inline bool UseViewpointEsp(const ViewpointSettings& settings,
                            const ViewpointStatus& status) {
    return CanUseViewpoint3D(settings, status);
}

inline bool ShouldDrawEsp(const ViewpointSettings& settings,
                          const ViewpointStatus& status) {
    // An active 3D renderer must wait for its camera.
    return !status.mod_enabled ||
        CanUseViewpoint3D(settings, status);
}

ViewpointSettings& GetViewpointSettings();
const ViewpointStatus& GetViewpointStatus();
void SetViewpointCompatibilityMode(bool enabled);

}  // namespace pztrainer::bridge
