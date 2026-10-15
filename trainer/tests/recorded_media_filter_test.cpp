#include <cassert>
#include <iostream>
#include "ui/recorded_media_filter.hpp"
int main() {
    using namespace pztrainer;
    bridge::ItemCatalogEntry item;item.full_type="RecordedMedia:test";item.display_name="Woodcraft Episode 1";
    item.recorded_media_base_type="Base.VHS_Retail";item.search_terms="木工 录像带 VHS";
    assert(ui::RecordedMediaMatches(item,"全部","Woodcraft"));
    assert(ui::RecordedMediaMatches(item,"零售 VHS","木工"));
    assert(!ui::RecordedMediaMatches(item,"CD",""));
    item.recorded_media_base_type="Base.VHS_Home";
    assert(ui::RecordedMediaMatches(item,"家庭 VHS",""));
    item.recorded_media_base_type="Base.Disc_Retail";
    assert(ui::RecordedMediaMatches(item,"CD",""));
    item.full_type="Base.Disc_Retail";
    assert(!ui::RecordedMediaMatches(item,"全部","")); // No random template in this page.
    item.full_type="Base.Axe";assert(!ui::RecordedMediaMatches(item,"全部",""));
    item.full_type="RecordedMedia:test";item.recorded_media_base_type.clear();
    assert(!ui::RecordedMediaMatches(item,"全部",""));
    std::cout<<"Recorded media page: named-only, VHS/CD category, search and random-template exclusion passed.\n";
}
