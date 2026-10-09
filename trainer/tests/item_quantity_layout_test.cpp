#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "bridge/item_bridge.hpp"
#include "settings/ui_preferences.hpp"
#include "settings/localization.hpp"
#include "ui/corpse_payload_panel.hpp"
#include "ui/color_picker.hpp"
#include "ui/controls/action_controls.hpp"
#include "ui/item_generator.hpp"
#include "ui/theme.hpp"

namespace {
float scale = 1.0f;
struct ItemGeometry {
    ImRect bounds;
    ImRect clip;
    float padding = 0.0f;
    float number_width = 0.0f;
};
std::map<ImGuiID, ItemGeometry> items;
std::map<std::string, ItemGeometry> controls;

void PrepareHeadlessFrame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;
}

void FinishHeadlessFrame() {
    ImGui::Render();
    // Acknowledge the atlas protocol without uploading pixels: the assertions
    // inspect real layout and input handling, with no GPU or game required.
    if (ImGui::GetDrawData()->Textures == nullptr) return;
    for (ImTextureData* texture : *ImGui::GetDrawData()->Textures) {
        if (texture->Status == ImTextureStatus_WantCreate ||
            texture->Status == ImTextureStatus_WantUpdates)
            texture->SetStatus(ImTextureStatus_OK);
        else if (texture->Status == ImTextureStatus_WantDestroy)
            texture->SetStatus(ImTextureStatus_Destroyed);
    }
}
}

// Only the game and persisted preferences are replaced. The UI and ImGui code
// under measurement are the same translation units used by the trainer.
namespace pztrainer::settings {
float UiScale() { return scale; }
}
// These decorations are linked by components.cpp but never drawn by the item page.
namespace pztrainer::ui {
ImTextureID BrandIconTexture() { return 0; }
bool DrawColorSwatch(const char*, ImVec4*, const ImVec2&) { return false; }
bool DrawColorSwatch(const char*, features::visual::StateColor*, const ImVec2&) { return false; }
}
namespace pztrainer::bridge {
bool RefreshItemCatalog() { return true; }
ItemSessionMode GetItemSessionMode() { return ItemSessionMode::Local; }
const std::vector<ItemCatalogEntry>& GetItemCatalog() {
    static const std::vector<ItemCatalogEntry> catalog = [] {
        ItemCatalogEntry item;
        item.full_type = "Base.Axe";
        item.display_name = "测试物品";
        item.category = "Weapon";
        item.spawn_method_mask = 0x1ff;
        return std::vector<ItemCatalogEntry>{item};
    }();
    return catalog;
}
const ItemSpawnResult& GetLastItemSpawnResult() {
    static ItemSpawnResult result;
    return result;
}
const ItemSpawnResult& SpawnItem(const std::string&, int,
                               ItemSpawnDestination, ItemSpawnMethod) {
    return GetLastItemSpawnResult();
}
const ItemSpawnResult& SpawnCorpsePayloadBatch(const std::vector<ItemSpawnBatchEntry>&) {
    return GetLastItemSpawnResult();
}
const std::string& GetWeaponAmmoExtractionStatus() { static std::string s; return s; }
const std::string& GetWeaponMagazineExtractionStatus() { static std::string s; return s; }
const std::string& GetExplosiveTrapWeaponStatus() { static std::string s; return s; }
const std::string& GetExplosiveTrapWeaponPartStatus() { static std::string s; return s; }
const std::string& GetPalletItemStatus() { static std::string s; return s; }
const std::string& GetCorpsePayloadStatus() { static std::string s; return s; }
int GetOnlineItemSpawnCooldownRemaining() { return 0; }
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, ImGuiID id,
                               const ImRect& bb, const ImGuiLastItemData*) {
    items[id] = {bb, ctx->CurrentWindow->ClipRect, ctx->Style.FramePadding.x,
                 ImGui::CalcTextSize("100").x};
    // ItemAdd runs for clipped buttons too, unlike ItemInfo. Lower controls
    // may need vertical scrolling, but must fit horizontally once reached.
    if (ctx->CurrentWindow->IDStack.empty()) return;
    for (const auto& label : {std::make_pair("背包", "backpack"),
                              std::make_pair("地面", "ground"),
                              std::make_pair("取消", "cancel")}) {
        if (id == ctx->CurrentWindow->GetID(pztrainer::settings::Translate(label.first)))
            controls[label.second] = items[id];
    }
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id,
                                const char* label, ImGuiItemStatusFlags) {
    if (!label || !items.count(id)) return;
    if (std::strcmp(label, "##ItemQuantity") == 0 ||
        std::strcmp(label, "##PayloadQuantity") == 0) {
        controls[label] = items[id];
    } else if (std::strcmp(label, "##value") == 0) {
        controls[std::strstr(GImGui->CurrentWindow->Name, "PayloadItems")
                     ? "##PayloadQuantity" : "##ItemQuantity"] = items[id];
    } else if (std::strcmp(label, pztrainer::settings::Translate("背包")) == 0) {
        controls["backpack"] = items[id];
    } else if (std::strcmp(label, pztrainer::settings::Translate("地面")) == 0) {
        controls["ground"] = items[id];
    } else if (std::strcmp(label, pztrainer::settings::Translate("取消")) == 0) {
        controls["cancel"] = items[id];
    }
}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) { return ""; }

