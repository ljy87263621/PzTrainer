#pragma once

#include <jni.h>
#include <string>

namespace pztrainer::bridge {
jclass LoadPlayerOverrideClass(JNIEnv* env, const char* name, std::string& error);
std::string PlayerOverrideMessage(JNIEnv* env, jclass type);
}
