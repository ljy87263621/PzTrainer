#pragma once

namespace pztrainer::bridge {

// PacketTypes.PacketAuthorization authorizes SyncPlayerStats from the
// connection role, with an explicit coop-host bypass.  IsoPlayer.role is a
// client-side mirror and cannot authorize a packet on the server by itself.
inline bool HasBodyStatsCapability(bool connection_role, bool coop_host) {
    return connection_role || coop_host;
}

inline bool HasClientBodyStatsCapability(bool player_role,
                                         bool connection_role,
                                         bool coop_host) {
    return player_role || connection_role || coop_host;
}

}  // namespace pztrainer::bridge
