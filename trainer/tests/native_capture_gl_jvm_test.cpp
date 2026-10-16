#include <filesystem>
#include <cstdlib>
#include "../src/features/visual/native_model_chams.cpp"
namespace {JNIEnv* test_env=nullptr;std::filesystem::path test_jar;}
namespace pztrainer::bridge {
JNIEnv* GetCurrentJniEnvironment() {return test_env;}
jclass LoadEmbeddedJavaClass(JNIEnv* env,const char* name,std::string&) {
    std::string path(name);std::replace(path.begin(),path.end(),'.','/');return env->FindClass(path.c_str());
}
bool IsViewpointActive() {return false;}
bool ShouldSuppressVisibilityPolygon(unsigned int) {return false;}
ViewpointSettings& GetViewpointSettings() {static ViewpointSettings s;return s;}
const ViewpointStatus& GetViewpointStatus() {static ViewpointStatus s;return s;}
}
namespace pztrainer::runtime {std::filesystem::path EnsurePlayerOverridesJar(std::string&) {return test_jar;}}
namespace pztrainer::features::visual {
void ResetViewpointModelChamsStatus(const std::string&) {}
void DrawViewpointModelChams(const std::array<ImVec4,12>&,const std::array<ImVec4,12>&,
                           const std::array<ImVec4,12>&,const std::array<int,12>&,const bridge::FrameSnapshot&) {std::abort();}
namespace {
bridge::FrameSnapshot selected;
VisualSettings zombies;
PlayerVisualSettings players;
AnimalVisualSettings animals;
VehicleVisualSettings vehicles;
}
}
extern "C" JNIEXPORT jstring JNICALL Java_NativeCaptureGlTest_initialize(JNIEnv* env,jclass,jstring jar) {
    _set_error_mode(_OUT_TO_STDERR);_set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    test_env=env;const char* path=env->GetStringUTFChars(jar,nullptr);test_jar=path;env->ReleaseStringUTFChars(jar,path);
    using namespace pztrainer::features::visual;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={64,64};io.DisplayFramebufferScale={1,1};
    selected.gate_status=pztrainer::bridge::GateStatus::SinglePlayerAllowed;selected.model_chams_status=2;
    selected.zombies.emplace_back();selected.zombies[0].identity=42;
    selected.players.emplace_back();selected.players[0].identity=84;
    players.player_esp=false;animals.animal_esp=false;vehicles.vehicle_esp=false;
    const bool ready=MH_Initialize()==MH_OK && NativeModelChams::Initialize() && InitializeOwnedCapture(env);
    return env->NewStringUTF(ready?"":g_owned_capture_error.c_str());
}
extern "C" JNIEXPORT jfloatArray JNICALL Java_NativeCaptureGlTest_begin(JNIEnv* env,jclass,jint mode,jint category) {
    using namespace pztrainer::features::visual;
    zombies.model_effect=mode==1?ZombieModelEffect::Disabled:ZombieModelEffect::Solid;
    zombies.model_edge_glow=mode!=0;zombies.model_edge_glow_color={0,1,0,1};
    zombies.model_colors.normal={1,0,0,1};
    zombies.zombie_esp=category==0;players.player_esp=category==1;
    players.model_effect=zombies.model_effect;players.model_edge_glow=zombies.model_edge_glow;
    players.model_edge_glow_color=zombies.model_edge_glow_color;players.model_colors.normal=zombies.model_colors.normal;
    NativeModelChams::SetCaptureEnabled(true,selected,zombies,players,animals,vehicles);
    const auto& colour=category==0?ModelCaptureMarker(ZombieVisualState::Default):PlayerModelCaptureMarker(ZombieVisualState::Default);
    float values[]{colour.x,colour.y,colour.z,1};auto result=env->NewFloatArray(4);env->SetFloatArrayRegion(result,0,4,values);return result;
}
extern "C" JNIEXPORT jint JNICALL Java_NativeCaptureGlTest_replay(JNIEnv*,jclass) {
    using namespace pztrainer::features::visual;
    const auto count=NativeModelChams::CapturedDrawCount();
    NativeModelChams::Replay(selected,zombies,players,animals,vehicles);
    return static_cast<jint>(count);
}
