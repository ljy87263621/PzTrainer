#include "bridge/item_bridge.hpp"

#include "vmprotect.hpp"

#include <jni.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_set>
#include <utility>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/ammo_item_bridge.hpp"
#include "bridge/corpse_item_bridge.hpp"
#include "bridge/explosive_trap_item_bridge.hpp"
#include "bridge/item_spawn_limiter.hpp"
#include "bridge/magazine_item_bridge.hpp"
#include "bridge/pallet_item_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"
#include "bridge/trap_item_bridge.hpp"

namespace pztrainer::bridge {
namespace {

struct Bindings {
    bool ready = false;
    jclass game_client = nullptr;
    jclass game_server = nullptr;
    jclass iso_player = nullptr;
    jclass script_manager = nullptr;
    jclass array_list = nullptr;
    jclass script_item = nullptr;
    jclass texture = nullptr;
    jclass inventory_item_factory = nullptr;
    jclass item_transaction_packet = nullptr;
    jclass transaction_manager = nullptr;
    jclass iso_directions = nullptr;
    jclass inventory_item = nullptr;
    jclass item_container = nullptr;
    jclass inventory_container = nullptr;
    jclass food = nullptr;
    jclass hand_weapon = nullptr;
    jclass weapon_part = nullptr;
    jclass ammo_type = nullptr;
    jclass packet_type = nullptr;
    jfieldID client_flag = nullptr;
    jfieldID server_flag = nullptr;
    jfieldID item_transaction_type = nullptr;
    jfieldID direction_n = nullptr;
    jfieldID script_manager_instance = nullptr;
    jmethodID get_player = nullptr;
    jmethodID get_all_items = nullptr;
    jmethodID list_size = nullptr;
    jmethodID list_get = nullptr;
    jmethodID item_hidden = nullptr;
    jmethodID item_obsolete = nullptr;
    jmethodID item_full_name = nullptr;
    jmethodID item_display_name = nullptr;
    jmethodID item_category = nullptr;
    jmethodID item_texture = nullptr;
    jmethodID create_item = nullptr;
    jmethodID item_visual = nullptr;
    jmethodID item_favorite = nullptr;
    jmethodID item_full_type = nullptr;
    jmethodID player_is_equipped = nullptr;
    jmethodID transaction_add = nullptr;
    jmethodID inventory_set_draw_dirty = nullptr;
    jmethodID texture_id = nullptr;
    jmethodID texture_x_start = nullptr;
    jmethodID texture_y_start = nullptr;
    jmethodID texture_x_end = nullptr;
    jmethodID texture_y_end = nullptr;
    jmethodID texture_width = nullptr;
    jmethodID texture_height = nullptr;
    jmethodID food_base_hunger = nullptr;
    jmethodID weapon_ammo_type = nullptr;
    jmethodID ammo_item_key = nullptr;
    jmethodID magazine_type = nullptr;
    jmethodID uses_external_magazine = nullptr;
    jmethodID weapon_part_mount_on = nullptr;
    jmethodID explosion_range = nullptr;
    jmethodID explosion_power = nullptr;
    jmethodID fire_range = nullptr;
    jmethodID fire_starting_energy = nullptr;
    jmethodID fire_starting_chance = nullptr;
    jmethodID smoke_range = nullptr;
    jmethodID noise_range = nullptr;
    jmethodID sensor_range = nullptr;
};

Bindings g_bindings;
std::vector<ItemCatalogEntry> g_catalog;
ItemSpawnResult g_last_result;
std::chrono::steady_clock::time_point g_inventory_refresh_at{};
int g_inventory_refresh_passes = 0;
AsyncObjectMethodCall g_inventory_refresh_call;

enum class LocalSpawnStage {
    Inventory,
    AddDirectToInventory,
    ValidateInventory,
    Square,
    SpawnGround,
};

struct LocalSpawnTask {
    bool active = false;
    std::string full_type;
    int requested_count = 0;
    ItemSpawnDestination destination = ItemSpawnDestination::Backpack;
    LocalSpawnStage stage = LocalSpawnStage::Inventory;
    jobject player = nullptr;
    jobject inventory = nullptr;
    jobject square = nullptr;
    jobject item = nullptr;
    AsyncObjectMethodCall call;
};

LocalSpawnTask g_local_spawn_task;
constexpr std::chrono::milliseconds kLocalSpawnStageTimeout{2000};

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader_class = env->FindClass("java/lang/ClassLoader");
    if (class_loader_class == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr
        ? nullptr
        : env->CallStaticObjectMethod(class_loader_class, get_system_loader);
    env->DeleteLocalRef(class_loader_class);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }

    std::string dotted_name(binary_name);
    std::replace(dotted_name.begin(), dotted_name.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted_name.c_str());
    jclass local = name == nullptr
        ? nullptr
        : static_cast<jclass>(env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* utf = env->GetStringUTFChars(value, nullptr);
    if (utf == nullptr || ClearException(env)) return {};
    std::string result(utf);
    env->ReleaseStringUTFChars(value, utf);
    return result;
}

PZ_VMP_NOINLINE bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    PZ_VMP_BEGIN_MUTATION("PZ.DLL.ItemCoreBindings");
    g_bindings.game_client = LoadGlobalClass(env, "zombie/network/GameClient");
    g_bindings.game_server = LoadGlobalClass(env, "zombie/network/GameServer");
    g_bindings.iso_player = LoadGlobalClass(env, "zombie/characters/IsoPlayer");
    g_bindings.script_manager = LoadGlobalClass(env, "zombie/scripting/ScriptManager");
    g_bindings.array_list = LoadGlobalClass(env, "java/util/ArrayList");
    g_bindings.script_item = LoadGlobalClass(env, "zombie/scripting/objects/Item");
    g_bindings.texture = LoadGlobalClass(env, "zombie/core/textures/Texture");
    g_bindings.inventory_item_factory = LoadGlobalClass(
        env, "zombie/inventory/InventoryItemFactory");
    g_bindings.item_transaction_packet = LoadGlobalClass(
        env, "zombie/network/packets/ItemTransactionPacket");
    g_bindings.transaction_manager = LoadGlobalClass(
        env, "zombie/core/TransactionManager");
    g_bindings.iso_directions = LoadGlobalClass(env, "zombie/iso/IsoDirections");
    g_bindings.inventory_item = LoadGlobalClass(
        env, "zombie/inventory/InventoryItem");
    g_bindings.item_container = LoadGlobalClass(
        env, "zombie/inventory/ItemContainer");
    g_bindings.inventory_container = LoadGlobalClass(
        env, "zombie/inventory/types/InventoryContainer");
    g_bindings.food = LoadGlobalClass(env, "zombie/inventory/types/Food");
    g_bindings.hand_weapon = LoadGlobalClass(
        env, "zombie/inventory/types/HandWeapon");
    g_bindings.weapon_part = LoadGlobalClass(
        env, "zombie/inventory/types/WeaponPart");
    g_bindings.ammo_type = LoadGlobalClass(
        env, "zombie/scripting/objects/AmmoType");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    if (g_bindings.game_client == nullptr || g_bindings.game_server == nullptr ||
        g_bindings.iso_player == nullptr || g_bindings.script_manager == nullptr ||
        g_bindings.array_list == nullptr || g_bindings.script_item == nullptr ||
        g_bindings.texture == nullptr || g_bindings.inventory_item_factory == nullptr ||
        g_bindings.item_transaction_packet == nullptr ||
        g_bindings.transaction_manager == nullptr ||
        g_bindings.iso_directions == nullptr ||
        g_bindings.inventory_item == nullptr ||
        g_bindings.item_container == nullptr ||
        g_bindings.inventory_container == nullptr ||
        g_bindings.food == nullptr || g_bindings.hand_weapon == nullptr ||
        g_bindings.weapon_part == nullptr || g_bindings.ammo_type == nullptr ||
        g_bindings.packet_type == nullptr) {
        return false;
    }

