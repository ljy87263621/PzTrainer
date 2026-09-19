#include "bridge/free_build_bridge.hpp"

#include <Windows.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "bridge/jni_game_bridge.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "bridge/main_thread_invoker.hpp"

namespace pztrainer::bridge {
namespace {

constexpr char kFreeBuildLua[] = R"lua(
local state = rawget(_G, "__PZTrainerFreeBuildState")

if state and state.version ~= 3 then
    state.enabled = false
    if state.pendingSequences then
        for transactionId, _ in pairs(state.pendingSequences) do
            pcall(removeAction, transactionId, true)
        end
    end
    if state.originalCreateBuildAction then
        createBuildAction = state.originalCreateBuildAction
    end
    if state.originalEntityIsValid and ISBuildIsoEntity then
        ISBuildIsoEntity.isValid = state.originalEntityIsValid
    end
    if state.originalBuildActionPerform and ISBuildAction then
        ISBuildAction.perform = state.originalBuildActionPerform
    end
    if state.originalBuildActionStop and ISBuildAction then
        ISBuildAction.stop = state.originalBuildActionStop
    end
    if state.originalBuildControlPrerender and ISWidgetBuildControl then
        ISWidgetBuildControl.prerender = state.originalBuildControlPrerender
    end
    if state.originalCreateBuildIsoEntity and ISBuildPanel then
        ISBuildPanel.createBuildIsoEntity = state.originalCreateBuildIsoEntity
    end
    if state.processSequences then
        pcall(Events.OnTick.Remove, state.processSequences)
    end
    if state.syncMenu then
        pcall(Events.OnTick.Remove, state.syncMenu)
    end
    if state.ensureBuildControlPatch then
        pcall(Events.OnTick.Remove, state.ensureBuildControlPatch)
    end
    if state.ensureBuildPanelPatch then
        pcall(Events.OnTick.Remove, state.ensureBuildPanelPatch)
    end
    rawset(_G, "__PZTrainerFreeBuildState", nil)
    state = nil
end

if not state then
    if type(createBuildAction) ~= "function" or not ISBuildMenu or
       not ISBuildIsoEntity or not ISWoodenWall or not ISWoodenFloor or
       not ISWoodenDoor or not ISWoodenDoorFrame or not ISSimpleFurniture then
        error("required vanilla building classes are unavailable")
    end

    state = {
        version = 3,
        enabled = false,
        originalCreateBuildAction = createBuildAction,
        originalEntityIsValid = ISBuildIsoEntity.isValid,
    }

    function state:inspectFace(face)
        if not face then return false, nil end
        local occupied = 0
        local result = nil
        for z = 0, face:getzLayers() - 1 do
            for x = 0, face:getWidth() - 1 do
                for y = 0, face:getHeight() - 1 do
                    local tile = face:getTileInfo(x, y, z)
                    if tile and
                       (tile:getSpriteName() or tile:isBlocking()) then
                        occupied = occupied + 1
                        if occupied > 1 or not tile:getSpriteName() then
                            return false, nil
                        end
                        local sprite = getSprite(tile:getSpriteName())
                        local props = sprite and sprite:getProperties() or nil
                        if not sprite or props and
                           (props:has(IsoFlagType.WindowN) or
                            props:has(IsoFlagType.WindowW) or
                            props:has(IsoFlagType.windowN) or
                            props:has(IsoFlagType.windowW)) then
                            return false, nil
                        end
                        result = {
                            x = x,
                            y = y,
                            z = z,
                            spriteName = tile:getSpriteName(),
                        }
                    end
                end
            end
        end
        return occupied == 1, result
    end

    function state:isSingleStructureInfo(info)
        if not info or not info.getFace then return false end
        local found = false
        for index = 0, 3 do
            local face = info:getFace(index)
            if face then
                found = true
                if not self:inspectFace(face) then return false end
            end
        end
        for _, name in ipairs({"n_open", "w_open"}) do
            local face = info:getFace(name)
            if face and not self:inspectFace(face) then return false end
        end
        return found
    end

