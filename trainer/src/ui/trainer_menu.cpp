#include "ui/trainer_menu.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <vector>

#include "bridge/lua_feature_registry.hpp"
#include "features/visual/visual_settings.hpp"
#include "features/visual/player_visual_settings.hpp"
#include "features/visual/animal_visual_settings.hpp"
#include "features/visual/vehicle_visual_settings.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "settings/localization.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animation.hpp"
#include "ui/animation_dropdown.hpp"
#include "ui/aimbot_page.hpp"
#include "ui/color_picker.hpp"
#include "ui/character_status.hpp"
#include "ui/components.hpp"
#include "ui/corpse_payload_panel.hpp"
#include "ui/experience_editor.hpp"
#include "ui/creation/character_creation_page.hpp"
#include "ui/vehicle/vehicle_control_page.hpp"
#include "ui/world/world_interaction_page.hpp"
#include "ui/glass_blur.hpp"
#include "ui/item_generator.hpp"
#include "ui/lua_page.hpp"
#include "ui/model_effect_controls.hpp"
#include "ui/navigation.hpp"
#include "ui/player_preview.hpp"
#include "ui/settings_page.hpp"
#include "ui/steam_profile_card.hpp"
#include "ui/theme.hpp"
#include "ui/animal_preview.hpp"
#include "ui/vehicle_preview.hpp"
#include "ui/world_visual.hpp"
#include "ui/zombie_preview.hpp"

namespace pztrainer::ui {
namespace {

using namespace components;

constexpr float kWindowWidth = 1010.0f;
constexpr float kWindowHeight = 720.0f;
constexpr float kSidebarWidth = 205.0f;
constexpr float kTopbarHeight = 66.0f;
constexpr float kDetachedPreviewWidth = 300.0f;
constexpr float kDetachedPreviewGap = 12.0f;

enum class Page {
    Rage,
    Legit,
    Visual,
    WorldVisual,
    Character,
    Items,
    Experience,
    VehicleControl,
    WorldInteraction,
    CharacterCreation,
    Lua,
    LuaRegistered,
    Settings,
};

struct SearchEntry {
    const char* label;
    const char* keywords;
    Page page;
};

struct SearchResult {
    std::string label;
    std::string keywords;
    Page page = Page::Visual;
    int visual_category = -1;
};

constexpr std::array<SearchEntry, 12> kSearchEntries{{
    {"Rage", "rage 暴力自瞄 自动瞄准 自动开枪 静默瞄准", Page::Rage},
    {"Legit", "legit 辅助瞄准 平滑 精度", Page::Legit},
    {"视觉 ESP", "视觉 esp 僵尸 玩家 动物 载具 透视 方框 骨骼", Page::Visual},
    {"世界视觉", "世界视觉 伪白天 暗区 天气 雾 地图迷雾 降水 枪口射线", Page::WorldVisual},
    {"角色状态", "角色状态 无限生命 无敌 穿墙 地图传送 体力 耐久 子弹 状态效果 暴击 近战 背包 修复 衣物 肌肉 隐身 水桶", Page::Character},
    {"物品生成", "物品生成 道具 武器 弹药", Page::Items},
    {"技能经验", "技能经验 熟练度 等级 xp", Page::Experience},
    {"载具控制", "载具 车辆 引擎 修复 汽油 传送 无碰撞 即停 Instant", Page::VehicleControl},
    {"世界交互", "世界 僵尸 AI 击杀 尸体 容器 战利品 动物 名字 音效 格子 门窗 解锁 碎玻璃 点火 投掷物", Page::WorldInteraction},
    {"角色创建", "角色创建 职业 特质 出生 地点 存档 种子 姓名 性别 发型 颜色 胡须 肤色 声音 音调 服装 纹理 预设 技能", Page::CharacterCreation},
    {"Lua", "lua 脚本 加载 重载 卸载", Page::Lua},
    {"设置", "设置 配置 选项", Page::Settings},
}};

struct SearchState {
    bool open = false;
    bool focus_requested = false;
    float amount = 0.0f;
    float velocity = 0.0f;
    std::array<char, 128> query{};
};

Page g_page = Page::Visual;
Page g_previous_page = Page::Visual;
float g_page_animation = 1.0f;
float g_page_velocity = 0.0f;
float g_menu_animation = 0.0f;
float g_menu_velocity = 0.0f;
ImVec2 g_window_position{};
bool g_window_position_initialized = false;
int g_visual_tab = 0;
SearchState g_search;
std::uint64_t g_applied_preferences_revision = 0;
std::string g_lua_page_title = "Lua";

float U(float value) {
    return value * settings::UiScale();
}

void DrawLuaNavigationIcon(
    ImDrawList* draw, const ImVec2& center, ImU32 color,
    const void* context) {
    const auto* category = static_cast<const bridge::LuaUiCategory*>(context);
    if (category != nullptr) {
        bridge::DrawLuaCategoryIcon(*category, draw, center, color);
    }
}

const char* PageTitle() {
    switch (g_page) {
        case Page::Rage: return "Rage";
        case Page::Legit: return "Legit";
        case Page::Visual: return settings::Translate("视觉");
        case Page::WorldVisual: return settings::Translate("世界视觉");
        case Page::Character: return settings::Translate("角色状态");
        case Page::Items: return settings::Translate("物品生成");
        case Page::Experience: return settings::Translate("技能经验");
        case Page::VehicleControl: return settings::Translate("载具控制");
        case Page::WorldInteraction: return settings::Translate("世界交互");
        case Page::CharacterCreation: return settings::Translate("角色创建");
        case Page::Lua: return "Lua";
        case Page::LuaRegistered: return g_lua_page_title.c_str();
        case Page::Settings: return settings::Translate("设置");
    }
    return settings::Translate("视觉");
}

void DrawTopbarPageBadge() {
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const float height = U(34.0f);
    if (SemiboldFont() != nullptr) ImGui::PushFont(SemiboldFont(), 0.0f);
    const char* title = PageTitle();
    const ImVec2 text_size = ImGui::CalcTextSize(title);
    const float width = std::max(U(104.0f), text_size.x + U(45.0f));
    const ImVec2 maximum(minimum.x + width, minimum.y + height);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(0.040f, 0.050f, 0.068f, 0.94f)),
        U(8.0f));
    draw->AddRect(
        minimum, maximum,
        ImGui::GetColorU32(ImVec4(0.20f, 0.46f, 0.95f, 0.43f)),
        U(8.0f), 0, U(1.0f));
    const ImVec2 accent_center(
        minimum.x + U(15.0f), minimum.y + height * 0.5f);
    draw->AddCircleFilled(
        accent_center, U(7.0f),
        ImGui::GetColorU32(ImVec4(0.10f, 0.42f, 1.0f, 0.10f)), 16);
    draw->AddCircleFilled(
        accent_center, U(3.1f),
        ImGui::GetColorU32(ImVec4(0.20f, 0.58f, 1.0f, 1.0f)), 12);
    draw->AddText(
        ImVec2(minimum.x + U(28.0f),
               minimum.y + (height - text_size.y) * 0.5f),
        ImGui::GetColorU32(ImVec4(0.89f, 0.92f, 0.98f, 1.0f)), title);
    if (SemiboldFont() != nullptr) ImGui::PopFont();
    ImGui::Dummy(ImVec2(width, height));
}

