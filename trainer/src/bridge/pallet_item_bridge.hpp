#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueuePalletItemRequests(JNIEnv* env, jobject player,
                            const std::string& full_type, int quantity,
                            std::string& detail);
void UpdatePalletItemBridge(JNIEnv* env, jobject player);
const std::string& GetPalletItemStatus();

}  // namespace pztrainer::bridge
