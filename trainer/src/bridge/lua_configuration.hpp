#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class LuaConfiguredControlType : std::uint8_t {
    Toggle,
    Slider,
    Input,
    Color,
    ItemMultiSelect,
};

struct LuaControlConfiguration {
    std::string category;
    std::string label;
    LuaConfiguredControlType type = LuaConfiguredControlType::Toggle;
    bool standalone = false;
    bool toggle = false;
    float value = 0.0f;
    std::string text;
    float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<std::string> selected_items;
};

struct LuaScriptConfiguration {
    std::filesystem::path path;
    bool auto_reload = false;
    std::vector<LuaControlConfiguration> controls;
};

}  // namespace pztrainer::bridge
