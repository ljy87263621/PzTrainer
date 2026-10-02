#include <cassert>

#include "bridge/packet_audit.hpp"

int main() {
    using namespace pztrainer::bridge;
    ClearPacketAudit();
    RecordPacketAudit(PacketAuditKind::PlayerXp, true, "submit", "skill=Axe delta=10");
    RecordPacketAudit(PacketAuditKind::FishingAction, false, "probe", "unsupported");
    const auto entries = SnapshotPacketAudit();
    assert(entries.size() == 2);
    assert(entries[0].kind == PacketAuditKind::PlayerXp);
    assert(std::string(PacketAuditKindName(entries[0].kind)) == "PlayerXp");
    assert(entries[0].accepted);
    assert(!entries[1].accepted);
    ClearPacketAudit();
    assert(SnapshotPacketAudit().empty());
    return 0;
}
