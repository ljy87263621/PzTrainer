#include "bridge/lua_feature_registry.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "bridge/player_ammo_bridge.hpp"
#include "bridge/player_carry_bridge.hpp"
#include "bridge/player_condition_bridge.hpp"
#include "bridge/player_health_bridge.hpp"
#include "bridge/player_movement_bridge.hpp"
#include "bridge/player_resource_bridge.hpp"
#include "bridge/player_teleport_bridge.hpp"
#include "bridge/world_map_reveal_bridge.hpp"
#include "bridge/world_map_player_bridge.hpp"
#include "bridge/world_visual_bridge.hpp"
#include "features/aim/aim_settings.hpp"
#include "features/visual/animal_visual_settings.hpp"
#include "features/visual/player_visual_settings.hpp"
#include "features/visual/vehicle_visual_settings.hpp"
#include "features/visual/visual_settings.hpp"
#include "features/visual/weapon_ray.hpp"
#include "settings/ui_preferences.hpp"

namespace pztrainer::bridge {
namespace {

struct Availability {
    bool available = true;
    std::string reason;
};

using Getter = std::function<LuaFeatureValue()>;
using Setter = std::function<bool(const LuaFeatureValue&, std::string&)>;
using AvailabilityGetter = std::function<Availability()>;

struct Entry {
    std::string path;
    std::string group;
    std::string label;
    LuaFeatureValueType type = LuaFeatureValueType::Boolean;
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 0.0;
    std::vector<std::string> options;
    Getter getter;
    Setter setter;
    AvailabilityGetter availability;
};

struct PendingWrite {
    std::string path;
    LuaFeatureValue value;
};

std::mutex g_state_mutex;
std::vector<LuaFeatureDescriptor> g_snapshot;
std::vector<PendingWrite> g_pending_writes;

LuaFeatureValue BooleanValue(bool value) {
    LuaFeatureValue result;
    result.type = LuaFeatureValueType::Boolean;
    result.boolean = value;
    return result;
}

LuaFeatureValue NumberValue(double value) {
    LuaFeatureValue result;
    result.type = LuaFeatureValueType::Number;
    result.number = value;
    return result;
}

LuaFeatureValue IntegerValue(std::int64_t value) {
    LuaFeatureValue result;
    result.type = LuaFeatureValueType::Integer;
    result.integer = value;
    return result;
}

LuaFeatureValue EnumerationValue(std::string value) {
    LuaFeatureValue result;
    result.type = LuaFeatureValueType::Enumeration;
    result.enumeration = std::move(value);
    return result;
}

LuaFeatureValue ColorValue(const ImVec4& value) {
    LuaFeatureValue result;
    result.type = LuaFeatureValueType::Color;
    result.color = {value.x, value.y, value.z, value.w};
    return result;
}

Availability AlwaysAvailable() { return {}; }

void AddBool(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, bool* value) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Boolean;
    entry.getter = [value] { return BooleanValue(*value); };
    entry.setter = [value](const LuaFeatureValue& requested, std::string&) {
        *value = requested.boolean;
        return true;
    };
    entry.availability = &AlwaysAvailable;
    entries.push_back(std::move(entry));
}

void AddBoolCallback(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, std::function<bool()> getter,
    std::function<void(bool)> setter,
    AvailabilityGetter availability = &AlwaysAvailable) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Boolean;
    entry.getter = [getter = std::move(getter)] {
        return BooleanValue(getter());
    };
    entry.setter = [setter = std::move(setter)](
                       const LuaFeatureValue& requested, std::string&) {
        setter(requested.boolean);
        return true;
    };
    entry.availability = std::move(availability);
    entries.push_back(std::move(entry));
}

void AddNumber(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, float* value, double minimum, double maximum,
    double step) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Number;
    entry.minimum = minimum;
    entry.maximum = maximum;
    entry.step = step;
    entry.getter = [value] { return NumberValue(*value); };
    entry.setter = [value, minimum, maximum](
                       const LuaFeatureValue& requested, std::string&) {
        *value = static_cast<float>(std::clamp(
            requested.number, minimum, maximum));
        return true;
    };
    entry.availability = &AlwaysAvailable;
    entries.push_back(std::move(entry));
}

void AddNumberCallback(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, std::function<double()> getter,
    std::function<void(double)> setter, double minimum, double maximum,
    double step, AvailabilityGetter availability = &AlwaysAvailable) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Number;
    entry.minimum = minimum;
    entry.maximum = maximum;
    entry.step = step;
    entry.getter = [getter = std::move(getter)] {
        return NumberValue(getter());
    };
    entry.setter = [setter = std::move(setter), minimum, maximum](
                       const LuaFeatureValue& requested, std::string&) {
        setter(std::clamp(requested.number, minimum, maximum));
        return true;
    };
    entry.availability = std::move(availability);
    entries.push_back(std::move(entry));
}

