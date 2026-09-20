#include <jni.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "bridge/player_overrides.hpp"

// Use the production JVMTI installer in an isolated JVM, without injecting the game.
namespace pztrainer::runtime {
std::filesystem::path EnsurePlayerOverridesJar(std::string&) { return std::filesystem::path(std::getenv("PZ_PLAYER_TEST_JAR")); }
}
namespace pztrainer::bridge {
jclass LoadEmbeddedJavaClass(JNIEnv* env, const char* dotted, std::string& error) {
    jclass test = env->FindClass("PlayerOverridesJvmTest");
    jmethodID load = env->GetStaticMethodID(test, "loadEmbedded", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = env->NewStringUTF(dotted);
    jclass result = static_cast<jclass>(env->CallStaticObjectMethod(test, load, name));
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(test);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); error = std::string("Test class loading failed: ") + dotted; }
    return result;
}
}
extern "C" JNIEXPORT jstring JNICALL Java_PlayerOverridesJvmTest_install(JNIEnv* env, jclass) {
    std::string error;
    jclass helper = pztrainer::bridge::LoadPlayerOverrideClass(env, "pztrainer.player.CarryOverrides", error);
    if (helper) { env->DeleteLocalRef(helper); error.clear(); }
    return env->NewStringUTF(error.c_str());
}
