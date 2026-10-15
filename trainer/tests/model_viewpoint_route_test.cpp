#include <cassert>
#include <cstdlib>
#include <iostream>
#include <MinHook.h>
#include "bridge/viewpoint_bridge.hpp"
#include "features/visual/native_model_chams.hpp"
#include "features/visual/viewpoint_model_chams.hpp"
#include "features/visual/model_visual_policy.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "runtime/lua_bridge_resource.hpp"
MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*) { std::abort(); }
MH_STATUS WINAPI MH_EnableHook(LPVOID) { std::abort(); }
namespace pztrainer::bridge {
JNIEnv* GetCurrentJniEnvironment() { return nullptr; }
bool QueueObjectMethodOnMainThread(JNIEnv*,jobject,const char*,std::initializer_list<jobject>,AsyncObjectMethodCall&) {std::abort();}
AsyncObjectMethodState PollObjectMethodOnMainThread(JNIEnv*,AsyncObjectMethodCall&,std::chrono::milliseconds,jobject*,std::string*) {std::abort();}
void ResetObjectMethodCall(JNIEnv*,AsyncObjectMethodCall&) {std::abort();}
jclass LoadEmbeddedJavaClass(JNIEnv*,const char*,std::string&) {std::abort();}
bool IsViewpointActive() { return true; }
bool ShouldSuppressVisibilityPolygon(unsigned int) { return false; }
ViewpointSettings& GetViewpointSettings() { static ViewpointSettings s{true,false,false,true}; return s; }
const ViewpointStatus& GetViewpointStatus() { static ViewpointStatus s{true,true,true,true,true,"ready"}; return s; }
}
namespace pztrainer::runtime {std::filesystem::path EnsurePlayerOverridesJar(std::string&) {std::abort();}}
namespace pztrainer::features::visual {
int calls=0; std::array<int,12> last_effects{}; std::array<ImVec4,12> last_edges{};
void ResetViewpointModelChamsStatus(const std::string&) {}
void DrawViewpointModelChams(const std::array<ImVec4,12>&,const std::array<ImVec4,12>&,
                           const std::array<ImVec4,12>& edges,const std::array<int,12>& effects,
                           const bridge::FrameSnapshot&) { ++calls; last_effects=effects; last_edges=edges; }
}
int main() {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    using namespace pztrainer; using namespace features::visual;
    ImGui::CreateContext();
    bridge::FrameSnapshot frame; frame.gate_status=bridge::GateStatus::SinglePlayerAllowed;
    frame.model_chams_status=0; // No vanilla capture interface or current GL context.
    VisualSettings zombies; zombies.model_effect=ZombieModelEffect::Thermal; zombies.model_edge_glow=false;
    PlayerVisualSettings players; players.player_esp=false;
    AnimalVisualSettings animals; animals.animal_esp=false;
    VehicleVisualSettings vehicles; vehicles.vehicle_esp=false;
    NativeModelChams::Replay(frame,zombies,players,animals,vehicles);
    assert(calls==1 && last_effects[0]==8 && last_edges[0].w==0);
    zombies.model_effect=ZombieModelEffect::Disabled; zombies.model_edge_glow=true;
    NativeModelChams::Replay(frame,zombies,players,animals,vehicles);
    assert(calls==2 && last_effects[0]==0 && last_edges[0].w>0);
    zombies.model_effect=ZombieModelEffect::Thermal;
    NativeModelChams::Replay(frame,zombies,players,animals,vehicles);
    assert(calls==3 && last_effects[0]==8 && last_edges[0].w>0);
    assert(last_effects[3]==0 && last_edges[3].w==0);
    bridge::GetViewpointSettings().esp_3d=false;
    NativeModelChams::Replay(frame,zombies,players,animals,vehicles); assert(calls==3);
    ImGui::DestroyContext();
    std::cout<<"Production C++ replay: 3D fill, rim and combined modes reach the helper without vanilla capture or GL context; disabled groups/gate preserved\n";
}
