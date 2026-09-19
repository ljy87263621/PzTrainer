#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

struct MaintenanceTrainingStatus {
    bool active = false;
    int packets_sent = 0;
    float target_xp = 0.0f;
    std::string message;
};

bool StartMaintenanceTraining(float current_xp, float amount);
void StopMaintenanceTraining(const char* message);
void UpdateMaintenanceTraining(JNIEnv* env, jobject player, float current_xp);
const MaintenanceTrainingStatus& GetMaintenanceTrainingStatus();

}  // namespace pztrainer::bridge
