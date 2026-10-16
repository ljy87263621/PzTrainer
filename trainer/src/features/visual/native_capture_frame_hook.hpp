#pragma once

#include <atomic>
#include <cstring>
#include <jni.h>
#include <jvmti.h>
#include <string>
#include "bridge/lua_ui_bridge.hpp"

namespace pztrainer::features::visual::native_capture_frame {
inline jvmtiEnv* tool=nullptr;
inline jclass transformer=nullptr;
inline jmethodID transform=nullptr;
inline std::atomic<bool> transformed{false};

inline void JNICALL OnClass(jvmtiEnv* agent,JNIEnv* env,jclass type,jobject,const char* name,
    jobject,jint size,const unsigned char* bytes,jint* output_size,unsigned char** output) {
    if(!type || !name || std::strcmp(name,"zombie/gameStates/IngameState")!=0 || !transformer)return;
    if(env->PushLocalFrame(4)!=JNI_OK)return;
    auto input=env->NewByteArray(size);
    if(input)env->SetByteArrayRegion(input,0,size,reinterpret_cast<const jbyte*>(bytes));
    auto result=input && !env->ExceptionCheck()?static_cast<jbyteArray>(env->CallStaticObjectMethod(transformer,transform,input)):nullptr;
    if(result && !env->ExceptionCheck()) {
        const jint length=env->GetArrayLength(result);unsigned char* memory=nullptr;
        if(length>0 && agent->Allocate(length,&memory)==JVMTI_ERROR_NONE) {
            env->GetByteArrayRegion(result,0,length,reinterpret_cast<jbyte*>(memory));
            if(!env->ExceptionCheck()) {*output_size=length;*output=memory;transformed.store(true);}
            else agent->Deallocate(memory);
        }
    }
    if(env->ExceptionCheck())env->ExceptionClear();env->PopLocalFrame(nullptr);
}

inline bool Install(JNIEnv* env,std::string& error) {
    JavaVM* vm=nullptr;
    if(!tool) {
        if(env->GetJavaVM(&vm)!=JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&tool),JVMTI_VERSION_1_2)!=JNI_OK)return false;
        jvmtiCapabilities caps{};caps.can_retransform_classes=1;
        if(tool->AddCapabilities(&caps)!=JVMTI_ERROR_NONE)return false;
    }
    if(!transformer) {
        jclass local=bridge::LoadEmbeddedJavaClass(env,"pztrainer.player.visual.NativeCaptureTransforms",error);
        if(!local)return false;
        transform=env->GetStaticMethodID(local,"transform","([B)[B");
        if(!env->ExceptionCheck() && transform)transformer=static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        if(env->ExceptionCheck())env->ExceptionClear();
        if(!transformer)return false;
    }
    jclass state=bridge::LoadEmbeddedJavaClass(env,"zombie.gameStates.IngameState",error);
    if(!state)return false;
    jvmtiEventCallbacks callbacks{};callbacks.ClassFileLoadHook=OnClass;
    transformed.store(false);
    const bool ready=tool->SetEventCallbacks(&callbacks,sizeof(callbacks))==JVMTI_ERROR_NONE &&
        tool->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr)==JVMTI_ERROR_NONE &&
        tool->RetransformClasses(1,&state)==JVMTI_ERROR_NONE && transformed.load();
    tool->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
    env->DeleteLocalRef(state);
    if(!ready && error.empty())error="Native world-frame hook validation failed";
    return ready;
}
}
