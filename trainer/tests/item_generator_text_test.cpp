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
    return 0;
}
