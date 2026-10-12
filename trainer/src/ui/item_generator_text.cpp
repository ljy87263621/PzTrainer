#include "ui/item_generator_text.hpp"

#include <array>
#include <string_view>

#include "settings/localization.hpp"

namespace pztrainer::ui {
namespace {

struct CategoryLabel {
    std::string_view source;
    const char* chinese;
};

constexpr std::array<CategoryLabel, 18> kChineseCategories{{
    {"Weapon", "武器"},
    {"Food", "食物"},
    {"Clothing", "服装"},
    {"Literature", "书籍"},
    {"Container", "容器"},
    {"Drainable", "可消耗物"},
    {"Key", "钥匙"},
    {"AlarmClock", "闹钟"},
    {"Normal", "普通"},
    {"Moveable", "可移动物"},
    {"WeaponPart", "武器配件"},
    {"Map", "地图"},
    {"Medical", "医疗"},
    {"Tool", "工具"},
    {"VehiclePart", "载具配件"},
    {"Radio", "收音机"},
    {"Fishing", "钓鱼"},
    {"Junk", "杂物"},
}};

struct MethodDescription {
    std::string_view method;
    std::string_view description;
};

constexpr std::array<MethodDescription, 5> kMethodDescriptions{{
    {"游戏原生", "仅单机，支持全部物品"},
    {"联机取物", "自动选择可用联机路径，仅支持背包"},
    {"尸体载荷", "在脚下投递服务器僵尸尸体"},
    {"灯具载荷", "需要附近空灯位的可改装灯具"},
    {"媒体载荷", "需要装备在主手或副手的便携收音机"},
}};

}  // namespace

const char* ItemCategoryLabel(const std::string& category) {
    if (settings::GetLanguage() == settings::Language::Chinese) {
        for (const CategoryLabel& label : kChineseCategories) {
            if (category == label.source) return label.chinese;
        }
    }
    return settings::Translate(category.c_str());
}

const char* ItemSpawnMethodDescription(const std::string& method) {
    for (const MethodDescription& description : kMethodDescriptions) {
        if (method == description.method) {
            return settings::Translate(std::string(description.description).c_str());
        }
    }
    return settings::Translate(method.c_str());
}

float ItemSpawnMethodPopupWidth(float available_width) {
    if (available_width <= 0.0f) return 0.0f;
    return std::min(480.0f, std::max(280.0f, available_width - 8.0f));
}

}  // namespace pztrainer::ui
