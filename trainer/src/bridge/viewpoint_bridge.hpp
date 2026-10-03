#pragma once

#include <jni.h>

#include "bridge/game_snapshot.hpp"
#include "bridge/viewpoint_settings.hpp"

namespace pztrainer::bridge {

void RefreshViewpointCamera(JNIEnv* env, float viewport_width, float viewport_height);
bool IsViewpointLoaded();
bool IsViewpointActive();
bool ProjectViewpointWorldPoint(const WorldPoint& point, ScreenPoint& screen);
FrameSnapshot MakeViewpointFrame(const FrameSnapshot& frame);
bool AimViewpointAt(JNIEnv* env, const WorldPoint& point, float response,
                    float* angle_error_degrees = nullptr);

}  // namespace pztrainer::bridge
