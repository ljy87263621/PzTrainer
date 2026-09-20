#include "ui/world/world_interaction_page.hpp"

#include <imgui.h>

#include "bridge/extension_bridge.hpp"

#include "ui/components.hpp"
#include "ui/controls/action_controls.hpp"
#include "ui/controls/card_layout.hpp"
#include "ui/extensions/extension_controls.hpp"

namespace pztrainer::ui {
namespace {
void DrawArea(int& radius, bool& destructive) {
    cards::BeginSection("Area", "操作范围");
    controls::IntegerRow("同层半径", &radius, 1, 15);
    components::CompactToggleRow("允许删除、清空与重刷", &destructive, false);
    components::EndCard();
}
void DrawZombies() {
    cards::BeginSection("Zombies", "僵尸控制");
    extensions::Toggle("僵尸不攻击", 32);
    extensions::Toggle("停用已加载僵尸 AI", 64);
    extensions::Toggle("范围内僵尸立即死亡", 128);
    extensions::BeginAvailable("flag:128");
    controls::IntegerRow("击杀半径", &bridge::GetExtensionOptions().kill_range, 1, 30);
    extensions::EndAvailable();
    controls::Hint("半径为 30 时检查全部已加载僵尸；联机只处理本机拥有模拟权的僵尸。关闭 AI 开关后恢复原属性。");
    components::EndCard();
}
void DrawCleanup(int radius, bool destructive) {
    cards::BeginSection("Cleanup", "尸体与容器");
    ImGui::BeginDisabled(!destructive);
    extensions::Action("删除范围内僵尸尸体", "corpses", {}, radius);
    extensions::Action("清空范围内容器", "containers_clear", {}, radius);
    extensions::Action("重刷范围内容器战利品", "containers_reroll", {}, radius);
    ImGui::EndDisabled();
    components::EndCard();
}
}  // namespace

void DrawWorldInteractionPage() {
    static int radius = 10;
    static bool destructive = false;
    if (cards::BeginColumns("WorldInteractionCards")) {
        ImGui::BeginDisabled(!bridge::ExtensionBridgeReady());
        DrawZombies();
        ImGui::EndDisabled();
        extensions::Status();
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginDisabled(!bridge::ExtensionBridgeReady());
        DrawArea(radius, destructive);
        DrawCleanup(radius, destructive);
        ImGui::EndDisabled();
        cards::EndColumns();
    }
}
}  // namespace pztrainer::ui