    function state:getProxyTile(item)
        if not item or item.Type ~= "ISBuildIsoEntity" or
           not self:isSingleStructureInfo(item.objectInfo) then
            return nil
        end
        local valid, tile = self:inspectFace(item:getFace())
        if not valid then return nil end
        local openFace = item:getOpenFace(item.north)
        if openFace then
            local openValid, openTile = self:inspectFace(openFace)
            if not openValid then return nil end
            tile.openSpriteName = openTile.spriteName
        end
        return tile
    end

    function state:makeProxy(character, item, tile)
        local sprite = getSprite(tile.spriteName)
        if not sprite then return nil end
        local props = sprite:getProperties()
        local spriteType = sprite:getType()
        local proxy = nil

        if props and props:has(IsoFlagType.solidfloor) then
            proxy = ISWoodenFloor:new(tile.spriteName, tile.spriteName)
        elseif spriteType == IsoObjectType.doorN or
               spriteType == IsoObjectType.doorW then
            local openSprite = tile.openSpriteName or tile.spriteName
            proxy = ISWoodenDoor:new(
                tile.spriteName, tile.spriteName, openSprite, openSprite)
        elseif props and
               (props:has("DoorWallN") or props:has("DoorWallW")) then
            proxy = ISWoodenDoorFrame:new(
                tile.spriteName, tile.spriteName, nil)
        elseif props and
               (props:has("WallN") or props:has("WallW") or
                props:has("WallNW")) then
            proxy = ISWoodenWall:new(
                tile.spriteName, tile.spriteName, nil)
        else
            proxy = ISSimpleFurniture:new(
                item.name or "Free Build", tile.spriteName, tile.spriteName)
        end

        proxy.character = character
        proxy.player = character:getPlayerNum()
        proxy.noNeedHammer = true
        proxy.modData = {}
        return proxy
    end

    createBuildAction = function(
            character, x, y, z, north, spriteName, item)
        if state.enabled and item and item.Type == "ISBuildIsoEntity" then
            local tile = state:getProxyTile(item)
            if not tile then return nil end
            local proxy = state:makeProxy(character, item, tile)
            if not proxy then return nil end
            return state.originalCreateBuildAction(
                character,
                x + tile.x,
                y + tile.y,
                z + tile.z,
                north,
                tile.spriteName,
                proxy)
        end
        return state.originalCreateBuildAction(
            character, x, y, z, north, spriteName, item)
    end

    ISBuildIsoEntity.isValid = function(item, square)
        if state.enabled and not state:getProxyTile(item) then
            return false
        end
        return state.originalEntityIsValid(item, square)
    end

    function state:updateBuildNotice(button, logic)
        if button.pzFreeBuildWarningApplied then
            button:setTooltip(button.pzFreeBuildPreviousTooltip)
            button.pzFreeBuildPreviousTooltip = nil
            button.pzFreeBuildWarningApplied = false
        end

        local info = logic and logic:getSelectedBuildObject() or nil
        local shouldShow = self.enabled and info ~= nil
        local supported = shouldShow and
            self:isSingleStructureInfo(info) or false
        if button.pzFreeBuildServerNoticeApplied and
           (not shouldShow or
            button.pzFreeBuildServerNoticeSupported ~= supported) then
            button:setTooltip(button.pzFreeBuildServerNoticePreviousTooltip)
            button.pzFreeBuildServerNoticePreviousTooltip = nil
            button.pzFreeBuildServerNoticeApplied = false
            button.pzFreeBuildServerNoticeSupported = nil
        end

        if shouldShow and not button.pzFreeBuildServerNoticeApplied then
            button.pzFreeBuildServerNoticePreviousTooltip = button.tooltip
            local notice = supported and
                "免费建造支持：该建筑只占 1 格且只生成 1 个结构。" or
                "免费建造不支持：仅允许占地 1 格的单结构建筑；多格、多层、窗户或复合结构会产生实体幽灵，因此已禁止构建。"
            if button.tooltip and button.tooltip ~= "" then
                notice = button.tooltip .. "\n" .. notice
            end
            button:setTooltip(notice)
            button.pzFreeBuildServerNoticeApplied = true
            button.pzFreeBuildServerNoticeSupported = supported
        elseif not shouldShow and button.pzFreeBuildServerNoticeApplied then
            button:setTooltip(button.pzFreeBuildServerNoticePreviousTooltip)
            button.pzFreeBuildServerNoticePreviousTooltip = nil
            button.pzFreeBuildServerNoticeApplied = false
            button.pzFreeBuildServerNoticeSupported = nil
        end
    end