template <typename Integer>
void AddInteger(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, Integer* value, std::int64_t minimum,
    std::int64_t maximum, std::int64_t step) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Integer;
    entry.minimum = static_cast<double>(minimum);
    entry.maximum = static_cast<double>(maximum);
    entry.step = static_cast<double>(step);
    entry.getter = [value] {
        return IntegerValue(static_cast<std::int64_t>(*value));
    };
    entry.setter = [value, minimum, maximum](
                       const LuaFeatureValue& requested, std::string&) {
        *value = static_cast<Integer>(std::clamp(
            requested.integer, minimum, maximum));
        return true;
    };
    entry.availability = &AlwaysAvailable;
    entries.push_back(std::move(entry));
}

template <typename Enumeration>
void AddEnumeration(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, Enumeration* value,
    std::vector<std::pair<std::string, Enumeration>> options) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Enumeration;
    for (const auto& option : options) entry.options.push_back(option.first);
    entry.getter = [value, options] {
        const auto found = std::find_if(
            options.begin(), options.end(),
            [value](const auto& option) { return option.second == *value; });
        return EnumerationValue(
            found == options.end() ? std::string{} : found->first);
    };
    entry.setter = [value, options](
                       const LuaFeatureValue& requested, std::string& error) {
        const auto found = std::find_if(
            options.begin(), options.end(), [&](const auto& option) {
                return option.first == requested.enumeration;
            });
        if (found == options.end()) {
            error = "Enumeration value is not supported.";
            return false;
        }
        *value = found->second;
        return true;
    };
    entry.availability = &AlwaysAvailable;
    entries.push_back(std::move(entry));
}

void AddColor(
    std::vector<Entry>& entries, std::string path, std::string group,
    std::string label, ImVec4* value) {
    Entry entry;
    entry.path = std::move(path);
    entry.group = std::move(group);
    entry.label = std::move(label);
    entry.type = LuaFeatureValueType::Color;
    entry.getter = [value] { return ColorValue(*value); };
    entry.setter = [value](const LuaFeatureValue& requested, std::string&) {
        value->x = std::clamp(requested.color[0], 0.0f, 1.0f);
        value->y = std::clamp(requested.color[1], 0.0f, 1.0f);
        value->z = std::clamp(requested.color[2], 0.0f, 1.0f);
        value->w = std::clamp(requested.color[3], 0.0f, 1.0f);
        return true;
    };
    entry.availability = &AlwaysAvailable;
    entries.push_back(std::move(entry));
}

void AddStateColor(
    std::vector<Entry>& entries, const std::string& prefix,
    const std::string& group, const std::string& label,
    features::visual::StateColor& colors) {
    AddColor(entries, prefix + ".default", group, label + " - 默认", &colors.normal);
    AddColor(entries, prefix + ".behind_wall", group, label + " - 墙后", &colors.behind_wall);
    AddColor(entries, prefix + ".in_view", group, label + " - 视野内", &colors.in_view);
    AddBool(
        entries, prefix + ".behind_wall_enabled", group,
        label + " - 启用墙后颜色", &colors.behind_wall_enabled);
    AddBool(
        entries, prefix + ".in_view_enabled", group,
        label + " - 启用视野内颜色", &colors.in_view_enabled);
}

std::vector<std::pair<std::string, features::visual::ZombieModelEffect>>
ModelEffectOptions() {
    using features::visual::ZombieModelEffect;
    return {
        {"disabled", ZombieModelEffect::Disabled},
        {"shaded", ZombieModelEffect::Shaded},
        {"solid", ZombieModelEffect::Solid},
        {"glow", ZombieModelEffect::Glow},
        {"glow_outline", ZombieModelEffect::GlowOutline},
        {"iridescent", ZombieModelEffect::Iridescent},
        {"water_flow", ZombieModelEffect::WaterFlow},
        {"glossy", ZombieModelEffect::Glossy},
    };
}

std::vector<std::pair<std::string, features::aim::WeaponGroup>>
WeaponGroupOptions() {
    using features::aim::WeaponGroup;
    return {
        {"global", WeaponGroup::Global}, {"pistol", WeaponGroup::Pistol},
        {"shotgun", WeaponGroup::Shotgun}, {"smg", WeaponGroup::Smg},
        {"rifle", WeaponGroup::Rifle}, {"sniper", WeaponGroup::Sniper},
    };
}

