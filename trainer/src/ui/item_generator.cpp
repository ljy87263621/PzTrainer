#include "ui/item_generator.hpp"
#include "ui/item_generator_text.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "bridge/ammo_item_bridge.hpp"
#include "bridge/corpse_item_bridge.hpp"
#include "bridge/explosive_trap_item_bridge.hpp"
#include "bridge/item_bridge.hpp"
#include "bridge/item_spawn_limiter.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "bridge/magazine_item_bridge.hpp"
#include "bridge/pallet_item_bridge.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/animation.hpp"
#include "ui/components.hpp"
#include "ui/corpse_payload_panel.hpp"

namespace pztrainer::ui {
namespace {

using bridge::ItemCatalogEntry;
using bridge::ItemSpawnDestination;
using bridge::ItemSpawnMethod;
using namespace components;

constexpr float kCardHeight = 136.0f;
constexpr float kCardGap = 10.0f;
constexpr std::size_t kMethodCount = 9;

struct MethodInfo {
    ItemSpawnMethod value;
    const char* label;
    const char* short_label;
    const char* description;
};

constexpr std::array<MethodInfo, kMethodCount> kMethods{{
    {ItemSpawnMethod::Game, "游戏原生", "原生", "仅单机，支持全部物品"},
    {ItemSpawnMethod::VariantTransform, "服装变体", "变体", "带外观数据的服装物品"},
    {ItemSpawnMethod::TrapAnimalFood, "陷阱猎物", "食物", "可由陷阱登记的食物"},
    {ItemSpawnMethod::ExplosiveTrapWeapon, "武器陷阱", "武器", "安全的武器物品"},
    {ItemSpawnMethod::WeaponAmmoExtract, "弹药拆出", "卸弹", "兼容枪械的散装弹药"},
    {ItemSpawnMethod::WeaponMagazineExtract, "弹匣拆出", "退匣", "兼容外置弹匣"},
    {ItemSpawnMethod::WeaponPartDetach, "配件拆出", "配件", "带挂载点的武器配件"},
    {ItemSpawnMethod::PalletItemExtract, "联机取物", "联机", "联机取物，支持有效物品"},
    {ItemSpawnMethod::CorpsePayload, "尸体载荷", "尸体", "在脚下投递包含目标物品的服务器僵尸尸体"},
}};

constexpr std::array<ItemSpawnMethod, kMethodCount> kExecutionPriority{{
    ItemSpawnMethod::CorpsePayload,
    ItemSpawnMethod::PalletItemExtract,
    ItemSpawnMethod::WeaponMagazineExtract,
    ItemSpawnMethod::WeaponPartDetach,
    ItemSpawnMethod::WeaponAmmoExtract,
    ItemSpawnMethod::ExplosiveTrapWeapon,
    ItemSpawnMethod::TrapAnimalFood,
    ItemSpawnMethod::VariantTransform,
    ItemSpawnMethod::Game,
}};

char g_search[96]{};
std::string g_category = "全部";
std::string g_selected_type;
int g_quantity = 1;
ItemSpawnDestination g_destination = ItemSpawnDestination::Backpack;
ItemSpawnMethod g_last_execution_method = ItemSpawnMethod::VariantTransform;
std::string g_status;

enum class SpawnScheme {
    Game,
    OnlineRetrieve,
    CorpsePayload,
};

SpawnScheme g_spawn_scheme = SpawnScheme::OnlineRetrieve;

enum class DefaultSessionMode {
    Unknown,
    Local,
    Network,
};

DefaultSessionMode g_default_session_mode = DefaultSessionMode::Unknown;

const char* T(const char* text) {
    return settings::Translate(text);
}

float U(float value) {
    return value * settings::UiScale();
}

float TextControlWidth(const char* text, float minimum, float padding) {
    return std::max(U(minimum), ImGui::CalcTextSize(text).x + U(padding));
}

bool MethodEnabled(ItemSpawnMethod method) {
    switch (g_spawn_scheme) {
        case SpawnScheme::Game:
            return method == ItemSpawnMethod::Game;
        case SpawnScheme::CorpsePayload:
            return method == ItemSpawnMethod::CorpsePayload;
        case SpawnScheme::OnlineRetrieve:
            return method != ItemSpawnMethod::Game &&
                   method != ItemSpawnMethod::CorpsePayload;
    }
    return false;
}

bool CorpseSchemeSelected() {
    return g_spawn_scheme == SpawnScheme::CorpsePayload;
}

bool OnlineRetrieveSchemeSelected() {
    return g_spawn_scheme == SpawnScheme::OnlineRetrieve;
}

void SelectSpawnScheme(SpawnScheme scheme) {
    g_spawn_scheme = scheme;
    g_selected_type.clear();
    if (scheme == SpawnScheme::CorpsePayload) {
        g_destination = ItemSpawnDestination::Ground;
    } else {
        g_destination = ItemSpawnDestination::Backpack;
    }
}

void ApplySessionDefault() {
    const bridge::ItemSessionMode runtime_mode = bridge::GetItemSessionMode();
    if (runtime_mode == bridge::ItemSessionMode::Unknown) return;
    const DefaultSessionMode session_mode =
        runtime_mode == bridge::ItemSessionMode::Local
        ? DefaultSessionMode::Local
        : DefaultSessionMode::Network;
    if (session_mode == g_default_session_mode) return;

    const ItemSpawnMethod default_method = session_mode == DefaultSessionMode::Network
        ? ItemSpawnMethod::CorpsePayload : ItemSpawnMethod::Game;
    g_spawn_scheme = session_mode == DefaultSessionMode::Network
        ? SpawnScheme::CorpsePayload : SpawnScheme::Game;
    g_destination = session_mode == DefaultSessionMode::Network
        ? ItemSpawnDestination::Ground
        : ItemSpawnDestination::Backpack;
    g_last_execution_method = default_method;
    g_selected_type.clear();
    g_category = "全部";
    g_status = session_mode == DefaultSessionMode::Network
        ? "已进入联机会话，默认使用尸体载荷投递到脚下"
        : "当前为本地会话，默认使用游戏原生";
    g_default_session_mode = session_mode;
}

bool ItemSupports(const ItemCatalogEntry& item, ItemSpawnMethod method) {
    return (item.spawn_method_mask & bridge::ItemSpawnMethodMask(method)) != 0;
}

bool MatchesMethodFilter(const ItemCatalogEntry& item) {
    for (const MethodInfo& method : kMethods) {
        if (MethodEnabled(method.value) && ItemSupports(item, method.value)) {
            return true;
        }
    }
    return false;
}

const MethodInfo& GetMethodInfo(ItemSpawnMethod method) {
    const std::size_t index = static_cast<std::size_t>(method);
    return index < kMethods.size() ? kMethods[index] : kMethods.front();
}

bool ResolveMethod(const ItemCatalogEntry& item, ItemSpawnMethod& method) {
    for (ItemSpawnMethod candidate : kExecutionPriority) {
        if (MethodEnabled(candidate) && ItemSupports(item, candidate)) {
            method = candidate;
            return true;
        }
    }
    return false;
}

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool MatchesSearch(const ItemCatalogEntry& item) {
    if (g_search[0] == '\0') return true;
    const std::string search = LowerAscii(g_search);
    return LowerAscii(item.display_name).find(search) != std::string::npos ||
           LowerAscii(item.full_type).find(search) != std::string::npos;
}

std::vector<std::string> BuildCategories(
    const std::vector<ItemCatalogEntry>& catalog) {
    std::vector<std::string> categories{"全部"};
    for (const ItemCatalogEntry& item : catalog) {
        if (!MatchesMethodFilter(item)) continue;
        if (categories.size() == 1 || categories.back() != item.category) {
            categories.push_back(item.category);
        }
    }
    return categories;
}

std::string MethodSelectionLabel() {
    switch (g_spawn_scheme) {
        case SpawnScheme::Game: return T("游戏原生");
        case SpawnScheme::OnlineRetrieve: return T("联机取物");
        case SpawnScheme::CorpsePayload: return T("尸体载荷");
    }
    return T("游戏原生");
}

void DrawMethodSelector() {
    const std::string preview = MethodSelectionLabel();
    if (!BeginAnimatedCombo(
            "##SpawnMethods", preview.c_str(), U(126.0f))) return;

    if (ImGui::Selectable(
            T("游戏原生"), g_spawn_scheme == SpawnScheme::Game)) {
        SelectSpawnScheme(SpawnScheme::Game);
        CloseAnimatedDropdown();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", T("仅单机，支持全部物品"));

    if (ImGui::Selectable(
            T("联机取物"), g_spawn_scheme == SpawnScheme::OnlineRetrieve)) {
        SelectSpawnScheme(SpawnScheme::OnlineRetrieve);
        CloseAnimatedDropdown();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", T("自动选择可用联机路径，仅支持背包"));

    if (ImGui::Selectable(
            T("尸体载荷"), g_spawn_scheme == SpawnScheme::CorpsePayload)) {
        SelectSpawnScheme(SpawnScheme::CorpsePayload);
        CloseAnimatedDropdown();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", T("在脚下投递服务器僵尸尸体"));
    EndAnimatedDropdown();
}

bool SelectionButton(const char* label, bool selected, float width) {
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        selected ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.28f)
                 : ImVec4(0.055f, 0.060f, 0.075f, 0.72f));
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        selected ? kAccent : ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, U(1.0f));
    const bool clicked = ImGui::Button(label, ImVec2(width, U(34.0f)));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    return clicked;
}

void DestinationButton(const char* label, ItemSpawnDestination value,
                       float width) {
    if (SelectionButton(label, g_destination == value, width)) {
        g_destination = value;
    }
}

bool DrawItemCard(const ItemCatalogEntry& item, ItemSpawnMethod method,
                  float width, bool corpse_mode) {
    ImGui::PushID(item.full_type.c_str());
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const ImVec2 maximum(minimum.x + width, minimum.y + U(kCardHeight));
    ImGui::InvisibleButton("item_card", ImVec2(width, U(kCardHeight)));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const bool selected = corpse_mode
        ? IsCorpsePayloadItemSelected(item.full_type)
        : g_selected_type == item.full_type;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(hovered ? ImVec4(0.085f, 0.095f, 0.125f, 0.88f)
                                  : ImVec4(0.050f, 0.055f, 0.071f, 0.78f)),
        U(9.0f));
    draw->AddRect(
        minimum, maximum,
        ImGui::GetColorU32(selected ? ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.80f)
                                   : ImVec4(1.0f, 1.0f, 1.0f, 0.10f)),
        U(9.0f), 0, U(1.0f));

