#pragma once

#include <cstddef>
#include <cstdint>

#include "bridge/game_snapshot.hpp"
#include "features/visual/player_visual_settings.hpp"
#include "features/visual/animal_visual_settings.hpp"
#include "features/visual/vehicle_visual_settings.hpp"
#include "features/visual/visual_settings.hpp"

namespace pztrainer::features::visual {

struct NativeModelChamsDiagnostics {
    int initialization_status = 0;
    std::uint64_t draw_calls = 0;
    std::uint64_t instanced_draw_calls = 0;
    std::uint64_t matrix_uploads = 0;
    std::uint64_t use_program_calls = 0;
    std::uint64_t capture_draw_calls = 0;
    std::uint64_t capture_draws_without_program = 0;
    std::uint64_t capture_enable_calls = 0;
    std::uint64_t capture_enable_true_calls = 0;
    std::uint64_t capture_context_binds = 0;
    std::uint64_t foreign_context_updates_ignored = 0;
    std::uint64_t foreign_context_draws_ignored = 0;
    std::size_t classified_programs = 0;
    std::size_t outline_programs = 0;
    std::size_t instanced_programs = 0;
    std::size_t vehicle_programs = 0;
    std::uint64_t vehicle_tinted_draw_calls = 0;
};

class NativeModelChams {
public:
    static bool Initialize();
    static void SetCaptureEnabled(
        bool enabled, const bridge::FrameSnapshot& frame,
        const VisualSettings& settings,
        const PlayerVisualSettings& player_settings,
        const AnimalVisualSettings& animal_settings,
        const VehicleVisualSettings& vehicle_settings);
    static void Replay(
        const bridge::FrameSnapshot& frame, const VisualSettings& settings,
        const PlayerVisualSettings& player_settings,
        const AnimalVisualSettings& animal_settings,
        const VehicleVisualSettings& vehicle_settings);
    static std::size_t CapturedDrawCount();
    static NativeModelChamsDiagnostics Diagnostics();
};

}  // namespace pztrainer::features::visual
