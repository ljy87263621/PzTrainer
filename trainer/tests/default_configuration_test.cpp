#include "settings/ui_preferences.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <Windows.h>
namespace pztrainer::settings {
Language testLanguage=Language::Chinese;
Language GetLanguage(){return testLanguage;}
void SetLanguage(Language value){testLanguage=value;}
}
int main(int argc,char** argv) {
    using namespace pztrainer::settings;
    const auto root=std::filesystem::temp_directory_path()/"pztrainer-default-config-regression";
    InitializeUiPreferences(root);
    if(argc>1) { assert(DefaultConfigurationName()=="222"); std::cout<<"Reload passed\n"; return 0; }
    std::string error;
    assert(SetDefaultConfiguration("",error));
    assert(!SetDefaultConfiguration("../outside",error));
    assert(!SetDefaultConfiguration("missing",error));
    std::ofstream(ConfigurationDirectory()/"111.pzcfg").put('1');
    std::ofstream(ConfigurationDirectory()/"222.pzcfg").put('2');
    assert(SetDefaultConfiguration("111",error)); assert(DefaultConfigurationName()=="111");
    assert(SetDefaultConfiguration("",error)); assert(DefaultConfigurationName().empty());
    assert(SetDefaultConfiguration("222",error)); assert(DefaultConfigurationName()=="222");
    std::cout<<"Default selection, cancellation, invalid/missing rejection and preference save passed\n";
}