    if (corpse_mode) {
        const float selection_amount = animation::Clamp01(animation::Spring(
            ImGui::GetID("corpse_selection_marker"), selected ? 1.0f : 0.0f,
            300.0f, 22.0f));
        const float marker_scale = settings::UiScale();
        const float marker_size = 30.0f * marker_scale;
        const ImVec4 marker_color = animation::Lerp(
            ImVec4(0.24f, 0.26f, 0.31f, 0.86f),
            ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.96f),
            selection_amount);
        draw->AddTriangleFilled(
            minimum, ImVec2(minimum.x + marker_size, minimum.y),
            ImVec2(minimum.x, minimum.y + marker_size),
            ImGui::GetColorU32(marker_color));
        const ImU32 check_color = ImGui::GetColorU32(ImVec4(
            1.0f, 1.0f, 1.0f, 0.46f + selection_amount * 0.54f));
        const ImVec2 check_center(
            minimum.x + marker_size / 3.0f,
            minimum.y + marker_size / 3.0f);
        draw->AddLine(
            ImVec2(check_center.x - 4.3f * marker_scale,
                   check_center.y + 0.3f * marker_scale),
            ImVec2(check_center.x - 1.3f * marker_scale,
                   check_center.y + 3.2f * marker_scale), check_color,
            (1.7f + selection_amount * 0.5f) * marker_scale);
        draw->AddLine(
            ImVec2(check_center.x - 1.3f * marker_scale,
                   check_center.y + 3.2f * marker_scale),
            ImVec2(check_center.x + 5.1f * marker_scale,
                   check_center.y - 3.7f * marker_scale), check_color,
            (1.7f + selection_amount * 0.5f) * marker_scale);
    }

    const char* route = T(GetMethodInfo(method).short_label);
    const ImVec2 route_size = ImGui::CalcTextSize(route);
    const ImVec2 route_min(
        maximum.x - route_size.x - U(18.0f), minimum.y + U(8.0f));
    const ImVec2 route_max(
        maximum.x - U(8.0f), minimum.y + U(28.0f));
    draw->AddRectFilled(
        route_min, route_max,
        ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.20f)),
        U(5.0f));
    draw->AddText(ImVec2(route_min.x + U(5.0f), route_min.y + U(2.0f)),
                  ImGui::GetColorU32(kText), route);

    const float image_box = U(70.0f);
    const ImVec2 image_center(
        minimum.x + width * 0.5f, minimum.y + U(46.0f));
    if (item.texture_id != 0) {
        float image_width = image_box;
        float image_height = image_box;
        if (item.texture_width > 0 && item.texture_height > 0) {
            const float aspect = static_cast<float>(item.texture_width) /
                                 static_cast<float>(item.texture_height);
            if (aspect > 1.0f) image_height /= aspect;
            else image_width *= aspect;
        }
        const ImVec2 image_min(image_center.x - image_width * 0.5f,
                               image_center.y - image_height * 0.5f);
        const ImVec2 image_max(image_center.x + image_width * 0.5f,
                               image_center.y + image_height * 0.5f);
        draw->AddImage(
            ImTextureRef(static_cast<ImTextureID>(item.texture_id)), image_min, image_max,
            ImVec2(item.uv_min_x, item.uv_min_y),
            ImVec2(item.uv_max_x, item.uv_max_y),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
    }

    draw->PushClipRect(ImVec2(minimum.x + U(10.0f), minimum.y),
                       ImVec2(maximum.x - U(10.0f), maximum.y), true);
    draw->AddText(ImVec2(minimum.x + U(10.0f), minimum.y + U(91.0f)),
                  ImGui::GetColorU32(kText), item.display_name.c_str());
    draw->AddText(ImVec2(minimum.x + U(10.0f), minimum.y + U(112.0f)),
                  ImGui::GetColorU32(kMuted), item.full_type.c_str());
    draw->PopClipRect();
    if (selected) {
        draw->AddRectFilled(
            ImVec2(minimum.x + U(10.0f), maximum.y - U(3.0f)),
            ImVec2(maximum.x - U(10.0f), maximum.y),
            ImGui::GetColorU32(kAccent), U(2.0f));
    }
    ImGui::PopID();
    return clicked;
}

