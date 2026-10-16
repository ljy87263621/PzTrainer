#include "features/visual/visibility_options.hpp"
#include "features/visual/viewpoint_model_chams.hpp"

#include <jvmti.h>
#include <chrono>
#include <atomic>
#include <cstring>
#include <cmath>
#include <vector>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "runtime/lua_bridge_resource.hpp"
#include "../../../../protection/vmprotect.hpp"

namespace pztrainer::features::visual {
namespace {

jclass g_helper = nullptr;
jmethodID g_render = nullptr;
jmethodID g_clear = nullptr;
jclass g_transformer = nullptr;
jmethodID g_transform = nullptr;
jvmtiEnv* g_hook_tool = nullptr;
std::atomic<bool> g_transformed{false};
bool g_ready = false;
jfieldID g_error = nullptr;
ViewpointModelChamsStatus g_status;
std::chrono::steady_clock::time_point g_retry{};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

void JNICALL TransformFrame(jvmtiEnv* tool,JNIEnv* env,jclass type,jobject,const char* name,
    jobject,jint size,const unsigned char* bytes,jint* new_size,unsigned char** new_bytes) {
    if(!type || !name || std::strcmp(name,"viewpoint/render/WorldRenderer")!=0 || !g_transformer)return;
    if(env->PushLocalFrame(4)!=JNI_OK)return;
    jbyteArray input=env->NewByteArray(size);
    if(input)env->SetByteArrayRegion(input,0,size,reinterpret_cast<const jbyte*>(bytes));
    auto output=input && !env->ExceptionCheck()?static_cast<jbyteArray>(env->CallStaticObjectMethod(g_transformer,g_transform,input)):nullptr;
    if(output && !env->ExceptionCheck()) {
        const jint length=env->GetArrayLength(output);unsigned char* result=nullptr;
        if(length>0 && tool->Allocate(length,&result)==JVMTI_ERROR_NONE) {
            env->GetByteArrayRegion(output,0,length,reinterpret_cast<jbyte*>(result));
            if(!env->ExceptionCheck()) {*new_size=length;*new_bytes=result;g_transformed.store(true);}
            else tool->Deallocate(result);
        }
    }
    ClearException(env);env->PopLocalFrame(nullptr);
}
bool InstallFrameHook(JNIEnv* env,std::string& error) {
    JavaVM* vm=nullptr;
    if(!g_hook_tool) {
        if(env->GetJavaVM(&vm)!=JNI_OK || vm->GetEnv(reinterpret_cast<void**>(&g_hook_tool),JVMTI_VERSION_1_2)!=JNI_OK)return false;
        jvmtiCapabilities caps{};caps.can_retransform_classes=1;
        if(g_hook_tool->AddCapabilities(&caps)!=JVMTI_ERROR_NONE)return false;
    }
    if(!g_transformer) {
        jclass local=bridge::LoadEmbeddedJavaClass(env,"pztrainer.player.visual.ViewpointChamsTransforms",error);
        if(!local)return false;
        g_transform=env->GetStaticMethodID(local,"transform","([B)[B");
        if(!ClearException(env) && g_transform)g_transformer=static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);if(!g_transformer)return false;
    }
    jclass renderer=bridge::LoadEmbeddedJavaClass(env,"viewpoint.render.WorldRenderer",error);
    if(!renderer)return false;
    jvmtiEventCallbacks callbacks{};callbacks.ClassFileLoadHook=TransformFrame;
    g_transformed.store(false);
    const bool installed=g_hook_tool->SetEventCallbacks(&callbacks,sizeof(callbacks))==JVMTI_ERROR_NONE &&
        g_hook_tool->SetEventNotificationMode(JVMTI_ENABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr)==JVMTI_ERROR_NONE &&
        g_hook_tool->RetransformClasses(1,&renderer)==JVMTI_ERROR_NONE && g_transformed.load();
    g_hook_tool->SetEventNotificationMode(JVMTI_DISABLE,JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,nullptr);
    env->DeleteLocalRef(renderer);return installed;
}

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_ready) return true;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_retry) return false;
    g_retry = now + std::chrono::seconds(2);
    PZ_VMP_BEGIN_MUTATION("PZ.DLL.ViewpointModelChamsBindings");
    JavaVM* vm = nullptr;
    jvmtiEnv* tool = nullptr;
    std::string error;
    const auto jar = runtime::EnsurePlayerOverridesJar(error);
    if (jar.empty() || env->GetJavaVM(&vm) != JNI_OK ||
        vm->GetEnv(reinterpret_cast<void**>(&tool), JVMTI_VERSION_1_2) != JNI_OK) {
        g_status.message = error.empty() ? "3D 模型上色接口尚未就绪" : error;
        return false;
    }
    const bool appended = tool->AddToSystemClassLoaderSearch(jar.u8string().c_str()) == JVMTI_ERROR_NONE;
    tool->DisposeEnvironment();
    if (!appended) {
        g_status.message = "3D 模型上色 Java 包加载失败";
        return false;
    }
    jclass helper = bridge::LoadEmbeddedJavaClass(env, "pztrainer.player.visual.ViewpointModelChams", error);
    if (!helper) {
        g_status.message = error;
        return false;
    }
    g_render = env->GetStaticMethodID(helper, "publish", "([F[F[F[I[I[I[III)I");
    g_error = env->GetStaticFieldID(helper, "error", "Ljava/lang/String;");
    g_clear = env->GetStaticMethodID(helper,"clearPublished","()V");
    if (!ClearException(env) && g_render && g_error && g_clear && !g_helper)
        g_helper = static_cast<jclass>(env->NewGlobalRef(helper));
    env->DeleteLocalRef(helper);
    g_ready=g_helper && InstallFrameHook(env,error);
    if(!g_ready)g_status.message=error.empty()?"ViewPoint 模型帧回调校验失败":error;
    PZ_VMP_END();
    return g_ready;
}

}  // namespace