    state.syncMenu = function()
        if state.enabled then
            ISBuildMenu.cheat = true
        else
            local player = getPlayer()
            ISBuildMenu.cheat = player and player:isBuildCheat() or false
        end
    end
    Events.OnTick.Add(state.syncMenu)

    state.ensureBuildControlPatch = function()
        if state.buildControlPatched or not ISWidgetBuildControl or
           type(ISWidgetBuildControl.prerender) ~= "function" then
            return
        end
        state.originalBuildControlPrerender = ISWidgetBuildControl.prerender
        ISWidgetBuildControl.prerender = function(control)
            state.originalBuildControlPrerender(control)
            if control.buttonCraft and control.logic then
                state:updateBuildNotice(control.buttonCraft, control.logic)
                if state.enabled and
                   control.logic:getSelectedBuildObject() ~= nil then
                    local canBuild = state:isSingleStructureInfo(
                        control.logic:getSelectedBuildObject())
                    if control.logic:isCraftActionInProgress() then
                        canBuild = false
                    end
                    control.buttonCraft.enable = canBuild
                    control.buttonCraft:setVisible(true)
                end
            end
        end
        state.buildControlPatched = true
    end

    state.ensureBuildPanelPatch = function()
        if state.buildPanelPatched or not ISBuildPanel or
           type(ISBuildPanel.createBuildIsoEntity) ~= "function" then
            return
        end
        state.originalCreateBuildIsoEntity =
            ISBuildPanel.createBuildIsoEntity
        ISBuildPanel.createBuildIsoEntity = function(panel, dontSetDrag)
            state.originalCreateBuildIsoEntity(panel, dontSetDrag)
            if state.enabled and panel.buildEntity and
               panel.buildEntity.Type == "ISBuildIsoEntity" then
                panel.buildEntity.blockBuild =
                    not state:getProxyTile(panel.buildEntity)
            end
        end
        state.buildPanelPatched = true
    end
    Events.OnTick.Add(state.ensureBuildControlPatch)
    Events.OnTick.Add(state.ensureBuildPanelPatch)
    rawset(_G, "__PZTrainerFreeBuildState", state)
end

state.enabled = __PZTRAINER_FREE_BUILD_TARGET__
state.ensureBuildControlPatch()
state.ensureBuildPanelPatch()
state.syncMenu()
)lua";

enum class OperationPhase { Idle, Compile, Execute };

struct Bindings {
    bool ready = false;
    jclass lua_manager = nullptr;
    jclass lua_compiler = nullptr;
    jclass lua_return = nullptr;
    jclass object = nullptr;
    jfieldID lua_environment = nullptr;
    jfieldID lua_caller = nullptr;
    jfieldID lua_thread = nullptr;
    jmethodID return_is_success = nullptr;
    jmethodID return_get_error = nullptr;
    jmethodID return_get_stack = nullptr;
};

Bindings g_bindings;
FreeBuildStatus g_status;
bool g_requested = false;
bool g_applied = false;
bool g_target = false;
OperationPhase g_phase = OperationPhase::Idle;
AsyncObjectMethodCall g_call;
jobject g_compiled = nullptr;

bool ClearException(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}

jclass LoadGlobalClass(JNIEnv* env, const char* binary_name) {
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    if (class_loader == nullptr || ClearException(env)) return nullptr;
    const jmethodID get_system_loader = env->GetStaticMethodID(
        class_loader, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
    const jmethodID load_class = env->GetMethodID(
        class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jobject loader = get_system_loader == nullptr ? nullptr :
        env->CallStaticObjectMethod(class_loader, get_system_loader);
    env->DeleteLocalRef(class_loader);
    if (loader == nullptr || load_class == nullptr || ClearException(env)) {
        if (loader != nullptr) env->DeleteLocalRef(loader);
        return nullptr;
    }
    std::string dotted(binary_name);
    std::replace(dotted.begin(), dotted.end(), '/', '.');
    jstring name = env->NewStringUTF(dotted.c_str());
    jclass local = name == nullptr ? nullptr : static_cast<jclass>(
        env->CallObjectMethod(loader, load_class, name));
    if (name != nullptr) env->DeleteLocalRef(name);
    env->DeleteLocalRef(loader);
    if (local == nullptr || ClearException(env)) return nullptr;
    jclass global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

bool Initialize(JNIEnv* env) {
    if (g_bindings.ready) return true;
    g_bindings.lua_manager = LoadGlobalClass(env, "zombie/Lua/LuaManager");
    g_bindings.lua_compiler = LoadGlobalClass(
        env, "se/krka/kahlua/luaj/compiler/LuaCompiler");
    g_bindings.lua_return = LoadGlobalClass(
        env, "se/krka/kahlua/integration/LuaReturn");
    g_bindings.object = LoadGlobalClass(env, "java/lang/Object");
    if (g_bindings.lua_manager == nullptr || g_bindings.lua_compiler == nullptr ||
        g_bindings.lua_return == nullptr || g_bindings.object == nullptr) {
        return false;
    }
    g_bindings.lua_environment = env->GetStaticFieldID(
        g_bindings.lua_manager, "env", "Lse/krka/kahlua/vm/KahluaTable;");
    g_bindings.lua_caller = env->GetStaticFieldID(
        g_bindings.lua_manager, "caller",
        "Lse/krka/kahlua/integration/LuaCaller;");
    g_bindings.lua_thread = env->GetStaticFieldID(
        g_bindings.lua_manager, "thread", "Lse/krka/kahlua/vm/KahluaThread;");
    g_bindings.return_is_success = env->GetMethodID(
        g_bindings.lua_return, "isSuccess", "()Z");
    g_bindings.return_get_error = env->GetMethodID(
        g_bindings.lua_return, "getErrorString", "()Ljava/lang/String;");
    g_bindings.return_get_stack = env->GetMethodID(
        g_bindings.lua_return, "getLuaStackTrace", "()Ljava/lang/String;");
    g_bindings.ready = !ClearException(env) &&
        g_bindings.lua_environment != nullptr && g_bindings.lua_caller != nullptr &&
        g_bindings.lua_thread != nullptr &&
        g_bindings.return_is_success != nullptr &&
        g_bindings.return_get_error != nullptr &&
        g_bindings.return_get_stack != nullptr;
    return g_bindings.ready;
}

jstring JavaStringFromUtf8(JNIEnv* env, const std::string& value) {
    if (value.empty()) return env->NewString(nullptr, 0);
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) return nullptr;
    std::wstring utf16(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), utf16.data(), length) != length) {
        return nullptr;
    }
    return env->NewString(
        reinterpret_cast<const jchar*>(utf16.data()), length);
}

std::string JavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* text = env->GetStringUTFChars(value, nullptr);
    if (text == nullptr) {
        ClearException(env);
        return {};
    }
    std::string result(text);
    env->ReleaseStringUTFChars(value, text);
    return result;
}

