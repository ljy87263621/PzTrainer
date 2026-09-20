#include "settings/configuration_store.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <utility>

#include "bridge/lua_script_bridge.hpp"
#include "bridge/extension_bridge.hpp"
#include "bridge/farming_mode_bridge.hpp"
#include "bridge/free_build_bridge.hpp"
#include "bridge/multi_hit_bridge.hpp"
#include "bridge/timed_action_bridge.hpp"
#include "bridge/player_ammo_bridge.hpp"
#include "bridge/player_weapon_reliability_bridge.hpp"
#include "bridge/player_condition_bridge.hpp"
#include "bridge/player_health_bridge.hpp"
#include "bridge/player_movement_bridge.hpp"
#include "bridge/player_teleport_bridge.hpp"
#include "bridge/player_resource_bridge.hpp"
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

namespace pztrainer::settings {
namespace {

constexpr std::array<char, 8> kMagic{'P', 'Z', 'C', 'F', 'G', '0', '1', '\0'};
constexpr std::uint32_t kVersion = 16;
constexpr std::uint32_t kTargetPointMask = (1u << 15) - 1u;
constexpr std::uint32_t kMaximumLuaScripts = 256;
constexpr std::uint32_t kMaximumLuaControls = 4096;
constexpr std::uint32_t kMaximumLuaSelectedItems = 4096;
constexpr std::uint32_t kMaximumLuaStringBytes = 32768;

struct FileHeader {
    std::array<char, 8> magic{};
    std::uint32_t version = 0;
    std::uint32_t payload_size = 0;
};

struct WorldVisualConfigurationV14 {
    bool fake_daylight = false;
    bool dark_area_brightness = false;
    bool force_360_vision = false;
    bool reveal_unexplored = false;
    bridge::WeatherVisualMode fog_mode = bridge::WeatherVisualMode::FollowGame;
    bridge::PrecipitationVisualMode precipitation_mode =
        bridge::PrecipitationVisualMode::FollowGame;
    float fog_intensity = 0.75f;
    float precipitation_intensity = 0.75f;
};

struct WorldVisualConfiguration {
    bool fake_daylight = false;
    bool dark_area_brightness = false;
    bool force_360_vision = false;
    bool reveal_unexplored = false;
    bridge::WeatherVisualMode fog_mode = bridge::WeatherVisualMode::FollowGame;
    bridge::PrecipitationVisualMode precipitation_mode =
        bridge::PrecipitationVisualMode::FollowGame;
    float fog_intensity = 0.75f;
    float precipitation_intensity = 0.75f;
    bool show_map_players = false;
};

struct CharacterConfiguration {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool reveal_world_map = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
    bool no_weapon_jam = false;
    bool fatigue_protection = false;
    bool panic_protection = false;
    bool hunger_protection = false;
    bool thirst_protection = false;
    bool negative_moodles_protection = false;
    bool infection_immunity = false;
    bool multi_hit = false;
    bool free_build = false;
    bool farming_mode = false;
    bool instant_timed_actions = false;
};

struct CharacterConfigurationV10 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool reveal_world_map = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
    bool fatigue_protection = false;
    bool panic_protection = false;
    bool hunger_protection = false;
    bool thirst_protection = false;
    bool negative_moodles_protection = false;
    bool infection_immunity = false;
    bool multi_hit = false;
    bool free_build = false;
    bool farming_mode = false;
    bool instant_timed_actions = false;
};

struct CharacterConfigurationV9 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool reveal_world_map = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
    bool fatigue_protection = false;
    bool panic_protection = false;
    bool hunger_protection = false;
    bool thirst_protection = false;
    bool negative_moodles_protection = false;
    bool infection_immunity = false;
    bool multi_hit = false;
    bool free_build = false;
};

struct CharacterConfigurationV8 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool reveal_world_map = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
    bool fatigue_protection = false;
    bool panic_protection = false;
    bool hunger_protection = false;
    bool thirst_protection = false;
    bool negative_moodles_protection = false;
    bool infection_immunity = false;
};

