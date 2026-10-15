// Exercise the real binary reader/writer without invoking live game setters.
#include "../src/settings/configuration_store.cpp"
#include "configuration_bridge_stubs.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
int main(int argc,char** argv) {
    using namespace pztrainer::settings;
    const auto root=std::filesystem::temp_directory_path()/("pztrainer-config-upgrade-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto path=root/"fixture.pzcfg";
    ConfigurationPayload payload{}; payload.ui_scale_percent=134;
    payload.zombies.model_edge_glow=true; payload.character.no_clip=true;
    pztrainer::bridge::LuaScriptConfiguration lua; lua.path="example.lua";lua.auto_reload=true;
    pztrainer::bridge::LuaControlConfiguration control;control.label="new-feature-test";control.toggle=true;
    lua.controls.push_back(control);
    std::vector<pztrainer::bridge::LuaScriptConfiguration> scripts{lua};
    pztrainer::bridge::ExtensionOptions extensions{};
    std::array<std::uint8_t,4> viewpoint{1,0,0,1};
    pztrainer::features::visual::VisibilityOptions visibility{};visibility.only_visible[0][0]=1;
    std::string error;
    assert(WriteConfigurationFile(path,payload,scripts,extensions,viewpoint,1,visibility,error));
    StoredConfiguration current;assert(ReadAndUpgradeConfigurationFile(path,current,error));
    assert(current.source_version==21 && current.visibility.only_visible[0][0]==1);
    assert(current.lua_configuration.size()==1 && current.lua_configuration[0].controls[0].toggle);
    assert(current.payload.ui_scale_percent==134 && current.payload.character.no_clip);
    auto legacy=bytes(path);legacy.resize(legacy.size()-48); // v20 has no visibility trailer.
    const std::uint32_t version=20;std::memcpy(legacy.data()+8,&version,4);
    {std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(legacy.data(),legacy.size());}
    StoredConfiguration upgraded;assert(ReadAndUpgradeConfigurationFile(path,upgraded,error));
    assert(upgraded.source_version==20 && upgraded.visibility.only_visible==pztrainer::features::visual::VisibilityOptions{}.only_visible);
    assert(upgraded.payload.zombies.model_edge_glow && upgraded.payload.character.no_clip);
    assert(upgraded.fast_aiming_delay==1 && upgraded.viewpoint_flags==viewpoint);
    assert(bytes(std::filesystem::path(path.wstring()+L".v20.bak"))==legacy);
    StoredConfiguration reloaded;assert(ReadAndUpgradeConfigurationFile(path,reloaded,error));assert(reloaded.source_version==21);
    auto missing=bytes(path);missing.resize(missing.size()-44);
    const std::uint32_t zero=0;std::memcpy(missing.data()+missing.size()-4,&zero,4);
    {std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(missing.data(),missing.size());}
    StoredConfiguration missing_options;assert(ReadAndUpgradeConfigurationFile(path,missing_options,error));
    assert(missing_options.missing_visibility_options && bytes(path).size()==missing.size()+44);
    assert(missing_options.visibility.only_visible==pztrainer::features::visual::VisibilityOptions{}.only_visible);
    InitializeUiPreferences(root);
    const auto startup=ConfigurationDirectory()/"startup.pzcfg";
    std::filesystem::copy_file(path,startup);
    assert(SetDefaultConfiguration("startup",error));
    pztrainer::features::visual::GetVisualSettings().zombie_esp=false;
    LoadDefaultConfigurationOnce();
    assert(pztrainer::features::visual::GetVisualSettings().zombie_esp);
    assert(!DefaultConfigurationLoadMessage().empty());
    assert(pztrainer::bridge::applied_lua.size()==1 && pztrainer::bridge::applied_lua[0].controls[0].toggle);
    pztrainer::features::visual::GetVisualSettings().zombie_esp=false;
    LoadDefaultConfigurationOnce();assert(!pztrainer::features::visual::GetVisualSettings().zombie_esp);
    assert(DeleteConfiguration("startup",error));assert(DefaultConfigurationName().empty());
    const auto good=bytes(path);auto corrupt=good;corrupt.pop_back();
    {std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(corrupt.data(),corrupt.size());}
    StoredConfiguration invalid;assert(!ReadAndUpgradeConfigurationFile(path,invalid,error));assert(bytes(path)==corrupt);
    if(argc>1) {
        const auto fixture=root/"user-copy.pzcfg";std::filesystem::copy_file(std::filesystem::u8path(argv[1]),fixture);
        StoredConfiguration user; if(!ReadAndUpgradeConfigurationFile(fixture,user,error)){std::cerr<<error;return 1;}
        assert(user.payload.ui_scale_percent>=50);
        std::cout<<"Saved user configuration copy read/upgraded successfully\n";
    }
    std::cout<<"Production codec: v21 roundtrip, v20 upgrade, disabled new options, preserved Lua/features, exact backup, atomic replacement and truncated-file rejection passed\n";
}
