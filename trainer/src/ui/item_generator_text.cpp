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

}  // namespace

const char* ItemCategoryLabel(const std::string& category) {
    if (settings::GetLanguage() == settings::Language::Chinese) {
        for (const CategoryLabel& label : kChineseCategories) {
            if (category == label.source) return label.chinese;
        }
    }
    return settings::Translate(category.c_str());
}

}  // namespace pztrainer::ui