    g_bindings.client_flag = env->GetStaticFieldID(g_bindings.game_client, "client", "Z");
    g_bindings.server_flag = env->GetStaticFieldID(g_bindings.game_server, "server", "Z");
    g_bindings.item_transaction_type = env->GetStaticFieldID(
        g_bindings.packet_type, "ItemTransaction",
        "Lzombie/network/PacketTypes$PacketType;");
    g_bindings.direction_n = env->GetStaticFieldID(
        g_bindings.iso_directions, "N", "Lzombie/iso/IsoDirections;");
    g_bindings.script_manager_instance = env->GetStaticFieldID(
        g_bindings.script_manager, "instance", "Lzombie/scripting/ScriptManager;");
    g_bindings.get_player = env->GetStaticMethodID(
        g_bindings.iso_player, "getInstance", "()Lzombie/characters/IsoPlayer;");
    g_bindings.get_all_items = env->GetMethodID(
        g_bindings.script_manager, "getAllItems", "()Ljava/util/ArrayList;");
    g_bindings.list_size = env->GetMethodID(g_bindings.array_list, "size", "()I");
    g_bindings.list_get = env->GetMethodID(
        g_bindings.array_list, "get", "(I)Ljava/lang/Object;");
    g_bindings.item_hidden = env->GetMethodID(g_bindings.script_item, "isHidden", "()Z");
    g_bindings.item_obsolete = env->GetMethodID(g_bindings.script_item, "getObsolete", "()Z");
    g_bindings.item_full_name = env->GetMethodID(
        g_bindings.script_item, "getFullName", "()Ljava/lang/String;");
    g_bindings.item_display_name = env->GetMethodID(
        g_bindings.script_item, "getDisplayName", "()Ljava/lang/String;");
    g_bindings.item_category = env->GetMethodID(
        g_bindings.script_item, "getDisplayCategory", "()Ljava/lang/String;");
    g_bindings.item_texture = env->GetMethodID(
        g_bindings.script_item, "getNormalTexture", "()Lzombie/core/textures/Texture;");
    g_bindings.create_item = env->GetStaticMethodID(
        g_bindings.inventory_item_factory, "CreateItem",
        "(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
    g_bindings.item_visual = env->GetMethodID(
        g_bindings.inventory_item, "getVisual",
        "()Lzombie/core/skinnedmodel/visual/ItemVisual;");
    g_bindings.item_favorite = env->GetMethodID(
        g_bindings.inventory_item, "isFavorite", "()Z");
    g_bindings.item_full_type = env->GetMethodID(
        g_bindings.inventory_item, "getFullType", "()Ljava/lang/String;");
    g_bindings.player_is_equipped = env->GetMethodID(
        g_bindings.iso_player, "isEquipped", "(Lzombie/inventory/InventoryItem;)Z");
    g_bindings.transaction_add = env->GetStaticMethodID(
        g_bindings.transaction_manager, "add", "(Lzombie/core/Transaction;)V");
    g_bindings.inventory_set_draw_dirty = env->GetMethodID(
        g_bindings.item_container, "setDrawDirty", "(Z)V");
    g_bindings.texture_id = env->GetMethodID(g_bindings.texture, "getID", "()I");
    g_bindings.texture_x_start = env->GetMethodID(g_bindings.texture, "getXStart", "()F");
    g_bindings.texture_y_start = env->GetMethodID(g_bindings.texture, "getYStart", "()F");
    g_bindings.texture_x_end = env->GetMethodID(g_bindings.texture, "getXEnd", "()F");
    g_bindings.texture_y_end = env->GetMethodID(g_bindings.texture, "getYEnd", "()F");
    g_bindings.texture_width = env->GetMethodID(g_bindings.texture, "getWidth", "()I");
    g_bindings.texture_height = env->GetMethodID(g_bindings.texture, "getHeight", "()I");
    g_bindings.food_base_hunger = env->GetMethodID(
        g_bindings.food, "getBaseHunger", "()F");
    g_bindings.weapon_ammo_type = env->GetMethodID(
        g_bindings.hand_weapon, "getAmmoType",
        "()Lzombie/scripting/objects/AmmoType;");
    g_bindings.ammo_item_key = env->GetMethodID(
        g_bindings.ammo_type, "getItemKey", "()Ljava/lang/String;");
    g_bindings.magazine_type = env->GetMethodID(
        g_bindings.hand_weapon, "getMagazineType", "()Ljava/lang/String;");
    g_bindings.uses_external_magazine = env->GetMethodID(
        g_bindings.hand_weapon, "usesExternalMagazine", "()Z");
    g_bindings.weapon_part_mount_on = env->GetMethodID(
        g_bindings.weapon_part, "getMountOn", "()Ljava/util/List;");
    g_bindings.explosion_range = env->GetMethodID(
        g_bindings.hand_weapon, "getExplosionRange", "()I");
    g_bindings.explosion_power = env->GetMethodID(
        g_bindings.hand_weapon, "getExplosionPower", "()I");
    g_bindings.fire_range = env->GetMethodID(
        g_bindings.hand_weapon, "getFireRange", "()I");
    g_bindings.fire_starting_energy = env->GetMethodID(
        g_bindings.hand_weapon, "getFireStartingEnergy", "()I");
    g_bindings.fire_starting_chance = env->GetMethodID(
        g_bindings.hand_weapon, "getFireStartingChance", "()I");
    g_bindings.smoke_range = env->GetMethodID(
        g_bindings.hand_weapon, "getSmokeRange", "()I");
    g_bindings.noise_range = env->GetMethodID(
        g_bindings.hand_weapon, "getNoiseRange", "()I");
    g_bindings.sensor_range = env->GetMethodID(
        g_bindings.hand_weapon, "getSensorRange", "()I");
    g_bindings.ready = !ClearException(env) && g_bindings.client_flag != nullptr &&
        g_bindings.server_flag != nullptr && g_bindings.item_transaction_type != nullptr &&
        g_bindings.direction_n != nullptr &&
        g_bindings.script_manager_instance != nullptr &&
        g_bindings.get_player != nullptr && g_bindings.get_all_items != nullptr &&
        g_bindings.list_size != nullptr && g_bindings.list_get != nullptr &&
        g_bindings.item_hidden != nullptr && g_bindings.item_obsolete != nullptr &&
        g_bindings.item_full_name != nullptr && g_bindings.item_display_name != nullptr &&
        g_bindings.item_category != nullptr && g_bindings.item_texture != nullptr &&
        g_bindings.create_item != nullptr &&
        g_bindings.item_visual != nullptr && g_bindings.item_favorite != nullptr &&
        g_bindings.item_full_type != nullptr &&
        g_bindings.player_is_equipped != nullptr &&
        g_bindings.transaction_add != nullptr &&
        g_bindings.inventory_set_draw_dirty != nullptr &&
        g_bindings.texture_id != nullptr && g_bindings.texture_x_start != nullptr &&
        g_bindings.texture_y_start != nullptr && g_bindings.texture_x_end != nullptr &&
        g_bindings.texture_y_end != nullptr && g_bindings.texture_width != nullptr &&
        g_bindings.texture_height != nullptr &&
        g_bindings.food_base_hunger != nullptr &&
        g_bindings.weapon_ammo_type != nullptr &&
        g_bindings.ammo_item_key != nullptr &&
        g_bindings.magazine_type != nullptr &&
        g_bindings.uses_external_magazine != nullptr &&
        g_bindings.weapon_part_mount_on != nullptr &&
        g_bindings.explosion_range != nullptr &&
        g_bindings.explosion_power != nullptr && g_bindings.fire_range != nullptr &&
        g_bindings.fire_starting_energy != nullptr &&
        g_bindings.fire_starting_chance != nullptr &&
        g_bindings.smoke_range != nullptr && g_bindings.noise_range != nullptr &&
        g_bindings.sensor_range != nullptr;
    PZ_VMP_END();
    return g_bindings.ready;
}

void DeleteLocalRef(JNIEnv* env, jobject value) {
    if (value != nullptr) env->DeleteLocalRef(value);
}

void DeleteGlobalRef(JNIEnv* env, jobject& value) {
    if (value != nullptr) env->DeleteGlobalRef(value);
    value = nullptr;
}

bool HasHazardousTrapEffect(JNIEnv* env, jobject weapon) {
    const int explosion_range = env->CallIntMethod(
        weapon, g_bindings.explosion_range);
    const int explosion_power = env->CallIntMethod(
        weapon, g_bindings.explosion_power);
    const int fire_range = env->CallIntMethod(weapon, g_bindings.fire_range);
    const int fire_energy = env->CallIntMethod(
        weapon, g_bindings.fire_starting_energy);
    const int fire_chance = env->CallIntMethod(
        weapon, g_bindings.fire_starting_chance);
    const int smoke_range = env->CallIntMethod(weapon, g_bindings.smoke_range);
    const int noise_range = env->CallIntMethod(weapon, g_bindings.noise_range);
    const int sensor_range = env->CallIntMethod(weapon, g_bindings.sensor_range);
    if (ClearException(env)) return true;
    return explosion_range > 0 || explosion_power > 0 || fire_range > 0 ||
        fire_energy > 0 || fire_chance > 0 || smoke_range > 0 ||
        noise_range > 0 || sensor_range > 0;
}

bool HasSafeWeaponPartCarrier(JNIEnv* env, jobject part) {
    jobject mounts = env->CallObjectMethod(
        part, g_bindings.weapon_part_mount_on);
    const jint mount_count = mounts == nullptr
        ? 0
        : env->CallIntMethod(mounts, g_bindings.list_size);
    if (ClearException(env)) {
        DeleteLocalRef(env, mounts);
        return false;
    }

    bool found = false;
    for (jint index = 0; index < mount_count; ++index) {
        jstring mount = static_cast<jstring>(env->CallObjectMethod(
            mounts, g_bindings.list_get, index));
        jobject carrier = mount == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item,
                  mount);
        const bool compatible = carrier != nullptr && !ClearException(env) &&
            env->IsInstanceOf(carrier, g_bindings.hand_weapon) == JNI_TRUE &&
            !HasHazardousTrapEffect(env, carrier);
        DeleteLocalRef(env, carrier);
        DeleteLocalRef(env, mount);
        if (compatible) {
            found = true;
            break;
        }
        ClearException(env);
    }
    DeleteLocalRef(env, mounts);
    return found;
}

bool InvokeIgnoringResult(JNIEnv* env, jobject target, const char* method_name,
                          std::initializer_list<jobject> arguments);

bool SendPacket(JNIEnv* env, jobject packet, jfieldID packet_type_field) {
    jobject packet_type = env->GetStaticObjectField(
        g_bindings.packet_type, packet_type_field);
    const bool sent = packet_type != nullptr && !ClearException(env) &&
        InvokeIgnoringResult(env, packet, "sendToServer", {packet_type});
    DeleteLocalRef(env, packet_type);
    return sent;
}

int SendVariantTransform(JNIEnv* env, jobject player, jobject inventory,
                         jobject item_type, int quantity, std::string& detail) {
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.ItemVariantTransform");
    bool invoked = false;
    jobject target = InvokeObjectMethodOnMainThread(
        env, g_bindings.inventory_item_factory, "CreateItem", {item_type}, &invoked);
    jobject target_visual = invoked && target != nullptr
        ? env->CallObjectMethod(target, g_bindings.item_visual)
        : nullptr;
    if (ClearException(env) || target == nullptr || target_visual == nullptr) {
        DeleteLocalRef(env, target_visual);
        DeleteLocalRef(env, target);
        detail = "目标物品没有可复制的 ItemVisual，已阻止高风险转换";
        return 0;
    }
    DeleteLocalRef(env, target_visual);
    DeleteLocalRef(env, target);

    jobject inventory_items = InvokeObjectMethodOnMainThread(
        env, inventory, "getItems", {}, &invoked);
    jobject donors = InvokeObjectMethodOnMainThread(
        env, g_bindings.array_list, "new", {}, &invoked);
    if (!invoked || inventory_items == nullptr || donors == nullptr) {
        DeleteLocalRef(env, donors);
        DeleteLocalRef(env, inventory_items);
        detail = "无法读取背包中的候选源物品";
        return 0;
    }

    const jint item_count = env->CallIntMethod(inventory_items, g_bindings.list_size);
    int donor_count = 0;
    std::string first_donor_type;
    for (jint index = 0; index < item_count && donor_count < quantity; ++index) {
        jobject candidate = env->CallObjectMethod(
            inventory_items, g_bindings.list_get, index);
        if (candidate == nullptr || ClearException(env)) {
            DeleteLocalRef(env, candidate);
            continue;
        }
        const bool container = env->IsInstanceOf(
            candidate, g_bindings.inventory_container) == JNI_TRUE;
        const bool favorite = env->CallBooleanMethod(
            candidate, g_bindings.item_favorite) == JNI_TRUE;
        const bool equipped = env->CallBooleanMethod(
            player, g_bindings.player_is_equipped, candidate) == JNI_TRUE;
        jobject visual = (!container && !favorite && !equipped && !ClearException(env))
            ? env->CallObjectMethod(candidate, g_bindings.item_visual)
            : nullptr;
        const bool eligible = visual != nullptr && !ClearException(env);
        DeleteLocalRef(env, visual);
        if (eligible) {
            if (first_donor_type.empty()) {
                jstring full_type = static_cast<jstring>(
                    env->CallObjectMethod(candidate, g_bindings.item_full_type));
                first_donor_type = JavaString(env, full_type);
                DeleteLocalRef(env, full_type);
                ClearException(env);
            }
            if (InvokeIgnoringResult(env, donors, "add", {candidate})) {
                ++donor_count;
            }
        }
        DeleteLocalRef(env, candidate);
    }
    DeleteLocalRef(env, inventory_items);
    if (donor_count == 0) {
        DeleteLocalRef(env, donors);
        detail = "背包里没有未装备、未收藏且带 ItemVisual 的可消耗物品";
        return 0;
    }

    jobject zero = BoxFloat(env, 0.0f);
    jobject direction = env->GetStaticObjectField(
        g_bindings.iso_directions, g_bindings.direction_n);
    jobject packet = zero == nullptr
        ? nullptr
        : InvokeObjectMethodOnMainThread(
            env, g_bindings.item_transaction_packet, "new", {}, &invoked);
    const bool prepared = invoked && packet != nullptr &&
        InvokeIgnoringResult(
            env, packet, "set",
            {player, donors, inventory, inventory, item_type, direction,
             zero, zero, zero});
    const bool sent = prepared &&
        SendPacket(env, packet, g_bindings.item_transaction_type);
    bool registered = false;
    if (sent) {
        env->CallStaticVoidMethod(
            g_bindings.transaction_manager, g_bindings.transaction_add, packet);
        registered = !ClearException(env);
    }
    DeleteLocalRef(env, packet);
    DeleteLocalRef(env, direction);
    DeleteLocalRef(env, zero);
    DeleteLocalRef(env, donors);
    if (!sent || !registered) {
        detail = sent
            ? "变体交易包已发送，但客户端事务登记失败"
            : "变体交易包构造或发送失败";
        return 0;
    }
    detail = "已提交并登记变体交易；将消耗 " + std::to_string(donor_count) +
        " 件合格物品";
    if (!first_donor_type.empty()) detail += "（首件：" + first_donor_type + "）";
    g_inventory_refresh_passes = 4;
    g_inventory_refresh_at =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    PZ_VMP_END();
    return donor_count;
}

bool InvokeIgnoringResult(JNIEnv* env, jobject target, const char* method_name,
                          std::initializer_list<jobject> arguments) {
    bool invoked = false;
    jobject result = InvokeObjectMethodOnMainThread(
        env, target, method_name, arguments, &invoked);
    DeleteLocalRef(env, result);
    return invoked;
}

const char* LocalSpawnStageName(LocalSpawnStage stage) {
    switch (stage) {
        case LocalSpawnStage::Inventory: return "读取背包";
        case LocalSpawnStage::AddDirectToInventory: return "原版直接加入背包";
        case LocalSpawnStage::ValidateInventory: return "验证背包状态";
        case LocalSpawnStage::Square: return "读取位置";
        case LocalSpawnStage::SpawnGround: return "生成到地面";
    }
    return "未知阶段";
}

void ResetLocalSpawnTask(JNIEnv* env) {
    ResetObjectMethodCall(env, g_local_spawn_task.call);
    DeleteGlobalRef(env, g_local_spawn_task.item);
    DeleteGlobalRef(env, g_local_spawn_task.square);
    DeleteGlobalRef(env, g_local_spawn_task.inventory);
    DeleteGlobalRef(env, g_local_spawn_task.player);
    g_local_spawn_task = LocalSpawnTask{};
}

void FailLocalSpawnTask(JNIEnv* env, const char* reason) {
    const std::string type = g_local_spawn_task.full_type;
    const std::string stage = LocalSpawnStageName(g_local_spawn_task.stage);
    const int created = g_last_result.created_count;
    g_last_result.succeeded = false;
    g_last_result.message = "生成失败：" + type + " · " + stage + " · " + reason;
    if (created > 0) {
        g_last_result.message += "（已完成 " + std::to_string(created) + " 个）";
    }
    ResetLocalSpawnTask(env);
}

void FinishLocalSpawnItem(JNIEnv* env) {
    ++g_last_result.created_count;
    DeleteGlobalRef(env, g_local_spawn_task.item);
    if (g_last_result.created_count >= g_local_spawn_task.requested_count) {
        const int count = g_last_result.created_count;
        const ItemSpawnDestination destination = g_local_spawn_task.destination;
        g_last_result.succeeded = true;
        g_last_result.message = "已生成 " + std::to_string(count) +
            (destination == ItemSpawnDestination::Backpack
                ? " 个到背包（原版 ItemContainer.AddItem）"
                : " 个到地面（逐帧游戏生成）");
        g_inventory_refresh_passes = destination == ItemSpawnDestination::Backpack ? 4 : 0;
        g_inventory_refresh_at =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        ResetLocalSpawnTask(env);
        return;
    }
    g_local_spawn_task.stage =
        g_local_spawn_task.destination == ItemSpawnDestination::Backpack
        ? LocalSpawnStage::AddDirectToInventory
        : LocalSpawnStage::SpawnGround;
    g_last_result.message = "正在生成 " + g_local_spawn_task.full_type + " · " +
        std::to_string(g_last_result.created_count) + "/" +
        std::to_string(g_local_spawn_task.requested_count);
}

bool StoreGlobalResult(JNIEnv* env, jobject local, jobject& global) {
    DeleteGlobalRef(env, global);
    if (local == nullptr) return false;
    global = env->NewGlobalRef(local);
    return global != nullptr && !ClearException(env);
}

bool QueueLocalSpawnStage(JNIEnv* env) {
    LocalSpawnTask& task = g_local_spawn_task;
    bool queued = false;
    switch (task.stage) {
        case LocalSpawnStage::Inventory:
            queued = QueueObjectMethodOnMainThread(
                env, task.player, "getInventory", {}, task.call);
            break;
        case LocalSpawnStage::AddDirectToInventory: {
            jstring item_type = env->NewStringUTF(task.full_type.c_str());
            if (item_type != nullptr) {
                queued = QueueObjectMethodOnMainThread(
                    env, task.inventory, "AddItem", {item_type}, task.call);
            }
            DeleteLocalRef(env, item_type);
            break;
        }
        case LocalSpawnStage::ValidateInventory:
            queued = QueueObjectMethodOnMainThread(
                env, task.item, "getContainer", {}, task.call);
            break;
        case LocalSpawnStage::Square:
            queued = QueueObjectMethodOnMainThread(
                env, task.player, "getCurrentSquare", {}, task.call);
            break;
        case LocalSpawnStage::SpawnGround: {
            jstring item_type = env->NewStringUTF(task.full_type.c_str());
            jobject x = BoxFloat(env, 0.5f);
            jobject y = BoxFloat(env, 0.5f);
            jobject z = BoxFloat(env, 0.0f);
            if (item_type != nullptr && x != nullptr && y != nullptr && z != nullptr) {
                queued = QueueObjectMethodOnMainThread(
                    env, task.square, "SpawnWorldInventoryItem",
                    {item_type, x, y, z}, task.call);
            }
            DeleteLocalRef(env, z);
            DeleteLocalRef(env, y);
            DeleteLocalRef(env, x);
            DeleteLocalRef(env, item_type);
            break;
        }
    }
    if (queued) {
        g_last_result.message = "正在生成 " + task.full_type + " · " +
            LocalSpawnStageName(task.stage) + " · " +
            std::to_string(g_last_result.created_count) + "/" +
            std::to_string(task.requested_count);
    }
    return queued;
}

void AdvanceLocalSpawnTask(JNIEnv* env) {
    LocalSpawnTask& task = g_local_spawn_task;
    if (!task.active) return;
    if (task.call.queue_item == nullptr) {
        if (!QueueLocalSpawnStage(env)) {
            FailLocalSpawnTask(env, "无法提交主线程任务");
        }
        return;
    }

    jobject result = nullptr;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, task.call, kLocalSpawnStageTimeout, &result);
    if (state == AsyncObjectMethodState::Pending) return;
    if (state == AsyncObjectMethodState::TimedOut) {
        FailLocalSpawnTask(env, "主线程执行超过 2 秒，已停止后续步骤");
        return;
    }
    if (state != AsyncObjectMethodState::Succeeded) {
        DeleteLocalRef(env, result);
        FailLocalSpawnTask(env, "游戏方法调用失败");
        return;
    }

