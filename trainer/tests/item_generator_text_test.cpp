#include "ui/item_generator_text.hpp"

#include <cassert>
#include <string>

int main() {
    assert(std::string(pztrainer::ui::ItemCategoryLabel("Weapon")) == "武器");
    assert(std::string(pztrainer::ui::ItemCategoryLabel("Food")) == "食物");
    assert(std::string(pztrainer::ui::ItemCategoryLabel("Clothing")) == "服装");
    assert(std::string(pztrainer::ui::ItemCategoryLabel("其他")) == "其他");
    assert(std::string(pztrainer::ui::ItemCategoryLabel("CustomCategory")) ==
           "CustomCategory");
    assert(std::string(pztrainer::ui::ItemSpawnMethodDescription("游戏原生")) ==
           "仅单机，支持全部物品");
    assert(std::string(pztrainer::ui::ItemSpawnMethodDescription("媒体载荷")) ==
           "需要装备在主手或副手的便携收音机");
    assert(pztrainer::ui::ItemSpawnMethodPopupWidth(800.0f) == 480.0f);
    assert(pztrainer::ui::ItemSpawnMethodPopupWidth(300.0f) == 292.0f);
    return 0;
}
