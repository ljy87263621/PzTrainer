#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "bridge/recorded_media_catalog.hpp"
#include "bridge/item_catalog_search.hpp"
using namespace pztrainer::bridge;
#include "bridge/lua_ui_bridge.hpp"
#include "runtime/lua_bridge_resource.hpp"
namespace pztrainer::bridge {
jclass LoadEmbeddedJavaClass(JNIEnv* env,const char* name,std::string&) {
    std::string path(name);std::replace(path.begin(),path.end(),'.','/');return env->FindClass(path.c_str());
}
}
namespace pztrainer::runtime {
std::filesystem::path test_jar;
std::filesystem::path EnsurePlayerOverridesJar(std::string&) {return test_jar;}
}
int wmain(int argc,wchar_t** argv) {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    assert(argc==4); pztrainer::runtime::test_jar=argv[3]; const std::filesystem::path game(argv[1]),classes(argv[2]);
    SetDllDirectoryW((game/L"jre64/bin").c_str());
    HMODULE module=LoadLibraryW((game/L"jre64/bin/server/jvm.dll").c_str());assert(module);
    auto create=reinterpret_cast<jint(JNICALL*)(JavaVM**,void**,void*)>(GetProcAddress(module,"JNI_CreateJavaVM"));assert(create);
    std::string classpath="-Djava.class.path="+(game/L"projectzomboid.jar").u8string()+";"+classes.u8string();
    std::string home="-Duser.home="+(classes/L"profile").u8string();
    JavaVMOption options[]{{classpath.data(),nullptr},{home.data(),nullptr}};
    JavaVMInitArgs args{}; args.version=JNI_VERSION_1_8;args.nOptions=2;args.options=options;
    JavaVM* vm=nullptr;JNIEnv* env=nullptr;assert(create(&vm,reinterpret_cast<void**>(&env),&args)==JNI_OK);
    jclass fixture=env->FindClass("RecordedMediaItemsTest");assert(fixture);
    auto prepare=env->GetStaticMethodID(fixture,"prepare","()V");assert(prepare);
    env->CallStaticVoidMethod(fixture,prepare);
    if(env->ExceptionCheck()) { env->ExceptionDescribe();return 1; }
    std::vector<ItemCatalogEntry> catalog(1);catalog[0].full_type="Base.VHS_Retail";catalog[0].texture_id=17;
    std::string error;auto count=AppendRecordedMediaCatalog(env,catalog,error);
    std::cout<<"Native catalog appended "<<count<<"; "<<error<<std::endl;
    assert(error.empty() && count==3 && catalog.size()==4);
    bool found=false;
    for(const auto& entry:catalog) if(entry.full_type=="RecordedMedia:retail-test") {
        found=true;assert(entry.recorded_media_base_type=="Base.VHS_Retail");assert(entry.texture_id==17);assert(ItemCatalogMatchesSearch(entry,"Woodcraft"));
        assert(ItemCatalogMatchesSearch(entry,"录像带"));assert(ItemCatalogMatchesSearch(entry,"Episode 1"));
        assert(entry.spawn_method_mask==ItemSpawnMethodMask(ItemSpawnMethod::Game));
    }
    assert(found && AppendRecordedMediaCatalog(env,catalog,error)==0 && catalog.size()==4);
    assert(NeedsPreparedItemCreation("Base.VHS_Retail") && NeedsPreparedItemCreation("RecordedMedia:home-test"));
    assert(NeedsPreparedItemCreation("Moveables.location_business_office_generic_01_18"));
    assert(!NeedsPreparedItemCreation("Base.Axe"));
    assert(!env->ExceptionCheck());assert(vm->DestroyJavaVM()==JNI_OK);
    std::cout<<"Native catalog: real media registry, titles, aliases, template textures, deduplication passed.\n";
}