    bool valid = true;
    switch (task.stage) {
        case LocalSpawnStage::Inventory:
            valid = StoreGlobalResult(env, result, task.inventory);
            task.stage = task.destination == ItemSpawnDestination::Backpack
                ? LocalSpawnStage::AddDirectToInventory
                : LocalSpawnStage::Square;
            break;
        case LocalSpawnStage::AddDirectToInventory:
            valid = StoreGlobalResult(env, result, task.item);
            task.stage = LocalSpawnStage::ValidateInventory;
            break;
        case LocalSpawnStage::ValidateInventory:
            valid = result != nullptr &&
                env->IsSameObject(result, task.inventory) == JNI_TRUE;
            if (valid) {
                DeleteLocalRef(env, result);
                FinishLocalSpawnItem(env);
                return;
            }
            break;
        case LocalSpawnStage::Square:
            valid = StoreGlobalResult(env, result, task.square);
            task.stage = LocalSpawnStage::SpawnGround;
            break;
        case LocalSpawnStage::SpawnGround:
            valid = StoreGlobalResult(env, result, task.item);
            if (valid) {
                DeleteLocalRef(env, result);
                FinishLocalSpawnItem(env);
                return;
            }
            break;
    }
    DeleteLocalRef(env, result);
    if (!valid) FailLocalSpawnTask(env, "返回的游戏对象无效");
}

}  // namespace