void AddAimFeatures(std::vector<Entry>& entries) {
    using namespace features::aim;
    AimSettings& settings = GetAimSettings();
    AddBool(entries, "aim.legit.master_enabled", "aim.legit", "启用辅助瞄准", &settings.legit_enabled);
    AddEnumeration(
        entries, "aim.legit.selected_weapon", "aim.legit", "辅助瞄准武器预设",
        &settings.selected_legit_weapon, WeaponGroupOptions());
    AddBool(entries, "aim.rage.master_enabled", "aim.rage", "启用 Rage", &settings.rage_enabled);
    AddEnumeration(
        entries, "aim.rage.selected_weapon", "aim.rage", "Rage 武器预设",
        &settings.selected_rage_weapon, WeaponGroupOptions());

    const std::vector<std::string> names{
        "global", "pistol", "shotgun", "smg", "rifle", "sniper"};
    for (std::size_t index = 0; index < names.size(); ++index) {
        LegitWeaponSettings& legit = settings.legit_presets[index];
        const std::string legit_prefix = "aim.legit.presets." + names[index];
        AddBool(entries, legit_prefix + ".enabled", "aim.legit", "启用预设", &legit.enabled);
        AddBool(entries, legit_prefix + ".automatic_aim", "aim.legit", "自动辅助瞄准", &legit.automatic_aim);
        AddBool(entries, legit_prefix + ".target_zombies", "aim.legit", "瞄准僵尸", &legit.target_zombies);
        AddBool(entries, legit_prefix + ".target_players", "aim.legit", "瞄准 PVP 玩家", &legit.target_players);
        AddBool(entries, legit_prefix + ".wall_check", "aim.legit", "检测墙壁", &legit.wall_check);
        AddBool(entries, legit_prefix + ".prioritize_upright", "aim.legit", "优先站立僵尸", &legit.prioritize_upright);
        AddBool(entries, legit_prefix + ".remove_visual_recoil", "aim.legit", "移除视觉后坐力", &legit.remove_visual_recoil);
        AddNumber(entries, legit_prefix + ".range", "aim.legit", "瞄准范围", &legit.range, 1.0, 100.0, 1.0);
        AddNumber(entries, legit_prefix + ".smoothing", "aim.legit", "自瞄平滑", &legit.smoothing, 1.0, 100.0, 1.0);
        AddNumber(entries, legit_prefix + ".accuracy", "aim.legit", "命中精度", &legit.accuracy, 0.0, 100.0, 1.0);
        AddNumber(entries, legit_prefix + ".minimum_damage", "aim.legit", "伤害偏好阈值", &legit.minimum_damage, 0.0, 100.0, 1.0);
        AddInteger(entries, legit_prefix + ".target_points", "aim.legit", "击中点位掩码", &legit.target_points, 0, (1 << 15) - 1, 1);

        RageWeaponSettings& rage = settings.rage_presets[index];
        const std::string rage_prefix = "aim.rage.presets." + names[index];
        AddBool(entries, rage_prefix + ".enabled", "aim.rage", "启用预设", &rage.enabled);
        AddBool(entries, rage_prefix + ".automatic_aim", "aim.rage", "自动瞄准", &rage.automatic_aim);
        AddBool(entries, rage_prefix + ".target_zombies", "aim.rage", "瞄准僵尸", &rage.target_zombies);
        AddBool(entries, rage_prefix + ".target_players", "aim.rage", "瞄准 PVP 玩家", &rage.target_players);
        AddBool(entries, rage_prefix + ".automatic_stop", "aim.rage", "自动急停", &rage.automatic_stop);
        AddBool(entries, rage_prefix + ".automatic_fire", "aim.rage", "自动开枪", &rage.automatic_fire);
        AddBool(entries, rage_prefix + ".silent_aim", "aim.rage", "静默瞄准", &rage.silent_aim);
        AddBool(entries, rage_prefix + ".wall_check", "aim.rage", "检测墙壁", &rage.wall_check);
        AddBool(entries, rage_prefix + ".no_spread", "aim.rage", "无扩散", &rage.no_spread);
        AddBool(entries, rage_prefix + ".no_recoil", "aim.rage", "无视觉后坐力", &rage.no_recoil);
        AddBool(entries, rage_prefix + ".double_tap", "aim.rage", "DT 双发模式", &rage.double_tap);
        AddNumber(entries, rage_prefix + ".range", "aim.rage", "攻击范围", &rage.range, 1.0, 100.0, 1.0);
        AddNumber(entries, rage_prefix + ".accuracy", "aim.rage", "命中精度", &rage.accuracy, 0.0, 100.0, 1.0);
        RageWeaponSettings* rage_settings = &rage;
        AddNumberCallback(
            entries, rage_prefix + ".minimum_damage", "aim.rage", "最低伤害",
            [rage_settings] { return rage_settings->minimum_damage; },
            [rage_settings](double value) {
                rage_settings->minimum_damage = static_cast<float>(value);
                rage_settings->maximum_damage = std::max(
                    rage_settings->maximum_damage,
                    rage_settings->minimum_damage);
            },
            0.1, 10.0, 0.1);
        AddNumberCallback(
            entries, rage_prefix + ".maximum_damage", "aim.rage", "最高伤害",
            [rage_settings] { return rage_settings->maximum_damage; },
            [rage_settings](double value) {
                rage_settings->maximum_damage = std::max(
                    static_cast<float>(value),
                    rage_settings->minimum_damage);
            },
            0.1, 10.0, 0.1);
        AddInteger(entries, rage_prefix + ".target_points", "aim.rage", "击中点位掩码", &rage.target_points, 0, (1 << 15) - 1, 1);
    }
}

