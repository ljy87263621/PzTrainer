#include "bridge/packet_audit.hpp"

#include <deque>
#include <mutex>
#include <utility>

namespace pztrainer::bridge {
namespace {

constexpr std::size_t kMaxEntries = 128;
std::mutex g_mutex;
std::deque<PacketAuditEntry> g_entries;

}  // namespace

const char* PacketAuditKindName(PacketAuditKind kind) {
    switch (kind) {
        case PacketAuditKind::ItemTransaction: return "ItemTransaction";
        case PacketAuditKind::PlayerXp: return "PlayerXp";
        case PacketAuditKind::BuildAction: return "BuildAction";
        case PacketAuditKind::GeneralAction: return "GeneralAction";
        case PacketAuditKind::FishingAction: return "FishingAction";
    }
    return "Unknown";
}

void RecordPacketAudit(PacketAuditKind kind, bool accepted,
                       std::string operation, std::string detail) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_entries.size() >= kMaxEntries) g_entries.pop_front();
    g_entries.push_back(PacketAuditEntry{
        kind, accepted, std::move(operation), std::move(detail)});
}

std::vector<PacketAuditEntry> SnapshotPacketAudit() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return {g_entries.begin(), g_entries.end()};
}

void ClearPacketAudit() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.clear();
}

}  // namespace pztrainer::bridge
