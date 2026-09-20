#pragma once

#include <string>

namespace pztrainer::ui::extensions {
void Toggle(const char* label, int flag);
void Action(const char* label, const char* action, const std::string& payload = {}, int radius = 10);
void Status();
bool BeginAvailable(const char* key);
void EndAvailable();
}  // namespace pztrainer::ui::extensions
