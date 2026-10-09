#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueueMediaItemPayload(JNIEnv* env, jobject player,
                          const std::string& full_type, int quantity,
                          std::string& detail);
void UpdateMediaItemPayloadBridge(JNIEnv* env, jobject player);
const std::string& GetMediaItemPayloadStatus();

}  // namespace pztrainer::bridge
