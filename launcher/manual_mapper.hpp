#pragma once

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace launcher {

bool ManualMapImage(
    DWORD process_id,
    const std::vector<std::uint8_t>& file_image,
    std::wstring& error);

}  // namespace launcher
