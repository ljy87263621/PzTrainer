#include "bridge/item_spawn_limiter.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace pztrainer::bridge {
namespace {

constexpr std::int64_t kOnlineItemSpawnCooldownMilliseconds = 2000;
std::atomic<std::int64_t> g_next_online_item_spawn_at{0};

std::int64_t NowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

bool TryAcquireOnlineItemSpawn(int& remaining_milliseconds) {
    const std::int64_t now = NowMilliseconds();
    std::int64_t next_allowed =
        g_next_online_item_spawn_at.load(std::memory_order_acquire);
    bool acquired = false;
    for (;;) {
        if (now < next_allowed) {
            remaining_milliseconds =
                static_cast<int>(next_allowed - now);
            break;
        }
        if (g_next_online_item_spawn_at.compare_exchange_weak(
                next_allowed, now + kOnlineItemSpawnCooldownMilliseconds,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            remaining_milliseconds = 0;
            acquired = true;
            break;
        }
    }
    return acquired;
}

int GetOnlineItemSpawnCooldownRemaining() {
    const std::int64_t remaining =
        g_next_online_item_spawn_at.load(std::memory_order_acquire) -
        NowMilliseconds();
    return remaining > 0 ? static_cast<int>(remaining) : 0;
}

}  // namespace pztrainer::bridge
