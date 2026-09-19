#pragma once

#include <jni.h>

#include <chrono>
#include <initializer_list>
#include <string>

namespace pztrainer::bridge {

class ScopedSynchronousMainThreadInvocationBlock {
public:
    ScopedSynchronousMainThreadInvocationBlock();
    ~ScopedSynchronousMainThreadInvocationBlock();

    ScopedSynchronousMainThreadInvocationBlock(
        const ScopedSynchronousMainThreadInvocationBlock&) = delete;
    ScopedSynchronousMainThreadInvocationBlock& operator=(
        const ScopedSynchronousMainThreadInvocationBlock&) = delete;

private:
    bool previously_blocked_ = false;
};

jobject InvokeObjectMethodOnMainThread(JNIEnv* env, jobject target,
                                       const char* method_name,
                                       std::initializer_list<jobject> arguments,
                                       bool* succeeded);

enum class AsyncObjectMethodState {
    Idle,
    Pending,
    Succeeded,
    Failed,
    TimedOut,
};

struct AsyncObjectMethodCall {
    jobject expression = nullptr;
    jobject queue_item = nullptr;
    std::chrono::steady_clock::time_point started_at{};
};

bool QueueObjectMethodOnMainThread(JNIEnv* env, jobject target,
                                   const char* method_name,
                                   std::initializer_list<jobject> arguments,
                                   AsyncObjectMethodCall& call);
AsyncObjectMethodState PollObjectMethodOnMainThread(
    JNIEnv* env, AsyncObjectMethodCall& call,
    std::chrono::milliseconds timeout, jobject* result,
    std::string* error = nullptr);
void ResetObjectMethodCall(JNIEnv* env, AsyncObjectMethodCall& call);
jobject BoxFloat(JNIEnv* env, float value);

}  // namespace pztrainer::bridge
