#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace pztrainer::features::vehicle_ghost
{
inline constexpr std::uint32_t KinematicObject = 0x2;
inline constexpr std::uint32_t NoContactResponse = 0x4;

[[nodiscard]] constexpr std::uint32_t GhostFlags(std::uint32_t original) noexcept { return original | KinematicObject | NoContactResponse; }

template <std::size_t Size> [[nodiscard]] constexpr bool Contains(std::span<const std::byte> bytes, const std::array<std::byte, Size>& pattern) noexcept
{
	if (bytes.size() < Size) {
		return false;
	}
	for (std::size_t offset = 0; offset <= bytes.size() - Size; ++offset) {
		bool equal = true;
		for (std::size_t index = 0; index < Size; ++index) {
			equal = equal && bytes[offset + index] == pattern[index];
		}
		if (equal) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] constexpr std::optional<std::size_t> FindSetStaticCall(std::span<const std::byte> bytes) noexcept
{
	constexpr std::array prefix{std::byte{0x41}, std::byte{0x80}, std::byte{0xf9}, std::byte{0x01}, std::byte{0x0f}, std::byte{0x94}, std::byte{0xc2}, std::byte{0xe8}};
	constexpr std::array suffix{std::byte{0xb8}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
	std::optional<std::size_t> found;
	for (std::size_t offset = 0; offset + prefix.size() + 4 + suffix.size() <= bytes.size(); ++offset) {
		bool match = true;
		for (std::size_t index = 0; index < prefix.size(); ++index) {
			match = match && bytes[offset + index] == prefix[index];
		}
		for (std::size_t index = 0; index < suffix.size(); ++index) {
			match = match && bytes[offset + prefix.size() + 4 + index] == suffix[index];
		}
		if (!match) {
			continue;
		}
		if (found) {
			return std::nullopt;
		}
		found = offset + prefix.size() - 1;
	}
	return found;
}

[[nodiscard]] constexpr std::optional<std::uintptr_t> RelativeCallTarget(std::span<const std::byte> bytes, std::uintptr_t address, std::size_t opcode) noexcept
{
	if (opcode + 5 > bytes.size() || bytes[opcode] != std::byte{0xe8}) {
		return std::nullopt;
	}
	const std::uint32_t encoded = std::to_integer<std::uint32_t>(bytes[opcode + 1]) | (std::to_integer<std::uint32_t>(bytes[opcode + 2]) << 8)
	    | (std::to_integer<std::uint32_t>(bytes[opcode + 3]) << 16) | (std::to_integer<std::uint32_t>(bytes[opcode + 4]) << 24);
	const auto displacement = std::bit_cast<std::int32_t>(encoded);
	return address + opcode + 5 + displacement;
}

[[nodiscard]] constexpr bool ValidHelper(std::span<const std::byte> bytes) noexcept
{
	constexpr std::array staticByte{std::byte{0x0f}, std::byte{0xb6}, std::byte{0x81}, std::byte{0xc0}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
	constexpr std::array bodyPointer{std::byte{0x48}, std::byte{0x8b}, std::byte{0x43}, std::byte{0x10}};
	constexpr std::array kinematicFlag{std::byte{0x83}, std::byte{0x88}, std::byte{0xa0}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x02}};
	return Contains(bytes, staticByte) && Contains(bytes, bodyPointer) && Contains(bytes, kinematicFlag);
}

enum class Operation : std::uint8_t
{
	Enter,
	Reassert,
	Restore
};
struct Result
{
	bool observed = false;
	bool applied = false;
	std::uint32_t originalFlags = 0;
};

[[nodiscard]] bool Install(void* exportedSetVehicleStatic) noexcept;
[[nodiscard]] bool Begin(Operation operation, std::uint32_t restoreFlags = 0) noexcept;
[[nodiscard]] Result Finish() noexcept;
void Cancel() noexcept;
[[nodiscard]] bool Cleanup() noexcept;
}
