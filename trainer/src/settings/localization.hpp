#pragma once

#include <cstdint>

namespace pztrainer::settings {

enum class Language : std::uint32_t {
    Chinese,
    English,
    Russian,
    German,
    Count,
};

Language GetLanguage();
void SetLanguage(Language language);
const char* LanguageName(Language language);
const char* Translate(const char* chinese_source);

}  // namespace pztrainer::settings
