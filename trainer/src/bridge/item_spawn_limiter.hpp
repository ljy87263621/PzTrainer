#pragma once

namespace pztrainer::bridge {

bool TryAcquireOnlineItemSpawn(int& remaining_milliseconds);
int GetOnlineItemSpawnCooldownRemaining();

}  // namespace pztrainer::bridge
