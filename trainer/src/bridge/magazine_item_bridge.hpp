#pragma once

#include <jni.h>

#include <string>

namespace pztrainer::bridge {

int QueueWeaponMagazineExtraction(JNIEnv* env, jobject player,
                                  const std::string& magazine_full_type,
                                  int quantity, std::string& detail);
void UpdateWeaponMagazineExtraction(JNIEnv* env, jobject player);
const std::string& GetWeaponMagazineExtractionStatus();

}  // namespace pztrainer::bridge
