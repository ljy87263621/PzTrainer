#pragma once

#include <string>
#include <vector>

namespace pztrainer::settings {

struct ConfigurationInfo {
    std::string name;
};

std::vector<ConfigurationInfo> ListConfigurations();
bool SaveConfiguration(const std::string& name, std::string& error);
bool LoadConfiguration(const std::string& name, std::string& error);
bool DeleteConfiguration(const std::string& name, std::string& error);

}  // namespace pztrainer::settings
