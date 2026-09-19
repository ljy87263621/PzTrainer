#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class ExperienceSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

enum class SkillOnlineRoute {
    None,
    RadioTeaching,
    FitnessExercise,
    StrengthExercise,
    MaintenanceTraining,
};

struct SkillExperienceEntry {
    std::string id;
    std::string display_name;
    std::string category;
    int level = 0;
    float total_xp = 0.0f;
    float level_start_xp = 0.0f;
    float next_level_xp = 0.0f;
    float max_level_xp = 0.0f;
    float multiplier = 0.0f;
    bool online_supported = false;
    SkillOnlineRoute online_route = SkillOnlineRoute::None;
};

struct ExperienceStatus {
    std::uint64_t session_generation = 0;
    ExperienceSessionMode session_mode = ExperienceSessionMode::Unknown;
    bool initialized = false;
    bool player_ready = false;
    bool nearby_receiver_ready = false;
    bool maintenance_training_active = false;
    int maintenance_packets_sent = 0;
    int media_level_cutoff = 0;
    std::string receiver_name;
    std::string message = "等待玩家对象";
    std::vector<SkillExperienceEntry> entries;
};

void UpdateExperienceBridge();
const ExperienceStatus& GetExperienceStatus();
bool AddSkillExperience(const std::string& id, float amount);
bool AddSkillExperienceVanilla(const std::string& id, float amount);
bool AddSkillLevel(const std::string& id);

}  // namespace pztrainer::bridge
