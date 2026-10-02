#include <cassert>

#include "bridge/server_role_capability.hpp"

int main() {
    assert(pztrainer::bridge::HasBodyStatsCapability(true, false));
    assert(pztrainer::bridge::HasBodyStatsCapability(false, true));
    assert(pztrainer::bridge::HasBodyStatsCapability(true, true));
    assert(!pztrainer::bridge::HasBodyStatsCapability(false, false));
    assert(pztrainer::bridge::HasClientBodyStatsCapability(true, false, false));
    assert(pztrainer::bridge::HasClientBodyStatsCapability(false, true, false));
    assert(pztrainer::bridge::HasClientBodyStatsCapability(false, false, true));
    assert(!pztrainer::bridge::HasClientBodyStatsCapability(false, false, false));
    return 0;
}
