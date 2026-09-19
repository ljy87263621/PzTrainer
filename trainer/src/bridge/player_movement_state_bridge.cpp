#include "bridge/player_movement_state_bridge.hpp"

#include <algorithm>
#include <chrono>
#include <string>

namespace pztrainer::bridge {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kActivationDelay = std::chrono::milliseconds(250);
constexpr auto kRefreshInterval = std::chrono::milliseconds(1000);
constexpr auto kRetryInterval = std::chrono::milliseconds(500);
constexpr auto kStateSafetyWindow = std::chrono::milliseconds(3500);

struct Bindings {
    bool ready = false;
    jclass iso_player = nullptr;
    jclass state_packet = nullptr;
    jclass character_id = nullptr;
    jclass state_id = nullptr;
    jclass pz_table = nullptr;
    jclass climb_state = nullptr;
    jclass state_stage = nullptr;
    jclass packet_type = nullptr;
    jclass network_packet = nullptr;
    jclass boolean_class = nullptr;
    jfieldID packet_character_id = nullptr;
    jfieldID packet_state = nullptr;
    jfieldID packet_is_sub_state = nullptr;
    jfieldID packet_params = nullptr;
    jfieldID packet_stage = nullptr;
    jmethodID packet_constructor = nullptr;
    jmethodID position_set = nullptr;
    jmethodID character_id_set = nullptr;
    jmethodID state_id_set = nullptr;
    jmethodID table_wipe = nullptr;
    jmethodID table_raw_set = nullptr;
    jmethodID send_to_server = nullptr;
    jmethodID get_x = nullptr;
    jmethodID get_y = nullptr;
    jmethodID get_z = nullptr;
    jobject climb_state_instance = nullptr;
    jobject enter_stage = nullptr;
    jobject exit_stage = nullptr;
    jobject state_packet_type = nullptr;
    jobject boolean_false = nullptr;
};

Bindings g_bindings;
PlayerMovementStateStatus g_status;
Clock::time_point g_entered_at{};
Clock::time_point g_last_success{};
Clock::time_point g_last_attempt{};

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

template <typename T>
void DeleteGlobalRef(JNIEnv* env, T& value) {
    if (value == nullptr) return;
    env->DeleteGlobalRef(value);
    value = nullptr;
}

void ReleaseBindings(JNIEnv* env) {
    DeleteGlobalRef(env, g_bindings.climb_state_instance);
    DeleteGlobalRef(env, g_bindings.enter_stage);
    DeleteGlobalRef(env, g_bindings.exit_stage);
    DeleteGlobalRef(env, g_bindings.state_packet_type);
    DeleteGlobalRef(env, g_bindings.boolean_false);
    DeleteGlobalRef(env, g_bindings.iso_player);
    DeleteGlobalRef(env, g_bindings.state_packet);
    DeleteGlobalRef(env, g_bindings.character_id);
    DeleteGlobalRef(env, g_bindings.state_id);
    DeleteGlobalRef(env, g_bindings.pz_table);
    DeleteGlobalRef(env, g_bindings.climb_state);
    DeleteGlobalRef(env, g_bindings.state_stage);
    DeleteGlobalRef(env, g_bindings.packet_type);
    DeleteGlobalRef(env, g_bindings.network_packet);
    DeleteGlobalRef(env, g_bindings.boolean_class);
    g_bindings = {};
}

jobject MakeGlobalObject(JNIEnv* env, jclass owner, jfieldID field) {
    jobject local = env->GetStaticObjectField(owner, field);
    if (local == nullptr || ClearException(env)) {
        if (local != nullptr) env->DeleteLocalRef(local);
        return nullptr;
    }
    jobject global = env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.iso_player = LoadGlobalClass(
        env, "zombie/characters/IsoPlayer");
    g_bindings.state_packet = LoadGlobalClass(
        env, "zombie/network/packets/actions/StatePacket");
    g_bindings.character_id = LoadGlobalClass(
        env, "zombie/network/fields/character/CharacterID");
    g_bindings.state_id = LoadGlobalClass(
        env, "zombie/network/fields/StateID");
    g_bindings.pz_table = LoadGlobalClass(
        env, "zombie/network/PZNetKahluaTableImpl");
    g_bindings.climb_state = LoadGlobalClass(
        env, "zombie/ai/states/ClimbThroughWindowState");
    g_bindings.state_stage = LoadGlobalClass(env, "zombie/ai/State$Stage");
    g_bindings.packet_type = LoadGlobalClass(
        env, "zombie/network/PacketTypes$PacketType");
    g_bindings.network_packet = LoadGlobalClass(
        env, "zombie/network/packets/INetworkPacket");
    g_bindings.boolean_class = LoadGlobalClass(env, "java/lang/Boolean");
    if (g_bindings.iso_player == nullptr ||
        g_bindings.state_packet == nullptr ||
        g_bindings.character_id == nullptr || g_bindings.state_id == nullptr ||
        g_bindings.pz_table == nullptr || g_bindings.climb_state == nullptr ||
        g_bindings.state_stage == nullptr || g_bindings.packet_type == nullptr ||
        g_bindings.network_packet == nullptr ||
        g_bindings.boolean_class == nullptr) {
        ReleaseBindings(env);
        return false;
    }

    g_bindings.packet_character_id = env->GetFieldID(
        g_bindings.state_packet, "characterId",
        "Lzombie/network/fields/character/CharacterID;");
    g_bindings.packet_state = env->GetFieldID(
        g_bindings.state_packet, "state", "Lzombie/network/fields/StateID;");
    g_bindings.packet_is_sub_state = env->GetFieldID(
        g_bindings.state_packet, "isSubState", "Z");
    g_bindings.packet_params = env->GetFieldID(
        g_bindings.state_packet, "params",
        "Lzombie/network/PZNetKahluaTableImpl;");
    g_bindings.packet_stage = env->GetFieldID(
        g_bindings.state_packet, "stage", "Lzombie/ai/State$Stage;");
    g_bindings.packet_constructor = env->GetMethodID(
        g_bindings.state_packet, "<init>", "()V");
    g_bindings.position_set = env->GetMethodID(
        g_bindings.state_packet, "set", "(FFF)V");
    g_bindings.character_id_set = env->GetMethodID(
        g_bindings.character_id, "set",
        "(Lzombie/characters/IsoGameCharacter;)V");
    g_bindings.state_id_set = env->GetMethodID(
        g_bindings.state_id, "set", "(Lzombie/ai/State;)V");
    g_bindings.table_wipe = env->GetMethodID(
        g_bindings.pz_table, "wipe", "()V");
    g_bindings.table_raw_set = env->GetMethodID(
        g_bindings.pz_table, "rawset",
        "(Ljava/lang/Object;Ljava/lang/Object;)V");
    g_bindings.send_to_server = env->GetMethodID(
        g_bindings.network_packet, "sendToServer",
        "(Lzombie/network/PacketTypes$PacketType;)V");
    g_bindings.get_x = env->GetMethodID(
        g_bindings.iso_player, "getX", "()F");
    g_bindings.get_y = env->GetMethodID(
        g_bindings.iso_player, "getY", "()F");
    g_bindings.get_z = env->GetMethodID(
        g_bindings.iso_player, "getZ", "()F");
    const jmethodID climb_instance = env->GetStaticMethodID(
        g_bindings.climb_state, "instance",
        "()Lzombie/ai/states/ClimbThroughWindowState;");
    const jfieldID enter_stage = env->GetStaticFieldID(
        g_bindings.state_stage, "Enter", "Lzombie/ai/State$Stage;");
    const jfieldID exit_stage = env->GetStaticFieldID(
        g_bindings.state_stage, "Exit", "Lzombie/ai/State$Stage;");
    const jfieldID state_packet_type = env->GetStaticFieldID(
        g_bindings.packet_type, "State",
        "Lzombie/network/PacketTypes$PacketType;");
    const jfieldID boolean_false = env->GetStaticFieldID(
        g_bindings.boolean_class, "FALSE", "Ljava/lang/Boolean;");
    if (ClearException(env) || climb_instance == nullptr ||
        enter_stage == nullptr || exit_stage == nullptr ||
        state_packet_type == nullptr || boolean_false == nullptr) {
        ReleaseBindings(env);
        return false;
    }

    jobject local_climb_state = env->CallStaticObjectMethod(
        g_bindings.climb_state, climb_instance);
    if (local_climb_state != nullptr && !ClearException(env)) {
        g_bindings.climb_state_instance = env->NewGlobalRef(local_climb_state);
        env->DeleteLocalRef(local_climb_state);
    }
    g_bindings.enter_stage = MakeGlobalObject(
        env, g_bindings.state_stage, enter_stage);
    g_bindings.exit_stage = MakeGlobalObject(
        env, g_bindings.state_stage, exit_stage);
    g_bindings.state_packet_type = MakeGlobalObject(
        env, g_bindings.packet_type, state_packet_type);
    g_bindings.boolean_false = MakeGlobalObject(
        env, g_bindings.boolean_class, boolean_false);

    g_bindings.ready = !ClearException(env) &&
        g_bindings.packet_character_id != nullptr &&
        g_bindings.packet_state != nullptr &&
        g_bindings.packet_is_sub_state != nullptr &&
        g_bindings.packet_params != nullptr &&
        g_bindings.packet_stage != nullptr &&
        g_bindings.packet_constructor != nullptr &&
        g_bindings.position_set != nullptr &&
        g_bindings.character_id_set != nullptr &&
        g_bindings.state_id_set != nullptr &&
        g_bindings.table_wipe != nullptr &&
        g_bindings.table_raw_set != nullptr &&
        g_bindings.send_to_server != nullptr &&
        g_bindings.get_x != nullptr && g_bindings.get_y != nullptr &&
        g_bindings.get_z != nullptr &&
        g_bindings.climb_state_instance != nullptr &&
        g_bindings.enter_stage != nullptr && g_bindings.exit_stage != nullptr &&
        g_bindings.state_packet_type != nullptr &&
        g_bindings.boolean_false != nullptr;
    if (!g_bindings.ready) ReleaseBindings(env);
    return g_bindings.ready;
}

bool SendStatePacket(JNIEnv* env, jobject player, jobject stage) {
    const jfloat x = env->CallFloatMethod(player, g_bindings.get_x);
    const jfloat y = env->CallFloatMethod(player, g_bindings.get_y);
    const jfloat z = env->CallFloatMethod(player, g_bindings.get_z);
    if (ClearException(env)) return false;

    jobject packet = env->NewObject(
        g_bindings.state_packet, g_bindings.packet_constructor);
    if (packet == nullptr || ClearException(env)) {
        if (packet != nullptr) env->DeleteLocalRef(packet);
        return false;
    }
    jobject character_id = env->GetObjectField(
        packet, g_bindings.packet_character_id);
    jobject state_id = env->GetObjectField(packet, g_bindings.packet_state);
    jobject params = env->GetObjectField(packet, g_bindings.packet_params);
    if (character_id == nullptr || state_id == nullptr || params == nullptr ||
        ClearException(env)) {
        if (character_id != nullptr) env->DeleteLocalRef(character_id);
        if (state_id != nullptr) env->DeleteLocalRef(state_id);
        if (params != nullptr) env->DeleteLocalRef(params);
        env->DeleteLocalRef(packet);
        return false;
    }

    jstring scratched = env->NewStringUTF("scratched");
    if (scratched == nullptr || ClearException(env)) {
        if (scratched != nullptr) env->DeleteLocalRef(scratched);
        env->DeleteLocalRef(character_id);
        env->DeleteLocalRef(state_id);
        env->DeleteLocalRef(params);
        env->DeleteLocalRef(packet);
        return false;
    }
    env->CallVoidMethod(packet, g_bindings.position_set, x, y, z);
    env->CallVoidMethod(
        character_id, g_bindings.character_id_set, player);
    env->CallVoidMethod(
        state_id, g_bindings.state_id_set,
        g_bindings.climb_state_instance);
    env->CallVoidMethod(params, g_bindings.table_wipe);
    env->CallVoidMethod(
        params, g_bindings.table_raw_set,
        scratched, g_bindings.boolean_false);
    env->SetBooleanField(
        packet, g_bindings.packet_is_sub_state, JNI_FALSE);
    env->SetObjectField(packet, g_bindings.packet_stage, stage);
    bool success = !ClearException(env);
    if (success) {
        env->CallVoidMethod(
            packet, g_bindings.send_to_server,
            g_bindings.state_packet_type);
        success = !ClearException(env);
    }

    env->DeleteLocalRef(scratched);
    env->DeleteLocalRef(character_id);
    env->DeleteLocalRef(state_id);
    env->DeleteLocalRef(params);
    env->DeleteLocalRef(packet);
    return success;
}

}  // namespace