template <typename Settings>
void AddModelFeatures(
    std::vector<Entry>& entries, const std::string& prefix,
    const std::string& group, Settings& settings) {
    AddStateColor(entries, prefix + ".model_color", group, "模型颜色", settings.model_colors);
    AddEnumeration(
        entries, prefix + ".model_effect", group, "模型效果",
        &settings.model_effect, ModelEffectOptions());
    AddBool(entries, prefix + ".model_edge_glow", group, "模型边缘发光", &settings.model_edge_glow);
    AddColor(entries, prefix + ".model_edge_glow_color", group, "模型边缘颜色", &settings.model_edge_glow_color);
    AddBool(entries, prefix + ".force_model_visibility", group, "强制显示上色模型", &settings.force_model_visibility);
}

void AddVisualFeatures(std::vector<Entry>& entries) {
    using namespace features::visual;
    VisualSettings& zombie = GetVisualSettings();
    AddBool(entries, "visual.zombie.enabled", "visual.zombie", "启用僵尸透视", &zombie.zombie_esp);
    AddBool(entries, "visual.zombie.show_box", "visual.zombie", "显示方框", &zombie.colored_marker);
    AddBool(entries, "visual.zombie.show_name", "visual.zombie", "显示名称", &zombie.show_name);
    AddBool(entries, "visual.zombie.show_distance", "visual.zombie", "显示距离", &zombie.show_distance);
    AddBool(entries, "visual.zombie.show_skeleton", "visual.zombie", "显示骨骼", &zombie.show_skeleton);
    AddBool(entries, "visual.zombie.show_health_bar", "visual.zombie", "显示血条", &zombie.show_health_bar);
    AddStateColor(entries, "visual.zombie.box_color", "visual.zombie", "方框颜色", zombie.box_colors);
    AddStateColor(entries, "visual.zombie.name_color", "visual.zombie", "名称颜色", zombie.name_colors);
    AddStateColor(entries, "visual.zombie.distance_color", "visual.zombie", "距离颜色", zombie.distance_colors);
    AddStateColor(entries, "visual.zombie.skeleton_color", "visual.zombie", "骨骼颜色", zombie.skeleton_colors);
    AddNumber(entries, "visual.zombie.max_distance", "visual.zombie", "最大显示距离", &zombie.max_distance, 5.0, 100.0, 5.0);
    AddModelFeatures(entries, "visual.zombie", "visual.zombie", zombie);

    PlayerVisualSettings& player = GetPlayerVisualSettings();
    AddBool(entries, "visual.player.enabled", "visual.player", "启用玩家透视", &player.player_esp);
    AddBool(entries, "visual.player.show_box", "visual.player", "显示方框", &player.colored_marker);
    AddBool(entries, "visual.player.show_name", "visual.player", "显示名称", &player.show_name);
    AddBool(entries, "visual.player.show_distance", "visual.player", "显示距离", &player.show_distance);
    AddBool(entries, "visual.player.show_skeleton", "visual.player", "显示骨骼", &player.show_skeleton);
    AddBool(entries, "visual.player.show_health_bar", "visual.player", "显示血条", &player.show_health_bar);
    AddBool(entries, "visual.player.direction_arrow", "visual.player", "方向箭头", &player.ray_esp);
    AddColor(entries, "visual.player.direction_arrow_color", "visual.player", "方向箭头颜色", &player.ray_color);
    AddBool(entries, "visual.player.show_admin_marker", "visual.player", "标记服务器管理员", &player.show_admin_marker);
    AddStateColor(entries, "visual.player.box_color", "visual.player", "方框颜色", player.box_colors);
    AddStateColor(entries, "visual.player.name_color", "visual.player", "名称颜色", player.name_colors);
    AddStateColor(entries, "visual.player.distance_color", "visual.player", "距离颜色", player.distance_colors);
    AddStateColor(entries, "visual.player.skeleton_color", "visual.player", "骨骼颜色", player.skeleton_colors);
    AddNumber(entries, "visual.player.max_distance", "visual.player", "最大显示距离", &player.max_distance, 5.0, 150.0, 5.0);
    AddModelFeatures(entries, "visual.player", "visual.player", player);

    AnimalVisualSettings& animal = GetAnimalVisualSettings();
    AddBool(entries, "visual.animal.enabled", "visual.animal", "启用动物透视", &animal.animal_esp);
    AddBool(entries, "visual.animal.show_box", "visual.animal", "显示方框", &animal.colored_marker);
    AddBool(entries, "visual.animal.show_name", "visual.animal", "显示名称", &animal.show_name);
    AddBool(entries, "visual.animal.show_distance", "visual.animal", "显示距离", &animal.show_distance);
    AddBool(entries, "visual.animal.show_skeleton", "visual.animal", "显示骨骼", &animal.show_skeleton);
    AddBool(entries, "visual.animal.show_health_bar", "visual.animal", "显示血条", &animal.show_health_bar);
    AddBool(entries, "visual.animal.direction_arrow", "visual.animal", "方向箭头", &animal.ray_esp);
    AddColor(entries, "visual.animal.direction_arrow_color", "visual.animal", "方向箭头颜色", &animal.ray_color);
    AddStateColor(entries, "visual.animal.box_color", "visual.animal", "方框颜色", animal.box_colors);
    AddStateColor(entries, "visual.animal.name_color", "visual.animal", "名称颜色", animal.name_colors);
    AddStateColor(entries, "visual.animal.distance_color", "visual.animal", "距离颜色", animal.distance_colors);
    AddStateColor(entries, "visual.animal.skeleton_color", "visual.animal", "骨骼颜色", animal.skeleton_colors);
    AddNumber(entries, "visual.animal.max_distance", "visual.animal", "最大显示距离", &animal.max_distance, 5.0, 150.0, 5.0);
    AddModelFeatures(entries, "visual.animal", "visual.animal", animal);

    VehicleVisualSettings& vehicle = GetVehicleVisualSettings();
    AddBool(entries, "visual.vehicle.enabled", "visual.vehicle", "启用载具透视", &vehicle.vehicle_esp);
    AddBool(entries, "visual.vehicle.show_box", "visual.vehicle", "显示方框", &vehicle.colored_marker);
    AddBool(entries, "visual.vehicle.show_name", "visual.vehicle", "显示名称", &vehicle.show_name);
    AddBool(entries, "visual.vehicle.show_distance", "visual.vehicle", "显示距离", &vehicle.show_distance);
    AddBool(entries, "visual.vehicle.show_details", "visual.vehicle", "显示车辆详情", &vehicle.show_details);
    AddBool(entries, "visual.vehicle.show_engine_bar", "visual.vehicle", "显示发动机状态条", &vehicle.show_engine_bar);
    AddBool(entries, "visual.vehicle.direction_arrow", "visual.vehicle", "方向箭头", &vehicle.ray_esp);
    AddColor(entries, "visual.vehicle.direction_arrow_color", "visual.vehicle", "方向箭头颜色", &vehicle.ray_color);
    AddStateColor(entries, "visual.vehicle.box_color", "visual.vehicle", "方框颜色", vehicle.box_colors);
    AddStateColor(entries, "visual.vehicle.name_color", "visual.vehicle", "名称颜色", vehicle.name_colors);
    AddStateColor(entries, "visual.vehicle.distance_color", "visual.vehicle", "距离颜色", vehicle.distance_colors);
    AddNumber(entries, "visual.vehicle.max_distance", "visual.vehicle", "最大显示距离", &vehicle.max_distance, 10.0, 250.0, 10.0);
    AddModelFeatures(entries, "visual.vehicle", "visual.vehicle", vehicle);

    WeaponRaySettings& ray = GetWeaponRaySettings();
    AddBool(entries, "world.weapon_ray.enabled", "world.visual", "显示枪口射线", &ray.enabled);
    AddColor(entries, "world.weapon_ray.color", "world.visual", "枪口射线颜色", &ray.color);
}

