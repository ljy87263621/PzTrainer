#include "bridge/model_outline_bridge.hpp"

#include <algorithm>
#include <string>

namespace pztrainer::bridge {
namespace {

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, jobject class_loader, jmethodID load_class,
                       const char* name) {
    std::string dotted_name(name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring java_name = env->NewStringUTF(dotted_name.c_str());
    if (java_name == nullptr || ClearException(env)) return nullptr;

    jclass local = static_cast<jclass>(
        env->CallObjectMethod(class_loader, load_class, java_name));
    env->DeleteLocalRef(java_name);
    if (local == nullptr || ClearException(env)) return nullptr;

    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

}  // namespace

bool ModelOutlineBridge::Initialize(JNIEnv* env, jobject class_loader, jmethodID load_class) {
    if (attempted_) return ready_;
    attempted_ = true;

    iso_object_ = LoadGlobalClass(env, class_loader, load_class, "zombie/iso/IsoObject");
    ui_manager_ = LoadGlobalClass(env, class_loader, load_class, "zombie/ui/UIManager");
    fbo_outline_class_ = LoadGlobalClass(
        env, class_loader, load_class,
        "zombie/iso/fboRenderChunk/FBORenderObjectOutline");
    if (iso_object_ == nullptr || ui_manager_ == nullptr ||
        fbo_outline_class_ == nullptr) {
        ClearException(env);
        return false;
    }

    ui_render_time_field_ = env->GetStaticFieldID(ui_manager_, "uiRenderTimeMS", "J");
    set_outline_color_ = env->GetMethodID(iso_object_, "setOutlineHighlightCol", "(IFFFF)V");
    set_outline_highlight_ = env->GetMethodID(iso_object_, "setOutlineHighlight", "(IZ)V");
    get_fbo_outline_ = env->GetStaticMethodID(
        fbo_outline_class_, "getInstance",
        "()Lzombie/iso/fboRenderChunk/FBORenderObjectOutline;");
    set_ui_render_time_ = env->GetMethodID(
        fbo_outline_class_, "setDuringUIRenderTime",
        "(ILzombie/iso/IsoObject;J)V");
    if (ClearException(env) || ui_render_time_field_ == nullptr ||
        set_outline_color_ == nullptr || set_outline_highlight_ == nullptr ||
        get_fbo_outline_ == nullptr || set_ui_render_time_ == nullptr) {
        return false;
    }

    jobject local_outline = env->CallStaticObjectMethod(fbo_outline_class_, get_fbo_outline_);
    if (local_outline == nullptr || ClearException(env)) return false;
    fbo_outline_ = env->NewGlobalRef(local_outline);
    env->DeleteLocalRef(local_outline);
    ready_ = fbo_outline_ != nullptr && !ClearException(env);
    return ready_;
}

bool ModelOutlineBridge::BeginFrame(JNIEnv* env, bool enabled) {
    enabled_ = enabled && ready_;
    ui_render_time_ = 0;
    if (!enabled_) return false;

    ui_render_time_ = env->GetStaticLongField(ui_manager_, ui_render_time_field_);
    if (ClearException(env) || ui_render_time_ <= 0) {
        enabled_ = false;
    }
    return enabled_;
}

bool ModelOutlineBridge::ApplyToObject(JNIEnv* env, jobject object,
                                       float red, float green, float blue,
                                       float alpha) {
    if (!enabled_ || object == nullptr) return false;

    env->CallVoidMethod(object, set_outline_color_, 0, red, green, blue, alpha);
    env->CallVoidMethod(object, set_outline_highlight_, 0, JNI_TRUE);
    env->CallVoidMethod(fbo_outline_, set_ui_render_time_, 0, object, ui_render_time_);
    if (ClearException(env)) {
        enabled_ = false;
        return false;
    }
    return true;
}

}  // namespace pztrainer::bridge
