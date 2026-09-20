#pragma once
#include <jni.h>
#include <string>

namespace pztrainer::features {
bool EnsureVehiclePhysicsBridge(JNIEnv* env, std::string& error);
}