Availability AvailableWhen(bool available, const std::string& reason) {
    return {available, available ? std::string{} : reason};
}

void AddBridgeFeatures(std::vector<Entry>& entries) {
    AddBoolCallback(
        entries, "character.infinite_health", "character", "无限生命",
        [] { return GetPlayerHealthStatus().infinite_health_enabled; },
        &SetInfiniteHealthEnabled);
    AddBoolCallback(
        entries, "character.invincibility", "character", "无敌模式",
        [] { return GetPlayerHealthStatus().invincibility_enabled; },
        &SetInvincibilityEnabled,
        [] {
            const auto& status = GetPlayerHealthStatus();
            return AvailableWhen(status.invincibility_available, status.message);
        });
    AddBoolCallback(
        entries, "character.no_clip", "character", "人物穿墙",
        [] { return GetPlayerMovementStatus().no_clip_enabled; },
        &SetNoClipEnabled,
        [] {
            const auto& status = GetPlayerMovementStatus();
            return AvailableWhen(status.no_clip_available, status.message);
        });
    AddBoolCallback(
        entries, "character.unlimited_carry", "character", "无限负重",
        [] { return GetPlayerCarryStatus().enabled; },
        &SetUnlimitedCarryEnabled,
        [] {
            const auto& status = GetPlayerCarryStatus();
            return AvailableWhen(status.available, status.message);
        });
    AddNumberCallback(
        entries, "character.carry_multiplier", "character", "最大负重倍率",
        [] { return GetPlayerCarryStatus().multiplier; },
        [](double value) { SetCarryWeightMultiplier(static_cast<float>(value)); },
        1.0, 100.0, 0.05,
        [] {
            const auto& status = GetPlayerCarryStatus();
            return AvailableWhen(status.available, status.message);
        });

    AddBoolCallback(
        entries, "character.no_fatigue", "character", "无疲劳",
        [] { return GetPlayerConditionStatus().fatigue_enabled; },
        &SetFatigueProtectionEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.fatigue_available, status.message);
        });
    AddBoolCallback(
        entries, "character.no_panic", "character", "无恐慌",
        [] { return GetPlayerConditionStatus().panic_enabled; },
        &SetPanicProtectionEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.panic_available, status.message);
        });
    AddBoolCallback(
        entries, "character.no_hunger", "character", "无饥饿",
        [] { return GetPlayerConditionStatus().hunger_enabled; },
        &SetHungerProtectionEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.hunger_available, status.message);
        });
    AddBoolCallback(
        entries, "character.no_thirst", "character", "无口渴",
        [] { return GetPlayerConditionStatus().thirst_enabled; },
        &SetThirstProtectionEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.thirst_available, status.message);
        });
    AddBoolCallback(
        entries, "character.remove_negative_moodles", "character", "移除负面心情",
        [] { return GetPlayerConditionStatus().negative_moodles_enabled; },
        &SetNegativeMoodlesProtectionEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.negative_moodles_available, status.message);
        });
    AddBoolCallback(
        entries, "character.infection_immunity", "character", "免疫感染/咬伤",
        [] { return GetPlayerConditionStatus().infection_immunity_enabled; },
        &SetInfectionImmunityEnabled,
        [] {
            const auto& status = GetPlayerConditionStatus();
            return AvailableWhen(status.infection_immunity_available, status.message);
        });
    AddBoolCallback(
        entries, "character.map_teleport", "character", "地图点击传送",
        [] { return GetPlayerTeleportStatus().enabled; },
        &SetPlayerTeleportEnabled);
    AddBoolCallback(
        entries, "character.fast_endurance_recovery", "character", "快速恢复体力",
        [] { return GetPlayerResourceStatus().endurance_recovery_enabled; },
        &SetEnduranceRecoveryEnabled);
    AddBoolCallback(
        entries, "character.infinite_held_durability", "character", "手持装备无限耐久",
        [] { return GetPlayerResourceStatus().durability_protection_enabled; },
        &SetDurabilityProtectionEnabled);
    AddBoolCallback(
        entries, "character.infinite_ammo", "character", "无限子弹",
        [] { return GetPlayerAmmoStatus().enabled; },
        &SetInfiniteAmmoEnabled);

    AddBoolCallback(
        entries, "world.fake_daylight", "world.visual", "伪白天",
        [] { return GetWorldVisualStatus().fake_daylight_enabled; },
        &SetFakeDaylightEnabled);
    AddBoolCallback(
        entries, "world.brighten_darkness", "world.visual", "暗区增亮",
        [] { return GetWorldVisualStatus().dark_area_brightness_enabled; },
        &SetDarkAreaBrightnessEnabled);
    AddBoolCallback(
        entries, "world.force_360_vision", "world.visual", "360°全向视野",
        [] { return GetWorldVisualStatus().force_360_vision_enabled; },
        &SetForce360VisionEnabled);
    AddBoolCallback(
        entries, "world.reveal_unexplored", "world.visual", "移除未探索黑幕",
        [] { return GetWorldVisualStatus().reveal_unexplored_enabled; },
        &SetRevealUnexploredEnabled);
    AddBoolCallback(
        entries, "world.reveal_world_map", "world.visual", "解锁世界地图迷雾",
        [] { return GetWorldMapRevealStatus().enabled; },
        &SetWorldMapRevealEnabled);
    AddBoolCallback(
        entries, "world.show_map_players", "world.visual", "地图显示其他玩家",
        [] { return GetWorldMapPlayerStatus().enabled; },
        &SetWorldMapPlayerEnabled);

    Entry fog_mode;
    fog_mode.path = "world.fog_mode";
    fog_mode.group = "world.visual";
    fog_mode.label = "雾效果";
    fog_mode.type = LuaFeatureValueType::Enumeration;
    fog_mode.options = {"follow_game", "force_off", "force_on"};
    fog_mode.getter = [] {
        switch (GetWorldVisualStatus().fog_mode) {
            case WeatherVisualMode::ForceOff: return EnumerationValue("force_off");
            case WeatherVisualMode::ForceOn: return EnumerationValue("force_on");
            default: return EnumerationValue("follow_game");
        }
    };
    fog_mode.setter = [](const LuaFeatureValue& requested, std::string& error) {
        if (requested.enumeration == "follow_game") {
            SetFogVisualMode(WeatherVisualMode::FollowGame);
        } else if (requested.enumeration == "force_off") {
            SetFogVisualMode(WeatherVisualMode::ForceOff);
        } else if (requested.enumeration == "force_on") {
            SetFogVisualMode(WeatherVisualMode::ForceOn);
        } else {
            error = "Enumeration value is not supported.";
            return false;
        }
        return true;
    };
    fog_mode.availability = &AlwaysAvailable;
    entries.push_back(std::move(fog_mode));

    Entry precipitation_mode;
    precipitation_mode.path = "world.precipitation_mode";
    precipitation_mode.group = "world.visual";
    precipitation_mode.label = "降水效果";
    precipitation_mode.type = LuaFeatureValueType::Enumeration;
    precipitation_mode.options = {"follow_game", "force_off", "rain", "snow"};
    precipitation_mode.getter = [] {
        switch (GetWorldVisualStatus().precipitation_mode) {
            case PrecipitationVisualMode::ForceOff: return EnumerationValue("force_off");
            case PrecipitationVisualMode::Rain: return EnumerationValue("rain");
            case PrecipitationVisualMode::Snow: return EnumerationValue("snow");
            default: return EnumerationValue("follow_game");
        }
    };
    precipitation_mode.setter = [](const LuaFeatureValue& requested, std::string& error) {
        if (requested.enumeration == "follow_game") {
            SetPrecipitationVisualMode(PrecipitationVisualMode::FollowGame);
        } else if (requested.enumeration == "force_off") {
            SetPrecipitationVisualMode(PrecipitationVisualMode::ForceOff);
        } else if (requested.enumeration == "rain") {
            SetPrecipitationVisualMode(PrecipitationVisualMode::Rain);
        } else if (requested.enumeration == "snow") {
            SetPrecipitationVisualMode(PrecipitationVisualMode::Snow);
        } else {
            error = "Enumeration value is not supported.";
            return false;
        }
        return true;
    };
    precipitation_mode.availability = &AlwaysAvailable;
    entries.push_back(std::move(precipitation_mode));

    AddNumberCallback(
        entries, "world.fog_intensity", "world.visual", "雾强度",
        [] { return GetWorldVisualStatus().fog_intensity; },
        [](double value) { SetFogVisualIntensity(static_cast<float>(value)); },
        0.05, 1.0, 0.05);
    AddNumberCallback(
        entries, "world.precipitation_intensity", "world.visual", "降水强度",
        [] { return GetWorldVisualStatus().precipitation_intensity; },
        [](double value) {
            SetPrecipitationVisualIntensity(static_cast<float>(value));
        },
        0.05, 1.0, 0.05);

    AddBoolCallback(
        entries, "system.safety_mode", "system", "安全模式",
        &settings::IsSafeModeEnabled, &settings::SetSafeModeEnabled);
}

