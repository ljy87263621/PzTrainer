// Probe DLL: exercise the production callback after the launcher's real manual map.
#include <cstdio>
#include <cstring>
#include "../src/features/vehicle/bullet_vehicle_ghost.cpp"

extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID, LPVOID, LPVOID*) { return MH_ERROR_UNSUPPORTED_FUNCTION; }
extern "C" MH_STATUS WINAPI MH_EnableHook(LPVOID) { return MH_OK; }
extern "C" MH_STATUS WINAPI MH_DisableHook(LPVOID) { return MH_OK; }
extern "C" MH_STATUS WINAPI MH_RemoveHook(LPVOID) { return MH_OK; }

namespace {
struct Fixture {
    alignas(16) std::byte vehicle[0xc8]{};
    alignas(16) std::byte body[0x1a8]{};
    Fixture() {
        void* pointer = body;
        std::memcpy(vehicle + 0x10, &pointer, sizeof(pointer));
        SetFlags(0x100);
    }
    void SetFlags(std::uint32_t flags) { std::memcpy(body + 0x1a0, &flags, sizeof(flags)); }
    std::uint32_t Flags() const { std::uint32_t flags; std::memcpy(&flags, body + 0x1a0, sizeof(flags)); return flags; }
};
void Original(void* vehicle, bool isStatic) {
    Body body{};
    if (!ReadBody(vehicle, body)) return;
    (void)Write(static_cast<std::byte*>(vehicle) + 0xc0, static_cast<std::uint8_t>(isStatic));
    (void)Write(static_cast<std::byte*>(body.pointer) + 0x1a0,
          isStatic ? body.flags | KinematicObject : body.flags & ~KinematicObject);
}
DWORD WINAPI ForeignThread(void*) {
    Fixture fixture;
    HookedSetStatic(fixture.vehicle, true);
    if (fixture.Flags() != 0x102 || Finish().observed || Begin(Operation::Enter)) return 1;
    Cancel(); // Must not cancel the other thread's pending request.
    return 0;
}
}

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)
extern "C" __declspec(dllexport) DWORD WINAPI RunVehicleGhostTest(void*) {
    g_original.store(&Original);
    g_enabled.store(true);
    g_acceptCallbacks.store(true);
    Fixture fixture;
    // The reported crash was an ordinary game callback with no pending request.
    HookedSetStatic(fixture.vehicle, true);
    CHECK(fixture.Flags() == 0x102 && !Finish().observed);
    HookedSetStatic(fixture.vehicle, false);
    CHECK(fixture.Flags() == 0x100);
    CHECK(Begin(Operation::Enter));
    CHECK(!Begin(Operation::Enter));
    HANDLE thread = CreateThread(nullptr, 0, &ForeignThread, nullptr, 0, nullptr);
    CHECK(thread != nullptr);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    DWORD result = 1;
    GetExitCodeThread(thread, &result); CloseHandle(thread);
    CHECK(result == 0);
    HookedSetStatic(fixture.vehicle, true);
    const Result entered = Finish();
    CHECK(entered.observed && entered.applied && entered.originalFlags == 0x100);
    CHECK(fixture.Flags() == 0x106);
    CHECK(Begin(Operation::Restore, entered.originalFlags));
    HookedSetStatic(fixture.vehicle, false);
    CHECK(Finish().applied && fixture.Flags() == 0x100);
    CHECK(Begin(Operation::Enter)); Cancel();
    CHECK(!Finish().observed);
    HookedSetStatic(fixture.vehicle, true);
    CHECK(fixture.Flags() == 0x102);
    CHECK(Begin(Operation::Enter));
    CHECK(!Finish().observed); // No native callback observed.
    CHECK(Cleanup());
    CHECK(!Begin(Operation::Enter));
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    wchar_t name[80];
    swprintf_s(name, L"Local\\PzVehicleGhostTest-%lu", GetCurrentProcessId());
    HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, name);
    if (!mapping) return FALSE;
    auto* address = static_cast<std::uintptr_t*>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(std::uintptr_t)));
    if (address) *address = reinterpret_cast<std::uintptr_t>(&RunVehicleGhostTest);
    if (address) UnmapViewOfFile(address);
    CloseHandle(mapping);
    return address != nullptr;
}
