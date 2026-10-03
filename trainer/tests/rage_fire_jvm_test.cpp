#include <jni.h>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "bridge/rage_fire_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::runtime {
std::filesystem::path EnsurePlayerOverridesJar(std::string&) {
    return std::filesystem::path(std::getenv("PZ_RAGE_FIRE_JAR"));
}
}
namespace pztrainer::bridge {
jclass LoadEmbeddedJavaClass(JNIEnv* env, const char* name, std::string& error) {
    std::string binary(name);
    for (char& value : binary) if (value == '.') value = '/';
    jclass result = env->FindClass(binary.c_str());
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        error = "Test class loading failed: " + binary;
    }
    return result;
}
bool QueueObjectMethodOnMainThread(JNIEnv*, jobject, const char*, std::initializer_list<jobject>, AsyncObjectMethodCall&) { return false; }
AsyncObjectMethodState PollObjectMethodOnMainThread(JNIEnv*, AsyncObjectMethodCall&, std::chrono::milliseconds, jobject*, std::string*) {
    return AsyncObjectMethodState::Idle;
}
}
extern "C" JNIEXPORT jstring JNICALL Java_RageFireJvmTest_install(JNIEnv* env, jclass) {
    std::string error;
    if (pztrainer::bridge::EnsureRageFireBridge(env, error)) error.clear();
    return env->NewStringUTF(error.c_str());
}