void UpdateActiveStatus() {
    const std::string* status = nullptr;
    switch (g_last_execution_method) {
        case ItemSpawnMethod::Game:
            status = &bridge::GetLastItemSpawnResult().message;
            break;
        case ItemSpawnMethod::ExplosiveTrapWeapon:
            status = &bridge::GetExplosiveTrapWeaponStatus();
            break;
        case ItemSpawnMethod::WeaponAmmoExtract:
            status = &bridge::GetWeaponAmmoExtractionStatus();
            break;
        case ItemSpawnMethod::WeaponMagazineExtract:
            status = &bridge::GetWeaponMagazineExtractionStatus();
            break;
        case ItemSpawnMethod::WeaponPartDetach:
            status = &bridge::GetExplosiveTrapWeaponPartStatus();
            break;
        case ItemSpawnMethod::PalletItemExtract:
            status = &bridge::GetPalletItemStatus();
            break;
        case ItemSpawnMethod::CorpsePayload:
            status = &bridge::GetCorpsePayloadStatus();
            break;
        default:
            break;
    }
    if (status != nullptr && !status->empty()) g_status = *status;
}

const ItemCatalogEntry* FindSelectedItem(
    const std::vector<ItemCatalogEntry>& catalog) {
    const auto found = std::find_if(
        catalog.begin(), catalog.end(), [](const ItemCatalogEntry& item) {
            return item.full_type == g_selected_type;
        });
    return found == catalog.end() ? nullptr : &*found;
}

