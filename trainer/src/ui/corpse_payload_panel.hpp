#pragma once

#include <imgui.h>

#include <cstddef>
#include <string>
#include <vector>

#include "bridge/item_bridge.hpp"

namespace pztrainer::ui {

bool IsCorpsePayloadItemSelected(const std::string& full_type);
void ToggleCorpsePayloadItem(const bridge::ItemCatalogEntry& item);
bool HasCorpsePayloadSelection();
std::size_t CorpsePayloadSelectionCount();
std::vector<bridge::ItemSpawnBatchEntry> BuildCorpsePayloadBatch();
void ClearCorpsePayloadSelection();
void DrawCorpsePayloadPanel(const ImVec2& host_minimum,
                            const ImVec2& host_maximum, bool enabled);

}  // namespace pztrainer::ui