std::vector<Entry> BuildEntries() {
    std::vector<Entry> entries;
    AddAimFeatures(entries);
    AddVisualFeatures(entries);
    AddBridgeFeatures(entries);
    std::sort(
        entries.begin(), entries.end(),
        [](const Entry& left, const Entry& right) {
            return left.path < right.path;
        });
    return entries;
}

const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = BuildEntries();
    return entries;
}

const Entry* FindEntry(const std::string& path) {
    const std::vector<Entry>& entries = Entries();
    const auto found = std::lower_bound(
        entries.begin(), entries.end(), path,
        [](const Entry& entry, const std::string& key) {
            return entry.path < key;
        });
    return found != entries.end() && found->path == path ? &*found : nullptr;
}

bool PendingPath(const std::string& path) {
    return std::any_of(
        g_pending_writes.begin(), g_pending_writes.end(),
        [&](const PendingWrite& write) { return write.path == path; });
}

LuaFeatureDescriptor Describe(const Entry& entry) {
    LuaFeatureDescriptor result;
    result.path = entry.path;
    result.group = entry.group;
    result.label = entry.label;
    result.type = entry.type;
    result.value = entry.getter();
    result.writable = static_cast<bool>(entry.setter);
    result.minimum = entry.minimum;
    result.maximum = entry.maximum;
    result.step = entry.step;
    result.options = entry.options;
    const Availability availability = entry.availability
        ? entry.availability()
        : Availability{};
    result.available = availability.available;
    result.reason = availability.reason;
    return result;
}