void SidebarNav(const char* label, navigation::Icon icon, Page page) {
    if (navigation::Item(label, icon, g_page == page)) {
        g_page = page;
    }
}

void DrawSidebarSectionLabel(const char* label) {
    ImGui::PushStyleColor(
        ImGuiCol_Text, ImVec4(0.48f, 0.58f, 0.69f, 0.90f));
    ImGui::TextUnformatted(settings::Translate(label));
    ImGui::PopStyleColor();
}

void DrawSidebar() {
    ImGui::SetCursorPos(ImVec2(U(1.0f), U(1.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(12.0f), U(12.0f)));
    ImGui::PushStyleVar(
        ImGuiStyleVar_ItemSpacing, ImVec2(U(8.0f), U(4.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::BeginChild(
        "Sidebar",
        ImVec2(U(kSidebarWidth), ImGui::GetWindowHeight() - U(2.0f)),
        ImGuiChildFlags_AlwaysUseWindowPadding);

    DrawBrandBadge("PZ");
    ImGui::SameLine(0.0f, U(12.0f));
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    if (SemiboldFont() != nullptr) ImGui::PushFont(SemiboldFont(), 0.0f);
    ImGui::TextUnformatted("PZ Solo Assist");
    if (SemiboldFont() != nullptr) ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted("Project Zomboid");
    ImGui::PopStyleColor();
    ImGui::EndGroup();

    ImGui::BeginChild("SidebarNavigation", ImVec2(0, ImGui::GetContentRegionAvail().y - U(76)),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::Dummy(ImVec2(0.0f, U(18.0f)));
    DrawSidebarSectionLabel("自瞄");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    navigation::BeginGroup("AimbotNavigation", 2);
    SidebarNav("Rage", navigation::Icon::Crosshair, Page::Rage);
    SidebarNav("Legit", navigation::Icon::Mouse, Page::Legit);
    navigation::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, U(11.0f)));
    DrawSidebarSectionLabel("视觉");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    navigation::BeginGroup("VisualNavigation", 2);
    SidebarNav("视觉 ESP", navigation::Icon::Eye, Page::Visual);
    SidebarNav("世界视觉", navigation::Icon::World, Page::WorldVisual);
    navigation::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, U(11.0f)));
    DrawSidebarSectionLabel("角色");
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    const std::vector<bridge::LuaUiCategory> lua_categories =
        bridge::SnapshotLuaUiCategories();
    const int integrated_lua_category_count = static_cast<int>(std::count_if(
        lua_categories.begin(), lua_categories.end(),
        [](const bridge::LuaUiCategory& category) {
            return !category.standalone;
        }));
    navigation::BeginGroup("CharacterNavigation", 4);
    SidebarNav("角色状态", navigation::Icon::User, Page::Character);
    SidebarNav("物品生成", navigation::Icon::Package, Page::Items);
    SidebarNav("技能经验", navigation::Icon::Spark, Page::Experience);
    SidebarNav("角色创建", navigation::Icon::User, Page::CharacterCreation);
    navigation::EndGroup();

    ImGui::Dummy(ImVec2(0, U(11)));
    DrawSidebarSectionLabel("世界");
    navigation::BeginGroup("WorldNavigation", 2);
    SidebarNav("载具控制", navigation::Icon::Vehicle, Page::VehicleControl);
    SidebarNav("世界交互", navigation::Icon::World, Page::WorldInteraction);
    navigation::EndGroup();

    ImGui::Dummy(ImVec2(0, U(11)));
    DrawSidebarSectionLabel("工具");
    navigation::BeginGroup("ToolsNavigation", 2 + integrated_lua_category_count);
    SidebarNav("Lua", navigation::Icon::Lua, Page::Lua);
    for (const bridge::LuaUiCategory& category : lua_categories) {
        if (category.standalone) continue;
        const bool selected = g_page == Page::LuaRegistered &&
            bridge::SelectedLuaUiCategory() == category.id;
        const bool clicked = category.icon_primitives.empty()
            ? navigation::Item(
                category.name.c_str(), navigation::Icon::Lua, selected)
            : navigation::Item(
                category.name.c_str(), selected,
                &DrawLuaNavigationIcon, &category);
        if (clicked) {
            g_page = Page::LuaRegistered;
            g_lua_page_title = category.name;
            bridge::SelectLuaUiCategory(category.id);
        }
    }
    SidebarNav("设置", navigation::Icon::Settings, Page::Settings);
    navigation::EndGroup();
    ImGui::EndChild();

    ImGui::SetCursorPosY(std::max(
        ImGui::GetCursorPosY() + U(11.0f),
        ImGui::GetWindowHeight() - U(76.0f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    DrawSteamProfileCard();

    ImGui::EndChild();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor();
}

bool SearchTextContains(const char* text, const char* query) {
    if (text == nullptr || query == nullptr || query[0] == '\0') return false;
    const std::string source(text);
    const std::string needle(query);
    return std::search(
        source.begin(), source.end(), needle.begin(), needle.end(),
        [](char left, char right) {
            const unsigned char lhs = static_cast<unsigned char>(left);
            const unsigned char rhs = static_cast<unsigned char>(right);
            if (lhs < 0x80 && rhs < 0x80) {
                return std::tolower(lhs) == std::tolower(rhs);
            }
            return lhs == rhs;
        }) != source.end();
}

bool ResolveFeatureSearchDestination(
        const bridge::LuaFeatureDescriptor& feature,
        Page& page, int& visual_category, const char*& section) {
    visual_category = -1;
    if (feature.group == "aim.legit") {
        page = Page::Legit;
        section = "Legit";
    } else if (feature.group == "aim.rage") {
        page = Page::Rage;
        section = "Rage";
    } else if (feature.group == "visual.zombie") {
        page = Page::Visual;
        visual_category = 0;
        section = "僵尸";
    } else if (feature.group == "visual.player") {
        page = Page::Visual;
        visual_category = 1;
        section = "玩家";
    } else if (feature.group == "visual.animal") {
        page = Page::Visual;
        visual_category = 2;
        section = "动物";
    } else if (feature.group == "visual.vehicle") {
        page = Page::Visual;
        visual_category = 3;
        section = "载具";
    } else if (feature.group == "world.visual") {
        page = Page::WorldVisual;
        section = "世界视觉";
    } else if (feature.group == "character") {
        page = Page::Character;
        section = "角色状态";
    } else if (feature.group == "extensions") {
        if (feature.path == "extensions.engine_running" || feature.path == "extensions.vehicle_instant") {
            page = Page::VehicleControl;
            section = "载具控制";
        } else if (feature.path == "extensions.passive_zombies" ||
                   feature.path == "extensions.useless_zombies" ||
                   feature.path == "extensions.instant_kill" ||
                   feature.path == "extensions.kill_range") {
            page = Page::WorldInteraction;
            section = "世界交互";
        } else {
            page = Page::Character;
            section = "角色状态";
        }
    } else {
        return false;
    }
    return true;
}

std::vector<SearchResult> BuildSearchResults() {
    std::vector<SearchResult> results;
    results.reserve(kSearchEntries.size() + 96);
    for (const SearchEntry& entry : kSearchEntries) {
        SearchResult result;
        result.label = settings::Translate(entry.label);
        result.keywords = std::string(entry.label) + " " +
            settings::Translate(entry.label) + " " + entry.keywords;
        result.page = entry.page;
        results.push_back(std::move(result));
    }

    const std::vector<bridge::LuaFeatureDescriptor> features =
        bridge::SnapshotLuaFeatureRegistry();
    for (const bridge::LuaFeatureDescriptor& feature : features) {
        Page page = Page::Visual;
        int visual_category = -1;
        const char* section = nullptr;
        if (!ResolveFeatureSearchDestination(
                feature, page, visual_category, section)) {
            continue;
        }
        const std::string translated_label = settings::Translate(
            feature.label.c_str());
        const bool duplicate = std::any_of(
            results.begin(), results.end(), [&](const SearchResult& result) {
                return result.page == page &&
                    result.visual_category == visual_category &&
                    result.label == std::string(settings::Translate(section)) +
                        " · " + translated_label;
            });
        if (duplicate) continue;

        SearchResult result;
        result.label = std::string(settings::Translate(section)) +
            " · " + translated_label;
        result.keywords = feature.label + " " + translated_label + " " +
            feature.path + " " + feature.group;
        result.page = page;
        result.visual_category = visual_category;
        results.push_back(std::move(result));
    }
    return results;
}

bool SearchResultMatches(const SearchResult& result) {
    return SearchTextContains(result.label.c_str(), g_search.query.data()) ||
           SearchTextContains(result.keywords.c_str(), g_search.query.data());
}

std::vector<SearchResult> MatchingSearchResults() {
    std::vector<SearchResult> matches;
    for (SearchResult& result : BuildSearchResults()) {
        if (SearchResultMatches(result)) {
            matches.push_back(std::move(result));
        }
    }
    return matches;
}

void ActivateSearchResult(const SearchResult& result) {
    g_page = result.page;
    if (result.visual_category >= 0) {
        g_visual_tab = result.visual_category;
    }
    g_search.query.fill('\0');
    g_search.open = false;
}

bool DrawSearchIconButton(const ImVec2& position, bool active,
                          bool& hovered) {
    ImGui::PushID("FunctionSearchButton");
    ImGui::SetCursorScreenPos(position);
    ImGui::InvisibleButton("search", ImVec2(U(30.0f), U(30.0f)));
    hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const float amount = animation::Clamp01(animation::Spring(
        ImGui::GetID("search_hover"), active ? 1.0f : (hovered ? 0.55f : 0.0f),
        240.0f, 21.0f));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (amount > 0.001f) {
        draw->AddRectFilled(
            position, ImVec2(position.x + U(30.0f), position.y + U(30.0f)),
            ImGui::GetColorU32(ImVec4(
                0.18f, 0.32f, 0.48f, 0.22f * amount)), 8.0f);
    }
    const ImU32 color = ImGui::GetColorU32(animation::Lerp(
        kMuted, active ? kAccent : kText, amount));
    const ImVec2 center(position.x + U(14.0f), position.y + U(13.0f));
    draw->AddCircle(center, U(5.5f), color, 0, U(1.8f));
    draw->AddLine(
        ImVec2(center.x + U(4.0f), center.y + U(4.0f)),
        ImVec2(center.x + U(8.0f), center.y + U(8.0f)), color, U(1.8f));
    ImGui::PopID();
    return clicked;
}

bool DrawSearchResults(const ImVec2& input_minimum, float width) {
    if (!g_search.open || g_search.amount < 0.72f ||
        g_search.query[0] == '\0') {
        return false;
    }

    const std::vector<SearchResult> results = MatchingSearchResults();
    const int result_count = static_cast<int>(results.size());
    constexpr int kMaximumVisibleResults = 7;
    const float height = U(14.0f) +
        static_cast<float>(std::max(
            std::min(result_count, kMaximumVisibleResults), 1)) * U(36.0f);
    ImGui::SetNextWindowPos(
        ImVec2(input_minimum.x, input_minimum.y + U(36.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(7.0f), U(7.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.0f));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.030f, 0.032f, 0.043f, 0.86f));
    ImGui::PushStyleColor(
        ImGuiCol_Border, ImVec4(0.34f, 0.48f, 0.66f, 0.32f));
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
    if (result_count <= kMaximumVisibleResults) {
        flags |= ImGuiWindowFlags_NoScrollbar;
    }
    bool hovered = false;
    if (ImGui::Begin("##FunctionSearchResults", nullptr, flags)) {
        hovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        if (result_count == 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
            ImGui::TextUnformatted(settings::Translate("没有匹配功能"));
            ImGui::PopStyleColor();
        } else {
            for (const SearchResult& result : results) {
                if (ImGui::Selectable(
                        result.label.c_str(), false,
                        ImGuiSelectableFlags_None,
                        ImVec2(0.0f, U(32.0f)))) {
                    ActivateSearchResult(result);
                }
                if (ImGui::IsItemHovered() &&
                    ImGui::CalcTextSize(result.label.c_str()).x >
                        ImGui::GetContentRegionAvail().x) {
                    components::RoundedTooltip(result.label.c_str());
                }
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    return hovered;
}

void DrawFunctionSearch() {
    animation::SpringValue(
        g_search.amount, g_search.velocity, g_search.open ? 1.0f : 0.0f,
        150.0f, 18.0f);
    const float amount = animation::Clamp01(g_search.amount);
    const ImVec2 topbar_position = ImGui::GetWindowPos();
    const float right = topbar_position.x + ImGui::GetWindowWidth() - U(18.0f);
    const ImVec2 icon_position(
        right - U(30.0f), topbar_position.y + U(18.0f));
    bool icon_hovered = false;
    if (DrawSearchIconButton(icon_position, g_search.open, icon_hovered)) {
        g_search.open = !g_search.open;
        g_search.focus_requested = g_search.open;
    }

    const float kSearchWidth = U(220.0f);
    const float input_width = kSearchWidth * amount;
    const ImVec2 input_minimum(
        icon_position.x - U(8.0f) - input_width, icon_position.y);
    bool input_hovered = false;
    bool input_active = false;
    bool submitted = false;
    if (input_width > 8.0f) {
        ImGui::SetCursorScreenPos(input_minimum);
        ImGui::SetNextItemWidth(input_width);
        if (g_search.focus_requested && amount > 0.42f) {
            ImGui::SetKeyboardFocusHere();
            g_search.focus_requested = false;
        }
        ImGui::PushStyleVar(
            ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * amount);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(
            ImGuiCol_FrameBg, ImVec4(0.042f, 0.044f, 0.058f, 0.96f));
        ImGui::PushStyleColor(
            ImGuiCol_Border, ImVec4(0.31f, 0.45f, 0.63f, 0.28f));
        submitted = ImGui::InputTextWithHint(
            "##FunctionSearchInput", settings::Translate("搜索功能..."),
            g_search.query.data(),
            g_search.query.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        input_hovered = ImGui::IsItemHovered();
        input_active = ImGui::IsItemActive();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

    if (submitted) {
        const std::vector<SearchResult> results = MatchingSearchResults();
        if (!results.empty()) {
            ActivateSearchResult(results.front());
        }
    }
    const bool results_hovered = DrawSearchResults(
        ImVec2(icon_position.x - U(8.0f) - kSearchWidth, icon_position.y),
        kSearchWidth);
    if (g_search.open && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        g_search.open = false;
    }
    if (g_search.open && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !icon_hovered && !input_hovered && !input_active &&
        !results_hovered) {
        g_search.open = false;
    }
}

void DrawTopbar() {
    ImGui::SetCursorPos(ImVec2(U(kSidebarWidth + 1.0f), U(1.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(18.0f), 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::BeginChild(
        "Topbar",
        ImVec2(
            ImGui::GetWindowWidth() - U(kSidebarWidth + 2.0f),
            U(kTopbarHeight)),
        ImGuiChildFlags_AlwaysUseWindowPadding,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!g_search.open &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        g_window_position.x += ImGui::GetIO().MouseDelta.x;
        g_window_position.y += ImGui::GetIO().MouseDelta.y;
    }
    ImGui::SetCursorPosY(U(16.0f));
    DrawTopbarPageBadge();
    if (g_page == Page::Legit || g_page == Page::Rage) {
        ImGui::SameLine(U(205.0f));
        DrawAimbotWeaponSelector(
            g_page == Page::Legit ? AimbotPage::Legit : AimbotPage::Rage);
    }
    DrawFunctionSearch();
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void DrawShellBackgrounds(const ImVec2& window_position, const ImVec2& window_size) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float sidebar_edge = window_position.x + U(kSidebarWidth + 1.0f);
    const ImVec2 sidebar_minimum(
        window_position.x + U(1.0f), window_position.y + U(1.0f));
    const ImVec2 sidebar_maximum(
        sidebar_edge, window_position.y + window_size.y - U(1.0f));
    draw->AddRectFilled(
        sidebar_minimum, sidebar_maximum,
        ImGui::GetColorU32(ImVec4(0.035f, 0.070f, 0.100f, 0.40f)),
        11.0f, ImDrawFlags_RoundCornersLeft);
    draw->AddRectFilled(
        ImVec2(sidebar_minimum.x + U(1.0f), sidebar_minimum.y + U(1.0f)),
        ImVec2(sidebar_maximum.x, sidebar_maximum.y - U(1.0f)),
        ImGui::GetColorU32(ImVec4(0.20f, 0.42f, 0.62f, 0.055f)),
        10.0f, ImDrawFlags_RoundCornersLeft);
    draw->AddRectFilled(
        ImVec2(sidebar_edge, window_position.y + U(1.0f)),
        ImVec2(window_position.x + window_size.x - U(1.0f),
               window_position.y + U(kTopbarHeight + 1.0f)),
        ImGui::GetColorU32(ImVec4(0.025f, 0.027f, 0.035f, 0.64f)),
        11.0f, ImDrawFlags_RoundCornersTopRight);
}

void DrawShellSeparators(const ImVec2& window_position, const ImVec2& window_size) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 separator = ImGui::GetColorU32(
        ImVec4(0.46f, 0.64f, 0.82f, 0.18f));
    const float sidebar_edge = window_position.x + U(kSidebarWidth + 1.0f);
    const float topbar_edge = window_position.y + U(kTopbarHeight + 1.0f);
    draw->AddLine(
        ImVec2(sidebar_edge, window_position.y + U(1.0f)),
        ImVec2(sidebar_edge, window_position.y + window_size.y - U(1.0f)),
        separator, 1.0f);
    draw->AddLine(
        ImVec2(sidebar_edge, topbar_edge),
        ImVec2(window_position.x + window_size.x - U(1.0f), topbar_edge),
        separator, 1.0f);
    draw->AddLine(
        ImVec2(window_position.x + U(16.0f), window_position.y + U(1.5f)),
        ImVec2(window_position.x + window_size.x - U(16.0f),
               window_position.y + U(1.5f)),
        ImGui::GetColorU32(ImVec4(0.82f, 0.87f, 1.0f, 0.10f)), 1.0f);
}

void DrawVisualCategoryLabel(const char* label, int category) {
    ImGui::PushStyleColor(
        ImGuiCol_Text, g_visual_tab == category ? kAccent : kMuted);
    ImGui::TextUnformatted(settings::Translate(label));
    ImGui::PopStyleColor();
}

void SelectVisualCategoryOnCardClick(int category) {
    if (ImGui::IsWindowHovered(
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_visual_tab = category;
    }
}

void DrawVisualControls() {
    features::visual::VisualSettings& settings = features::visual::GetVisualSettings();
    DrawVisualCategoryLabel("僵尸", 0);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    BeginCompactCard("ZombieControls", nullptr, ImVec2(0.0f, 0.0f));
    CompactToggleRow("启用僵尸透视", &settings.zombie_esp);
    CompactToggleRow(
        "显示方框", &settings.colored_marker, &settings.box_colors);
    CompactToggleRow("显示名称", &settings.show_name, &settings.name_colors);
    CompactToggleRow(
        "显示距离", &settings.show_distance, &settings.distance_colors);
    CompactToggleRow(
        "显示骨骼", &settings.show_skeleton, &settings.skeleton_colors);
    CompactToggleRow("显示血条", &settings.show_health_bar);
    DrawModelEffectDropdown(&settings);
    StepperRow("最大显示距离", &settings.max_distance, 5.0f, 100.0f, 5.0f, "%.0f 格", false);
    SelectVisualCategoryOnCardClick(0);
    EndCard();
}

void DrawPlayerVisualControls() {
    features::visual::PlayerVisualSettings& settings =
        features::visual::GetPlayerVisualSettings();
    DrawVisualCategoryLabel("玩家", 1);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    BeginCompactCard("PlayerControls", nullptr, ImVec2(0.0f, 0.0f));
    CompactToggleRow("启用玩家透视", &settings.player_esp);
    CompactToggleRow(
        "显示方框", &settings.colored_marker, &settings.box_colors);
    CompactToggleRow("显示名称", &settings.show_name, &settings.name_colors);
    CompactToggleRow(
        "显示距离", &settings.show_distance, &settings.distance_colors);
    CompactToggleRow(
        "显示骨骼", &settings.show_skeleton, &settings.skeleton_colors);
    CompactToggleRow("显示血条", &settings.show_health_bar);
    CompactToggleRow("方向箭头", &settings.ray_esp, &settings.ray_color);
    CompactToggleRow("标记服务器管理员", &settings.show_admin_marker);
    DrawPlayerModelEffectDropdown(&settings);
    StepperRow("最大显示距离", &settings.max_distance,
               5.0f, 150.0f, 5.0f, "%.0f 格", false);
    SelectVisualCategoryOnCardClick(1);
    EndCard();
}

void DrawAnimalVisualControls() {
    features::visual::AnimalVisualSettings& settings =
        features::visual::GetAnimalVisualSettings();
    DrawVisualCategoryLabel("动物", 2);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    BeginCompactCard("AnimalControls", nullptr, ImVec2(0.0f, 0.0f));
    CompactToggleRow("启用动物透视", &settings.animal_esp);
    CompactToggleRow(
        "显示方框", &settings.colored_marker, &settings.box_colors);
    CompactToggleRow("显示名称", &settings.show_name, &settings.name_colors);
    CompactToggleRow(
        "显示距离", &settings.show_distance, &settings.distance_colors);
    CompactToggleRow(
        "显示骨骼", &settings.show_skeleton, &settings.skeleton_colors);
    CompactToggleRow("显示血条", &settings.show_health_bar);
    CompactToggleRow("方向箭头", &settings.ray_esp, &settings.ray_color);
    DrawAnimalModelEffectDropdown(&settings);
    StepperRow("最大显示距离", &settings.max_distance,
               5.0f, 150.0f, 5.0f, "%.0f 格", false);
    SelectVisualCategoryOnCardClick(2);
    EndCard();
}

void DrawVehicleVisualControls() {
    features::visual::VehicleVisualSettings& settings =
        features::visual::GetVehicleVisualSettings();
    DrawVisualCategoryLabel("载具", 3);
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    BeginCompactCard("VehicleControls", nullptr, ImVec2(0.0f, 0.0f));
    CompactToggleRow("启用载具透视", &settings.vehicle_esp);
    CompactToggleRow(
        "显示方框", &settings.colored_marker, &settings.box_colors);
    CompactToggleRow("显示名称", &settings.show_name, &settings.name_colors);
    CompactToggleRow(
        "显示距离", &settings.show_distance, &settings.distance_colors);
    CompactToggleRow("显示车辆详情", &settings.show_details);
    CompactToggleRow("显示发动机状态条", &settings.show_engine_bar);
    CompactToggleRow("方向箭头", &settings.ray_esp, &settings.ray_color);
    DrawVehicleModelEffectDropdown(&settings);
    StepperRow("最大显示距离", &settings.max_distance,
               10.0f, 250.0f, 10.0f, "%.0f 格", false);
    SelectVisualCategoryOnCardClick(3);
    EndCard();
}

void DrawVisualPage() {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f));
    if (!ImGui::BeginTable(
            "VisualEspCards", 2,
            ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    DrawVisualControls();
    ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
    DrawAnimalVisualControls();
    ImGui::TableSetColumnIndex(1);
    DrawPlayerVisualControls();
    ImGui::Dummy(ImVec2(0.0f, U(8.0f)));
    DrawVehicleVisualControls();
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void DrawUnavailablePage(const char* title) {
    SectionLabel(title);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    BeginCard("UnavailablePage", nullptr, ImVec2(0.0f, 0.0f));
    ImGui::TextUnformatted(settings::Translate("该页面尚未实现"));
    EndCard();
}

void DrawContent(const bridge::FrameSnapshot& frame) {
    if (g_previous_page != g_page) {
        g_previous_page = g_page;
        g_page_animation = 0.0f;
        g_page_velocity = 0.0f;
    }
    animation::SpringValue(g_page_animation, g_page_velocity, 1.0f, 260.0f, 17.0f);
    const float page_alpha = animation::Clamp01(g_page_animation);
    const float page_offset = (1.0f - g_page_animation) * U(16.0f);

    ImGui::SetCursorPos(
        ImVec2(U(kSidebarWidth + 21.0f), U(kTopbarHeight + 20.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.018f, 0.020f, 0.027f, 0.10f));
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(12.0f), U(8.0f)));
    ImGui::BeginChild(
        "Content",
        ImVec2(
            ImGui::GetWindowWidth() - U(kSidebarWidth + 41.0f),
            ImGui::GetWindowHeight() - U(kTopbarHeight + 37.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * page_alpha);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + page_offset);
    switch (g_page) {
        case Page::Rage: DrawAimbotPage(AimbotPage::Rage); break;
        case Page::Legit: DrawAimbotPage(AimbotPage::Legit); break;
        case Page::Visual: DrawVisualPage(); break;
        case Page::WorldVisual: DrawWorldVisual(); break;
        case Page::Character: DrawCharacterStatus(); break;
        case Page::Items: DrawItemGenerator(frame); break;
        case Page::Experience: DrawExperienceEditor(); break;
        case Page::VehicleControl: DrawVehicleControlPage(); break;
        case Page::WorldInteraction: DrawWorldInteractionPage(); break;
        case Page::CharacterCreation: DrawCharacterCreationPage(); break;
        case Page::Lua: DrawLuaPage(); break;
        case Page::LuaRegistered:
            bridge::DrawLuaCategoryPage(bridge::SelectedLuaUiCategory());
            break;
        case Page::Settings: DrawSettingsPage(); break;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

bool HasDetachedPreview() {
    return g_page == Page::Rage || g_page == Page::Legit ||
           g_page == Page::Visual;
}

void CalculateDetachedPreviewLayout(TrainerMenuLayout& layout,
                                    const ImVec2& host_position,
                                    const ImVec2& host_size) {
    layout.detached_preview_visible = HasDetachedPreview();
    if (!layout.detached_preview_visible) return;

    const ImGuiIO& io = ImGui::GetIO();
    const float preview_width = std::min(
        U(kDetachedPreviewWidth), io.DisplaySize.x * 0.42f);
    const float requested_height = U(
        g_page == Page::Visual ? 620.0f : 600.0f);
    const float preview_height = std::min(
        requested_height, std::max(U(360.0f), io.DisplaySize.y - U(24.0f)));
    ImVec2 position(
        host_position.x + host_size.x + U(kDetachedPreviewGap),
        host_position.y + std::min(U(kTopbarHeight), host_size.y * 0.10f));
    if (position.x + preview_width > io.DisplaySize.x - U(12.0f)) {
        position.x = host_position.x - preview_width - U(kDetachedPreviewGap);
    }
    position.x = std::clamp(
        position.x, U(12.0f),
        std::max(U(12.0f), io.DisplaySize.x - preview_width - U(12.0f)));
    position.y = std::clamp(
        position.y, U(12.0f),
        std::max(U(12.0f), io.DisplaySize.y - preview_height - U(12.0f)));
    layout.detached_preview_position = position;
    layout.detached_preview_size = ImVec2(preview_width, preview_height);
}

TrainerMenuLayout CalculateTrainerMenuLayout() {
    const ImGuiIO& io = ImGui::GetIO();
    const float desired_width = U(kWindowWidth);
    const float desired_height = U(kWindowHeight);
    const float detached_preview_reservation = HasDetachedPreview()
        ? std::min(U(kDetachedPreviewWidth), io.DisplaySize.x * 0.42f) +
            U(kDetachedPreviewGap + 24.0f)
        : U(24.0f);
    const float available_window_width = std::min(
        io.DisplaySize.x - U(24.0f),
        std::max(U(420.0f), io.DisplaySize.x - detached_preview_reservation));

    TrainerMenuLayout layout{};
    layout.host_size = ImVec2(
        std::min(desired_width, available_window_width),
        io.DisplaySize.y < desired_height + U(40.0f)
            ? io.DisplaySize.y - U(24.0f) : desired_height);
    const ImVec2 base_position(
        std::max(U(12.0f), (io.DisplaySize.x - layout.host_size.x) * 0.5f),
        std::max(U(12.0f), (io.DisplaySize.y - layout.host_size.y) * 0.5f));
    layout.host_position = g_window_position_initialized
        ? g_window_position : base_position;

    float maximum_window_x = std::max(0.0f, io.DisplaySize.x - layout.host_size.x);
    const float detached_width = U(kDetachedPreviewWidth + kDetachedPreviewGap);
    if (HasDetachedPreview() &&
        io.DisplaySize.x >= layout.host_size.x + detached_width + U(24.0f)) {
        maximum_window_x = std::max(
            U(12.0f),
            io.DisplaySize.x - layout.host_size.x - detached_width - U(12.0f));
    }
    layout.host_position.x = std::clamp(
        layout.host_position.x, 0.0f, maximum_window_x);
    layout.host_position.y = std::clamp(
        layout.host_position.y, 0.0f,
        std::max(0.0f, io.DisplaySize.y - layout.host_size.y));
    CalculateDetachedPreviewLayout(
        layout, layout.host_position, layout.host_size);
    return layout;
}

const char* DetachedPreviewTitle() {
    if (g_page == Page::Rage) return settings::Translate("Rage 击中点");
    if (g_page == Page::Legit) return settings::Translate("Legit 击中点");
    switch (g_visual_tab) {
        case 0: return settings::Translate("3D 僵尸预览");
        case 1: return settings::Translate("3D 玩家预览");
        case 2: return settings::Translate("3D 动物预览");
        default: return settings::Translate("3D 载具预览");
    }
}

void DrawVisualDetachedPreview() {
    const float controls_height = U(g_visual_tab == 3 ? 54.0f : 112.0f);
    const float preview_height = std::max(
        U(260.0f), ImGui::GetContentRegionAvail().y - controls_height);
    const ImVec2 preview_size(
        ImGui::GetContentRegionAvail().x, preview_height);
    if (g_visual_tab == 0) {
        features::visual::VisualSettings& settings =
            features::visual::GetVisualSettings();
        DrawZombiePreview(preview_size, settings);
        ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
        DrawPreviewStateSelector(&settings.preview_state);
        DrawAnimationDropdown(&settings.preview_animation);
    } else if (g_visual_tab == 1) {
        features::visual::PlayerVisualSettings& settings =
            features::visual::GetPlayerVisualSettings();
        DrawPlayerPreview(preview_size, settings);
        ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
        DrawPreviewStateSelector(&settings.preview_state);
        DrawAnimationDropdown(&settings.preview_animation, true);
    } else if (g_visual_tab == 2) {
        features::visual::AnimalVisualSettings& settings =
            features::visual::GetAnimalVisualSettings();
        DrawAnimalPreview(preview_size, settings);
        ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
        DrawPreviewStateSelector(&settings.preview_state);
        DrawAnimationDropdown(&settings.preview_animation, false, true);
    } else {
        features::visual::VehicleVisualSettings& settings =
            features::visual::GetVehicleVisualSettings();
        DrawVehiclePreview(preview_size, settings);
        ImGui::Dummy(ImVec2(0.0f, U(6.0f)));
        DrawPreviewStateSelector(&settings.preview_state);
    }
}

void DrawDetachedPreviewWindow(const ImVec2& host_position,
                               const ImVec2& host_size) {
    TrainerMenuLayout layout{};
    CalculateDetachedPreviewLayout(layout, host_position, host_size);
    if (!layout.detached_preview_visible) return;

    ImGui::SetNextWindowPos(layout.detached_preview_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.detached_preview_size, ImGuiCond_Always);
    ImGui::PushStyleVar(
        ImGuiStyleVar_WindowPadding, ImVec2(U(11.0f), U(10.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, U(10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, U(1.0f));
    ImGui::PushStyleColor(
        ImGuiCol_WindowBg, ImVec4(0.018f, 0.021f, 0.030f, 0.78f));
    ImGui::PushStyleColor(
        ImGuiCol_Border, ImVec4(0.42f, 0.47f, 0.58f, 0.34f));
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("##DetachedTrainerPreview", nullptr, flags)) {
        SectionLabel(DetachedPreviewTitle());
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        if (g_page == Page::Visual) {
            DrawVisualDetachedPreview();
        } else {
            DrawAimbotTargetPreview(
                g_page == Page::Rage ? AimbotPage::Rage : AimbotPage::Legit,
                ImGui::GetContentRegionAvail());
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

}  // namespace

TrainerMenuLayout GetTrainerMenuLayout() {
    return CalculateTrainerMenuLayout();
}

void PrepareTrainerMenuStartupReveal() {
    const TrainerMenuLayout layout = CalculateTrainerMenuLayout();
    g_window_position = layout.host_position;
    g_window_position_initialized = true;
    g_menu_animation = 1.0f;
    g_menu_velocity = 0.0f;
}

void DrawTrainerMenu(std::atomic_bool& visible, const bridge::FrameSnapshot& frame) {
    const std::uint64_t preferences_revision =
        settings::UiPreferencesRevision();
    if (preferences_revision != g_applied_preferences_revision) {
        ApplyTheme();
        g_applied_preferences_revision = preferences_revision;
    }
    const bool requested_visible = visible.load();
    animation::SpringValue(g_menu_animation, g_menu_velocity, requested_visible ? 1.0f : 0.0f, 105.0f, 13.0f);
    const float menu_alpha = animation::Clamp01(g_menu_animation);
    if (!requested_visible && menu_alpha < 0.002f) {
        DrawColorPickerOverlay(false, ImVec2{}, ImVec2{});
        return;
    }

    const TrainerMenuLayout layout = CalculateTrainerMenuLayout();
    g_window_position = layout.host_position;
    g_window_position_initialized = true;
    ImGui::SetNextWindowSize(layout.host_size, ImGuiCond_Always);
    const float animated_y = g_window_position.y +
        (1.0f - g_menu_animation) * U(22.0f);
    ImGui::SetNextWindowPos(ImVec2(g_window_position.x, animated_y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * menu_alpha);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin("PZ Solo Assist", nullptr, flags)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 host_minimum = ImGui::GetWindowPos();
    const ImVec2 host_size = ImGui::GetWindowSize();
    const ImVec2 host_maximum(
        host_minimum.x + host_size.x,
        host_minimum.y + host_size.y);

    DrawGlassPanel(ImGui::GetWindowDrawList(), host_minimum, host_maximum,
                   ImGui::GetStyle().WindowRounding);
    DrawShellBackgrounds(host_minimum, host_size);
    DrawSidebar();
    DrawTopbar();
    DrawContent(frame);
    DrawShellSeparators(host_minimum, host_size);

    ImGui::End();
    DrawCorpsePayloadPanel(
        host_minimum, host_maximum,
        requested_visible && g_page == Page::Items &&
            IsCorpsePayloadSchemeActive());
    DrawSteamProfilePopup(requested_visible);
    DrawDetachedPreviewWindow(host_minimum, host_size);
    ImGui::PopStyleVar();
    DrawColorPickerOverlay(requested_visible, host_minimum, host_maximum);
}

}  // namespace pztrainer::ui