bool RefreshItemCatalog() {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return false;
    if (!g_catalog.empty()) return true;

    jobject manager = env->GetStaticObjectField(
        g_bindings.script_manager, g_bindings.script_manager_instance);
    jobject items = manager == nullptr
        ? nullptr
        : env->CallObjectMethod(manager, g_bindings.get_all_items);
    if (manager != nullptr) env->DeleteLocalRef(manager);
    if (items == nullptr || ClearException(env)) {
        if (items != nullptr) env->DeleteLocalRef(items);
        return false;
    }

    const jint count = env->CallIntMethod(items, g_bindings.list_size);
    if (ClearException(env) || count <= 0) {
        env->DeleteLocalRef(items);
        return false;
    }
    g_catalog.reserve(static_cast<std::size_t>(count));
    std::unordered_set<std::string> ammo_types;
    std::unordered_set<std::string> magazine_types;
    for (jint index = 0; index < count; ++index) {
        jobject item = env->CallObjectMethod(items, g_bindings.list_get, index);
        if (item == nullptr || ClearException(env)) {
            if (item != nullptr) env->DeleteLocalRef(item);
            continue;
        }
        const bool hidden = env->CallBooleanMethod(item, g_bindings.item_hidden) == JNI_TRUE;
        const bool obsolete = env->CallBooleanMethod(item, g_bindings.item_obsolete) == JNI_TRUE;
        if (ClearException(env) || hidden || obsolete) {
            env->DeleteLocalRef(item);
            continue;
        }

        jstring full_name = static_cast<jstring>(
            env->CallObjectMethod(item, g_bindings.item_full_name));
        jstring display_name = static_cast<jstring>(
            env->CallObjectMethod(item, g_bindings.item_display_name));
        jstring category = static_cast<jstring>(
            env->CallObjectMethod(item, g_bindings.item_category));
        ItemCatalogEntry entry{};
        entry.full_type = JavaString(env, full_name);
        entry.display_name = JavaString(env, display_name);
        entry.category = JavaString(env, category);
        entry.spawn_method_mask = ItemSpawnMethodMask(ItemSpawnMethod::Game) |
            ItemSpawnMethodMask(ItemSpawnMethod::PalletItemExtract) |
            ItemSpawnMethodMask(ItemSpawnMethod::CorpsePayload);
        jobject sample = full_name == nullptr
            ? nullptr
            : env->CallStaticObjectMethod(
                  g_bindings.inventory_item_factory, g_bindings.create_item,
                  full_name);
        if (sample != nullptr && !ClearException(env)) {
            jobject visual = env->CallObjectMethod(sample, g_bindings.item_visual);
            if (visual != nullptr && !ClearException(env)) {
                entry.spawn_method_mask |=
                    ItemSpawnMethodMask(ItemSpawnMethod::VariantTransform);
            }
            DeleteLocalRef(env, visual);
            ClearException(env);

            if (env->IsInstanceOf(sample, g_bindings.food) == JNI_TRUE) {
                const float base_hunger = env->CallFloatMethod(
                    sample, g_bindings.food_base_hunger);
                if (!ClearException(env) && base_hunger < -0.0001f) {
                    entry.spawn_method_mask |=
                        ItemSpawnMethodMask(ItemSpawnMethod::TrapAnimalFood);
                }
            }
            if (env->IsInstanceOf(sample, g_bindings.hand_weapon) == JNI_TRUE) {
                if (!HasHazardousTrapEffect(env, sample)) {
                    entry.spawn_method_mask |= ItemSpawnMethodMask(
                        ItemSpawnMethod::ExplosiveTrapWeapon);
                }
                jobject ammo_type = env->CallObjectMethod(
                    sample, g_bindings.weapon_ammo_type);
                jstring ammo_key = ammo_type == nullptr
                    ? nullptr
                    : static_cast<jstring>(env->CallObjectMethod(
                          ammo_type, g_bindings.ammo_item_key));
                const std::string ammo = JavaString(env, ammo_key);
                if (!ammo.empty()) ammo_types.insert(ammo);
                DeleteLocalRef(env, ammo_key);
                DeleteLocalRef(env, ammo_type);
                ClearException(env);

                const bool external = env->CallBooleanMethod(
                    sample, g_bindings.uses_external_magazine) == JNI_TRUE;
                jstring magazine = external && !ClearException(env)
                    ? static_cast<jstring>(env->CallObjectMethod(
                          sample, g_bindings.magazine_type))
                    : nullptr;
                const std::string magazine_name = JavaString(env, magazine);
                if (!magazine_name.empty()) {
                    magazine_types.insert(magazine_name);
                }
                DeleteLocalRef(env, magazine);
                ClearException(env);
            }
            if (env->IsInstanceOf(sample, g_bindings.weapon_part) == JNI_TRUE) {
                if (HasSafeWeaponPartCarrier(env, sample)) {
                    entry.spawn_method_mask |=
                        ItemSpawnMethodMask(ItemSpawnMethod::WeaponPartDetach);
                }
            }
        } else {
            ClearException(env);
        }
        DeleteLocalRef(env, sample);
        if (full_name != nullptr) env->DeleteLocalRef(full_name);
        if (display_name != nullptr) env->DeleteLocalRef(display_name);
        if (category != nullptr) env->DeleteLocalRef(category);
        if (entry.full_type.empty() || entry.display_name.empty() || ClearException(env)) {
            env->DeleteLocalRef(item);
            continue;
        }
        if (entry.category.empty()) entry.category = "其他";

        jobject texture = env->CallObjectMethod(item, g_bindings.item_texture);
        if (texture != nullptr && !ClearException(env)) {
            const jint texture_id = env->CallIntMethod(texture, g_bindings.texture_id);
            entry.texture_id = texture_id > 0 ? static_cast<unsigned int>(texture_id) : 0;
            entry.uv_min_x = env->CallFloatMethod(texture, g_bindings.texture_x_start);
            entry.uv_min_y = env->CallFloatMethod(texture, g_bindings.texture_y_start);
            entry.uv_max_x = env->CallFloatMethod(texture, g_bindings.texture_x_end);
            entry.uv_max_y = env->CallFloatMethod(texture, g_bindings.texture_y_end);
            entry.texture_width = env->CallIntMethod(texture, g_bindings.texture_width);
            entry.texture_height = env->CallIntMethod(texture, g_bindings.texture_height);
            ClearException(env);
            env->DeleteLocalRef(texture);
        } else {
            ClearException(env);
        }
        env->DeleteLocalRef(item);
        g_catalog.push_back(std::move(entry));
    }
    env->DeleteLocalRef(items);
    for (ItemCatalogEntry& entry : g_catalog) {
        if (ammo_types.find(entry.full_type) != ammo_types.end()) {
            entry.spawn_method_mask |=
                ItemSpawnMethodMask(ItemSpawnMethod::WeaponAmmoExtract);
        }
        if (magazine_types.find(entry.full_type) != magazine_types.end()) {
            entry.spawn_method_mask |=
                ItemSpawnMethodMask(ItemSpawnMethod::WeaponMagazineExtract);
        }
    }
    std::sort(g_catalog.begin(), g_catalog.end(),
              [](const ItemCatalogEntry& left, const ItemCatalogEntry& right) {
                  if (left.category != right.category) return left.category < right.category;
                  return left.display_name < right.display_name;
              });
    return !g_catalog.empty();
}