bool Compatible(
    const LuaFeatureDescriptor& descriptor,
    const LuaFeatureValue& value) {
    return descriptor.type == value.type;
}

}  // namespace

const char* LuaFeatureValueTypeName(LuaFeatureValueType type) {
    switch (type) {
        case LuaFeatureValueType::Boolean: return "boolean";
        case LuaFeatureValueType::Number: return "number";
        case LuaFeatureValueType::Integer: return "integer";
        case LuaFeatureValueType::Enumeration: return "enum";
        case LuaFeatureValueType::Color: return "color";
    }
    return "unknown";
}

void ApplyPendingLuaFeatureWrites() {
    std::vector<PendingWrite> pending;
    {
        std::lock_guard lock(g_state_mutex);
        pending.swap(g_pending_writes);
    }
    for (const PendingWrite& write : pending) {
        const Entry* entry = FindEntry(write.path);
        if (entry == nullptr || !entry->setter) continue;
        const Availability availability = entry->availability
            ? entry->availability()
            : Availability{};
        if (!availability.available) continue;
        std::string ignored_error;
        entry->setter(write.value, ignored_error);
    }
}

void RefreshLuaFeatureRegistry() {
    std::vector<LuaFeatureDescriptor> snapshot;
    snapshot.reserve(Entries().size());
    for (const Entry& entry : Entries()) snapshot.push_back(Describe(entry));
    std::lock_guard lock(g_state_mutex);
    for (LuaFeatureDescriptor& descriptor : snapshot) {
        descriptor.pending = PendingPath(descriptor.path);
    }
    g_snapshot = std::move(snapshot);
}

