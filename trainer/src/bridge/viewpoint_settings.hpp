#pragma once

namespace pztrainer::bridge {

struct ViewpointSettings {
    bool esp_3d = false;
    bool legit_3d = false;
    bool rage_3d = false;
};

ViewpointSettings& GetViewpointSettings();

}  // namespace pztrainer::bridge