void UpdateItemBridge() {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env != nullptr && Initialize(env)) {
        AdvanceLocalSpawnTask(env);
        jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
        if (player != nullptr && !ClearException(env)) {
            UpdateExplosiveTrapWeaponBridge(env, player);
            UpdateWeaponAmmoExtraction(env, player);
            UpdateWeaponMagazineExtraction(env, player);
            UpdatePalletItemBridge(env, player);
            UpdateCorpsePayloadBridge(env, player);
        }
        DeleteLocalRef(env, player);
    }

    if (g_inventory_refresh_call.queue_item != nullptr) {
        if (env == nullptr || !Initialize(env)) return;
        jobject inventory = nullptr;
        const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
            env, g_inventory_refresh_call, kLocalSpawnStageTimeout, &inventory);
        if (state == AsyncObjectMethodState::Pending) return;
        if (state == AsyncObjectMethodState::Succeeded && inventory != nullptr) {
            env->CallVoidMethod(
                inventory, g_bindings.inventory_set_draw_dirty, JNI_TRUE);
            ClearException(env);
        }
        DeleteLocalRef(env, inventory);
        --g_inventory_refresh_passes;
        g_inventory_refresh_at =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        return;
    }
    if (g_inventory_refresh_passes <= 0 ||
        std::chrono::steady_clock::now() < g_inventory_refresh_at ||
        env == nullptr || !Initialize(env)) {
        return;
    }

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    const bool queued = player != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, player, "getInventory", {}, g_inventory_refresh_call);
    DeleteLocalRef(env, player);
    if (!queued) {
        --g_inventory_refresh_passes;
        g_inventory_refresh_at =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    }
}

