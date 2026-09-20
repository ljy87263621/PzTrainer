#include <Windows.h>
#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include "features/vehicle/bullet_vehicle_ghost.h"

int main(int argc, char** argv) {
    using namespace pztrainer::features::vehicle_ghost;
    assert(GhostFlags(0x100) == 0x106);
    assert(GhostFlags(0x106) == 0x106);
    std::array<std::byte, 32> invalid{};
    assert(!FindSetStaticCall(invalid));
    assert(!ValidHelper(invalid));
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<char> file{std::istreambuf_iterator<char>(input), {}};
    assert(file.size() > sizeof(IMAGE_DOS_HEADER));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    assert(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0);
    assert(static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) < file.size());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    assert(nt->Signature == IMAGE_NT_SIGNATURE && nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    std::vector<std::byte> image(nt->OptionalHeader.SizeOfImage);
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        assert(static_cast<size_t>(section[i].PointerToRawData) + section[i].SizeOfRawData <= file.size());
        assert(static_cast<size_t>(section[i].VirtualAddress) + section[i].SizeOfRawData <= image.size());
        std::memcpy(image.data() + section[i].VirtualAddress, file.data() + section[i].PointerToRawData, section[i].SizeOfRawData);
    }
    const DWORD export_rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(image.data() + export_rva);
    const auto* names = reinterpret_cast<const DWORD*>(image.data() + exports->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(image.data() + exports->AddressOfNameOrdinals);
    const auto* functions = reinterpret_cast<const DWORD*>(image.data() + exports->AddressOfFunctions);
    DWORD rva = 0;
    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        if (std::strcmp(reinterpret_cast<const char*>(image.data() + names[i]), "Java_zombie_core_physics_Bullet_setVehicleStatic") == 0)
            rva = functions[ordinals[i]];
    }
    assert(rva != 0 && rva + 0xc0 < image.size());
    std::span<const std::byte> code(image.data() + rva, 0xc0);
    const auto call = FindSetStaticCall(code);
    assert(call);
    const auto target = RelativeCallTarget(code, rva, *call);
    assert(target && *target + 0xc0 < image.size());
    assert(ValidHelper(std::span<const std::byte>(image.data() + *target, 0xc0)));
    auto altered = std::vector<std::byte>(code.begin(), code.end());
    altered[*call] = std::byte{0x90};
    assert(!FindSetStaticCall(altered));
    assert(!RelativeCallTarget(altered, rva, *call));
    std::cout << "Vehicle ghost: collision flags, incompatible signatures and local Bullet signature passed\n";
}