void ExecuteSelected(const std::vector<ItemCatalogEntry>& catalog, int quantity) {
    if (CorpseSchemeSelected()) {
        const std::vector<bridge::ItemSpawnBatchEntry> items =
            BuildCorpsePayloadBatch();
        if (items.empty()) {
            g_status = "请先在物品卡片左上角勾选尸体载荷物品";
            return;
        }
        g_last_execution_method = ItemSpawnMethod::CorpsePayload;
        const bridge::ItemSpawnResult& result =
            bridge::SpawnCorpsePayloadBatch(items);
        g_status = result.message;
        return;
    }
    if (OnlineRetrieveSchemeSelected() &&
        g_destination != ItemSpawnDestination::Backpack) {
        g_destination = ItemSpawnDestination::Backpack;
        g_status = "联机取物仅支持背包，不能生成到地面";
        return;
    }
    const ItemCatalogEntry* item = FindSelectedItem(catalog);
    ItemSpawnMethod method = ItemSpawnMethod::Game;
    if (item == nullptr || !ResolveMethod(*item, method)) {
        g_status = "当前选择的方案不支持该物品";
        return;
    }
    g_last_execution_method = method;
    const bridge::ItemSpawnResult& result = bridge::SpawnItem(
        item->full_type, quantity, g_destination, method);
    g_status = result.message;
}

}  // namespace

