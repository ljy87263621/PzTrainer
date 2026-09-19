#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueueWeaponAmmoExtraction(JNIEnv* env, jobject player,
                              const std::string& ammo_full_type, int quantity,
                              std::string& detail);
void UpdateWeaponAmmoExtraction(JNIEnv* env, jobject player);
const std::string& GetWeaponAmmoExtractionStatus();

}  // namespace pztrainer::bridge
