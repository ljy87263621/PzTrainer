#include "ui/creation/character_creation_page.hpp"

#include <array>
#include <string>
#include <unordered_map>
#include <imgui.h>
#include "bridge/extension_bridge.hpp"
#include "settings/ui_preferences.hpp"
#include "ui/animated_dropdown.hpp"
#include "ui/color_picker.hpp"
#include "ui/components.hpp"
#include "ui/controls/action_controls.hpp"
#include "ui/controls/card_layout.hpp"
#include "ui/extensions/extension_controls.hpp"

namespace pztrainer::ui {
namespace {
void Choices(const char* label, const char* kind, const std::string& suffix = {}) {
    extensions::BeginAvailable((std::string("creator_") + kind).c_str());
    static std::unordered_map<std::string, std::array<char, 128>> filters;
    auto& filter = filters[kind];
    ImGui::PushID(kind);
    ImGui::TextUnformatted(label);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * settings::UiScale(), 8 * settings::UiScale()));
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (BeginAnimatedCombo("choices", "选择后应用", 270 * settings::UiScale())) {
        controls::TextField("筛选", filter.data(), filter.size(), "输入名称");
        bool any = false;
        for (const auto& entry : bridge::GetCreationOptions()) {
            if (entry.kind != kind) continue;
            if (filter[0] != '\0' && entry.label.find(filter.data()) == std::string::npos) continue;
            any = true;
            ImGui::PushID(entry.key.c_str());
            if (controls::ChoiceRow(entry.label.c_str())) {
                bridge::QueueExtensionCommand((std::string("creator_") + kind).c_str(), entry.key + suffix);
                CloseAnimatedDropdown();
            }
            ImGui::PopID();
        }
        if (!any) controls::Hint("暂无选项；请进入对应创建步骤并刷新。");
        EndAnimatedDropdown();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
    ImGui::PopID();
    extensions::EndAvailable();
}
void DrawCreationSession();

void DrawBackground() {
    cards::BeginSection("NewWorld", "出生与存档");
    Choices("出生地点", "spawn");
    extensions::BeginAvailable("creator_world");
    static char world[128]{}, seed[128]{};
    controls::TextField("新存档名称", world, sizeof(world));
    controls::TextField("世界种子", seed, sizeof(seed));
    extensions::Action("应用新世界参数", "creator_world", std::string(world) + "\t" + seed);
    extensions::EndAvailable();
    components::EndCard();
    DrawCreationSession();
    ImGui::TableSetColumnIndex(1);
    cards::BeginSection("Profession", "职业");
    Choices("选择职业", "profession");
    components::EndCard();
}
void DrawTraits() {
    cards::BeginSection("Traits", "特质与点数");
    Choices("添加特质", "trait_add");
    Choices("移除特质", "trait_remove");
    static int points = 100;
    controls::IntegerRow("剩余点数", &points, 0, 1000);
    extensions::Action("设置点数", "creator_points", std::to_string(points));
    components::EndCard();
    DrawCreationSession();
    ImGui::TableSetColumnIndex(1);
    cards::BeginSection("StartingSkills", "出生技能");
    static int level = 3;
    controls::IntegerRow("技能等级", &level, 0, 10);
    Choices("选择技能并应用等级", "skill", "\t" + std::to_string(level));
    extensions::Action("清除出生技能覆盖", "creator_skills_clear");
    controls::Hint("角色初始化后应用，进入存档后清除覆盖。原版预设不保存这组技能覆盖值。");
    components::EndCard();
}
void DrawAppearance() {
    cards::BeginSection("Identity", "姓名与性别");
    static char first[128]{}, last[128]{};
    controls::TextField("名", first, sizeof(first));
    controls::TextField("姓", last, sizeof(last));
    extensions::Action("应用姓名", "creator_name", std::string(first) + "\t" + last);
    extensions::Action("设为女性", "creator_gender", "1");
    extensions::Action("设为男性", "creator_gender", "2");
    components::EndCard();
    cards::BeginSection("Voice", "声音");
    Choices("声音类型", "voice");
    static int pitch = 0;
    controls::IntegerRow("音调", &pitch, -100, 100);
    extensions::Action("应用音调", "creator_voice_pitch", std::to_string(pitch));
    components::EndCard();
    DrawCreationSession();
    ImGui::TableSetColumnIndex(1);
    cards::BeginSection("Appearance", "面容");
    Choices("发型", "hair");
    Choices("胡须", "beard");
    static ImVec4 color(0.2f, 0.15f, 0.1f, 1);
    ImGui::TextUnformatted("头发与胡须颜色");
    DrawColorSwatch("HairColor", &color, ImVec2(64 * settings::UiScale(), 28 * settings::UiScale()));
    extensions::Action("应用颜色", "creator_hair_color",
        std::to_string(color.x) + "," + std::to_string(color.y) + "," + std::to_string(color.z));
    static int skin = 1;
    controls::IntegerRow("肤色序号", &skin, 1, 5);
    extensions::Action("应用肤色", "creator_skin", std::to_string(skin));
    components::EndCard();
    cards::BeginSection("Clothing", "服装");
    Choices("服装部位 / 服装", "clothing");
    Choices("服装纹理", "clothing_texture");
    components::EndCard();
}
void DrawPresets() {
    cards::BeginSection("ProfessionPresets", "职业与特质预设");
    extensions::Action("保存当前职业 / 特质", "creator_save_profession");
    Choices("载入预设", "load_profession");
    components::EndCard();
    DrawCreationSession();
    ImGui::TableSetColumnIndex(1);
    cards::BeginSection("AppearancePresets", "外观预设");
    extensions::Action("保存当前外观", "creator_save_appearance");
    Choices("载入预设", "load_appearance");
    controls::Hint("保存会打开原版命名窗口；暂时关闭 DLL 菜单，在游戏内输入预设名称。");
    components::EndCard();
}
void DrawCreationSession() {
    cards::BeginSection("CreationSession", "创建界面");
    controls::Hint("先进入原版角色创建流程，再刷新列表。职业与特质在职业页编辑，姓名与外观在外观页编辑。");
    extensions::Action("刷新当前创建界面", "creator_refresh");
    for (const auto& entry : bridge::GetCreationOptions()) {
        if (entry.kind == "stage") ImGui::Text("当前阶段：%s", entry.label.c_str());
        if (entry.kind == "points") ImGui::Text("剩余点数：%s", entry.label.c_str());
    }
    components::EndCard();
}
}  // namespace

void DrawCharacterCreationPage() {
    static int tab = 0;
    const char* labels[]{"世界与职业", "特质与技能", "角色外观", "预设管理"};
    const float width = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4;
    for (int i = 0; i < 4; ++i) {
        if (i != 0) ImGui::SameLine();
        if (components::TopTab(labels[i], tab == i, width)) tab = i;
    }
    ImGui::Dummy(ImVec2(0, 6 * settings::UiScale()));
    if (cards::BeginColumns("CharacterCreationCards")) {
        ImGui::BeginDisabled(!bridge::ExtensionBridgeReady());
        if (tab == 0) DrawBackground();
        if (tab == 1) DrawTraits();
        if (tab == 2) DrawAppearance();
        if (tab == 3) DrawPresets();
        ImGui::EndDisabled();
        extensions::Status();
        cards::EndColumns();
    }
}
}  // namespace pztrainer::ui
