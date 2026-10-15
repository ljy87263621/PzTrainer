#pragma once
#include "bridge/item_catalog_search.hpp"
namespace pztrainer::ui {
inline const char* RecordedMediaCategory(const bridge::ItemCatalogEntry& item) {
    if(item.recorded_media_base_type=="Base.VHS_Retail")return "零售 VHS";
    if(item.recorded_media_base_type=="Base.VHS_Home")return "家庭 VHS";
    if(item.recorded_media_base_type=="Base.Disc_Retail")return "CD";
    return "";
}
inline bool RecordedMediaMatches(const bridge::ItemCatalogEntry& item,const std::string& category,const std::string& query) {
    return bridge::IsRecordedMediaKey(item.full_type) && RecordedMediaCategory(item)[0]!=0 &&
        (category=="全部" || category==RecordedMediaCategory(item)) && bridge::ItemCatalogMatchesSearch(item,query);
}
}
