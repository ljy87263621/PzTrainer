#include <cassert>
#include <cstdlib>
#include <iostream>
#include "features/aim/rage_targeting_policy.hpp"
using namespace pztrainer::features::aim;
// The script compiles the actual production collection routines into this test.
namespace pztrainer::features::aim {
AimSettings test_settings;
AimSettings& GetAimSettings() { return test_settings; }
bool RageAimNeedsZombieData();
bool RageAimNeedsPlayerData();
float RageAimCollectionRange();
}
int main() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    auto& s=GetAimSettings(); s.rage_enabled=true;
    auto& p=s.rage_presets[0]; p.enabled=true;
    p.automatic_aim=p.automatic_fire=p.magic_bullet=false; p.silent_aim=true;
    assert(RageAimNeedsZombieData() && RageAimCollectionRange()==p.range);
    assert(!RageAimNeedsPlayerData()); p.target_players=true;
    assert(RageAimNeedsPlayerData());
    assert(ShouldAdjustRageAim(p,true) && !ShouldAdjustRageAim(p,false));
    s.rage_enabled=false; assert(!RageAimNeedsZombieData() && !RageAimNeedsPlayerData());
    s.rage_enabled=true; p.target_points=0;
    assert(!RageAimNeedsZombieData() && !RageAimNeedsPlayerData() && RageAimCollectionRange()==0);
    p.target_points=1; p.enabled=false;
    assert(!RageAimNeedsZombieData() && RageAimCollectionRange()==0); p.enabled=true;
    for(int modes=0;modes<16;++modes) {
        p.automatic_aim=(modes&1)!=0; p.automatic_fire=(modes&2)!=0;
        p.silent_aim=(modes&4)!=0; p.magic_bullet=(modes&8)!=0;
        assert(HasRageTargetingMode(p)==(modes!=0));
        assert(RageAimNeedsZombieData()==(modes!=0));
        assert(RageAimNeedsPlayerData()==(modes!=0));
        assert(RageAimCollectionRange()==(modes!=0?p.range:0));
        assert(ShouldAdjustRageAim(p,true)==((modes&13)!=0));
        assert(!ShouldAdjustRageAim(p,false));
    }
    std::cout<<"Rage production collection and manual/automatic targeting policy matrix passed.\n";
}