struct CharacterConfigurationV4 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool reveal_world_map = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
};

struct CharacterConfigurationV3 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool map_teleport = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
};

struct CharacterConfigurationV2 {
    bool infinite_health = false;
    bool invincibility = false;
    bool no_clip = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
};

struct CharacterConfigurationV1 {
    bool infinite_health = false;
    bool invincibility = false;
    bool endurance_recovery = false;
    bool durability_protection = false;
    bool infinite_ammo = false;
};

struct ConfigurationPayload {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfiguration world{};
    CharacterConfiguration character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV14 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfiguration character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV10 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV10 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV9 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV9 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV8 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV8 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV1 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV1 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV2 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV2 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV3 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV3 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

struct ConfigurationPayloadV4 {
    features::aim::AimSettings aim{};
    features::visual::VisualSettings zombies{};
    features::visual::PlayerVisualSettings players{};
    features::visual::AnimalVisualSettings animals{};
    features::visual::VehicleVisualSettings vehicles{};
    features::visual::WeaponRaySettings weapon_ray{};
    WorldVisualConfigurationV14 world{};
    CharacterConfigurationV4 character{};
    float ui_scale_percent = 100.0f;
    Language language = Language::Chinese;
};

static_assert(
    std::is_trivially_copyable_v<ConfigurationPayload>,
    "Configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV14>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV10>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV9>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV8>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV1>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV2>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV3>,
    "Legacy configuration payload must remain safe for binary serialization.");
static_assert(
    std::is_trivially_copyable_v<ConfigurationPayloadV4>,
    "Legacy configuration payload must remain safe for binary serialization.");

template <typename Value>
bool WriteValue(std::ostream& output, const Value& value) {
    static_assert(std::is_trivially_copyable_v<Value>);
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    return static_cast<bool>(output);
}

bool WriteString(std::ostream& output, const std::string& value) {
    if (value.size() > kMaximumLuaStringBytes) return false;
    const std::uint32_t length = static_cast<std::uint32_t>(value.size());
    if (!WriteValue(output, length)) return false;
    if (length != 0) output.write(value.data(), length);
    return static_cast<bool>(output);
}

bool WriteLuaConfiguration(
    std::ostream& output,
    const std::vector<bridge::LuaScriptConfiguration>& scripts) {
    if (scripts.size() > kMaximumLuaScripts) return false;
    const std::uint32_t script_count =
        static_cast<std::uint32_t>(scripts.size());
    if (!WriteValue(output, script_count)) return false;
    for (const bridge::LuaScriptConfiguration& script : scripts) {
        if (!WriteString(output, script.path.u8string())) return false;
        const std::uint8_t auto_reload = script.auto_reload ? 1 : 0;
        if (!WriteValue(output, auto_reload) ||
            script.controls.size() > kMaximumLuaControls) {
            return false;
        }
        const std::uint32_t control_count =
            static_cast<std::uint32_t>(script.controls.size());
        if (!WriteValue(output, control_count)) return false;
        for (const bridge::LuaControlConfiguration& control : script.controls) {
            if (!WriteString(output, control.category) ||
                !WriteString(output, control.label)) {
                return false;
            }
            const std::uint8_t type = static_cast<std::uint8_t>(control.type);
            const std::uint8_t standalone = control.standalone ? 1 : 0;
            const std::uint8_t toggle = control.toggle ? 1 : 0;
            if (!WriteValue(output, type) ||
                !WriteValue(output, standalone) ||
                !WriteValue(output, toggle) ||
                !WriteValue(output, control.value) ||
                !WriteString(output, control.text)) {
                return false;
            }
            for (float component : control.color) {
                if (!WriteValue(output, component)) return false;
            }
            if (control.selected_items.size() > kMaximumLuaSelectedItems) {
                return false;
            }
            const std::uint32_t selected_count =
                static_cast<std::uint32_t>(control.selected_items.size());
            if (!WriteValue(output, selected_count)) return false;
            for (const std::string& full_type : control.selected_items) {
                if (!WriteString(output, full_type)) return false;
            }
        }
    }
    return true;
}

template <typename Value>
bool ReadValue(std::istream& input, Value& value) {
    static_assert(std::is_trivially_copyable_v<Value>);
    input.read(reinterpret_cast<char*>(&value), sizeof(value));
    return static_cast<bool>(input);
}

bool ReadString(std::istream& input, std::string& value) {
    std::uint32_t length = 0;
    if (!ReadValue(input, length) || length > kMaximumLuaStringBytes) {
        return false;
    }
    value.resize(length);
    if (length != 0) input.read(value.data(), length);
    return static_cast<bool>(input);
}

bool ReadLuaConfiguration(
    std::istream& input,
    std::vector<bridge::LuaScriptConfiguration>& scripts,
    bool includes_text, bool includes_extended_controls) {
    std::uint32_t script_count = 0;
    if (!ReadValue(input, script_count) || script_count > kMaximumLuaScripts) {
        return false;
    }
    scripts.clear();
    scripts.reserve(script_count);
    for (std::uint32_t script_index = 0;
         script_index < script_count; ++script_index) {
        std::string path;
        std::uint8_t auto_reload = 0;
        std::uint32_t control_count = 0;
        if (!ReadString(input, path) || !ReadValue(input, auto_reload) ||
            !ReadValue(input, control_count) ||
            control_count > kMaximumLuaControls) {
            return false;
        }
        bridge::LuaScriptConfiguration script;
        script.path = std::filesystem::u8path(path);
        script.auto_reload = auto_reload != 0;
        script.controls.reserve(control_count);
        for (std::uint32_t control_index = 0;
             control_index < control_count; ++control_index) {
            bridge::LuaControlConfiguration control;
            std::uint8_t type = 0;
            std::uint8_t standalone = 0;
            std::uint8_t toggle = 0;
            if (!ReadString(input, control.category) ||
                !ReadString(input, control.label) ||
                !ReadValue(input, type) ||
                !ReadValue(input, standalone) ||
                !ReadValue(input, toggle) ||
                !ReadValue(input, control.value) ||
                type > static_cast<std::uint8_t>(
                    includes_extended_controls
                        ? bridge::LuaConfiguredControlType::ItemMultiSelect
                        : includes_text
                            ? bridge::LuaConfiguredControlType::Input
                            : bridge::LuaConfiguredControlType::Slider)) {
                return false;
            }
            if (includes_text && !ReadString(input, control.text)) {
                return false;
            }
            if (includes_extended_controls) {
                for (float& component : control.color) {
                    if (!ReadValue(input, component)) return false;
                }
                std::uint32_t selected_count = 0;
                if (!ReadValue(input, selected_count) ||
                    selected_count > kMaximumLuaSelectedItems) {
                    return false;
                }
                control.selected_items.reserve(selected_count);
                for (std::uint32_t selected_index = 0;
                     selected_index < selected_count; ++selected_index) {
                    std::string full_type;
                    if (!ReadString(input, full_type)) return false;
                    control.selected_items.push_back(std::move(full_type));
                }
            }
            control.type = static_cast<bridge::LuaConfiguredControlType>(type);
            control.standalone = standalone != 0;
            control.toggle = toggle != 0;
            script.controls.push_back(std::move(control));
        }
        scripts.push_back(std::move(script));
    }
    return true;
}

void UpgradeWorldConfiguration(
        WorldVisualConfiguration& target,
        const WorldVisualConfigurationV14& legacy) {
    target.fake_daylight = legacy.fake_daylight;
    target.dark_area_brightness = legacy.dark_area_brightness;
    target.force_360_vision = legacy.force_360_vision;
    target.reveal_unexplored = legacy.reveal_unexplored;
    target.fog_mode = legacy.fog_mode;
    target.precipitation_mode = legacy.precipitation_mode;
    target.fog_intensity = legacy.fog_intensity;
    target.precipitation_intensity = legacy.precipitation_intensity;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV14& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character = legacy.character;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV10& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.map_teleport = legacy.character.map_teleport;
    payload.character.reveal_world_map = legacy.character.reveal_world_map;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.character.fatigue_protection =
        legacy.character.fatigue_protection;
    payload.character.panic_protection = legacy.character.panic_protection;
    payload.character.hunger_protection = legacy.character.hunger_protection;
    payload.character.thirst_protection = legacy.character.thirst_protection;
    payload.character.negative_moodles_protection =
        legacy.character.negative_moodles_protection;
    payload.character.infection_immunity =
        legacy.character.infection_immunity;
    payload.character.multi_hit = legacy.character.multi_hit;
    payload.character.free_build = legacy.character.free_build;
    payload.character.farming_mode = legacy.character.farming_mode;
    payload.character.instant_timed_actions =
        legacy.character.instant_timed_actions;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV9& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.map_teleport = legacy.character.map_teleport;
    payload.character.reveal_world_map = legacy.character.reveal_world_map;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.character.fatigue_protection =
        legacy.character.fatigue_protection;
    payload.character.panic_protection = legacy.character.panic_protection;
    payload.character.hunger_protection = legacy.character.hunger_protection;
    payload.character.thirst_protection = legacy.character.thirst_protection;
    payload.character.negative_moodles_protection =
        legacy.character.negative_moodles_protection;
    payload.character.infection_immunity =
        legacy.character.infection_immunity;
    payload.character.multi_hit = legacy.character.multi_hit;
    payload.character.free_build = legacy.character.free_build;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV8& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.map_teleport = legacy.character.map_teleport;
    payload.character.reveal_world_map = legacy.character.reveal_world_map;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.character.fatigue_protection =
        legacy.character.fatigue_protection;
    payload.character.panic_protection = legacy.character.panic_protection;
    payload.character.hunger_protection = legacy.character.hunger_protection;
    payload.character.thirst_protection = legacy.character.thirst_protection;
    payload.character.negative_moodles_protection =
        legacy.character.negative_moodles_protection;
    payload.character.infection_immunity =
        legacy.character.infection_immunity;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV4& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.map_teleport = legacy.character.map_teleport;
    payload.character.reveal_world_map = legacy.character.reveal_world_map;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV3& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.map_teleport = legacy.character.map_teleport;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV2& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.no_clip = legacy.character.no_clip;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

ConfigurationPayload UpgradeConfiguration(
    const ConfigurationPayloadV1& legacy) {
    ConfigurationPayload payload{};
    payload.aim = legacy.aim;
    payload.zombies = legacy.zombies;
    payload.players = legacy.players;
    payload.animals = legacy.animals;
    payload.vehicles = legacy.vehicles;
    payload.weapon_ray = legacy.weapon_ray;
    UpgradeWorldConfiguration(payload.world, legacy.world);
    payload.character.infinite_health = legacy.character.infinite_health;
    payload.character.invincibility = legacy.character.invincibility;
    payload.character.endurance_recovery =
        legacy.character.endurance_recovery;
    payload.character.durability_protection =
        legacy.character.durability_protection;
    payload.character.infinite_ammo = legacy.character.infinite_ammo;
    payload.ui_scale_percent = legacy.ui_scale_percent;
    payload.language = legacy.language;
    return payload;
}

bool NormalizeName(const std::string& raw, std::string& normalized,
                   std::string& error) {
    const std::size_t first = raw.find_first_not_of(" \t.");
    const std::size_t last = raw.find_last_not_of(" \t.");
    if (first == std::string::npos || last == std::string::npos) {
        error = "Configuration name is empty.";
        return false;
    }
    normalized = raw.substr(first, last - first + 1);
    if (normalized.size() > 64) {
        error = "Configuration name is too long.";
        return false;
    }
    for (const unsigned char value : normalized) {
        if (value < 0x20 || value == '<' || value == '>' || value == ':' ||
            value == '"' || value == '/' || value == '\\' || value == '|' ||
            value == '?' || value == '*') {
            error = "Configuration name contains invalid characters.";
            return false;
        }
    }
    return true;
}

bool ConfigurationPath(const std::string& name, std::filesystem::path& path,
                       std::string& error) {
    std::string normalized;
    if (!NormalizeName(name, normalized, error)) return false;
    path = ConfigurationDirectory() /
        (std::filesystem::u8path(normalized).wstring() + L".pzcfg");
    return true;
}

ConfigurationPayload CaptureConfiguration() {
    ConfigurationPayload payload{};
    payload.aim = features::aim::GetAimSettings();
    payload.zombies = features::visual::GetVisualSettings();
    payload.players = features::visual::GetPlayerVisualSettings();
    payload.animals = features::visual::GetAnimalVisualSettings();
    payload.vehicles = features::visual::GetVehicleVisualSettings();
    payload.weapon_ray = features::visual::GetWeaponRaySettings();

    const bridge::WorldVisualStatus& world = bridge::GetWorldVisualStatus();
    payload.world.fake_daylight = world.fake_daylight_enabled;
    payload.world.dark_area_brightness = world.dark_area_brightness_enabled;
    payload.world.force_360_vision = world.force_360_vision_enabled;
    payload.world.reveal_unexplored = world.reveal_unexplored_enabled;
    payload.world.fog_mode = world.fog_mode;
    payload.world.precipitation_mode = world.precipitation_mode;
    payload.world.fog_intensity = world.fog_intensity;
    payload.world.precipitation_intensity = world.precipitation_intensity;
    payload.world.show_map_players =
        bridge::GetWorldMapPlayerStatus().enabled;

    const bridge::PlayerHealthStatus& health = bridge::GetPlayerHealthStatus();
    const bridge::PlayerResourceStatus& resources =
        bridge::GetPlayerResourceStatus();
    const bridge::PlayerMovementStatus& movement =
        bridge::GetPlayerMovementStatus();
    const bridge::PlayerTeleportStatus& teleport =
        bridge::GetPlayerTeleportStatus();
    const bridge::WorldMapRevealStatus& map_reveal =
        bridge::GetWorldMapRevealStatus();
    const bridge::PlayerAmmoStatus& ammo = bridge::GetPlayerAmmoStatus();
    const bridge::PlayerWeaponReliabilityStatus& weapon_reliability =
        bridge::GetPlayerWeaponReliabilityStatus();
    payload.character.infinite_health = health.infinite_health_enabled;
    payload.character.invincibility = health.invincibility_enabled;
    payload.character.no_clip = movement.no_clip_enabled;
    payload.character.map_teleport = teleport.enabled;
    payload.character.reveal_world_map = map_reveal.enabled;
    payload.character.endurance_recovery = resources.endurance_recovery_enabled;
    payload.character.durability_protection =
        resources.durability_protection_enabled;
    payload.character.infinite_ammo = ammo.enabled;
    payload.character.no_weapon_jam = weapon_reliability.enabled;
    payload.character.fatigue_protection =
        bridge::IsFatigueProtectionRequested();
    payload.character.panic_protection =
        bridge::IsPanicProtectionRequested();
    payload.character.hunger_protection =
        bridge::IsHungerProtectionRequested();
    payload.character.thirst_protection =
        bridge::IsThirstProtectionRequested();
    payload.character.negative_moodles_protection =
        bridge::IsNegativeMoodlesProtectionRequested();
    payload.character.infection_immunity =
        bridge::IsInfectionImmunityRequested();
    payload.character.multi_hit = bridge::GetMultiHitStatus().enabled;
    payload.character.free_build = bridge::GetFreeBuildStatus().enabled;
    payload.character.farming_mode = bridge::GetFarmingModeStatus().enabled;
    payload.character.instant_timed_actions =
        bridge::GetTimedActionStatus().enabled;
    payload.ui_scale_percent = UiScalePercent();
    payload.language = GetLanguage();
    return payload;
}

template <typename Enum>
Enum ClampEnum(Enum value, int count) {
    const int integer = static_cast<int>(value);
    return integer >= 0 && integer < count ? value : static_cast<Enum>(0);
}

void ValidateAim(features::aim::AimSettings& settings) {
    constexpr std::size_t count =
        static_cast<std::size_t>(features::aim::WeaponGroup::Count);
    if (static_cast<std::size_t>(settings.selected_legit_weapon) >= count) {
        settings.selected_legit_weapon = features::aim::WeaponGroup::Global;
    }
    if (static_cast<std::size_t>(settings.selected_rage_weapon) >= count) {
        settings.selected_rage_weapon = features::aim::WeaponGroup::Global;
    }
    for (features::aim::LegitWeaponSettings& preset : settings.legit_presets) {
        preset.range = std::clamp(preset.range, 1.0f, 100.0f);
        preset.smoothing = std::clamp(preset.smoothing, 1.0f, 100.0f);
        preset.accuracy = std::clamp(preset.accuracy, 0.0f, 100.0f);
        preset.minimum_damage = std::clamp(
            preset.minimum_damage, 0.0f, 100.0f);
        preset.target_points &= kTargetPointMask;
    }
    for (features::aim::RageWeaponSettings& preset : settings.rage_presets) {
        preset.range = std::clamp(preset.range, 1.0f, 100.0f);
        preset.accuracy = std::clamp(preset.accuracy, 0.0f, 100.0f);
        preset.minimum_damage = std::clamp(
            preset.minimum_damage, 0.0f, 100.0f);
        preset.maximum_damage = std::clamp(
            preset.maximum_damage, preset.minimum_damage, 100.0f);
        preset.target_points &= kTargetPointMask;
    }
}

void ValidatePayload(ConfigurationPayload& payload) {
    ValidateAim(payload.aim);
    payload.zombies.max_distance = std::clamp(
        payload.zombies.max_distance, 5.0f, 100.0f);
    payload.players.max_distance = std::clamp(
        payload.players.max_distance, 5.0f, 150.0f);
    payload.animals.max_distance = std::clamp(
        payload.animals.max_distance, 5.0f, 150.0f);
    payload.vehicles.max_distance = std::clamp(
        payload.vehicles.max_distance, 10.0f, 250.0f);
    payload.world.fog_mode = ClampEnum(payload.world.fog_mode, 3);
    payload.world.precipitation_mode = ClampEnum(
        payload.world.precipitation_mode, 4);
    payload.world.fog_intensity = std::clamp(
        payload.world.fog_intensity, 0.05f, 1.0f);
    payload.world.precipitation_intensity = std::clamp(
        payload.world.precipitation_intensity, 0.05f, 1.0f);
    payload.ui_scale_percent = std::clamp(
        payload.ui_scale_percent, 50.0f, 250.0f);
    payload.language = ClampEnum(
        payload.language, static_cast<int>(Language::Count));
}

void ApplyConfiguration(ConfigurationPayload payload) {
    ValidatePayload(payload);
    features::aim::GetAimSettings() = payload.aim;
    features::visual::GetVisualSettings() = payload.zombies;
    features::visual::GetPlayerVisualSettings() = payload.players;
    features::visual::GetAnimalVisualSettings() = payload.animals;
    features::visual::GetVehicleVisualSettings() = payload.vehicles;
    features::visual::GetWeaponRaySettings() = payload.weapon_ray;

    bridge::SetFakeDaylightEnabled(payload.world.fake_daylight);
    bridge::SetDarkAreaBrightnessEnabled(payload.world.dark_area_brightness);
    bridge::SetForce360VisionEnabled(payload.world.force_360_vision);
    bridge::SetRevealUnexploredEnabled(payload.world.reveal_unexplored);
    bridge::SetFogVisualMode(payload.world.fog_mode);
    bridge::SetPrecipitationVisualMode(payload.world.precipitation_mode);
    bridge::SetFogVisualIntensity(payload.world.fog_intensity);
    bridge::SetPrecipitationVisualIntensity(
        payload.world.precipitation_intensity);
    bridge::SetWorldMapPlayerEnabled(payload.world.show_map_players);

    bridge::SetInfiniteHealthEnabled(payload.character.infinite_health);
    bridge::SetInvincibilityEnabled(payload.character.invincibility);
    bridge::SetNoClipEnabled(payload.character.no_clip);
    bridge::SetPlayerTeleportEnabled(payload.character.map_teleport);
    bridge::SetWorldMapRevealEnabled(payload.character.reveal_world_map);
    bridge::SetEnduranceRecoveryEnabled(payload.character.endurance_recovery);
    bridge::SetDurabilityProtectionEnabled(
        payload.character.durability_protection);
    bridge::SetInfiniteAmmoEnabled(payload.character.infinite_ammo);
    bridge::SetNoWeaponJamEnabled(payload.character.no_weapon_jam);
    bridge::SetFatigueProtectionEnabled(
        payload.character.fatigue_protection);
    bridge::SetPanicProtectionEnabled(payload.character.panic_protection);
    bridge::SetHungerProtectionEnabled(payload.character.hunger_protection);
    bridge::SetThirstProtectionEnabled(payload.character.thirst_protection);
    bridge::SetNegativeMoodlesProtectionEnabled(
        payload.character.negative_moodles_protection);
    bridge::SetInfectionImmunityEnabled(
        payload.character.infection_immunity);
    bridge::SetMultiHitEnabled(payload.character.multi_hit);
    bridge::SetFreeBuildEnabled(payload.character.free_build);
    bridge::SetFarmingModeEnabled(payload.character.farming_mode);
    bridge::SetTimedActionInstantEnabled(
        payload.character.instant_timed_actions);
    SetUiScalePercent(payload.ui_scale_percent);
    SetUiLanguage(payload.language);
    std::string ignored_error;
    SaveUiPreferences(ignored_error);
}

}  // namespace

std::vector<ConfigurationInfo> ListConfigurations() {
    std::vector<ConfigurationInfo> configurations;
    std::error_code error;
    std::filesystem::create_directories(ConfigurationDirectory(), error);
    if (error) return configurations;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(ConfigurationDirectory(), error)) {
        if (error) break;
        if (!entry.is_regular_file() || entry.path().extension() != L".pzcfg") {
            continue;
        }
        configurations.push_back(
            {entry.path().stem().u8string()});
    }
    std::sort(
        configurations.begin(), configurations.end(),
        [](const ConfigurationInfo& left, const ConfigurationInfo& right) {
            return left.name < right.name;
        });
    return configurations;
}