void DrawItemGenerator(const bridge::FrameSnapshot& frame) {
    (void)frame;
    ApplySessionDefault();
    UpdateActiveStatus();
    if (!bridge::RefreshItemCatalog()) {
        SectionLabel("物品生成");
        ImGui::TextUnformatted(settings::Translate(
            "正在读取游戏物品目录，请稍后重试。"));
        return;
    }

    const std::vector<ItemCatalogEntry>& catalog = bridge::GetItemCatalog();
    const bool corpse_scheme = CorpseSchemeSelected();
    const bool online_retrieve_scheme = OnlineRetrieveSchemeSelected();
    const std::vector<std::string> categories = BuildCategories(catalog);
    if (std::find(categories.begin(), categories.end(), g_category) ==
        categories.end()) {
        g_category = "全部";
    }

    SectionLabel("物品筛选与生成");
    ImGui::Dummy(ImVec2(0.0f, U(3.0f)));
    BeginCard("ItemGeneratorControls", nullptr, ImVec2(0.0f, 0.0f));
    const float control_width = ImGui::GetContentRegionAvail().x;
    const char* category_preview = ItemCategoryLabel(g_category);
    const std::string method_preview = MethodSelectionLabel();
    const float category_width = TextControlWidth(
        category_preview, 160.0f, 42.0f);
    const float method_width = TextControlWidth(
        method_preview.c_str(), 238.0f, 42.0f);
    const float field_gap = U(8.0f);
    const bool wrap_filters = control_width < U(620.0f);
    const float search_width = wrap_filters
        ? control_width
        : std::max(
            U(180.0f),
            control_width - category_width - method_width - field_gap * 2.0f);
    ImGui::SetNextItemWidth(search_width);
    ImGui::InputTextWithHint(
        "##ItemSearch", settings::Translate("搜索物品名称或完整类型"),
        g_search, sizeof(g_search));
    if (wrap_filters) {
        ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
    } else {
        ImGui::SameLine(0.0f, field_gap);
    }
    ImGui::SetNextItemWidth(category_width);
    const float category_popup_height = std::min(
        U(320.0f), U(16.0f + static_cast<float>(categories.size()) * 28.0f));
    if (BeginAnimatedCombo(
            "##ItemCategory", category_preview, category_popup_height)) {
        for (const std::string& category : categories) {
            const bool selected = category == g_category;
            ImGui::PushID(category.c_str());
            if (ImGui::Selectable(ItemCategoryLabel(category), selected)) {
                g_category = category;
                CloseAnimatedDropdown();
            }
            ImGui::PopID();
            if (selected) ImGui::SetItemDefaultFocus();
        }
        EndAnimatedDropdown();
    }
    ImGui::SameLine(0.0f, field_gap);
    ImGui::SetNextItemWidth(method_width);
    DrawMethodSelector();

    ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
    const ImVec2 control_row_start = ImGui::GetCursorScreenPos();
    if (!corpse_scheme) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(T("数量"));
        ImGui::SameLine(0.0f, U(8.0f));
        ImGui::SetNextItemWidth(U(156.0f));
        if (ImGui::InputInt("##ItemQuantity", &g_quantity, 1, 10)) {
            g_quantity = std::clamp(g_quantity, 1, 100);
        }
        ImGui::SameLine(0.0f, U(18.0f));
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(T("生成到"));
    ImGui::SameLine(0.0f, U(8.0f));
    if (corpse_scheme) {
        g_destination = ItemSpawnDestination::Ground;
        ImGui::PushStyleColor(
            ImGuiCol_Text, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.92f));
        ImGui::TextUnformatted(T("脚下僵尸尸体"));
        ImGui::PopStyleColor();
    } else {
        const char* backpack_label = T("背包");
        const char* ground_label = T("地面");
        const float backpack_width = TextControlWidth(
            backpack_label, 82.0f, 24.0f);
        const float ground_width = TextControlWidth(
            ground_label, 82.0f, 24.0f);
        DestinationButton(
            backpack_label, ItemSpawnDestination::Backpack, backpack_width);
        ImGui::SameLine(0.0f, U(7.0f));
        if (online_retrieve_scheme) {
            g_destination = ItemSpawnDestination::Backpack;
            ImGui::BeginDisabled();
            SelectionButton(ground_label, false, ground_width);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", T(
                    "联机取物仅支持背包，不能生成到地面"));
            }
        } else {
            DestinationButton(
                ground_label, ItemSpawnDestination::Ground, ground_width);
        }
    }

    const bool online_cooldown_active =
        bridge::GetItemSessionMode() == bridge::ItemSessionMode::MultiplayerClient;
    const int online_cooldown_milliseconds = online_cooldown_active
        ? bridge::GetOnlineItemSpawnCooldownRemaining()
        : 0;
    char cooldown_label[64]{};
    const char* execute_label = T("执行方案");
    if (online_cooldown_milliseconds > 0) {
        std::snprintf(
            cooldown_label, sizeof(cooldown_label), "请稍候 %.1f 秒",
            static_cast<double>(online_cooldown_milliseconds) / 1000.0);
        execute_label = cooldown_label;
    }
    const float execute_width = TextControlWidth(execute_label, 116.0f, 24.0f);
    const float action_width = execute_width;
    const float action_left = control_row_start.x + control_width - action_width;
    const float destination_right = ImGui::GetItemRectMax().x;
    if (destination_right + U(12.0f) <= action_left) {
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(ImVec2(action_left, control_row_start.y));
    } else {
        ImGui::SetCursorScreenPos(ImVec2(
            std::max(control_row_start.x, action_left),
            ImGui::GetItemRectMax().y + U(6.0f)));
    }
    ImGui::BeginDisabled(
        (corpse_scheme ? !HasCorpsePayloadSelection() : g_selected_type.empty()) ||
        online_cooldown_milliseconds > 0);
    ImGui::PushStyleColor(
        ImGuiCol_Button, ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.78f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccent);
    if (ImGui::Button(
            execute_label, ImVec2(execute_width, U(34.0f)))) {
        ExecuteSelected(catalog, g_quantity);
    }
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();

    if (online_retrieve_scheme) {
        ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
        ImGui::PushStyleColor(
            ImGuiCol_Text, ImVec4(1.0f, 0.68f, 0.24f, 0.96f));
        ImGui::TextUnformatted(T(
            "联机取物仅支持背包，不能生成到地面"));
        ImGui::PopStyleColor();
    }

    const ItemCatalogEntry* selected_item = FindSelectedItem(catalog);
    ItemSpawnMethod resolved_method = ItemSpawnMethod::Game;
    if (corpse_scheme && HasCorpsePayloadSelection()) {
        ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::Text(
            T("当前路径：尸体载荷  ·  已选择 %zu 种物品"),
            CorpsePayloadSelectionCount());
        ImGui::PopStyleColor();
    } else if (selected_item != nullptr &&
               ResolveMethod(*selected_item, resolved_method)) {
        ImGui::Dummy(ImVec2(0.0f, U(4.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::Text(T("当前路径：%s  ·  %s"),
                    T(GetMethodInfo(resolved_method).label),
                    selected_item->full_type.c_str());
        ImGui::PopStyleColor();
    }
    if (!g_status.empty()) {
        ImGui::Dummy(ImVec2(0.0f, U(2.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("%s", T(g_status.c_str()));
        ImGui::PopStyleColor();
    }
    EndCard();

    std::vector<const ItemCatalogEntry*> filtered;
    filtered.reserve(catalog.size());
    for (const ItemCatalogEntry& item : catalog) {
        if (MatchesMethodFilter(item) &&
            (g_category == "全部" || item.category == g_category) &&
            MatchesSearch(item)) {
            filtered.push_back(&item);
        }
    }
    if (!corpse_scheme && !g_selected_type.empty()) {
        const bool selected_visible = std::any_of(
            filtered.begin(), filtered.end(), [](const ItemCatalogEntry* item) {
                return item->full_type == g_selected_type;
            });
        if (!selected_visible) g_selected_type.clear();
    }

    ImGui::Dummy(ImVec2(0.0f, U(9.0f)));
    char available_label[64]{};
    std::snprintf(
        available_label, sizeof(available_label),
        T("可用物品 · %zu"), filtered.size());
    SectionLabel(available_label);
    ImGui::Dummy(ImVec2(0.0f, U(3.0f)));
    ImGui::BeginChild("ItemCatalog", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (filtered.empty()) {
        ImGui::Dummy(ImVec2(0.0f, U(24.0f)));
        ImGui::TextDisabled("%s", settings::Translate(
            "当前方案下没有匹配物品，请调整方案或搜索条件。"));
        ImGui::EndChild();
        return;
    }

    const float available_width = ImGui::GetContentRegionAvail().x;
    const float card_gap = U(kCardGap);
    const float card_height = U(kCardHeight);
    const int columns = std::max(
        2, static_cast<int>((available_width + card_gap) / U(164.0f)));
    const float card_width =
        (available_width - card_gap * static_cast<float>(columns - 1)) /
        static_cast<float>(columns);
    const int rows = static_cast<int>((filtered.size() + columns - 1) / columns);
    const float start_y = ImGui::GetCursorPosY();
    ImGuiListClipper clipper;
    clipper.Begin(rows, card_height + card_gap);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            ImGui::SetCursorPosY(start_y + row * (card_height + card_gap));
            for (int column = 0; column < columns; ++column) {
                const int index = row * columns + column;
                if (index >= static_cast<int>(filtered.size())) break;
                if (column > 0) ImGui::SameLine(0.0f, card_gap);
                ItemSpawnMethod method = ItemSpawnMethod::Game;
                ResolveMethod(*filtered[static_cast<std::size_t>(index)], method);
                if (DrawItemCard(
                        *filtered[static_cast<std::size_t>(index)], method,
                        card_width, corpse_scheme)) {
                    const ItemCatalogEntry& clicked_item =
                        *filtered[static_cast<std::size_t>(index)];
                    if (corpse_scheme) {
                        ToggleCorpsePayloadItem(clicked_item);
                    } else {
                        g_selected_type = clicked_item.full_type;
                    }
                }
            }
        }
    }
    ImGui::EndChild();
}

bool IsCorpsePayloadSchemeActive() {
    return CorpseSchemeSelected();
}

}  // namespace pztrainer::ui