void DrawViewpointModelChams(const std::array<ImVec4, 12>& markers,
                            const std::array<ImVec4, 12>& colors,
                            const std::array<ImVec4, 12>& edges,
                            const std::array<int, 12>& effects,
                            const bridge::FrameSnapshot& frame) {
    g_status.painted_draws = 0;
    JNIEnv* env = bridge::GetCurrentJniEnvironment();
    if (!env || !Initialize(env) || env->PushLocalFrame(12) != JNI_OK) return;
    float marker_values[48]{}, color_values[48]{}, edge_values[48]{};
    for (std::size_t index = 0; index < markers.size(); ++index) {
        const ImVec4& marker = markers[index];
        const ImVec4& color = colors[index];
        const std::size_t at = index * 4;
        marker_values[at] = marker.x; marker_values[at + 1] = marker.y;
        marker_values[at + 2] = marker.z; marker_values[at + 3] = marker.w;
        color_values[at] = color.x; color_values[at + 1] = color.y;
        color_values[at + 2] = color.z; color_values[at + 3] = color.w;
        edge_values[at]=edges[index].x; edge_values[at+1]=edges[index].y;
        edge_values[at+2]=edges[index].z; edge_values[at+3]=edges[index].w;
    }
    std::vector<jint> identities, groups, layers;
    const auto append=[&](const auto& objects,int base) {
        for(const auto& object:objects) {
            identities.push_back(object.identity);
            groups.push_back(base+(object.behind_wall?1:object.in_view?2:0));
            const auto category=static_cast<VisualCategory>(base/3);
            layers.push_back((GetVisibilityOptions().Allows(category,VisualFeature::Fill,object.game_visible)?1:0) |
                (GetVisibilityOptions().Allows(category,VisualFeature::Rim,object.game_visible)?2:0));
        }
    };
    append(frame.zombies,0); append(frame.players,3); append(frame.animals,6); append(frame.vehicles,9);
    jfloatArray marker_array = env->NewFloatArray(48), color_array = env->NewFloatArray(48);
    jfloatArray edge_array=env->NewFloatArray(48);
    jintArray effect_array = env->NewIntArray(12);
    const jsize count=static_cast<jsize>(identities.size());
    jintArray identity_array=env->NewIntArray(count),group_array=env->NewIntArray(count),layer_array=env->NewIntArray(count);
    if (marker_array && color_array && edge_array && effect_array && identity_array && group_array && layer_array) {
        env->SetFloatArrayRegion(marker_array, 0, 48, marker_values);
        env->SetFloatArrayRegion(color_array, 0, 48, color_values);
        env->SetIntArrayRegion(effect_array, 0, 12, effects.data());
        env->SetFloatArrayRegion(edge_array,0,48,edge_values);
        if(count>0) {
            env->SetIntArrayRegion(identity_array,0,count,identities.data());
            env->SetIntArrayRegion(group_array,0,count,groups.data());
            env->SetIntArrayRegion(layer_array,0,count,layers.data());
        }
        const ImGuiIO& io = ImGui::GetIO();
        if (!ClearException(env)) g_status.painted_draws = env->CallStaticIntMethod(g_helper, g_render,
            marker_array, color_array, edge_array, effect_array, identity_array, group_array, layer_array,
            static_cast<jint>(std::lround(io.DisplaySize.x * io.DisplayFramebufferScale.x)),
            static_cast<jint>(std::lround(io.DisplaySize.y * io.DisplayFramebufferScale.y)));
        g_status.message = g_status.painted_draws > 0 ? "已绘制 Viewpoint 3D 模型" : "等待符合所选效果的 3D 模型";
        if (ClearException(env)) {
            g_status.message = "3D 模型绘制调用失败";
        } else {
            auto detail = static_cast<jstring>(env->GetStaticObjectField(g_helper, g_error));
            if (detail && !ClearException(env)) {
                const char* text = env->GetStringUTFChars(detail, nullptr);
                if (text) {
                    if (*text) g_status.message = std::string("3D 模型上色失败：") + text;
                    env->ReleaseStringUTFChars(detail, text);
                }
            }
        }
    }
    ClearException(env);
    env->PopLocalFrame(nullptr);
}

void ResetViewpointModelChamsStatus(const std::string& message) {
    g_status.painted_draws = 0;
    g_status.message = message;
    JNIEnv* env=bridge::GetCurrentJniEnvironment();
    if(env && g_helper && g_clear) {env->CallStaticVoidMethod(g_helper,g_clear);ClearException(env);}
}

const ViewpointModelChamsStatus& GetViewpointModelChamsStatus() { return g_status; }

}  // namespace pztrainer::features::visual