void FinishOperation(JNIEnv* env) {
    ResetObjectMethodCall(env, g_call);
    if (g_compiled != nullptr) env->DeleteGlobalRef(g_compiled);
    g_compiled = nullptr;
    g_phase = OperationPhase::Idle;
    g_status.operation_pending = false;
}

bool QueueExecute(JNIEnv* env) {
    jobject caller = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_caller);
    jobject thread = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_thread);
    jobjectArray arguments = env->NewObjectArray(0, g_bindings.object, nullptr);
    const bool queued = caller != nullptr && thread != nullptr &&
        arguments != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, caller, "protectedCall",
            {thread, g_compiled, arguments}, g_call);
    if (arguments != nullptr) env->DeleteLocalRef(arguments);
    if (thread != nullptr) env->DeleteLocalRef(thread);
    if (caller != nullptr) env->DeleteLocalRef(caller);
    if (queued) g_phase = OperationPhase::Execute;
    return queued;
}

bool BeginOperation(JNIEnv* env, bool enabled) {
    std::string source(kFreeBuildLua);
    const std::string marker = "__PZTRAINER_FREE_BUILD_TARGET__";
    source.replace(source.find(marker), marker.size(), enabled ? "true" : "false");

    jobject environment = env->GetStaticObjectField(
        g_bindings.lua_manager, g_bindings.lua_environment);
    jstring java_source = JavaStringFromUtf8(env, source);
    std::string reader_error;
    jobject reader = java_source == nullptr ? nullptr :
        CreateLuaUtf8Reader(env, java_source, reader_error);
    jstring name = env->NewStringUTF("pztrainer_free_build.lua");
    const bool queued = environment != nullptr && reader != nullptr &&
        name != nullptr && !ClearException(env) &&
        QueueObjectMethodOnMainThread(
            env, g_bindings.lua_compiler, "loadis",
            {reader, name, environment}, g_call);
    if (name != nullptr) env->DeleteLocalRef(name);
    if (reader != nullptr) env->DeleteLocalRef(reader);
    if (java_source != nullptr) env->DeleteLocalRef(java_source);
    if (environment != nullptr) env->DeleteLocalRef(environment);
    if (!queued) {
        g_status.message = reader_error.empty()
            ? "无法提交免费建造脚本"
            : reader_error;
        return false;
    }
    g_target = enabled;
    g_phase = OperationPhase::Compile;
    g_status.operation_pending = true;
    g_status.message = enabled
        ? "正在安装单格免费建造转换"
        : "正在关闭单格免费建造转换";
    return true;
}

