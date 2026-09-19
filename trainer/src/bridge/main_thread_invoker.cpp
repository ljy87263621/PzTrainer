#include "bridge/main_thread_invoker.hpp"

#include "vmprotect.hpp"

#include <algorithm>
#include <string>
#include <thread>

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass main_thread = nullptr;
    jclass main_thread_queue_item = nullptr;
    jclass expression = nullptr;
    jclass event_handler = nullptr;
    jclass runnable = nullptr;
    jclass object = nullptr;
    jclass float_class = nullptr;
    jclass throwable = nullptr;
    jmethodID expression_constructor = nullptr;
    jmethodID expression_get_value = nullptr;
    jmethodID create_event_handler = nullptr;
    jmethodID invoke_on_main_thread = nullptr;
    jmethodID queue_invoke_on_main_thread = nullptr;
    jmethodID queue_item_alloc = nullptr;
    jmethodID queue_item_is_finished = nullptr;
    jmethodID queue_item_get_thrown = nullptr;
    jmethodID float_value_of = nullptr;
    jmethodID throwable_to_string = nullptr;
};

Bindings g_bindings;
thread_local bool g_synchronous_invocation_blocked = false;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(class_loader_class, get_system_loader);
    env->DeleteLocalRef(class_loader_class);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.MainThreadBridgeBindings");
    g_bindings.main_thread = LoadGlobalClass(env, "zombie/MainThread");
    g_bindings.main_thread_queue_item = LoadGlobalClass(
        env, "zombie/MainThreadQueueItem");
    g_bindings.expression = LoadGlobalClass(env, "java/beans/Expression");
    g_bindings.event_handler = LoadGlobalClass(env, "java/beans/EventHandler");
    g_bindings.runnable = LoadGlobalClass(env, "java/lang/Runnable");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    g_bindings.float_class = LoadGlobalClass(env, "java/lang/Float");
    g_bindings.throwable = LoadGlobalClass(env, "java/lang/Throwable");
    if (g_bindings.main_thread == nullptr ||
        g_bindings.main_thread_queue_item == nullptr ||
        g_bindings.expression == nullptr ||
        g_bindings.event_handler == nullptr || g_bindings.runnable == nullptr ||
        g_bindings.object == nullptr || g_bindings.float_class == nullptr ||
        g_bindings.throwable == nullptr) {
        return false;
    }

    g_bindings.expression_constructor = env->GetMethodID(
        g_bindings.expression, "<init>",
        "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)V");
    g_bindings.expression_get_value = env->GetMethodID(
        g_bindings.expression, "getValue", "()Ljava/lang/Object;");
    g_bindings.create_event_handler = env->GetStaticMethodID(
        g_bindings.event_handler, "create",
        "(Ljava/lang/Class;Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
    g_bindings.invoke_on_main_thread = env->GetStaticMethodID(
        g_bindings.main_thread, "invokeOnMainThread", "(Ljava/lang/Runnable;)V");
    g_bindings.queue_invoke_on_main_thread = env->GetStaticMethodID(
        g_bindings.main_thread, "queueInvokeOnMainThread",
        "(Lzombie/MainThreadQueueItem;)V");
    g_bindings.queue_item_alloc = env->GetStaticMethodID(
        g_bindings.main_thread_queue_item, "alloc",
        "(Ljava/lang/Runnable;)Lzombie/MainThreadQueueItem;");
    g_bindings.queue_item_is_finished = env->GetMethodID(
        g_bindings.main_thread_queue_item, "isFinished", "()Z");
    g_bindings.queue_item_get_thrown = env->GetMethodID(
        g_bindings.main_thread_queue_item, "getThrown", "()Ljava/lang/Throwable;");
    g_bindings.float_value_of = env->GetStaticMethodID(
        g_bindings.float_class, "valueOf", "(F)Ljava/lang/Float;");
    g_bindings.throwable_to_string = env->GetMethodID(
        g_bindings.throwable, "toString", "()Ljava/lang/String;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.expression_constructor != nullptr &&
        g_bindings.expression_get_value != nullptr &&
        g_bindings.create_event_handler != nullptr &&
        g_bindings.invoke_on_main_thread != nullptr &&
        g_bindings.queue_invoke_on_main_thread != nullptr &&
        g_bindings.queue_item_alloc != nullptr &&
        g_bindings.queue_item_is_finished != nullptr &&
        g_bindings.queue_item_get_thrown != nullptr &&
        g_bindings.float_value_of != nullptr &&
        g_bindings.throwable_to_string != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

}  // namespace

ScopedSynchronousMainThreadInvocationBlock::
ScopedSynchronousMainThreadInvocationBlock()
    : previously_blocked_(g_synchronous_invocation_blocked) {
    g_synchronous_invocation_blocked = true;
}

ScopedSynchronousMainThreadInvocationBlock::
~ScopedSynchronousMainThreadInvocationBlock() {
    g_synchronous_invocation_blocked = previously_blocked_;
}

jobject InvokeObjectMethodOnMainThread(JNIEnv* env, jobject target,
                                       const char* method_name,
                                       std::initializer_list<jobject> arguments,
                                       bool* succeeded) {
    if (succeeded != nullptr) *succeeded = false;
    if (g_synchronous_invocation_blocked || env == nullptr || target == nullptr ||
        method_name == nullptr || !Initialize(env)) {
        return nullptr;
    }

    jobjectArray java_arguments = env->NewObjectArray(
        static_cast<jsize>(arguments.size()), g_bindings.object, nullptr);
    jsize index = 0;
    for (jobject argument : arguments) {
        env->SetObjectArrayElement(java_arguments, index++, argument);
    }
    jstring java_method_name = env->NewStringUTF(method_name);
    jobject expression = java_arguments == nullptr || java_method_name == nullptr
        ? nullptr
        : env->NewObject(g_bindings.expression, g_bindings.expression_constructor,
                         target, java_method_name, java_arguments);
    if (java_arguments != nullptr) env->DeleteLocalRef(java_arguments);
    if (java_method_name != nullptr) env->DeleteLocalRef(java_method_name);
    if (expression == nullptr || ClearException(env)) {
        if (expression != nullptr) env->DeleteLocalRef(expression);
        return nullptr;
    }

    jstring get_value = env->NewStringUTF("getValue");
    jobject runnable = get_value == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.event_handler, g_bindings.create_event_handler,
            g_bindings.runnable, expression, get_value);
    if (get_value != nullptr) env->DeleteLocalRef(get_value);
    if (runnable == nullptr || ClearException(env)) {
        if (runnable != nullptr) env->DeleteLocalRef(runnable);
        env->DeleteLocalRef(expression);
        return nullptr;
    }

    env->CallStaticVoidMethod(
        g_bindings.main_thread, g_bindings.invoke_on_main_thread, runnable);
    env->DeleteLocalRef(runnable);
    if (ClearException(env)) {
        env->DeleteLocalRef(expression);
        return nullptr;
    }

    jobject value = env->CallObjectMethod(expression, g_bindings.expression_get_value);
    const bool failed = ClearException(env);
    env->DeleteLocalRef(expression);
    if (failed) {
        if (value != nullptr) env->DeleteLocalRef(value);
        return nullptr;
    }
    if (succeeded != nullptr) *succeeded = true;
    return value;
}

