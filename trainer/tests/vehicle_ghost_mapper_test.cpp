#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "manual_mapper.hpp"

// A mapped DLL must never use the host executable's static TLS slot.
thread_local unsigned char hostTlsSentinel[4096];
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    std::memset(hostTlsSentinel, 0xcd, sizeof(hostTlsSentinel));
    wchar_t name[80];
    swprintf_s(name, L"Local\\PzVehicleGhostTest-%lu", GetCurrentProcessId());
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(std::uintptr_t), name);
    if (!mapping) return 3;
    auto* address = static_cast<std::uintptr_t*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(std::uintptr_t)));
    if (!address) return 4;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input), {}};
    std::wstring error;
    if (!launcher::ManualMapImage(GetCurrentProcessId(), image, error)) {
        std::fwprintf(stderr, L"Manual map failed: %ls\n", error.c_str()); return 5;
    }
    auto run = reinterpret_cast<LPTHREAD_START_ROUTINE>(*address);
    UnmapViewOfFile(address); CloseHandle(mapping);
    if (!run) return 6;
    DWORD result = run(nullptr); // Existing host thread, as in the game's MainThread.
    if (result) { std::fprintf(stderr, "Callback regression failed at line %lu\n", result); return 7; }
    for (unsigned char byte : hostTlsSentinel) if (byte != 0xcd) return 8;
    HANDLE thread = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    if (!thread || WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) return 9;
    GetExitCodeThread(thread, &result); CloseHandle(thread);
    if (result) { std::fprintf(stderr, "New thread regression failed at line %lu\n", result); return 10; }
    std::puts("Manual-map vehicle callbacks passed on existing/new threads; host TLS intact, foreign callbacks isolated, enter/restore/cancel passed.");
    return 0;
}
