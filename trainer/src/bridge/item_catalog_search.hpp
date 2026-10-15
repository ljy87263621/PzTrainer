#pragma once
#include <algorithm>
#include <cctype>
#include <string_view>
#include "bridge/item_bridge.hpp"

namespace pztrainer::bridge {
inline bool IsRecordedMediaKey(const std::string& type) {
    return type.compare(0,14,"RecordedMedia:")==0;
}
inline bool NeedsRecordedMediaCreation(const std::string& type) {
    return IsRecordedMediaKey(type) || type=="Base.VHS_Retail" ||
        type=="Base.VHS_Home" || type=="Base.Disc_Retail";
}
inline bool NeedsPreparedItemCreation(const std::string& type);
inline constexpr std::string_view kDynamicMoveablePrefix="Moveables.";
inline bool IsDynamicMoveableItemType(const std::string& type) {
    return type.compare(0,kDynamicMoveablePrefix.size(),kDynamicMoveablePrefix.data())==0 &&
        type.size()>kDynamicMoveablePrefix.size() && type!="Moveables.Moveable";
}
inline bool NeedsPreparedItemCreation(const std::string& type) {
    return IsDynamicMoveableItemType(type) || NeedsRecordedMediaCreation(type);
}
inline std::string ItemSearchLower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c) {
        return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return value;
}
inline std::string MoveableSearchTerms(const std::string& raw_name,const std::string& display_name) {
    std::string terms=raw_name;
    const std::string name=ItemSearchLower(raw_name+" "+display_name);
    if (name.find("cubicle")!=std::string::npos || name.find("divider")!=std::string::npos ||
        name.find("partition")!=std::string::npos || name.find("隔间")!=std::string::npos ||
        name.find("隔断")!=std::string::npos || name.find("屏风")!=std::string::npos) {
        terms+=" 隔板 隔断 屏风 办公隔间 cubicle divider partition";
    }
    return terms;
}
inline bool ItemCatalogMatchesSearch(const ItemCatalogEntry& item,const std::string& query) {
    if(query.empty())return true;
    const std::string search=ItemSearchLower(query);
    return ItemSearchLower(item.display_name).find(search)!=std::string::npos ||
        ItemSearchLower(item.full_type).find(search)!=std::string::npos ||
        ItemSearchLower(item.search_terms).find(search)!=std::string::npos;
}
}
