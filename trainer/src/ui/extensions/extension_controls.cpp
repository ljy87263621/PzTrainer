#include "ui/extensions/extension_controls.hpp"

#include <imgui.h>
#include <vector>
#include "bridge/extension_bridge.hpp"
#include "ui/components.hpp"
#include "ui/controls/action_controls.hpp"
#include "ui/controls/card_layout.hpp"

namespace pztrainer::ui::extensions {
namespace { std::vector<std::string> g_reasons; }
bool BeginAvailable(const char* key) {
    g_reasons.push_back(bridge::ExtensionDisabledReason(key));
    ImGui::BeginGroup();
    ImGui::BeginDisabled(!g_reasons.back().empty());
    return g_reasons.back().empty();
}
void EndAvailable() {
    ImGui::EndDisabled();
    ImGui::EndGroup();
    if (!g_reasons.back().empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        components::RoundedTooltip(g_reasons.back().c_str());
    g_reasons.pop_back();
}
void Toggle(const char* label, int flag) {
    const bool available = BeginAvailable(("flag:" + std::to_string(flag)).c_str());
    auto& options = bridge::GetExtensionOptions();
    bool enabled = available && (options.flags & flag) != 0;
    if (components::CompactToggleRow(label, &enabled)) {
        if (enabled) options.flags |= flag;
        else options.flags &= ~flag;
    }
    EndAvailable();
}
void Action(const char* label, const char* action, const std::string& payload, int radius) {
    BeginAvailable(action);
    if (controls::ActionButton(label)) bridge::QueueExtensionCommand(action, payload, radius);
    EndAvailable();
}
void Status() {
    cards::BeginSection("OperationStatus", "操作反馈");
    controls::Hint(bridge::GetExtensionStatus().c_str());
    components::EndCard();
}
}  // namespace pztrainer::ui::extensions
