#pragma once

#include <jni.h>

namespace pztrainer::bridge {

class ModelOutlineBridge {
public:
    bool Initialize(JNIEnv* env, jobject class_loader, jmethodID load_class);
    bool BeginFrame(JNIEnv* env, bool enabled);
    bool ApplyToObject(JNIEnv* env, jobject object,
                       float red, float green, float blue, float alpha);

private:
    bool attempted_ = false;
    bool ready_ = false;
    bool enabled_ = false;
    jlong ui_render_time_ = 0;

    jclass iso_object_ = nullptr;
    jclass ui_manager_ = nullptr;
    jclass fbo_outline_class_ = nullptr;
    jobject fbo_outline_ = nullptr;

    jfieldID ui_render_time_field_ = nullptr;
    jmethodID set_outline_color_ = nullptr;
    jmethodID set_outline_highlight_ = nullptr;
    jmethodID get_fbo_outline_ = nullptr;
    jmethodID set_ui_render_time_ = nullptr;
};

}  // namespace pztrainer::bridge
