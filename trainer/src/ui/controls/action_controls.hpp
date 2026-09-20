#pragma once

#include <cstddef>

namespace pztrainer::ui::controls {

bool ActionButton(const char* label, bool selected = false);
bool ChoiceRow(const char* label);
bool TextField(const char* label, char* value, std::size_t capacity,
               const char* hint = "");
bool IntegerRow(const char* label, int* value, int minimum, int maximum);
bool IntegerField(const char* id, int* value, int minimum, int maximum, float width);
void Hint(const char* text);
void ScrollRail();

}  // namespace pztrainer::ui::controls
