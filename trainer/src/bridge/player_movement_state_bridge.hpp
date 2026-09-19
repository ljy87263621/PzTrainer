#pragma once

#include <jni.h>

#include <cstdint>
#include <string>

namespace pztrainer::bridge {

struct PlayerMovementStateStatus {
    bool initialized = false;
    bool state_declared = false;
    bool ready_for_movement = false;
    std::uint64_t sent_count = 0;
    std::string message = "攀窗网络状态桥接尚未初始化";
};

void UpdatePlayerMovementStateBridge(
    JNIEnv* env, jobject player, bool enabled);
const PlayerMovementStateStatus& GetPlayerMovementStateStatus();

}  // namespace pztrainer::bridge
