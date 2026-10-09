#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueueLightItemPayload(JNIEnv* env, jobject player,
                          const std::string& full_type, int quantity,
                          std::string& detail);
void UpdateLightItemPayloadBridge(JNIEnv* env, jobject player);
const std::string& GetLightItemPayloadStatus();

}  // namespace pztrainer::bridge