void UpdatePlayerMovementStateBridge(
    JNIEnv* env, jobject player, bool enabled) {
    if (env == nullptr || player == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.state_declared = false;
        g_status.ready_for_movement = false;
        g_status.message = "攀窗网络状态桥接尚未初始化";
        return;
    }

    g_status.initialized = true;
    const Clock::time_point now = Clock::now();
    if (!enabled) {
        g_status.ready_for_movement = false;
        if (!g_status.state_declared) {
            g_status.message = "攀窗网络状态桥接待命";
            return;
        }
        if (g_last_attempt != Clock::time_point{} &&
            now - g_last_attempt < kRetryInterval) {
            g_status.message = "正在撤销攀窗网络状态";
            return;
        }
        g_last_attempt = now;
        if (SendStatePacket(env, player, g_bindings.exit_stage)) {
            ++g_status.sent_count;
            g_status.state_declared = false;
            g_entered_at = {};
            g_last_success = {};
            g_status.message = "攀窗网络状态已撤销";
        } else {
            g_status.message = "无法发送攀窗网络状态退出包";
        }
        return;
    }

    const bool refresh_due = !g_status.state_declared ||
        g_last_success == Clock::time_point{} ||
        now - g_last_success >= kRefreshInterval;
    const bool retry_ready = g_last_attempt == Clock::time_point{} ||
        now - g_last_attempt >= kRetryInterval;
    if (refresh_due && retry_ready) {
        g_last_attempt = now;
        if (SendStatePacket(env, player, g_bindings.enter_stage)) {
            ++g_status.sent_count;
            if (!g_status.state_declared) g_entered_at = now;
            g_status.state_declared = true;
            g_last_success = now;
        }
    }

    g_status.ready_for_movement = g_status.state_declared &&
        g_entered_at != Clock::time_point{} &&
        g_last_success != Clock::time_point{} &&
        now - g_entered_at >= kActivationDelay &&
        now - g_last_success < kStateSafetyWindow;
    if (g_status.ready_for_movement) {
        g_status.message = "攀窗网络状态已声明";
    } else if (g_status.state_declared) {
        g_status.message = "正在等待攀窗网络状态生效";
    } else {
        g_status.message = "无法发送攀窗网络状态进入包";
    }
}

const PlayerMovementStateStatus& GetPlayerMovementStateStatus() {
    return g_status;
}

}  // namespace pztrainer::bridge
