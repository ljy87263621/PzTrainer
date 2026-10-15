#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <iostream>
#include "bridge/moveable_item_catalog.hpp"
#include "bridge/item_catalog_search.hpp"
using namespace pztrainer::bridge;
int wmain(int argc,wchar_t** argv) {
    _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    assert(argc==3); const std::filesystem::path game(argv[1]),classes(argv[2]);
    SetDllDirectoryW((game/L"jre64/bin").c_str());
    HMODULE module=LoadLibraryW((game/L"jre64/bin/server/jvm.dll").c_str());assert(module);
    auto create=reinterpret_cast<jint(JNICALL*)(JavaVM**,void**,void*)>(GetProcAddress(module,"JNI_CreateJavaVM"));assert(create);
    std::string classpath="-Djava.class.path="+(game/L"projectzomboid.jar").u8string()+";"+classes.u8string();
    std::string home="-Duser.home="+(classes/L"profile").u8string();
    JavaVMOption options[]{{classpath.data(),nullptr},{home.data(),nullptr}};
    JavaVMInitArgs args{}; args.version=JNI_VERSION_1_8;args.nOptions=2;args.options=options;
    JavaVM* vm=nullptr;JNIEnv* env=nullptr;assert(create(&vm,reinterpret_cast<void**>(&env),&args)==JNI_OK);
    jclass fixture=env->FindClass("MoveableCatalogFixture");assert(fixture);
    auto prepare=env->GetStaticMethodID(fixture,"prepare","()V");assert(prepare);
    env->CallStaticVoidMethod(fixture,prepare);
    if(env->ExceptionCheck()) { env->ExceptionDescribe();return 1; }
    std::vector<ItemCatalogEntry> catalog(1);catalog[0].full_type="Base.Axe";
    std::string error; auto count=AppendMoveableItemCatalog(env,catalog,error);
    std::cout<<"Collector appended "<<count<<"; "<<error<<std::endl;
    assert(error.empty() && count==2 && catalog.size()==3);
    bool found=false;
    for(const auto& entry:catalog) if(entry.full_type=="Moveables.location_business_office_generic_01_18") {
        found=true;assert(entry.category=="Furniture");assert(ItemCatalogMatchesSearch(entry,"隔板"));
        assert(entry.display_name.find("[E]")!=std::string::npos);
        assert(entry.spawn_method_mask==ItemSpawnMethodMask(ItemSpawnMethod::Game));
    }
    assert(found && AppendMoveableItemCatalog(env,catalog,error)==0 && catalog.size()==3);
    auto load=env->GetStaticMethodID(fixture,"loadActualTiles","(Ljava/lang/String;)V");assert(load);
    std::string tile_file=(game/L"media/newtiledefinitions.tiles").u8string();
    jstring file=env->NewStringUTF(tile_file.c_str());env->CallStaticVoidMethod(fixture,load,file);
    if(env->ExceptionCheck()) { env->ExceptionDescribe();return 1; }
    catalog.clear();count=AppendMoveableItemCatalog(env,catalog,error);
    std::size_t dividers=0;for(const auto& entry:catalog) if(ItemCatalogMatchesSearch(entry,"隔板"))++dividers;
    std::cout<<"Actual base tile data: "<<count<<" furniture entries, "<<dividers<<" divider entries; "<<error<<std::endl;
    assert(error.empty() && count>1000 && dividers>=12);
    assert(!env->ExceptionCheck());assert(vm->DestroyJavaVM()==JNI_OK);
    std::cout<<"Actual JVM: furniture factory, catalogue, aliases, ordinary items, non-moveable exclusion and deduplication passed.\n";
}
