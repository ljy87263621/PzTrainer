#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int SendTrapAnimalFoodRequests(JNIEnv* env, jobject player,
                               const std::string& full_type, int quantity,
                               std::string& detail);

}  // namespace pztrainer::bridge
