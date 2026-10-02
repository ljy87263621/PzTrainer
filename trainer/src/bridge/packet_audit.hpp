#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class PacketAuditKind {
    ItemTransaction,
    PlayerXp,
    BuildAction,
    GeneralAction,
    FishingAction,
};

const char* PacketAuditKindName(PacketAuditKind kind);

struct PacketAuditEntry {
    PacketAuditKind kind = PacketAuditKind::ItemTransaction;
    bool accepted = false;
    std::string operation;
    std::string detail;
};

void RecordPacketAudit(PacketAuditKind kind, bool accepted,
                       std::string operation, std::string detail);
std::vector<PacketAuditEntry> SnapshotPacketAudit();
void ClearPacketAudit();

}  // namespace pztrainer::bridge
