#pragma once

namespace pztrainer::bridge {

enum class GameStartupGateState {
    WaitingForJvm,
    WaitingForBindings,
    WaitingForMainMenu,
    Ready,
};

struct GameStartupGateResult {
    GameStartupGateState state = GameStartupGateState::WaitingForJvm;
    const char* message = "waiting for JVM";
};

GameStartupGateResult PollGameStartupGate();

}  // namespace pztrainer::bridge
