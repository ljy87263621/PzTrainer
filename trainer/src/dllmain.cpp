#include <Windows.h>

#include "runtime/overlay_runtime.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        pztrainer::runtime::Start(module);
    }
    return TRUE;
}
