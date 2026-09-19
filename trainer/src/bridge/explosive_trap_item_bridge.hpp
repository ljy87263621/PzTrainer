#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueueExplosiveTrapWeaponRequests(JNIEnv* env, jobject player,
                                     const std::string& full_type, int quantity,
                                     std::string& detail);
int QueueExplosiveTrapWeaponPartRequests(JNIEnv* env, jobject player,
                                         const std::string& full_type,
                                         int quantity, std::string& detail);
void UpdateExplosiveTrapWeaponBridge(JNIEnv* env, jobject player);
const std::string& GetExplosiveTrapWeaponStatus();
const std::string& GetExplosiveTrapWeaponPartStatus();

}  // namespace pztrainer::bridge
