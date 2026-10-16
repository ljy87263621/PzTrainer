#include <cassert>
#include <cstdlib>
#include "bridge/item_catalog_search.hpp"
using namespace pztrainer::bridge;
int main() {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    assert(IsDynamicMoveableItemType("Moveables.location_business_office_generic_01_18"));
    assert(!IsDynamicMoveableItemType("Moveables.Moveable"));
    assert(!IsDynamicMoveableItemType("Moveables."));
    assert(!IsDynamicMoveableItemType("Base.Axe"));
    ItemCatalogEntry divider;
    divider.full_type="Moveables.furniture_office_01_0";
    divider.display_name="蓝色隔间";
    divider.search_terms=MoveableSearchTerms("Blue Cubicle",divider.display_name);
    assert(ItemCatalogMatchesSearch(divider,"隔板"));
    assert(ItemCatalogMatchesSearch(divider,"隔断"));
    assert(ItemCatalogMatchesSearch(divider,"CUBICLE"));
    assert(ItemCatalogMatchesSearch(divider,"FURNITURE_OFFICE"));
    assert(!ItemCatalogMatchesSearch(divider,"冰箱"));
    ItemCatalogEntry ordinary;
    ordinary.full_type="Base.Axe"; ordinary.display_name="斧头";
    assert(ItemCatalogMatchesSearch(ordinary,"axe"));
    assert(ItemCatalogMatchesSearch(ordinary,"斧头"));
    assert(ItemCatalogMatchesSearch(ordinary,""));
    assert(!ItemCatalogMatchesSearch(ordinary,"隔板"));
    assert(MoveableSearchTerms("Chair","椅子").find("隔板")==std::string::npos);
    for(int index=0;index<=static_cast<int>(ItemSpawnMethod::MediaPayload);++index) {
        const auto method=static_cast<ItemSpawnMethod>(index);
        const bool furniture_supported=method==ItemSpawnMethod::Game || method==ItemSpawnMethod::CorpsePayload ||
            method==ItemSpawnMethod::LightPayload || method==ItemSpawnMethod::MediaPayload;
        const bool media_supported=method==ItemSpawnMethod::Game || method==ItemSpawnMethod::CorpsePayload || method==ItemSpawnMethod::MediaPayload;
        assert(PreparedItemSupportsMethod(divider.full_type,method)==furniture_supported);
        assert(PreparedItemSupportsMethod("RecordedMedia:programme",method)==media_supported);
        assert(PreparedItemSupportsMethod("Base.VHS_Retail",method)==media_supported);
        assert(PreparedItemSupportsMethod("Base.VHS_Home",method)==media_supported);
        assert(PreparedItemSupportsMethod("Base.Disc_Retail",method)==media_supported);
        assert(PreparedItemSupportsMethod("Base.Axe",method));
    }
}
