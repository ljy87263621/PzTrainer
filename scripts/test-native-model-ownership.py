import pathlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'trainer/src/features/visual/native_model_chams.cpp').read_text()
start = source.index('int ResolveTrackedEntityColorIndex(')
end = source.index('\njclass g_owned_capture_helper', start)
function = source[start:end]
test = r'''
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "features/visual/model_owner_policy.hpp"
using namespace pztrainer::features::visual;
namespace ImGui {
struct IO { struct Size { float x=1024, y=600; } DisplaySize, DisplayFramebufferScale{1,1}; };
IO& GetIO() { static IO io; return io; }
}
using ImGuiIO=ImGui::IO;
struct ImVec2 { float x,y; ImVec2(float a,float b):x(a),y(b){} };
using GLboolean=bool;
constexpr bool GL_TRUE=true;
namespace bridge=pztrainer::bridge;
struct DrawCommand {
 bool instanced=false;
 int target_color_index=-1;
 int owner_identity=0;
 bool model_view_projection_transpose=false;
 std::array<float,16> model_view_projection_value{};
 std::array<int,4> viewport{0,0,1024,600};
};
'''
test += function
test += r'''
int main() {
 bridge::FrameSnapshot frame; frame.zombies.emplace_back();
 frame.zombies[0].identity=42;
 frame.zombies[0].screen_x=512; frame.zombies[0].screen_y=310; frame.zombies[0].screen_top_y=220;
 DrawCommand player; player.model_view_projection_value[15]=1;
 const int actual=ResolveTrackedEntityColorIndex(player,BuildModelOwnerMap(frame));
 if(actual!=-1) { std::cerr<<"FAIL: unmarked overlapping player was assigned zombie group "<<actual<<"\n"; return 1; }
 player.target_color_index=0;
 if(ResolveTrackedEntityColorIndex(player,BuildModelOwnerMap(frame))!=-1) return 4;
 player.owner_identity=42;
 if(ResolveTrackedEntityColorIndex(player,BuildModelOwnerMap(frame))!=0) return 2;
 player.target_color_index=3;
 if(ResolveTrackedEntityColorIndex(player,BuildModelOwnerMap(frame))!=0) return 3;
 frame.players.emplace_back(); frame.players[0].identity=42;
 if(ResolveTrackedEntityColorIndex(player,BuildModelOwnerMap(frame))!=-1) return 5;
 std::cout<<"Native ownership: unmarked overlapping player excluded, explicit zombie/player groups preserved\n";
}
'''
path = root / 'obj/native-ownership-test.cpp'
path.write_text(test)
subprocess.run([r'D:\Develope\Tools\CodexToolchain\codex-msvc.cmd','cl','/nologo','/std:c++17','/EHsc','/utf-8','/Itrainer/src',str(path),'/Foobj/native-ownership-test.obj','/Feobj/native-ownership-test.exe'],cwd=root,check=True)
raise SystemExit(subprocess.run([str(root/'obj/native-ownership-test.exe')],cwd=root).returncode)