const std::vector<ItemCatalogEntry>& GetItemCatalog() {
    return g_catalog;
}

const ItemSpawnResult& GetLastItemSpawnResult() {
    return g_last_result;
}

ItemSessionMode GetItemSessionMode() {
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) return ItemSessionMode::Unknown;
    const bool multiplayer_client =
        env->GetStaticBooleanField(
            g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool multiplayer_server =
        env->GetStaticBooleanField(
            g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) return ItemSessionMode::Unknown;
    if (multiplayer_client) return ItemSessionMode::MultiplayerClient;
    if (multiplayer_server) return ItemSessionMode::DedicatedServer;
    return ItemSessionMode::Local;
}

const ItemSpawnResult& SpawnItem(const std::string& full_type, int quantity,
                                 ItemSpawnDestination destination,
                                 ItemSpawnMethod method) {
    PZ_VMP_BEGIN_MUTATION("PZ.DLL.ItemSpawnDispatch");
    if (g_local_spawn_task.active) {
        g_last_result.message = "已有单机生成任务正在执行：" +
            g_local_spawn_task.full_type + " · " +
            std::to_string(g_last_result.created_count) + "/" +
            std::to_string(g_local_spawn_task.requested_count);
        return g_last_result;
    }
    g_last_result = ItemSpawnResult{};
    g_last_result.attempted = true;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_last_result.message = "物品桥接尚未初始化";
        return g_last_result;
    }
    const bool multiplayer_client =
        env->GetStaticBooleanField(g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool multiplayer_server =
        env->GetStaticBooleanField(g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) {
        g_last_result.message = "无法确认当前联机状态";
        return g_last_result;
    }

    const int safe_quantity = std::clamp(quantity, 1, 100);
    if (multiplayer_client) {
        if (method == ItemSpawnMethod::Game) {
            g_last_result.message = "游戏原生生成仅适用于单机";
            return g_last_result;
        }
        if (method != ItemSpawnMethod::CorpsePayload &&
            destination != ItemSpawnDestination::Backpack) {
            g_last_result.message = "联机取物仅支持背包，不能生成到地面";
            return g_last_result;
        }

        int cooldown_remaining = 0;
        if (!TryAcquireOnlineItemSpawn(cooldown_remaining)) {
            const int seconds_remaining =
                (cooldown_remaining + 999) / 1000;
            g_last_result.message = "联机操作过于频繁，请等待 " +
                std::to_string(seconds_remaining) + " 秒后再执行";
            return g_last_result;
        }

        jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
        bool invoked = false;
        if (player == nullptr || ClearException(env)) {
            DeleteLocalRef(env, player);
            g_last_result.message = "联机玩家对象尚未就绪";
            return g_last_result;
        }
        jstring item_type = env->NewStringUTF(full_type.c_str());
        if (method == ItemSpawnMethod::CorpsePayload) {
            if (destination != ItemSpawnDestination::Ground) {
                g_last_result.message = "尸体载荷只会投递到玩家脚下";
            } else {
                std::string detail;
                const std::vector<std::pair<std::string, int>> items{
                    {full_type, safe_quantity}};
                g_last_result.created_count = QueueCorpsePayloadRequests(
                    env, player, items, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::VariantTransform) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "服装变体方案只会写入真实背包";
            } else if (item_type != nullptr) {
                jobject inventory = InvokeObjectMethodOnMainThread(
                    env, player, "getInventory", {}, &invoked);
                std::string detail;
                if (invoked && inventory != nullptr) {
                    g_last_result.created_count = SendVariantTransform(
                        env, player, inventory, item_type, safe_quantity, detail);
                } else {
                    detail = "无法读取玩家背包";
                }
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
                if (g_last_result.created_count > 0 && !g_last_result.succeeded) {
                    g_last_result.message += "；背包中合格源物品不足请求数量";
                }
                DeleteLocalRef(env, inventory);
            }
        } else if (method == ItemSpawnMethod::TrapAnimalFood) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "陷阱猎物方案只会写入真实背包";
            } else {
                std::string detail;
                g_last_result.created_count = SendTrapAnimalFoodRequests(
                    env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::ExplosiveTrapWeapon) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "武器陷阱方案只会回收到真实背包";
            } else {
                std::string detail;
                g_last_result.created_count = QueueExplosiveTrapWeaponRequests(
                    env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::WeaponAmmoExtract) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "弹药拆出方案只会写入真实背包";
            } else {
                std::string detail;
                g_last_result.created_count = QueueWeaponAmmoExtraction(
                    env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::WeaponMagazineExtract) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "弹匣拆出方案只会写入真实背包";
            } else {
                std::string detail;
                g_last_result.created_count = QueueWeaponMagazineExtraction(
                    env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::WeaponPartDetach) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "配件拆出方案只会写入真实背包";
            } else {
                std::string detail;
                g_last_result.created_count =
                    QueueExplosiveTrapWeaponPartRequests(
                        env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else if (method == ItemSpawnMethod::PalletItemExtract) {
            if (destination != ItemSpawnDestination::Backpack) {
                g_last_result.message = "联机取物仅支持背包，不能生成到地面";
            } else {
                std::string detail;
                g_last_result.created_count = QueuePalletItemRequests(
                    env, player, full_type, safe_quantity, detail);
                g_last_result.succeeded =
                    g_last_result.created_count == safe_quantity;
                g_last_result.message = detail;
            }
        } else {
            g_last_result.message = "当前生成方案不支持联机客户端";
        }
        DeleteLocalRef(env, item_type);
        DeleteLocalRef(env, player);
        return g_last_result;
    }
    if (multiplayer_server) {
        g_last_result.message = "当前为专用服务器进程，没有可接收物品的本地玩家";
        return g_last_result;
    }
    if (method == ItemSpawnMethod::VariantTransform ||
        method == ItemSpawnMethod::TrapAnimalFood ||
        method == ItemSpawnMethod::ExplosiveTrapWeapon ||
        method == ItemSpawnMethod::WeaponAmmoExtract ||
        method == ItemSpawnMethod::WeaponMagazineExtract ||
        method == ItemSpawnMethod::WeaponPartDetach ||
        method == ItemSpawnMethod::PalletItemExtract ||
        method == ItemSpawnMethod::CorpsePayload) {
        if (method == ItemSpawnMethod::CorpsePayload) {
            g_last_result.message = "尸体载荷仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::VariantTransform) {
            g_last_result.message = "服装变体交易仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::TrapAnimalFood) {
            g_last_result.message = "陷阱猎物请求仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::ExplosiveTrapWeapon) {
            g_last_result.message = "武器陷阱请求仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::WeaponMagazineExtract) {
            g_last_result.message = "弹匣拆出请求仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::WeaponPartDetach) {
            g_last_result.message = "配件拆出请求仅适用于联机客户端";
        } else if (method == ItemSpawnMethod::PalletItemExtract) {
            g_last_result.message = "联机取物仅适用于联机客户端";
        } else {
            g_last_result.message = "弹药拆出请求仅适用于联机客户端";
        }
        return g_last_result;
    }

    jobject player = env->CallStaticObjectMethod(g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        if (player != nullptr) env->DeleteLocalRef(player);
        g_last_result.message = "请先进入单机存档";
        return g_last_result;
    }
    g_local_spawn_task = LocalSpawnTask{};
    g_local_spawn_task.active = true;
    g_local_spawn_task.full_type = full_type;
    g_local_spawn_task.requested_count = safe_quantity;
    g_local_spawn_task.destination = destination;
    g_local_spawn_task.stage = LocalSpawnStage::Inventory;
    g_local_spawn_task.player = env->NewGlobalRef(player);
    env->DeleteLocalRef(player);
    if (g_local_spawn_task.player == nullptr || ClearException(env)) {
        ResetLocalSpawnTask(env);
        g_last_result.message = "无法保存当前玩家对象";
        return g_last_result;
    }
    g_last_result.message = "已排队生成 " + full_type + " · 0/" +
        std::to_string(safe_quantity);
    PZ_VMP_END();
    return g_last_result;
}

const ItemSpawnResult& SpawnCorpsePayloadBatch(
    const std::vector<ItemSpawnBatchEntry>& items) {
    PZ_VMP_BEGIN_MUTATION("PZ.DLL.ItemCorpsePayloadBatchDispatch");
    g_last_result = ItemSpawnResult{};
    g_last_result.attempted = true;
    if (items.empty()) {
        g_last_result.message = "尸体载荷清单为空";
        return g_last_result;
    }

    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_last_result.message = "物品桥接尚未初始化";
        return g_last_result;
    }
    const bool multiplayer_client =
        env->GetStaticBooleanField(
            g_bindings.game_client, g_bindings.client_flag) == JNI_TRUE;
    const bool multiplayer_server =
        env->GetStaticBooleanField(
            g_bindings.game_server, g_bindings.server_flag) == JNI_TRUE;
    if (ClearException(env)) {
        g_last_result.message = "无法确认当前联机状态";
        return g_last_result;
    }
    if (multiplayer_server) {
        g_last_result.message = "当前为专用服务器进程，没有可接收物品的本地玩家";
        return g_last_result;
    }
    if (!multiplayer_client) {
        g_last_result.message = "尸体载荷仅适用于联机客户端";
        return g_last_result;
    }

    std::vector<std::pair<std::string, int>> payload;
    payload.reserve(items.size());
    int requested_total = 0;
    for (const ItemSpawnBatchEntry& item : items) {
        if (item.full_type.empty()) continue;
        const int quantity = std::clamp(item.quantity, 1, 100);
        payload.emplace_back(item.full_type, quantity);
        requested_total += quantity;
    }
    if (payload.empty()) {
        g_last_result.message = "尸体载荷清单中没有有效物品";
        return g_last_result;
    }

    int cooldown_remaining = 0;
    if (!TryAcquireOnlineItemSpawn(cooldown_remaining)) {
        const int seconds_remaining = (cooldown_remaining + 999) / 1000;
        g_last_result.message = "联机操作过于频繁，请等待 " +
            std::to_string(seconds_remaining) + " 秒后再执行";
        return g_last_result;
    }

    jobject player = env->CallStaticObjectMethod(
        g_bindings.iso_player, g_bindings.get_player);
    if (player == nullptr || ClearException(env)) {
        DeleteLocalRef(env, player);
        g_last_result.message = "联机玩家对象尚未就绪";
        return g_last_result;
    }
    std::string detail;
    g_last_result.created_count = QueueCorpsePayloadRequests(
        env, player, payload, detail);
    DeleteLocalRef(env, player);
    g_last_result.succeeded =
        g_last_result.created_count == requested_total;
    g_last_result.message = detail;
    PZ_VMP_END();
    return g_last_result;
}

}  // namespace pztrainer::bridge
