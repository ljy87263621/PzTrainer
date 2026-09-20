#include "bullet_vehicle_ghost.h"

#include <MinHook.h>
#include <windows.h>

#include <atomic>
#include <cstring>
#include <limits>

namespace
{
using namespace pztrainer::features::vehicle_ghost;
using NativeSetStatic = void (*)(void*, bool);

struct Request
{
	Operation operation{};
	std::uint32_t restoreFlags = 0;
	Result result{};
};

std::atomic<NativeSetStatic> g_original{nullptr};
std::atomic_uint32_t g_callbacks{0};
std::atomic_bool g_acceptCallbacks{false}, g_created{false}, g_enabled{false};
void* g_target = nullptr;
// The launcher manual-maps this DLL, so compiler-managed static TLS is unavailable.
// Requests are synchronous; only the owning thread may observe or finish one.
Request g_request{};
std::atomic<DWORD> g_requestThread{0};

[[nodiscard]] bool Accessible(const void* address, std::size_t size, bool write) noexcept
{
	if (!address || size == 0) {
		return false;
	}
	MEMORY_BASIC_INFORMATION information{};
	if (VirtualQuery(address, &information, sizeof(information)) != sizeof(information) || information.State != MEM_COMMIT || (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
		return false;
	}
	const auto begin = reinterpret_cast<std::uintptr_t>(address);
	const auto region = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
	if (begin < region || size > std::numeric_limits<std::uintptr_t>::max() - begin || begin + size > region + information.RegionSize) {
		return false;
	}
	if (!write) {
		return true;
	}
	const DWORD protection = information.Protect & 0xff;
	return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

[[nodiscard]] bool Executable(const void* address, std::size_t size, const void* allocation) noexcept
{
	if (!Accessible(address, size, false)) {
		return false;
	}
	MEMORY_BASIC_INFORMATION information{};
	VirtualQuery(address, &information, sizeof(information));
	const DWORD protection = information.Protect & 0xff;
	return information.AllocationBase == allocation && (protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY);
}

template <typename Value> [[nodiscard]] bool Read(const void* address, Value& value) noexcept
{
	if (!Accessible(address, sizeof(Value), false)) {
		return false;
	}
	std::memcpy(&value, address, sizeof(Value));
	return true;
}

template <typename Value> [[nodiscard]] bool Write(void* address, Value value) noexcept
{
	if (!Accessible(address, sizeof(Value), true)) {
		return false;
	}
	std::memcpy(address, &value, sizeof(Value));
	Value confirmed{};
	return Read(address, confirmed) && confirmed == value;
}

struct Body
{
	void* pointer = nullptr;
	std::uint32_t flags = 0;
};

[[nodiscard]] bool ReadBody(void* vehicle, Body& body) noexcept
{
	void* pointer = nullptr;
	if (!Read(static_cast<std::byte*>(vehicle) + 0x10, pointer) || !pointer) {
		return false;
	}
	std::uint32_t flags = 0;
	if (!Read(static_cast<std::byte*>(pointer) + 0x1a0, flags)) {
		return false;
	}
	body = {pointer, flags};
	return true;
}

void HookedSetStatic(void* vehicle, bool isStatic) noexcept
{
	g_callbacks.fetch_add(1, std::memory_order_acq_rel);
	const auto original = g_original.load(std::memory_order_acquire);
	Request* request = g_acceptCallbacks.load(std::memory_order_acquire)
	    && g_requestThread.load(std::memory_order_acquire) == GetCurrentThreadId() ? &g_request : nullptr;
	Body before{};
	const bool capturedBefore = request && ReadBody(vehicle, before);
	if (original) {
		original(vehicle, isStatic);
	}
	if (request) {
		Body after{};
		std::uint8_t staticState = 0;
		request->result.observed = capturedBefore && ReadBody(vehicle, after) && after.pointer == before.pointer && Read(static_cast<std::byte*>(vehicle) + 0xc0, staticState)
		    && (staticState != 0) == isStatic;
		request->result.originalFlags = before.flags;
		if (request->result.observed) {
			const auto flags = request->operation == Operation::Restore ? request->restoreFlags : GhostFlags(after.flags);
			request->result.applied = Write(static_cast<std::byte*>(after.pointer) + 0x1a0, flags);
		}
	}
	g_callbacks.fetch_sub(1, std::memory_order_acq_rel);
}
}

namespace pztrainer::features::vehicle_ghost
{
bool Install(void* exportedSetVehicleStatic) noexcept
{
	if (g_enabled.load(std::memory_order_acquire)) {
		return true;
	}
	if (!exportedSetVehicleStatic || !Accessible(exportedSetVehicleStatic, 0xc0, false)) {
		return false;
	}
	MEMORY_BASIC_INFORMATION exportMemory{};
	if (VirtualQuery(exportedSetVehicleStatic, &exportMemory, sizeof(exportMemory)) != sizeof(exportMemory)) {
		return false;
	}
	const auto code = std::span{static_cast<const std::byte*>(exportedSetVehicleStatic), std::size_t{0xc0}};
	const auto call = FindSetStaticCall(code);
	const auto target = call ? RelativeCallTarget(code, reinterpret_cast<std::uintptr_t>(exportedSetVehicleStatic), *call) : std::nullopt;
	if (!target || !Executable(reinterpret_cast<void*>(*target), 0xc0, exportMemory.AllocationBase)) {
		return false;
	}
	const auto helper = std::span{reinterpret_cast<const std::byte*>(*target), std::size_t{0xc0}};
	if (!ValidHelper(helper)) {
		return false;
	}

	if (!g_created.load(std::memory_order_acquire)) {
		void* original = nullptr;
		g_target = reinterpret_cast<void*>(*target);
		if (MH_CreateHook(g_target, reinterpret_cast<void*>(&HookedSetStatic), &original) != MH_OK || !original) {
			g_target = nullptr;
			return false;
		}
		g_original.store(reinterpret_cast<NativeSetStatic>(original), std::memory_order_release);
		g_created.store(true, std::memory_order_release);
	}
	if (MH_EnableHook(g_target) != MH_OK) {
		return false;
	}
	g_acceptCallbacks.store(true, std::memory_order_release);
	g_enabled.store(true, std::memory_order_release);
	return true;
}

bool Begin(Operation operation, std::uint32_t restoreFlags) noexcept
{
	if (!g_enabled.load(std::memory_order_acquire) || !g_acceptCallbacks.load(std::memory_order_acquire)) {
		return false;
	}
	DWORD unowned = 0;
	if (!g_requestThread.compare_exchange_strong(unowned, GetCurrentThreadId(), std::memory_order_acq_rel)) {
		return false;
	}
	g_request = {.operation = operation, .restoreFlags = restoreFlags};
	return true;
}

Result Finish() noexcept
{
	if (g_requestThread.load(std::memory_order_acquire) != GetCurrentThreadId()) {
		return {};
	}
	const Result result = g_request.result;
	g_requestThread.store(0, std::memory_order_release);
	return result;
}

void Cancel() noexcept
{
	DWORD owner = GetCurrentThreadId();
	g_requestThread.compare_exchange_strong(owner, 0, std::memory_order_acq_rel);
}

bool Cleanup() noexcept
{
	g_acceptCallbacks.store(false, std::memory_order_release);
	Cancel();
	if (g_enabled.exchange(false, std::memory_order_acq_rel) && g_target && MH_DisableHook(g_target) != MH_OK) {
		g_enabled.store(true, std::memory_order_release);
		g_acceptCallbacks.store(true, std::memory_order_release);
		return false;
	}
	if (g_callbacks.load(std::memory_order_acquire) != 0 || g_requestThread.load(std::memory_order_acquire) != 0) {
		return false;
	}
	if (g_created.load(std::memory_order_acquire) && g_target && MH_RemoveHook(g_target) != MH_OK) {
		return false;
	}
	g_created.store(false, std::memory_order_release);
	g_original.store(nullptr, std::memory_order_release);
	g_target = nullptr;
	return true;
}
}
