#pragma once

#include <jni.h>
#include <imgui.h>

#include <cstddef>
#include <array>
#include <string>
#include <vector>

#include "bridge/lua_configuration.hpp"

namespace pztrainer::bridge {

struct LuaUiControl {
    enum class Type { Toggle, Slider, Input, Color, ItemMultiSelect };
    Type type = Type::Toggle;
    std::string label;
    std::string tooltip;
    bool toggle = false;
    float value = 0.0f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    std::string text;
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<std::string> selected_items;
    std::string search;
};

struct LuaUiIconPrimitive {
    enum class Type { Line, Rectangle, Circle };
    Type type = Type::Line;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float radius = 0.0f;
    float rounding = 0.0f;
    float thickness = 1.5f;
    bool filled = false;
};

struct LuaUiCategory {
    std::size_t id = 0;
    std::string name;
    std::vector<LuaUiControl> controls;
    std::vector<LuaUiIconPrimitive> icon_primitives;
    std::string aim_mode;
    std::string owner_script;
    bool standalone = false;
};

// Registers the PZSA Lua namespace and updates the native UI model.
bool EnsureLuaUiApi(JNIEnv* env, std::string& error);
jobject CreateLuaUtf8Reader(JNIEnv* env, jstring source, std::string& error);
void SetLuaUiRegistrationOwner(const std::string& owner_script);
std::vector<LuaControlConfiguration> CaptureLuaUiConfiguration(
    const std::string& owner_script);
void RestoreLuaUiConfiguration(
    const std::string& owner_script,
    const std::vector<LuaControlConfiguration>& controls);
void RemoveLuaUiForScript(const std::string& owner_script);
void DrawLuaCategoryIcon(
    const LuaUiCategory& category, ImDrawList* draw,
    const ImVec2& center, ImU32 color);
void DrawLuaUi();
void DrawLuaCategoryPage(std::size_t category_id);
void DrawLuaAimRange();
void ClearLuaUi();
const std::vector<LuaUiCategory>& GetLuaUiCategories();
std::vector<LuaUiCategory> SnapshotLuaUiCategories();
void SelectLuaUiCategory(std::size_t index);
std::size_t SelectedLuaUiCategory();

}  // namespace pztrainer::bridge