void ResetObjectMethodCall(JNIEnv* env, AsyncObjectMethodCall& call) {
    if (env != nullptr) {
        if (call.expression != nullptr) env->DeleteGlobalRef(call.expression);
        if (call.queue_item != nullptr) env->DeleteGlobalRef(call.queue_item);
    }
    call = AsyncObjectMethodCall{};
}

bool QueueObjectMethodOnMainThread(JNIEnv* env, jobject target,
                                   const char* method_name,
                                   std::initializer_list<jobject> arguments,
                                   AsyncObjectMethodCall& call) {
    if (env == nullptr || target == nullptr || method_name == nullptr ||
        call.queue_item != nullptr || !Initialize(env)) {
        return false;
    }

    jobjectArray java_arguments = env->NewObjectArray(
        static_cast<jsize>(arguments.size()), g_bindings.object, nullptr);
    jsize index = 0;
    for (jobject argument : arguments) {
        env->SetObjectArrayElement(java_arguments, index++, argument);
    }
    jstring java_method_name = env->NewStringUTF(method_name);
    jobject expression = java_arguments == nullptr || java_method_name == nullptr
        ? nullptr
        : env->NewObject(g_bindings.expression, g_bindings.expression_constructor,
                         target, java_method_name, java_arguments);
    if (java_arguments != nullptr) env->DeleteLocalRef(java_arguments);
    if (java_method_name != nullptr) env->DeleteLocalRef(java_method_name);
    if (expression == nullptr || ClearException(env)) {
        if (expression != nullptr) env->DeleteLocalRef(expression);
        return false;
    }

    jstring get_value = env->NewStringUTF("getValue");
    jobject runnable = get_value == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.event_handler, g_bindings.create_event_handler,
            g_bindings.runnable, expression, get_value);
    if (get_value != nullptr) env->DeleteLocalRef(get_value);
    jobject queue_item = runnable == nullptr || ClearException(env)
        ? nullptr
        : env->CallStaticObjectMethod(
            g_bindings.main_thread_queue_item, g_bindings.queue_item_alloc, runnable);
    if (runnable != nullptr) env->DeleteLocalRef(runnable);
    if (queue_item == nullptr || ClearException(env)) {
        if (queue_item != nullptr) env->DeleteLocalRef(queue_item);
        env->DeleteLocalRef(expression);
        return false;
    }

    call.expression = env->NewGlobalRef(expression);
    call.queue_item = env->NewGlobalRef(queue_item);
    call.started_at = std::chrono::steady_clock::now();
    env->DeleteLocalRef(expression);
    if (call.expression == nullptr || call.queue_item == nullptr || ClearException(env)) {
        env->DeleteLocalRef(queue_item);
        ResetObjectMethodCall(env, call);
        return false;
    }

    JavaVM* vm = nullptr;
    jobject dispatch_ref = env->NewGlobalRef(queue_item);
    env->DeleteLocalRef(queue_item);
    if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr || dispatch_ref == nullptr ||
        ClearException(env)) {
        if (dispatch_ref != nullptr) env->DeleteGlobalRef(dispatch_ref);
        ResetObjectMethodCall(env, call);
        return false;
    }
    try {
        std::thread([vm, dispatch_ref]() {
            JNIEnv* thread_env = nullptr;
            bool attached = false;
            if (vm->GetEnv(
                    reinterpret_cast<void**>(&thread_env), JNI_VERSION_1_8) != JNI_OK) {
                if (vm->AttachCurrentThread(
                        reinterpret_cast<void**>(&thread_env), nullptr) != JNI_OK) {
                    return;
                }
                attached = true;
            }
            thread_env->CallStaticVoidMethod(
                g_bindings.main_thread, g_bindings.queue_invoke_on_main_thread,
                dispatch_ref);
            ClearException(thread_env);
            thread_env->DeleteGlobalRef(dispatch_ref);
            if (attached) vm->DetachCurrentThread();
        }).detach();
    } catch (...) {
        env->DeleteGlobalRef(dispatch_ref);
        ResetObjectMethodCall(env, call);
        return false;
    }
    return true;
}

