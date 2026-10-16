#include <Windows.h>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include "../src/bridge/corpse_item_bridge.cpp"

namespace {
std::filesystem::path helper_jar;
jobject last_result=nullptr;
int prepared_calls=0;
}
namespace pztrainer::runtime {
std::filesystem::path EnsurePlayerOverridesJar(std::string&) {return helper_jar;}
}
namespace pztrainer::bridge {
jclass LoadEmbeddedJavaClass(JNIEnv* env,const char* name,std::string&) {
    std::string path(name);std::replace(path.begin(),path.end(),'.','/');return env->FindClass(path.c_str());
}
void ResetObjectMethodCall(JNIEnv* env,AsyncObjectMethodCall& call) {
    if(call.queue_item)env->DeleteGlobalRef(call.queue_item);
    call=AsyncObjectMethodCall{};
}
AsyncObjectMethodState PollObjectMethodOnMainThread(JNIEnv*,AsyncObjectMethodCall&,std::chrono::milliseconds,jobject*,std::string*) {
    return AsyncObjectMethodState::Failed;
}
jobject BoxFloat(JNIEnv* env,float value) {
    auto type=env->FindClass("java/lang/Float");
    return env->CallStaticObjectMethod(type,env->GetStaticMethodID(type,"valueOf","(F)Ljava/lang/Float;"),value);
}
bool QueueObjectMethodOnMainThread(JNIEnv* env,jobject target,const char* method,
    std::initializer_list<jobject> arguments,AsyncObjectMethodCall& call) {
    if(std::string(method)=="addToContainer" || std::string(method)=="AddItems") {
        if(std::string(method)=="addToContainer")++prepared_calls;
        auto object=env->FindClass("java/lang/Object");
        auto array=env->NewObjectArray(static_cast<jsize>(arguments.size()),object,nullptr);
        int index=0;for(jobject argument:arguments)env->SetObjectArrayElement(array,index++,argument);
        auto type=env->FindClass("java/beans/Expression");
        auto name=env->NewStringUTF(method);
        auto expression=env->NewObject(type,env->GetMethodID(type,"<init>","(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)V"),target,name,array);
        auto result=env->CallObjectMethod(expression,env->GetMethodID(type,"getValue","()Ljava/lang/Object;"));
        if(env->ExceptionCheck()) {env->ExceptionDescribe();return false;}
        if(last_result)env->DeleteGlobalRef(last_result);
        last_result=env->NewGlobalRef(result);
        env->DeleteLocalRef(result);env->DeleteLocalRef(expression);env->DeleteLocalRef(name);env->DeleteLocalRef(type);env->DeleteLocalRef(array);env->DeleteLocalRef(object);
    }
    call.queue_item=env->NewGlobalRef(target);
    return true;
}
}
int wmain(int argc,wchar_t** argv) {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    assert(argc==4);
    const std::filesystem::path game(argv[1]),classes(argv[2]);helper_jar=argv[3];
    SetDllDirectoryW((game/L"jre64/bin").c_str());
    HMODULE jvm=LoadLibraryW((game/L"jre64/bin/server/jvm.dll").c_str());assert(jvm);
    auto create=reinterpret_cast<jint(JNICALL*)(JavaVM**,void**,void*)>(GetProcAddress(jvm,"JNI_CreateJavaVM"));assert(create);
    std::string cp="-Djava.class.path="+classes.u8string()+";"+(game/L"projectzomboid.jar").u8string();
    std::string home="-Duser.home="+(classes/L"profile").u8string();
    JavaVMOption options[]{{cp.data(),nullptr},{home.data(),nullptr}};
    JavaVMInitArgs args{};args.version=JNI_VERSION_1_8;args.nOptions=2;args.options=options;
    JavaVM* vm=nullptr;JNIEnv* env=nullptr;assert(create(&vm,reinterpret_cast<void**>(&env),&args)==JNI_OK);
    auto fixture=env->FindClass("PreparedItemRoutesTest");assert(fixture);
    env->CallStaticVoidMethod(fixture,env->GetStaticMethodID(fixture,"prepare","()V"));
    if(env->ExceptionCheck()) {env->ExceptionDescribe();return 1;}
    auto type=env->FindClass("zombie/inventory/ItemContainer");
    auto container=env->NewObject(type,env->GetMethodID(type,"<init>","()V"));
    using namespace pztrainer::bridge;
    assert(Initialize(env));
    for(const std::string key:{"Moveables.location_business_office_generic_01_18","RecordedMedia:retail-test","RecordedMedia:cd-test","Base.VHS_Home"}) {
        std::string detail;
        assert(QueueCorpsePayloadRequests(env,container,{{key,2}},detail)==2);
        g_pending.container=env->NewGlobalRef(container);
        assert(QueueCurrentPayloadItem(env));
        assert(last_result && env->CallIntMethod(last_result,g_bindings.list_size)==2);
        ResetPending(env);
    }
    assert(prepared_calls==4 && !env->ExceptionCheck());
    {
        std::string detail;
        assert(QueueCorpsePayloadRequests(env,container,{{"Base.Moveable",2}},detail)==2);
        g_pending.container=env->NewGlobalRef(container);
        assert(QueueCurrentPayloadItem(env));
        assert(last_result && env->CallIntMethod(last_result,g_bindings.list_size)==2 && prepared_calls==4);
        ResetPending(env);
    }
    env->DeleteGlobalRef(last_result);last_result=nullptr;
    std::cout<<"Production corpse dispatch: prepared preflight, main-thread static expression, exact quantity and mixed furniture/VHS/CD routing passed.\n";
    assert(vm->DestroyJavaVM()==JNI_OK);
}
