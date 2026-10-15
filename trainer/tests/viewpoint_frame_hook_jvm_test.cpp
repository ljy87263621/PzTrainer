#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include "../src/features/visual/viewpoint_model_chams.cpp"
namespace {std::filesystem::path test_jar;JNIEnv* test_env=nullptr;}
namespace ImGui {
ImGuiIO& GetIO() {std::abort();} // Installing the hook must not query overlay/GL state.
}
namespace pztrainer::runtime {
std::filesystem::path EnsurePlayerOverridesJar(std::string&) {return test_jar;}
}
namespace pztrainer::bridge {
JNIEnv* GetCurrentJniEnvironment() {return test_env;}
jclass LoadEmbeddedJavaClass(JNIEnv* env,const char* name,std::string&) {
    std::string path(name);std::replace(path.begin(),path.end(),'.','/');return env->FindClass(path.c_str());
}
}
extern "C" JNIEXPORT jstring JNICALL Java_ViewpointFrameHookTest_install(JNIEnv* env,jclass,jstring jar) {
    const char* path=env->GetStringUTFChars(jar,nullptr);test_jar=path;env->ReleaseStringUTFChars(jar,path);test_env=env;
    using namespace pztrainer::features::visual;
    const bool ready=Initialize(env);
    return env->NewStringUTF(ready?"":g_status.message.c_str());
}
