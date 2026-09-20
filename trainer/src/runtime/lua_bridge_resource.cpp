#include "runtime/lua_bridge_resource.hpp"

#include "vmprotect.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <cstdint>
#include <limits>
#include <vector>

#include "runtime/resource.h"

namespace pztrainer::runtime {
namespace {

HMODULE g_resource_module = nullptr;

constexpr std::size_t kSha256Size = 32;
using Sha256Digest = std::array<unsigned char, kSha256Size>;

class Sha256Hasher {
public:
    ~Sha256Hasher() {
        if (hash_ != nullptr) BCryptDestroyHash(hash_);
        if (algorithm_ != nullptr) BCryptCloseAlgorithmProvider(algorithm_, 0);
    }

    bool Initialize() {
        if (BCryptOpenAlgorithmProvider(
                &algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
            return false;
        }
        DWORD object_size = 0;
        DWORD copied = 0;
        if (BCryptGetProperty(
                algorithm_, BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                &copied, 0) < 0 || object_size == 0) {
            return false;
        }
        object_.resize(object_size);
        return BCryptCreateHash(
            algorithm_, &hash_, object_.data(),
            static_cast<ULONG>(object_.size()), nullptr, 0, 0) >= 0;
    }

    bool Update(const void* bytes, std::size_t size) {
        const auto* cursor = static_cast<const unsigned char*>(bytes);
        while (size > 0) {
            const ULONG chunk = static_cast<ULONG>(std::min<std::size_t>(
                size, std::numeric_limits<ULONG>::max()));
            if (BCryptHashData(
                    hash_, const_cast<PUCHAR>(cursor), chunk, 0) < 0) {
                return false;
            }
            cursor += chunk;
            size -= chunk;
        }
        return true;
    }

    bool Finish(Sha256Digest& digest) {
        return BCryptFinishHash(
            hash_, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    }

private:
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    std::vector<unsigned char> object_;
};

bool HashPayload(
    const void* bytes,
    std::size_t size,
    Sha256Digest& digest) {
    Sha256Hasher hasher;
    return hasher.Initialize() && hasher.Update(bytes, size) &&
        hasher.Finish(digest);
}

bool HashFile(
    const std::filesystem::path& path,
    Sha256Digest& digest) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    Sha256Hasher hasher;
    if (!hasher.Initialize()) return false;
    std::array<char, 65536> buffer{};
    while (input) {
        input.read(
            buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 && !hasher.Update(
                buffer.data(), static_cast<std::size_t>(count))) {
            return false;
        }
    }
    return input.eof() && hasher.Finish(digest);
}

std::string DigestText(const Sha256Digest& digest) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(digest.size() * 2, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2] = kHex[digest[index] >> 4];
        result[index * 2 + 1] = kHex[digest[index] & 0x0F];
    }
    return result;
}

bool ReadResource(
    HMODULE module,
    int resource_id,
    const void*& bytes,
    DWORD& size,
    std::string& error) {
    if (module == nullptr) {
        error = "The trainer module address is unavailable.";
        return false;
    }
    HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(resource_id), MAKEINTRESOURCEW(10));
    if (resource == nullptr) {
        error = "Embedded Lua bridge resource is missing.";
        return false;
    }
    HGLOBAL loaded = LoadResource(module, resource);
    bytes = loaded == nullptr ? nullptr : LockResource(loaded);
    size = SizeofResource(module, resource);
    if (bytes == nullptr || size == 0) {
        error = "Embedded Lua bridge resource is empty.";
        return false;
    }
    return true;
}

std::filesystem::path EnsureBridgeJar(int resource_id, const wchar_t* filename, std::string& error) {
    PZ_VMP_BEGIN_ULTRA("PZ.DLL.LuaBridgeCache");
    const void* bytes = nullptr;
    DWORD size = 0;
    if (!ReadResource(g_resource_module, resource_id, bytes, size, error)) return {};

    wchar_t temporary_root[MAX_PATH]{};
    const DWORD temporary_length = GetTempPathW(MAX_PATH, temporary_root);
    if (temporary_length == 0 || temporary_length >= MAX_PATH) {
        error = "Unable to resolve the system temporary directory.";
        return {};
    }

    Sha256Digest expected_digest{};
    if (!HashPayload(bytes, size, expected_digest)) {
        error = "Unable to calculate the embedded Lua bridge SHA-256.";
        return {};
    }
    const std::string hash = DigestText(expected_digest);
    const std::filesystem::path directory =
        std::filesystem::path(temporary_root) / L"PZSoloAssist" / L"cache" /
        std::filesystem::path(hash);
    const std::filesystem::path target = directory / filename;
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        error = "Unable to create the Lua bridge cache directory.";
        return {};
    }
    if (std::filesystem::exists(target, filesystem_error) && !filesystem_error) {
        Sha256Digest cached_digest{};
        if (HashFile(target, cached_digest) &&
            cached_digest == expected_digest) {
            return target;
        }
    }
    filesystem_error.clear();
    const std::filesystem::path temporary = target.wstring() + L"." +
        std::to_wstring(GetCurrentProcessId()) + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Unable to write the Lua bridge cache file.";
            return {};
        }
        output.write(static_cast<const char*>(bytes), size);
        if (!output) {
            error = "Unable to write the complete Lua bridge cache file.";
            std::filesystem::remove(temporary, filesystem_error);
            return {};
        }
    }

    Sha256Digest written_digest{};
    if (!HashFile(temporary, written_digest) ||
        written_digest != expected_digest) {
        error = "The temporary Lua bridge failed SHA-256 verification.";
        std::filesystem::remove(temporary, filesystem_error);
        return {};
    }
    if (!MoveFileExW(
            temporary.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Sha256Digest concurrent_digest{};
        if (HashFile(target, concurrent_digest) &&
            concurrent_digest == expected_digest) {
            std::filesystem::remove(temporary, filesystem_error);
            return target;
        }
        error = "Unable to commit the Lua bridge cache file.";
        std::filesystem::remove(temporary, filesystem_error);
        return {};
    }
    PZ_VMP_END();
    return target;
}

}  // namespace

void SetLuaBridgeResourceModule(HMODULE module) {
    g_resource_module = module;
}

std::filesystem::path EnsureLuaBridgeJar(std::string& error) {
    return EnsureBridgeJar(IDR_PZSA_LUA_BRIDGE, L"pztrainer-lua-bridge.jar", error);
}

std::filesystem::path EnsurePlayerOverridesJar(std::string& error) {
    return EnsureBridgeJar(IDR_PZSA_PLAYER_OVERRIDES, L"pztrainer-player-overrides.jar", error);
}

}  // namespace pztrainer::runtime
