// Test boundary: no live game or Lua VM calls.
namespace pztrainer::bridge {
inline std::vector<LuaScriptConfiguration> applied_lua;
void ApplyLuaScriptConfiguration(std::vector<LuaScriptConfiguration> configuration){applied_lua=std::move(configuration);}
std::vector<LuaScriptConfiguration> CaptureLuaScriptConfiguration(){return applied_lua;}
ExtensionOptions& GetExtensionOptions(){static ExtensionOptions value{};return value;}
const FarmingModeStatus& GetFarmingModeStatus(){static FarmingModeStatus value{};return value;}
const FreeBuildStatus& GetFreeBuildStatus(){static FreeBuildStatus value{};return value;}
const MultiHitStatus& GetMultiHitStatus(){static MultiHitStatus value{};return value;}
const PlayerAimingDelayStatus& GetPlayerAimingDelayStatus(){static PlayerAimingDelayStatus value{};return value;}
const PlayerAmmoStatus& GetPlayerAmmoStatus(){static PlayerAmmoStatus value{};return value;}
const PlayerHealthStatus& GetPlayerHealthStatus(){static PlayerHealthStatus value{};return value;}
const PlayerMovementStatus& GetPlayerMovementStatus(){static PlayerMovementStatus value{};return value;}
const PlayerResourceStatus& GetPlayerResourceStatus(){static PlayerResourceStatus value{};return value;}
const PlayerTeleportStatus& GetPlayerTeleportStatus(){static PlayerTeleportStatus value{};return value;}
const PlayerWeaponReliabilityStatus& GetPlayerWeaponReliabilityStatus(){static PlayerWeaponReliabilityStatus value{};return value;}
const TimedActionStatus& GetTimedActionStatus(){static TimedActionStatus value{};return value;}
ViewpointSettings& GetViewpointSettings(){static ViewpointSettings value{};return value;}
const WorldMapPlayerStatus& GetWorldMapPlayerStatus(){static WorldMapPlayerStatus value{};return value;}
const WorldMapRevealStatus& GetWorldMapRevealStatus(){static WorldMapRevealStatus value{};return value;}
const WorldVisualStatus& GetWorldVisualStatus(){static WorldVisualStatus value{};return value;}
bool IsFatigueProtectionRequested(){return {};}
bool IsHungerProtectionRequested(){return {};}
bool IsInfectionImmunityRequested(){return {};}
bool IsNegativeMoodlesProtectionRequested(){return {};}
bool IsPanicProtectionRequested(){return {};}
bool IsThirstProtectionRequested(){return {};}
void SetDarkAreaBrightnessEnabled(bool enabled){}
void SetDurabilityProtectionEnabled(bool enabled){}
void SetEnduranceRecoveryEnabled(bool enabled){}
void SetFakeDaylightEnabled(bool enabled){}
void SetFarmingModeEnabled(bool enabled){}
void SetFastAimingDelayEnabled(bool enabled){}
void SetFatigueProtectionEnabled(bool enabled){}
void SetFogVisualIntensity(float intensity){}
void SetFogVisualMode(WeatherVisualMode mode){}
void SetForce360VisionEnabled(bool enabled){}
void SetFreeBuildEnabled(bool enabled){}
void SetHungerProtectionEnabled(bool enabled){}
void SetInfectionImmunityEnabled(bool enabled){}
void SetInfiniteAmmoEnabled(bool enabled){}
void SetInfiniteHealthEnabled(bool enabled){}
void SetInvincibilityEnabled(bool enabled){}
void SetMultiHitEnabled(bool enabled){}
void SetNegativeMoodlesProtectionEnabled(bool enabled){}
void SetNoClipEnabled(bool enabled){}
void SetNoWeaponJamEnabled(bool enabled){}
void SetPanicProtectionEnabled(bool enabled){}
void SetPlayerTeleportEnabled(bool enabled){}
void SetPrecipitationVisualIntensity(float intensity){}
void SetPrecipitationVisualMode(PrecipitationVisualMode mode){}
void SetRevealUnexploredEnabled(bool enabled){}
void SetThirstProtectionEnabled(bool enabled){}
void SetTimedActionInstantEnabled(bool enabled){}
void SetWorldMapPlayerEnabled(bool enabled){}
void SetWorldMapRevealEnabled(bool enabled){}
}
namespace pztrainer::features::visual { WeaponRaySettings& GetWeaponRaySettings(){static WeaponRaySettings value;return value;} }
