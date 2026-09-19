#pragma once

#include <jni.h>

#include <string>
#include <utility>
#include <vector>

namespace pztrainer::bridge {

int QueueCorpsePayloadRequests(JNIEnv* env, jobject player,
                               const std::vector<std::pair<std::string, int>>& items,
                               std::string& detail);
void UpdateCorpsePayloadBridge(JNIEnv* env, jobject player);
const std::string& GetCorpsePayloadStatus();

}  // namespace pztrainer::bridge
