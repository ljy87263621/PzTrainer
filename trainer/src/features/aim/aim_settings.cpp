#include "features/aim/aim_settings.hpp"

namespace pztrainer::features::aim {

AimSettings& GetAimSettings() {
    static AimSettings settings = [] {
        AimSettings defaults;
        const std::size_t global =
            static_cast<std::size_t>(WeaponGroup::Global);
        defaults.legit_presets[global].enabled = true;
        defaults.rage_presets[global].enabled = true;
        return defaults;
    }();
    return settings;
}

const char* WeaponGroupName(WeaponGroup group) {
    switch (group) {
        case WeaponGroup::Global: return "全局";
        case WeaponGroup::Pistol: return "手枪";
        case WeaponGroup::Shotgun: return "霰弹枪";
        case WeaponGroup::Smg: return "冲锋枪";
        case WeaponGroup::Rifle: return "步枪";
        case WeaponGroup::Sniper: return "狙击枪";
        case WeaponGroup::Count: break;
    }
    return "未知";
}

}  // namespace pztrainer::features::aim
