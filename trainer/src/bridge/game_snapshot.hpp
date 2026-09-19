#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pztrainer::bridge {

inline constexpr std::size_t kZombieBoneCount = 19;
inline constexpr std::size_t kPlayerBoneCount = kZombieBoneCount;
inline constexpr std::size_t kAnimalBoneCount = kZombieBoneCount;

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct WorldPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class GateStatus {
    Initializing,
    WaitingForWorld,
    SinglePlayerAllowed,
    BlockedMultiplayer,
    Error,
};

struct ZombieSnapshot {
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float screen_top_y = 0.0f;
    float distance = 0.0f;
    float current_health = 0.0f;
    float health_fraction = 1.0f;
    std::int32_t game_id = -1;
    std::int32_t identity = 0;
    std::int16_t online_id = -1;
    bool prone = false;
    bool crawling = false;
    bool dead = false;
    bool unstable_pose = false;
    bool behind_wall = false;
    bool in_view = false;
    float forward_x = 0.0f;
    float forward_y = 1.0f;
    int model_instance_count = 0;
    bool has_bones = false;
    int bone_failure = 0;
    float bone_debug_row[3]{};
    float bone_debug_column[3]{};
    std::array<ScreenPoint, kZombieBoneCount> bones{};
    std::array<WorldPoint, kZombieBoneCount> bone_world_positions{};
};

struct PlayerSnapshot {
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float screen_top_y = 0.0f;
    float distance = 0.0f;
    float current_health = 0.0f;
    float health_fraction = 1.0f;
    std::int32_t game_id = -1;
    std::int32_t identity = 0;
    std::int16_t online_id = -1;
    std::string name;
    bool prone = false;
    bool dead = false;
    bool pvp_enabled = false;
    bool admin = false;
    bool invisible = false;
    bool behind_wall = false;
    bool in_view = false;
    bool has_bones = false;
    int bone_failure = 0;
    float bone_debug_row[3]{};
    float bone_debug_column[3]{};
    std::array<ScreenPoint, kPlayerBoneCount> bones{};
    std::array<WorldPoint, kPlayerBoneCount> bone_world_positions{};
};

struct AnimalSnapshot : PlayerSnapshot {
    std::string animal_type;
    bool baby = false;
    float size = 1.0f;
};

struct VehicleSnapshot {
    float screen_x = 0.0f;
    float screen_y = 0.0f;
    float screen_top_y = 0.0f;
    float distance = 0.0f;
    float engine_health_fraction = 0.0f;
    std::int16_t vehicle_id = -1;
    std::string name;
    bool behind_wall = false;
    bool in_view = false;
    bool all_doors_locked = false;
    bool any_door_locked = false;
    bool has_key = false;
    bool keys_in_ignition = false;
    bool hotwired = false;
    bool hotwire_broken = false;
    bool engine_running = false;
    bool engine_working = false;
    bool operational = false;
    bool can_drive_now = false;
};

struct FrameSnapshot {
    GateStatus gate_status = GateStatus::Initializing;
    std::string gate_message = "正在连接 JVM";
    float camera_zoom = 1.0f;
    float local_screen_x = 0.0f;
    float local_screen_y = 0.0f;
    bool has_local_screen_position = false;
    bool local_pvp_enabled = false;
    int model_chams_status = 0;
    std::vector<ZombieSnapshot> zombies;
    std::vector<PlayerSnapshot> players;
    std::vector<AnimalSnapshot> animals;
    std::vector<VehicleSnapshot> vehicles;
};

}  // namespace pztrainer::bridge