std::vector<LuaFeatureDescriptor> SnapshotLuaFeatureRegistry() {
    std::lock_guard lock(g_state_mutex);
    return g_snapshot;
}

bool FindLuaFeature(
    const std::string& path, LuaFeatureDescriptor& descriptor) {
    std::lock_guard lock(g_state_mutex);
    const auto found = std::lower_bound(
        g_snapshot.begin(), g_snapshot.end(), path,
        [](const LuaFeatureDescriptor& feature, const std::string& key) {
            return feature.path < key;
        });
    if (found == g_snapshot.end() || found->path != path) return false;
    descriptor = *found;
    descriptor.pending = PendingPath(path);
    return true;
}

LuaFeatureWriteResult QueueLuaFeatureWrite(
    const std::string& path, LuaFeatureValue value) {
    LuaFeatureWriteResult result;
    std::lock_guard lock(g_state_mutex);
    const auto found = std::lower_bound(
        g_snapshot.begin(), g_snapshot.end(), path,
        [](const LuaFeatureDescriptor& feature, const std::string& key) {
            return feature.path < key;
        });
    if (found == g_snapshot.end() || found->path != path) {
        result.reason = g_snapshot.empty()
            ? "Feature registry is not ready."
            : "Feature path was not found.";
        return result;
    }
    result.feature = *found;
    if (!found->writable) {
        result.reason = "Feature is read-only.";
        return result;
    }
    if (!found->available) {
        result.reason = found->reason.empty()
            ? "Feature is not available in the current session."
            : found->reason;
        return result;
    }
    if (!Compatible(*found, value)) {
        result.reason = "Feature value type does not match the descriptor.";
        return result;
    }
    if (value.type == LuaFeatureValueType::Enumeration &&
        std::find(
            found->options.begin(), found->options.end(),
            value.enumeration) == found->options.end()) {
        result.reason = "Enumeration value is not supported.";
        return result;
    }
    const auto pending = std::find_if(
        g_pending_writes.begin(), g_pending_writes.end(),
        [&](const PendingWrite& write) { return write.path == path; });
    if (pending == g_pending_writes.end()) {
        g_pending_writes.push_back({path, std::move(value)});
    } else {
        pending->value = std::move(value);
    }
    result.ok = true;
    result.pending = true;
    result.feature.pending = true;
    return result;
}

}  // namespace pztrainer::bridge