void PollOperation(JNIEnv* env) {
    if (g_phase == OperationPhase::Idle) return;
    jobject result = nullptr;
    std::string error;
    const AsyncObjectMethodState state = PollObjectMethodOnMainThread(
        env, g_call, std::chrono::seconds(8), &result, &error);
    if (state == AsyncObjectMethodState::Pending) return;
    if (state != AsyncObjectMethodState::Succeeded) {
        if (result != nullptr) env->DeleteLocalRef(result);
        g_status.message = error.empty()
            ? "免费建造脚本执行失败"
            : error;
        FinishOperation(env);
        return;
    }

    if (g_phase == OperationPhase::Compile) {
        if (result == nullptr) {
            g_status.message = "免费建造脚本编译失败";
            FinishOperation(env);
            return;
        }
        g_compiled = env->NewGlobalRef(result);
        env->DeleteLocalRef(result);
        ResetObjectMethodCall(env, g_call);
        if (g_compiled == nullptr || ClearException(env) || !QueueExecute(env)) {
            g_status.message = "无法在游戏主线程执行免费建造脚本";
            FinishOperation(env);
        }
        return;
    }

    const bool valid_return = result != nullptr &&
        env->IsInstanceOf(result, g_bindings.lua_return) == JNI_TRUE;
    const bool success = valid_return && env->CallBooleanMethod(
        result, g_bindings.return_is_success) == JNI_TRUE &&
        !ClearException(env);
    if (!success) {
        std::string detail;
        if (valid_return) {
            jstring message = static_cast<jstring>(env->CallObjectMethod(
                result, g_bindings.return_get_error));
            jstring stack = static_cast<jstring>(env->CallObjectMethod(
                result, g_bindings.return_get_stack));
            detail = JavaString(env, message);
            const std::string trace = JavaString(env, stack);
            if (message != nullptr) env->DeleteLocalRef(message);
            if (stack != nullptr) env->DeleteLocalRef(stack);
            if (!trace.empty()) detail += detail.empty() ? trace : "\n" + trace;
        }
        g_status.message = detail.empty()
            ? "免费建造脚本返回错误"
            : detail;
    } else {
        g_applied = g_target;
        g_status.applied = g_applied;
        g_status.message = g_applied
            ? "免费建造已启用：仅允许占地一格的单结构建筑"
            : "免费建造待命";
    }
    if (result != nullptr) env->DeleteLocalRef(result);
    FinishOperation(env);
}

}  // namespace

void UpdateFreeBuildBridge() {
    g_status.enabled = g_requested;
    JNIEnv* env = GetCurrentJniEnvironment();
    if (env == nullptr || !Initialize(env)) {
        g_status.initialized = false;
        g_status.message = "免费建造桥接尚未初始化";
        return;
    }
    g_status.initialized = true;
    PollOperation(env);
    if (g_phase == OperationPhase::Idle && g_applied != g_requested) {
        BeginOperation(env, g_requested);
    }
    g_status.applied = g_applied;
}

const FreeBuildStatus& GetFreeBuildStatus() {
    return g_status;
}

void SetFreeBuildEnabled(bool enabled) {
    g_requested = enabled;
    g_status.enabled = enabled;
}

}  // namespace pztrainer::bridge
