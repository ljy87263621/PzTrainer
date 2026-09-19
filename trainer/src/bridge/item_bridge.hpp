#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pztrainer::bridge {

enum class ItemSpawnDestination {
    Backpack,
    Ground,
};

enum class ItemSpawnMethod {
    Game,
    VariantTransform,
    TrapAnimalFood,
    ExplosiveTrapWeapon,
    WeaponAmmoExtract,
    WeaponMagazineExtract,
    WeaponPartDetach,
    PalletItemExtract,
    CorpsePayload,
};

enum class ItemSessionMode {
    Unknown,
    Local,
    MultiplayerClient,
    DedicatedServer,
};

constexpr std::uint32_t ItemSpawnMethodMask(ItemSpawnMethod method) {
    return 1u << static_cast<unsigned int>(method);
}

struct ItemCatalogEntry {
    std::string full_type;
    std::string display_name;
    std::string category;
    unsigned int texture_id = 0;
    float uv_min_x = 0.0f;
    float uv_min_y = 0.0f;
    float uv_max_x = 1.0f;
    float uv_max_y = 1.0f;
    int texture_width = 0;
    int texture_height = 0;
    std::uint32_t spawn_method_mask = 0;
};

struct ItemSpawnResult {
    bool attempted = false;
    bool succeeded = false;
    int created_count = 0;
    std::string message;
};

struct ItemSpawnBatchEntry {
    std::string full_type;
    int quantity = 1;
};

bool RefreshItemCatalog();
void UpdateItemBridge();
ItemSessionMode GetItemSessionMode();
const std::vector<ItemCatalogEntry>& GetItemCatalog();
const ItemSpawnResult& GetLastItemSpawnResult();
const ItemSpawnResult& SpawnItem(const std::string& full_type, int quantity,
                                 ItemSpawnDestination destination,
                                 ItemSpawnMethod method);
const ItemSpawnResult& SpawnCorpsePayloadBatch(
    const std::vector<ItemSpawnBatchEntry>& items);

}  // namespace pztrainer::bridge
