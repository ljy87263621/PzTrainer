#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <type_traits>
#include <utility>

namespace pztrainer::features::visual {
enum class VisualCategory : unsigned { Zombies, Players, Animals, Vehicles };
enum class VisualFeature : unsigned { Box, Name, Distance, Skeleton, Health, Direction, Admin, Details, Engine, Fill, Rim, Count };
struct VisibilityOptions {
    std::array<std::array<std::uint8_t,11>,4> only_visible{};
    bool Allows(VisualCategory category,VisualFeature feature,bool game_visible) const {
        return game_visible || only_visible[static_cast<unsigned>(category)][static_cast<unsigned>(feature)]==0;
    }
};
inline VisibilityOptions& GetVisibilityOptions() { static VisibilityOptions value; return value; }
inline bool WriteVisibilityOptions(std::ostream& stream,const VisibilityOptions& value) {
    const std::uint32_t size=44;
    stream.write(reinterpret_cast<const char*>(&size),sizeof(size));
    for(const auto& row:value.only_visible)stream.write(reinterpret_cast<const char*>(row.data()),row.size());
    return static_cast<bool>(stream);
}
inline bool ReadVisibilityOptions(std::istream& stream,VisibilityOptions& value,bool* missing_options=nullptr) {
    std::uint32_t size=0; stream.read(reinterpret_cast<char*>(&size),sizeof(size));
    if(!stream || size>4096)return false;
    VisibilityOptions candidate{};
    for(std::uint32_t i=0;i<size;i++) {
        const int flag=stream.get(); if(flag<0 || flag>1)return false;
        if(i<44)candidate.only_visible[i/11][i%11]=static_cast<std::uint8_t>(flag);
    }
    value=candidate;
    if(missing_options)*missing_options=size<44;
    return true;
}
#define PZ_OPTIONAL_VISUAL(member) \
template<class T,class=void>struct Has_##member:std::false_type{}; \
template<class T>struct Has_##member<T,std::void_t<decltype(std::declval<T>().member)>>:std::true_type{};
PZ_OPTIONAL_VISUAL(show_skeleton)
PZ_OPTIONAL_VISUAL(show_health_bar)
PZ_OPTIONAL_VISUAL(ray_esp)
PZ_OPTIONAL_VISUAL(show_admin_marker)
PZ_OPTIONAL_VISUAL(show_details)
PZ_OPTIONAL_VISUAL(show_engine_bar)
#undef PZ_OPTIONAL_VISUAL
template<class T>T FilterVisibility(T value,const VisibilityOptions& options,VisualCategory category,bool visible) {
    const auto allow=[&](VisualFeature feature){return options.Allows(category,feature,visible);};
    value.colored_marker &= allow(VisualFeature::Box); value.show_name &= allow(VisualFeature::Name);
    value.show_distance &= allow(VisualFeature::Distance);
    if constexpr(Has_show_skeleton<T>::value)value.show_skeleton &= allow(VisualFeature::Skeleton);
    if constexpr(Has_show_health_bar<T>::value)value.show_health_bar &= allow(VisualFeature::Health);
    if constexpr(Has_ray_esp<T>::value)value.ray_esp &= allow(VisualFeature::Direction);
    if constexpr(Has_show_admin_marker<T>::value)value.show_admin_marker &= allow(VisualFeature::Admin);
    if constexpr(Has_show_details<T>::value)value.show_details &= allow(VisualFeature::Details);
    if constexpr(Has_show_engine_bar<T>::value)value.show_engine_bar &= allow(VisualFeature::Engine);
    return value;
}
}
