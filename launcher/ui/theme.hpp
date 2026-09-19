#pragma once

#include <d2d1.h>

namespace launcher::ui::theme {

inline constexpr D2D1_COLOR_F Background{0.008f, 0.031f, 0.055f, 1.0f};
inline constexpr D2D1_COLOR_F Panel{0.010f, 0.043f, 0.073f, 1.0f};
inline constexpr D2D1_COLOR_F PanelRaised{0.014f, 0.058f, 0.096f, 1.0f};
inline constexpr D2D1_COLOR_F Border{0.020f, 0.145f, 0.220f, 1.0f};
inline constexpr D2D1_COLOR_F BorderBright{0.020f, 0.255f, 0.380f, 1.0f};
inline constexpr D2D1_COLOR_F Accent{0.025f, 0.650f, 0.925f, 1.0f};
inline constexpr D2D1_COLOR_F AccentPressed{0.020f, 0.475f, 0.720f, 1.0f};
inline constexpr D2D1_COLOR_F Text{0.925f, 0.955f, 0.980f, 1.0f};
inline constexpr D2D1_COLOR_F Muted{0.590f, 0.690f, 0.760f, 1.0f};
inline constexpr D2D1_COLOR_F Dimmed{0.360f, 0.445f, 0.505f, 1.0f};
inline constexpr D2D1_COLOR_F Success{0.235f, 0.820f, 0.540f, 1.0f};
inline constexpr D2D1_COLOR_F Danger{0.930f, 0.340f, 0.390f, 1.0f};

}  // namespace launcher::ui::theme