int main() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    const char* windows_directory = std::getenv("WINDIR");
    const std::string font_directory = std::string(windows_directory ? windows_directory : "C:/Windows") + "/Fonts/";
    ImFont* font = io.Fonts->AddFontFromFileTTF((font_directory + "segoeui.ttf").c_str(), 16.0f);
    ImFontConfig merge;
    merge.MergeMode = true;
    static const ImWchar chinese_ranges[] = {0x0020, 0x00ff, 0x3000, 0x303f, 0x4e00, 0x9fff, 0};
    io.Fonts->AddFontFromFileTTF((font_directory + "msyh.ttc").c_str(), 16.0f, &merge, chinese_ranges);
    io.FontDefault = font;
    pztrainer::ui::SetInterfaceFonts(font, font);
    GImGui->TestEngineHookItems = true;
    pztrainer::ui::ToggleCorpsePayloadItem(pztrainer::bridge::GetItemCatalog().front());

    int failures = 0;
    int cases = 0;
    for (const auto language : {pztrainer::settings::Language::Chinese,
                               pztrainer::settings::Language::English,
                               pztrainer::settings::Language::Russian,
                               pztrainer::settings::Language::German}) {
      pztrainer::settings::SetLanguage(language);
      for (const ImVec2 display : {ImVec2(960, 540), ImVec2(1280, 720),
                                  ImVec2(1920, 1080), ImVec2(2560, 1440)}) {
        for (const float zoom : {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f}) {
            ++cases;
            scale = zoom;
            pztrainer::ui::ApplyTheme();
            for (int frame = 0; frame < 90; ++frame) {
                items.clear();
                controls.clear();
                PrepareHeadlessFrame();
                io.DisplaySize = display;
                ImGui::NewFrame();
                const ImVec2 size(std::min(1010.0f * scale, display.x - 24.0f * scale),
                                  std::min(720.0f * scale, display.y - 24.0f * scale));
                const ImVec2 position((display.x - size.x) * 0.5f, (display.y - size.y) * 0.5f);
                ImGui::SetNextWindowPos(position);
                ImGui::SetNextWindowSize(size);
                ImGui::Begin("AuditHost", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
                ImGui::SetCursorPos(ImVec2(226.0f * scale, 86.0f * scale));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 8.0f * scale));
                ImGui::BeginChild("Content", ImVec2(size.x - 246.0f * scale, size.y - 103.0f * scale));
                pztrainer::ui::DrawItemGenerator({});
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::End();
                pztrainer::ui::DrawCorpsePayloadPanel(position, ImVec2(position.x + size.x, position.y + size.y), true);
                FinishHeadlessFrame();
            }
            for (const char* required : {"##ItemQuantity", "##PayloadQuantity", "backpack", "ground", "cancel"}) {
                if (!controls.count(required)) {
                    std::printf("FAIL missing %s (%dx%d, %.0f%%, language %d)\n", required,
                                (int)display.x, (int)display.y, zoom * 100, (int)language);
                    ++failures;
                }
            }
            for (const auto& entry : controls) {
                const auto& geometry = entry.second;
                const auto& bb = geometry.bounds;
                const auto& clip = geometry.clip;
                const bool quantity = entry.first[0] == '#';
                const bool text_fits = !quantity ||
                    bb.GetWidth() - 2.0f * geometry.padding >= geometry.number_width + scale;
                const bool horizontal_fits = bb.Min.x >= clip.Min.x - 1.0f && bb.Max.x <= clip.Max.x + 1.0f;
                // Quantity is the primary action and must remain initially visible;
                // lower filter/destination rows may be reached by normal scrolling.
                const bool number_visible = entry.first != "##ItemQuantity" ||
                    (bb.Min.y >= clip.Min.y - 1.0f && bb.Max.y <= clip.Max.y + 1.0f);
                if (text_fits && horizontal_fits && number_visible) continue;
                ++failures;
                std::printf("FAIL language=%d %dx%d zoom=%.2f %s width=%.1f text_space=%.1f needed=%.1f rect=(%.1f,%.1f)-(%.1f,%.1f) clip=(%.1f,%.1f)-(%.1f,%.1f)\n",
                            (int)language,
                            (int)display.x, (int)display.y, zoom, entry.first.c_str(),
                            bb.GetWidth(), bb.GetWidth() - 2.0f * geometry.padding,
                            geometry.number_width, bb.Min.x, bb.Min.y, bb.Max.x, bb.Max.y,
                            clip.Min.x, clip.Min.y, clip.Max.x, clip.Max.y);
            }
        }
      }
    }
    int interaction_checks = 0;
    scale = 1.0f;
    pztrainer::ui::ApplyTheme();
    for (const float width : {156.0f, 80.0f}) {
        int quantity = 5;
        ImGuiID decrease = 0, increase = 0, number = 0;
        const auto frame = [&] {
            items.clear();
            PrepareHeadlessFrame();
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20));
            ImGui::SetNextWindowSize(ImVec2(600, 240));
            ImGui::Begin("StepperExercise", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
            const ImGuiID owner = ImGui::GetID("quantity");
            decrease = ImHashStr("step", 0, ImHashStr("-", 0, owner));
            increase = ImHashStr("step", 0, ImHashStr("+", 0, owner));
            number = ImHashStr("##value", 0, ImHashStr("number", 0, owner));
            pztrainer::ui::controls::IntegerStepperField("quantity", &quantity, 1, 100, width);
            ImGui::End();
            FinishHeadlessFrame();
        };
        const auto click = [&](ImGuiID id) {
            const ImVec2 center = items.at(id).bounds.GetCenter();
            io.AddMousePosEvent(center.x, center.y);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        const auto expect = [&](bool passed, const char* behavior) {
            ++interaction_checks;
            if (!passed) {
                ++failures;
                std::printf("FAIL stepper width=%.0f %s (quantity=%d)\n", width, behavior, quantity);
            }
        };
        frame();
        frame();
        click(decrease);
        expect(quantity == 4, "decrement");
        click(increase);
        expect(quantity == 5, "increment");
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        quantity = 95;
        click(increase);
        expect(quantity == 100, "Ctrl increment clamps to 100");
        quantity = 4;
        click(decrease);
        expect(quantity == 1, "Ctrl decrement clamps to 1");
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        quantity = 1;
        click(decrease);
        expect(quantity == 1, "lower bound");
        quantity = 100;
        click(increase);
        expect(quantity == 100, "upper bound");
        const auto blur = [&] {
            io.AddMousePosEvent(590, 230);
            frame();
            io.AddMouseButtonEvent(0, true);
            frame();
            io.AddMouseButtonEvent(0, false);
            frame();
        };
        const auto type = [&](const char* text) {
            // Deactivate between edits so InputInt selects the existing value.
            blur();
            click(number);
            io.AddInputCharactersUTF8(text);
            frame();
            blur();
        };
        type("250");
        expect(quantity == 100, "typed upper bound");
        type("0");
        expect(quantity == 1, "typed lower bound");
        quantity = 10;
        frame();
        const ImVec2 center = items.at(increase).bounds.GetCenter();
        io.AddMousePosEvent(center.x, center.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        for (int held = 0; held < 45; ++held) frame();
        expect(quantity > 11, "holding repeats increment");
        io.AddMouseButtonEvent(0, false);
        frame();
    }
    ImGui::DestroyContext();
    std::printf("%d layout cases, %d interaction checks, %d failures\n", cases, interaction_checks, failures);
    return failures ? 1 : 0;
}
