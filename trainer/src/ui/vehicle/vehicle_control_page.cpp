#include "ui/vehicle/vehicle_control_page.hpp"

#include <imgui.h>
#include "ui/components.hpp"
#include "ui/controls/action_controls.hpp"
#include "ui/controls/card_layout.hpp"
#include "ui/extensions/extension_controls.hpp"

namespace pztrainer::ui {
void DrawVehicleControlPage() {
    using namespace extensions;
    if (!cards::BeginColumns("VehicleControlCards")) return;
    cards::BeginSection("Driving", "驾驶辅助");
    Toggle("驾驶时保持引擎运行", 512);
    Toggle("车辆无碰撞 / 即停", 1024);
    controls::Hint("使用原版方向键或手柄；打开菜单或输入文字时停止移动。仅控制本机驾驶的车辆，联机遵循服务器限速，拖挂车辆不支持。");
    components::EndCard();
    Status();

    ImGui::TableSetColumnIndex(1);
    cards::BeginSection("VehicleMaintenance", "车辆维护");
    controls::Hint("目标为当前乘坐车辆；未乘车时选择同层 20 格内最近的已加载车辆。");
    controls::Hint("联机启动引擎需要坐在驾驶位；维修与加油由服务器处理，维修不会补装缺失部件。");
    Action("启动引擎", "vehicle_start");
    Action("修复车辆", "vehicle_repair");
    Action("加满汽油", "vehicle_fuel");
    Action("传送到车辆旁空地", "vehicle_teleport");
    components::EndCard();
    cards::EndColumns();
}
}  // namespace pztrainer::ui