AsyncObjectMethodState PollObjectMethodOnMainThread(
    JNIEnv* env, AsyncObjectMethodCall& call,
    std::chrono::milliseconds timeout, jobject* result,
    std::string* error) {
    if (result != nullptr) *result = nullptr;
    if (error != nullptr) error->clear();
    if (env == nullptr || call.queue_item == nullptr || call.expression == nullptr) {
        return AsyncObjectMethodState::Idle;
    }

    const jboolean finished = env->CallBooleanMethod(
        call.queue_item, g_bindings.queue_item_is_finished);
    if (ClearException(env)) {
        ResetObjectMethodCall(env, call);
        return AsyncObjectMethodState::Failed;
    }
    if (finished != JNI_TRUE) {
        if (std::chrono::steady_clock::now() - call.started_at < timeout) {
            return AsyncObjectMethodState::Pending;
        }
        ResetObjectMethodCall(env, call);
        return AsyncObjectMethodState::TimedOut;
    }

    jobject thrown = env->CallObjectMethod(
        call.queue_item, g_bindings.queue_item_get_thrown);
    if (thrown != nullptr && error != nullptr) {
        jstring description = static_cast<jstring>(env->CallObjectMethod(
            thrown, g_bindings.throwable_to_string));
        if (description != nullptr && !env->ExceptionCheck()) {
            const char* utf = env->GetStringUTFChars(description, nullptr);
            if (utf != nullptr) {
                *error = utf;
                env->ReleaseStringUTFChars(description, utf);
            }
        }
        if (description != nullptr) env->DeleteLocalRef(description);
        ClearException(env);
    }
    const bool failed = ClearException(env) || thrown != nullptr;
    if (thrown != nullptr) env->DeleteLocalRef(thrown);
    if (failed) {
        ResetObjectMethodCall(env, call);
        return AsyncObjectMethodState::Failed;
    }

    jobject value = env->CallObjectMethod(
        call.expression, g_bindings.expression_get_value);
    if (ClearException(env)) {
        if (value != nullptr) env->DeleteLocalRef(value);
        ResetObjectMethodCall(env, call);
        return AsyncObjectMethodState::Failed;
    }
    ResetObjectMethodCall(env, call);
    if (result != nullptr) {
        *result = value;
    } else if (value != nullptr) {
        env->DeleteLocalRef(value);
    }
    return AsyncObjectMethodState::Succeeded;
}

jobject BoxFloat(JNIEnv* env, float value) {
    if (env == nullptr || !Initialize(env)) return nullptr;
    jobject boxed = env->CallStaticObjectMethod(
        g_bindings.float_class, g_bindings.float_value_of, value);
    if (ClearException(env)) {
        if (boxed != nullptr) env->DeleteLocalRef(boxed);
        return nullptr;
    }
    return boxed;
}

}  // namespace pztrainer::bridge
