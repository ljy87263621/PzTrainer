#include "ui/character_status.hpp"

#include <imgui.h>

#include "bridge/player_ammo_bridge.hpp"
#include "bridge/player_weapon_reliability_bridge.hpp"
#include "bridge/player_carry_bridge.hpp"
#include "bridge/player_condition_bridge.hpp"
#include "bridge/player_health_bridge.hpp"
#include "bridge/player_movement_bridge.hpp"
#include "bridge/player_teleport_bridge.hpp"
#include "bridge/player_resource_bridge.hpp"
#include "bridge/farming_mode_bridge.hpp"
#include "bridge/free_build_bridge.hpp"
#include "bridge/multi_hit_bridge.hpp"
#include "bridge/timed_action_bridge.hpp"
#include "settings/localization.hpp"
#include "ui/components.hpp"
#include "ui/player_effect_editor.hpp"

namespace pztrainer::ui {
void DrawCharacterStatus() {
    const bridge::PlayerHealthStatus& status = bridge::GetPlayerHealthStatus();
    const bridge::PlayerCarryStatus& carry_status =
        bridge::GetPlayerCarryStatus();
    const bridge::PlayerConditionStatus& condition_status =
        bridge::GetPlayerConditionStatus();
    const bridge::PlayerResourceStatus& resource_status =
        bridge::GetPlayerResourceStatus();
    const bridge::PlayerMovementStatus& movement_status =
        bridge::GetPlayerMovementStatus();
    const bridge::PlayerTeleportStatus& teleport_status =
        bridge::GetPlayerTeleportStatus();
    const bridge::PlayerAmmoStatus& ammo_status = bridge::GetPlayerAmmoStatus();
    const bridge::PlayerWeaponReliabilityStatus& weapon_reliability_status =
        bridge::GetPlayerWeaponReliabilityStatus();
    const bridge::MultiHitStatus& multi_hit_status =
        bridge::GetMultiHitStatus();
    const bridge::FreeBuildStatus& free_build_status =
        bridge::GetFreeBuildStatus();
    const bridge::FarmingModeStatus& farming_mode_status =
        bridge::GetFarmingModeStatus();
    const bridge::TimedActionStatus& timed_action_status =
        bridge::GetTimedActionStatus();

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 0.0f));
    if (!ImGui::BeginTable(
            "CharacterStatusCards", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    components::SectionLabel("生命保护");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "HealthProtection", nullptr, ImVec2(0.0f, 0.0f));

    bool infinite_health = status.infinite_health_enabled;
    if (components::CompactToggleRow("无限生命", &infinite_health)) {
        bridge::SetInfiniteHealthEnabled(infinite_health);
    }

    if (status.invincibility_available) {
        bool invincibility = status.invincibility_enabled;
        if (components::CompactToggleRow(
                "无敌模式", &invincibility, false)) {
            bridge::SetInvincibilityEnabled(invincibility);
        }
    } else {
        components::CompactDisabledToggleRow(
            "无敌模式", false,
            "该功能在线不可用。进入联机后会自动关闭，只保留全身快速恢复与服务器伤势同步。",
            true);
    }

    if (movement_status.no_clip_available) {
        bool no_clip = movement_status.no_clip_enabled;
        if (components::CompactToggleRow(
                "人物穿墙", &no_clip, false)) {
            bridge::SetNoClipEnabled(no_clip);
        }
    } else {
        components::CompactDisabledToggleRow(
            "人物穿墙", false,
            "人物移动桥接尚未初始化。",
            false);
    }
    if (carry_status.available) {
        bool unlimited_carry = carry_status.enabled;
        if (components::CompactToggleRow(
                "无限负重", &unlimited_carry)) {
            bridge::SetUnlimitedCarryEnabled(unlimited_carry);
        }
        float carry_multiplier = carry_status.multiplier;
        ImGui::BeginGroup();
        if (components::StepperRow(
                "最大负重倍率", &carry_multiplier, 1.0f, 100.0f,
                0.05f, "%.2fx", false)) {
            bridge::SetCarryWeightMultiplier(carry_multiplier);
        }
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "启用后，最大负重等于游戏当前原版最大负重乘以此倍率。倍率不会逐帧重复叠加；联机同步只在目标整数变化时发送。");
        }
    } else {
        components::CompactDisabledToggleRow(
            "无限负重", false, "无限负重桥接尚未初始化。",
            false);
    }
    if (condition_status.fatigue_available) {
        bool fatigue_protection = condition_status.fatigue_enabled;
        if (components::CompactToggleRow("无疲劳", &fatigue_protection)) {
            bridge::SetFatigueProtectionEnabled(fatigue_protection);
        }
    } else {
        components::CompactDisabledToggleRow(
            "无疲劳", false,
            "当前联机角色缺少 CanModifyBodyStats 权限，服务器不会接受该属性同步。"
        );
    }
    if (condition_status.panic_available) {
        bool panic_protection = condition_status.panic_enabled;
        if (components::CompactToggleRow("无恐慌", &panic_protection)) {
            bridge::SetPanicProtectionEnabled(panic_protection);
        }
    } else {
        components::CompactDisabledToggleRow(
            "无恐慌", false,
            "当前联机角色缺少 CanModifyBodyStats 权限，服务器不会接受该属性同步。"
        );
    }
    if (condition_status.hunger_available) {
        bool hunger_protection = condition_status.hunger_enabled;
        if (components::CompactToggleRow("无饥饿", &hunger_protection)) {
            bridge::SetHungerProtectionEnabled(hunger_protection);
        }
    } else {
        components::CompactDisabledToggleRow(
            "无饥饿", false,
            "当前联机角色缺少 CanModifyBodyStats 权限，服务器不会接受该属性同步。"
        );
    }
    if (condition_status.thirst_available) {
        bool thirst_protection = condition_status.thirst_enabled;
        if (components::CompactToggleRow("无口渴", &thirst_protection)) {
            bridge::SetThirstProtectionEnabled(thirst_protection);
        }
    } else {
        components::CompactDisabledToggleRow(
            "无口渴", false, "当前会话没有可用的口渴同步路径。"
        );
    }
    if (condition_status.negative_moodles_available) {
        bool negative_moodles = condition_status.negative_moodles_enabled;
        if (components::CompactToggleRow("移除负面心情", &negative_moodles)) {
            bridge::SetNegativeMoodlesProtectionEnabled(negative_moodles);
        }
        if (ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "只清理可编辑的负面角色属性；不会修改体温、负重或实际伤口。");
        }
    } else {
        components::CompactDisabledToggleRow(
            "移除负面心情", false,
            "当前联机角色缺少 CanModifyBodyStats 权限，服务器不会接受该属性同步。"
        );
    }
    if (condition_status.infection_immunity_available) {
        bool infection_immunity = condition_status.infection_immunity_enabled;
        if (components::CompactToggleRow(
                "免疫感染/咬伤", &infection_immunity)) {
            bridge::SetInfectionImmunityEnabled(infection_immunity);
        }
        if (ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "清除尸毒感染、假感染、普通伤口感染和咬伤状态，并通过 PlayerDamage 同步；不代表服务器不会产生新的伤害事件。");
        }
    } else {
        components::CompactDisabledToggleRow(
            "免疫感染/咬伤", false, "当前会话没有可用的伤势同步路径。"
        );
    }
    bool map_teleport = teleport_status.enabled;
    if (components::CompactToggleRow(
            "地图点击传送", &map_teleport, false)) {
        bridge::SetPlayerTeleportEnabled(map_teleport);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "打开世界地图后，按住 Alt 并点击鼠标左键。联机服务器仍可能因速度或穿墙检查踢出玩家。");
    }
    if (teleport_status.enabled) {
        if (teleport_status.cooldown_active) {
            ImGui::TextDisabled(
                settings::Translate("下一次传送需要等待 %u 秒"),
                teleport_status.cooldown_remaining_seconds);
        } else {
            ImGui::TextDisabled(
                "%s", settings::Translate(teleport_status.message.c_str()));
        }
    }
    components::EndCard();

    ImGui::TableSetColumnIndex(1);
    components::SectionLabel("体力与装备");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "PlayerResources", nullptr, ImVec2(0.0f, 0.0f));
    bool endurance_recovery = resource_status.endurance_recovery_enabled;
    if (components::CompactToggleRow("联机休息补偿／快速体力", &endurance_recovery)) {
        bridge::SetEnduranceRecoveryEnabled(endurance_recovery);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "不生成也不消耗任何物品。联机跑步／冲刺时额外提交普通移动状态，让服务器走自然恢复分支；攻击的独立体力消耗、严重超重状态仍可能短暂扣除，停止攻击后由服务器自然恢复。普通定时动作、载具状态或服务器反作弊可能打断／拒绝补偿；这不是权限版的真正无限体力。单人模式仍直接锁定满体力。"
        );
    }
    if (endurance_recovery &&
        resource_status.session_mode ==
            bridge::PlayerResourceSessionMode::MultiplayerClient) {
        ImGui::TextDisabled(
            "%s", settings::Translate(resource_status.message.c_str()));
    }
    bool durability_protection = resource_status.durability_protection_enabled;
    if (components::CompactToggleRow(
            "手持装备无限耐久", &durability_protection)) {
        bridge::SetDurabilityProtectionEnabled(durability_protection);
    }
    bool infinite_ammo = ammo_status.enabled;
    if (components::CompactToggleRow("无限子弹", &infinite_ammo, false)) {
        bridge::SetInfiniteAmmoEnabled(infinite_ammo);
    }
    bool no_weapon_jam = weapon_reliability_status.enabled;
    if (components::CompactToggleRow("枪械防卡壳", &no_weapon_jam, false)) {
        bridge::SetNoWeaponJamEnabled(no_weapon_jam);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "仅对当前主手远程枪械生效。开启时把原版卡壳概率临时设为 0；切枪或关闭后恢复该枪原值。若枪械已经卡壳，会清除状态并通过 SyncHandWeaponFields 同步服务器。不会修改弹药、射速、装填或耐久，普通联机账号可用。"
        );
    }
    components::EndCard();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    components::SectionLabel("联机战斗与建造");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    components::BeginCompactCard(
        "MultiplayerCombatBuild", nullptr, ImVec2(0.0f, 0.0f));
    bool multi_hit = multi_hit_status.enabled;
    if (components::CompactToggleRow("多目标攻击僵尸", &multi_hit)) {
        bridge::SetMultiHitEnabled(multi_hit);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "使用游戏原版多目标命中列表；服务器仍会检查武器、距离、目标、PVP 规则和反作弊。无需管理权限。如果服务器允许 PVP，附近可攻击玩家也可能进入原版挥击列表。联机实战仍需验证。"
        );
    }
    bool free_build = free_build_status.enabled;
    if (components::CompactToggleRow(
            "创造模式／免费建造", &free_build, false)) {
        bridge::SetFreeBuildEnabled(free_build);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "不刷物品，也不伪造背包材料。仅允许占地 1 格、只生成 1 个结构的建筑免费构建；多格、多层、窗户或复合结构会生成实体幽灵且不可正常拆除，因此会直接禁止构建。无需管理权限。"
        );
    }
    bool farming_mode = farming_mode_status.enabled;
    if (components::CompactToggleRow(
            "作物耕种模式", &farming_mode, false)) {
        bridge::SetFarmingModeEnabled(farming_mode);
    }
    if (ImGui::IsItemHovered()) {
        components::RoundedTooltip(
            "启用原版农业调试菜单，可催熟、补满水分并调整作物状态。请求由服务器农业系统执行；普通账号无需管理权限，不会在背包中刷出种子、工具或水。"
        );
    }
    if (timed_action_status.available) {
        bool timed_action = timed_action_status.enabled;
        if (components::CompactToggleRow(
                "瞬间完成定时动作", &timed_action, false)) {
            bridge::SetTimedActionInstantEnabled(timed_action);
        }
        if (ImGui::IsItemHovered()) {
            components::RoundedTooltip(
                "仅用于单机。启用后，新创建的原版定时动作使用最短时长；已在队列中的动作需要重新开始。"
            );
        }
    } else {
        components::CompactDisabledToggleRow(
            "瞬间完成定时动作", false,
            "联机动作会在服务器重新创建并计算时长；普通账号无法把读书等服务器动作改成瞬间完成。",
            true);
    }
    if (multi_hit_status.enabled && !multi_hit_status.applied) {
        ImGui::TextDisabled(
            "%s", settings::Translate(multi_hit_status.message.c_str()));
    }
    if (free_build_status.operation_pending ||
        (free_build_status.enabled && !free_build_status.applied)) {
        ImGui::TextDisabled(
            "%s", settings::Translate(free_build_status.message.c_str()));
    }
    if (farming_mode_status.enabled && !farming_mode_status.applied) {
        ImGui::TextDisabled(
            "%s", settings::Translate(farming_mode_status.message.c_str()));
    }
    if (timed_action_status.enabled && !timed_action_status.applied) {
        ImGui::TextDisabled(
            "%s", settings::Translate(timed_action_status.message.c_str()));
    }
    components::EndCard();

    ImGui::EndTable();
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    DrawPlayerEffectEditor();
}

}  // namespace pztrainer::ui
