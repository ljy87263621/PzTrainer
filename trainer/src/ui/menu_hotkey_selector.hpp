#pragma once

#include <cstdint>

namespace pztrainer::ui {

bool DrawMenuHotkeySelector(int current_virtual_key,
                            int* selected_virtual_key);
bool HandleMenuHotkeyCaptureMessage(unsigned int message,
                                    std::uintptr_t wparam);

}  // namespace pztrainer::ui