bool SaveConfiguration(const std::string& name, std::string& error) {
    std::filesystem::path path;
    if (!ConfigurationPath(name, path, error)) return false;
    std::error_code filesystem_error;
    std::filesystem::create_directories(
        path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    const ConfigurationPayload payload = CaptureConfiguration();
    const std::vector<bridge::LuaScriptConfiguration> lua_configuration =
        bridge::CaptureLuaScriptConfiguration();
    const FileHeader header{kMagic, kVersion,
                            static_cast<std::uint32_t>(sizeof(payload))};
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Unable to open configuration file.";
        return false;
    }
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&payload), sizeof(payload));
    const bridge::ExtensionOptions extensions = bridge::GetExtensionOptions();
    if (!output || !WriteLuaConfiguration(output, lua_configuration) ||
        !WriteValue(output, extensions.flags) || !WriteValue(output, extensions.kill_range)) {
        error = "Unable to write configuration file.";
        return false;
    }
    return true;
}

bool LoadConfiguration(const std::string& name, std::string& error) {
    std::filesystem::path path;
    if (!ConfigurationPath(name, path, error)) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Configuration file was not found.";
        return false;
    }
    FileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || header.magic != kMagic) {
        error = "Configuration file is invalid or uses another version.";
        return false;
    }
    ConfigurationPayload payload{};
    std::vector<bridge::LuaScriptConfiguration> lua_configuration;
    bridge::ExtensionOptions extensions;
    bool has_lua_configuration = false;
    if ((header.version == kVersion || header.version == 15) &&
        header.payload_size == sizeof(ConfigurationPayload)) {
        input.read(reinterpret_cast<char*>(&payload), sizeof(payload));
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, true);
        if (header.version == kVersion &&
            (!ReadValue(input, extensions.flags) || !ReadValue(input, extensions.kill_range) ||
             extensions.flags < 0 || extensions.flags > 2047 ||
             extensions.kill_range < 1 || extensions.kill_range > 30)) {
            error = "Extension configuration is invalid or incomplete.";
            return false;
        }
    } else if ((header.version == 14 || header.version == 13 ||
                header.version == 12 || header.version == 11) &&
        header.payload_size == sizeof(ConfigurationPayloadV14)) {
        ConfigurationPayloadV14 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, true);
    } else if (header.version == 10 &&
               header.payload_size == sizeof(ConfigurationPayloadV10)) {
        ConfigurationPayloadV10 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, true);
    } else if (header.version == 9 &&
               header.payload_size == sizeof(ConfigurationPayloadV9)) {
        ConfigurationPayloadV9 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, true);
    } else if (header.version == 8 &&
               header.payload_size == sizeof(ConfigurationPayloadV8)) {
        ConfigurationPayloadV8 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, true);
    } else if (header.version == 7 &&
               header.payload_size == sizeof(ConfigurationPayloadV8)) {
        ConfigurationPayloadV8 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, true, false);
    } else if (header.version == 6 &&
               header.payload_size == sizeof(ConfigurationPayloadV8)) {
        ConfigurationPayloadV8 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
        has_lua_configuration = input &&
            ReadLuaConfiguration(input, lua_configuration, false, false);
    } else if (header.version == 5 &&
               header.payload_size == sizeof(ConfigurationPayloadV8)) {
        ConfigurationPayloadV8 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
    } else if (header.version == 4 &&
               header.payload_size == sizeof(ConfigurationPayloadV4)) {
        ConfigurationPayloadV4 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
    } else if (header.version == 3 &&
               header.payload_size == sizeof(ConfigurationPayloadV3)) {
        ConfigurationPayloadV3 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
    } else if (header.version == 2 &&
               header.payload_size == sizeof(ConfigurationPayloadV2)) {
        ConfigurationPayloadV2 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
    } else if (header.version == 1 &&
               header.payload_size == sizeof(ConfigurationPayloadV1)) {
        ConfigurationPayloadV1 legacy{};
        input.read(reinterpret_cast<char*>(&legacy), sizeof(legacy));
        if (input) payload = UpgradeConfiguration(legacy);
    } else {
        error = "Configuration file is invalid or uses another version.";
        return false;
    }
    if (!input ||
        ((header.version == kVersion || header.version == 15 || header.version == 14 ||
          header.version == 13 || header.version == 12 || header.version == 11 ||
          header.version == 10 ||
          header.version == 9 ||
          header.version == 8 ||
          header.version == 7 || header.version == 6) &&
         !has_lua_configuration)) {
        error = "Configuration file is incomplete.";
        return false;
    }
    if (header.version < 12) {
        for (features::aim::RageWeaponSettings& preset : payload.aim.rage_presets) {
            preset.magic_bullet = false;
        }
    }
    if (header.version < 13) {
        for (features::aim::RageWeaponSettings& preset : payload.aim.rage_presets) {
            preset.minimum_damage = 100.0f;
            preset.maximum_damage = 100.0f;
        }
    }
    if (header.version < 14) {
        payload.players.show_admin_marker = false;
    }
    if (header.version < 15) {
        payload.world.show_map_players = false;
    }
    ApplyConfiguration(payload);
    bridge::GetExtensionOptions() = extensions;
    if (has_lua_configuration) {
        bridge::ApplyLuaScriptConfiguration(std::move(lua_configuration));
    }
    return true;
}

bool DeleteConfiguration(const std::string& name, std::string& error) {
    std::filesystem::path path;
    if (!ConfigurationPath(name, path, error)) return false;
    std::error_code filesystem_error;
    const bool removed = std::filesystem::remove(path, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    if (!removed) {
        error = "Configuration file was not found.";
        return false;
    }
    return true;
}

}  // namespace pztrainer::settings
