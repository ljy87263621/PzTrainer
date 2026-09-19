#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class LuaFeatureValueType {
    Boolean,
    Number,
    Integer,
    Enumeration,
    Color,
};

struct LuaFeatureValue {
    LuaFeatureValueType type = LuaFeatureValueType::Boolean;
    bool boolean = false;
    double number = 0.0;
    std::int64_t integer = 0;
    std::string enumeration;
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct LuaFeatureDescriptor {
    std::string path;
    std::string group;
    std::string label;
    LuaFeatureValueType type = LuaFeatureValueType::Boolean;
    LuaFeatureValue value;
    bool writable = false;
    bool available = true;
    bool pending = false;
    double minimum = 0.0;
    double maximum = 0.0;
    double step = 0.0;
    std::vector<std::string> options;
    std::string reason;
};

struct LuaFeatureWriteResult {
    bool ok = false;
    bool pending = false;
    std::string reason;
    LuaFeatureDescriptor feature;
};

const char* LuaFeatureValueTypeName(LuaFeatureValueType type);
void ApplyPendingLuaFeatureWrites();
void RefreshLuaFeatureRegistry();
std::vector<LuaFeatureDescriptor> SnapshotLuaFeatureRegistry();
bool FindLuaFeature(
    const std::string& path, LuaFeatureDescriptor& descriptor);
LuaFeatureWriteResult QueueLuaFeatureWrite(
    const std::string& path, LuaFeatureValue value);

}  // namespace pztrainer::bridge
