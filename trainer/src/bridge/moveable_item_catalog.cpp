#include "bridge/moveable_item_catalog.hpp"
#include "bridge/item_catalog_search.hpp"
#include <unordered_set>

namespace pztrainer::bridge {
namespace {
struct Bindings {
    jclass manager=nullptr,moveable=nullptr,factory=nullptr;
    jfieldID instance=nullptr;
    jmethodID named_map=nullptr,keys=nullptr,to_array=nullptr,lookup=nullptr;
    jmethodID properties=nullptr,has=nullptr,get=nullptr,create=nullptr,read=nullptr;
    jmethodID display=nullptr,texture=nullptr;
    jmethodID id=nullptr,x0=nullptr,y0=nullptr,x1=nullptr,y1=nullptr,width=nullptr,height=nullptr;
};
Bindings g_bindings;
bool Clear(JNIEnv* env) { if(!env->ExceptionCheck())return false; env->ExceptionClear(); return true; }
jclass Load(JNIEnv* env,const char* name) {
    jclass loader=env->FindClass("java/lang/ClassLoader");
    if(!loader || Clear(env))return nullptr;
    auto get=env->GetStaticMethodID(loader,"getSystemClassLoader","()Ljava/lang/ClassLoader;");
    auto load=env->GetMethodID(loader,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
    jobject object=get?env->CallStaticObjectMethod(loader,get):nullptr;
    jstring text=env->NewStringUTF(name);
    jclass local=object && load && text?static_cast<jclass>(env->CallObjectMethod(object,load,text)):nullptr;
    jclass result=local && !Clear(env)?static_cast<jclass>(env->NewGlobalRef(local)):nullptr;
    if(local)env->DeleteLocalRef(local); if(text)env->DeleteLocalRef(text);
    if(object)env->DeleteLocalRef(object); env->DeleteLocalRef(loader); Clear(env);
    return result;
}
bool Initialize(JNIEnv* env) {
    if(g_bindings.manager)return true;
    if(env->PushLocalFrame(16)!=JNI_OK) { Clear(env);return false; }
    Bindings b;
    b.manager=Load(env,"zombie.iso.sprite.IsoSpriteManager");
    b.moveable=Load(env,"zombie.inventory.types.Moveable");
    b.factory=Load(env,"zombie.inventory.InventoryItemFactory");
    jclass map=env->FindClass("java/util/Map"), collection=env->FindClass("java/util/Collection");
    jclass sprite=Load(env,"zombie.iso.sprite.IsoSprite"), props=Load(env,"zombie.core.properties.PropertyContainer");
    jclass inventory=Load(env,"zombie.inventory.InventoryItem"), texture=Load(env,"zombie.core.textures.Texture");
    bool ready=b.manager && b.moveable && b.factory && sprite && props && inventory && texture && map && collection && !Clear(env);
    if(ready) {
        b.instance=env->GetStaticFieldID(b.manager,"instance","Lzombie/iso/sprite/IsoSpriteManager;");
        b.named_map=env->GetMethodID(b.manager,"getNamedMap","()Ljava/util/Map;");
        b.keys=env->GetMethodID(map,"keySet","()Ljava/util/Set;");
        b.to_array=env->GetMethodID(collection,"toArray","()[Ljava/lang/Object;");
        b.lookup=env->GetMethodID(map,"get","(Ljava/lang/Object;)Ljava/lang/Object;");
        b.properties=env->GetMethodID(sprite,"getProperties","()Lzombie/core/properties/PropertyContainer;");
        b.has=env->GetMethodID(props,"has","(Ljava/lang/String;)Z");
        b.get=env->GetMethodID(props,"get","(Ljava/lang/String;)Ljava/lang/String;");
        b.create=env->GetStaticMethodID(b.factory,"CreateItem","(Ljava/lang/String;)Lzombie/inventory/InventoryItem;");
        b.read=env->GetMethodID(b.moveable,"ReadFromWorldSprite","(Ljava/lang/String;)Z");
        b.display=env->GetMethodID(inventory,"getDisplayName","()Ljava/lang/String;");
        b.texture=env->GetMethodID(inventory,"getTex","()Lzombie/core/textures/Texture;");
        b.id=env->GetMethodID(texture,"getID","()I");
        b.x0=env->GetMethodID(texture,"getXStart","()F");b.y0=env->GetMethodID(texture,"getYStart","()F");
        b.x1=env->GetMethodID(texture,"getXEnd","()F");b.y1=env->GetMethodID(texture,"getYEnd","()F");
        b.width=env->GetMethodID(texture,"getWidth","()I");b.height=env->GetMethodID(texture,"getHeight","()I");
        ready=!Clear(env) && b.instance && b.named_map && b.keys && b.to_array && b.lookup && b.properties && b.has && b.get && b.create && b.read && b.display && b.texture && b.id && b.x0 && b.y0 && b.x1 && b.y1 && b.width && b.height;
    }
    for(jclass type:{sprite,props,inventory,texture})if(type)env->DeleteGlobalRef(type);
    if(ready)g_bindings=b;
    else for(jclass type:{b.manager,b.moveable,b.factory})if(type)env->DeleteGlobalRef(type);
    env->PopLocalFrame(nullptr); Clear(env); return ready;
}
std::string Text(JNIEnv* env,jstring value) {
    if(!value)return {};
    const char* bytes=env->GetStringUTFChars(value,nullptr);
    if(!bytes || Clear(env))return {};
    std::string result(bytes);env->ReleaseStringUTFChars(value,bytes);return result;
}
std::string Property(JNIEnv* env,jobject props,const char* name) {
    jstring key=env->NewStringUTF(name);
    jstring value=key?static_cast<jstring>(env->CallObjectMethod(props,g_bindings.get,key)):nullptr;
    return Clear(env)?std::string{}:Text(env,value);
}
}

std::size_t AppendMoveableItemCatalog(JNIEnv* env,std::vector<ItemCatalogEntry>& entries,std::string& error) {
    error.clear();
    if(!env || !Initialize(env)) { error="可移动家具接口尚未就绪";return 0; }
    if(env->PushLocalFrame(8)!=JNI_OK) { Clear(env);error="无法读取可移动家具目录";return 0; }
    jobject manager=env->GetStaticObjectField(g_bindings.manager,g_bindings.instance);
    jobject map=manager?env->CallObjectMethod(manager,g_bindings.named_map):nullptr;
    jobject values=map?env->CallObjectMethod(map,g_bindings.keys):nullptr;
    jobjectArray sprites=values?static_cast<jobjectArray>(env->CallObjectMethod(values,g_bindings.to_array)):nullptr;
    if(Clear(env) || !sprites) { env->PopLocalFrame(nullptr);error="读取家具图块列表失败";return 0; }
    std::unordered_set<std::string> types;
    for(const auto& entry:entries)types.insert(entry.full_type);
    const auto before=entries.size();
    const jsize count=env->GetArrayLength(sprites);
    if(count==0)error="家具图块尚未加载，可进入存档后刷新列表";
    for(jsize index=0;index<count;index++) {
        if(env->PushLocalFrame(20)!=JNI_OK) { Clear(env);error="家具目录扫描中断";break; }
        jstring name=static_cast<jstring>(env->GetObjectArrayElement(sprites,index));
        jobject sprite=name?env->CallObjectMethod(map,g_bindings.lookup,name):nullptr;
        jobject props=sprite?env->CallObjectMethod(sprite,g_bindings.properties):nullptr;
        jstring flag=env->NewStringUTF("IsMoveAble");
        bool eligible=props && flag && env->CallBooleanMethod(props,g_bindings.has,flag)==JNI_TRUE;
        if(Clear(env) || !eligible) { env->PopLocalFrame(nullptr);continue; }
        const std::string sprite_name=Text(env,name);
        if(Clear(env) || sprite_name.empty() || sprite_name.find('.')!=std::string::npos) { env->PopLocalFrame(nullptr);continue; }
        ItemCatalogEntry entry;
        entry.full_type="Moveables."+sprite_name;
        if(types.find(entry.full_type)!=types.end()) { env->PopLocalFrame(nullptr);continue; }
        jstring full_type=env->NewStringUTF(entry.full_type.c_str());
        jobject sample=full_type?env->CallStaticObjectMethod(g_bindings.factory,g_bindings.create,full_type):nullptr;
        bool valid=sample && !Clear(env) && env->IsInstanceOf(sample,g_bindings.moveable)==JNI_TRUE &&
            env->CallBooleanMethod(sample,g_bindings.read,name)==JNI_TRUE;
        if(Clear(env) || !valid) { env->PopLocalFrame(nullptr);continue; }
        jstring display=static_cast<jstring>(env->CallObjectMethod(sample,g_bindings.display));
        entry.display_name=Text(env,display);
        if(Clear(env) || entry.display_name.empty()) { env->PopLocalFrame(nullptr);continue; }
        entry.category="Furniture";
        entry.search_terms=MoveableSearchTerms(Property(env,props,"GroupName")+" "+Property(env,props,"CustomName"),entry.display_name);
        const std::string facing=Property(env,props,"Facing");
        if(!facing.empty())entry.display_name+=" ["+facing+"]";
        entry.spawn_method_mask=PreparedItemSpawnMethods(entry.full_type);
        jobject texture=env->CallObjectMethod(sample,g_bindings.texture);
        if(texture && !Clear(env)) {
            const jint id=env->CallIntMethod(texture,g_bindings.id);
            entry.texture_id=id>0?static_cast<unsigned int>(id):0;
            entry.uv_min_x=env->CallFloatMethod(texture,g_bindings.x0);entry.uv_min_y=env->CallFloatMethod(texture,g_bindings.y0);
            entry.uv_max_x=env->CallFloatMethod(texture,g_bindings.x1);entry.uv_max_y=env->CallFloatMethod(texture,g_bindings.y1);
            entry.texture_width=env->CallIntMethod(texture,g_bindings.width);entry.texture_height=env->CallIntMethod(texture,g_bindings.height);
        }
        Clear(env); types.insert(entry.full_type); entries.push_back(std::move(entry));
        env->PopLocalFrame(nullptr);
    }
    env->PopLocalFrame(nullptr);return entries.size()-before;
}
}
