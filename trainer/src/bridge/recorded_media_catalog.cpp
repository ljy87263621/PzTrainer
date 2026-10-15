#include "bridge/recorded_media_catalog.hpp"
#include "bridge/lua_ui_bridge.hpp"
#include "runtime/lua_bridge_resource.hpp"
#include <jvmti.h>
#include <unordered_map>
#include <unordered_set>

namespace pztrainer::bridge {
namespace {
jclass g_helper=nullptr;
bool Clear(JNIEnv* env) {
    if(!env->ExceptionCheck()) return false;
    env->ExceptionClear(); return true;
}
std::string Text(JNIEnv* env,jstring value) {
    if(!value) return {};
    const char* chars=env->GetStringUTFChars(value,nullptr);
    if(!chars) return {};
    std::string result(chars); env->ReleaseStringUTFChars(value,chars); return result;
}
}
jclass RecordedMediaItemHelper(JNIEnv* env,std::string& error) {
    if(g_helper) return g_helper;
    JavaVM* vm=nullptr; jvmtiEnv* tool=nullptr;
    if(!env || env->GetJavaVM(&vm)!=JNI_OK ||
        vm->GetEnv(reinterpret_cast<void**>(&tool),JVMTI_VERSION_1_2)!=JNI_OK) {
        error="录像带接口尚未就绪"; return nullptr;
    }
    const auto jar=runtime::EnsurePlayerOverridesJar(error);
    const bool appended=!jar.empty() && tool->AddToSystemClassLoaderSearch(jar.u8string().c_str())==JVMTI_ERROR_NONE;
    tool->DisposeEnvironment();
    if(!appended) { error="无法加载录像带创建接口";return nullptr; }
    jclass local=LoadEmbeddedJavaClass(env,"pztrainer.player.items.RecordedMediaItems",error);
    if(local) { g_helper=static_cast<jclass>(env->NewGlobalRef(local));env->DeleteLocalRef(local); }
    if(Clear(env)) { g_helper=nullptr;error="无法绑定录像带创建接口"; }
    return g_helper;
}
std::size_t AppendRecordedMediaCatalog(JNIEnv* env,std::vector<ItemCatalogEntry>& entries,std::string& error) {
    error.clear();
    jclass helper=RecordedMediaItemHelper(env,error);
    if(!helper) return 0;
    if(env->PushLocalFrame(16)!=JNI_OK) { Clear(env);error="无法扫描录像带目录";return 0; }
    auto method=env->GetStaticMethodID(helper,"catalog","()[Ljava/lang/String;");
    auto rows=method?static_cast<jobjectArray>(env->CallStaticObjectMethod(helper,method)):nullptr;
    if(Clear(env) || !rows) { env->PopLocalFrame(nullptr);error="读取录像带节目目录失败";return 0; }
    const jsize length=env->GetArrayLength(rows);
    if(length==0) error="录像带节目尚未加载，可进入存档后刷新列表";
    if(length%5!=0) {env->PopLocalFrame(nullptr);error="录像带目录格式无效";return 0;}
    std::unordered_map<std::string,ItemCatalogEntry> templates;
    std::unordered_set<std::string> keys;
    for(const auto& entry:entries) {templates.emplace(entry.full_type,entry);keys.insert(entry.full_type);}
    const auto before=entries.size();
    for(jsize i=0;i<length;i+=5) {
        std::string fields[5];
        for(jsize j=0;j<5;++j) {
            auto value=static_cast<jstring>(env->GetObjectArrayElement(rows,i+j));
            fields[j]=Text(env,value); if(value) env->DeleteLocalRef(value);
        }
        if(Clear(env)) {error="录像带目录扫描中断";break;}
        if(fields[0].empty() || !keys.insert(fields[0]).second) continue;
        ItemCatalogEntry entry;
        auto sample=templates.find(fields[4]);
        if(sample!=templates.end()) entry=sample->second;
        entry.full_type=fields[0];entry.display_name=fields[1];entry.category=fields[2];entry.search_terms=fields[3];
        entry.spawn_method_mask=ItemSpawnMethodMask(ItemSpawnMethod::Game);
        entries.push_back(std::move(entry));
    }
    env->PopLocalFrame(nullptr); return entries.size()-before;
}
}
