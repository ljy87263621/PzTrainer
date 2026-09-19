#include "manual_mapper.hpp"

#include "vmprotect.hpp"

#include <TlHelp32.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace launcher {
namespace {

using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using RtlAddFunctionTableFn = BOOLEAN(WINAPI*)(
    PRUNTIME_FUNCTION, DWORD, DWORD64);
using RtlDeleteFunctionTableFn = BOOLEAN(WINAPI*)(PRUNTIME_FUNCTION);
using DllEntryPointFn = BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID);
using TlsCallbackFn = void(NTAPI*)(PVOID, DWORD, PVOID);

enum class LoaderStatus : DWORD {
    Pending = 0,
    ResolvingImports = 1,
    RegisteringExceptions = 2,
    RunningTls = 3,
    RunningEntryPoint = 4,
    Complete = 5,
    InvalidImage = 100,
    ImportModuleFailed = 101,
    ImportFunctionFailed = 102,
    ExceptionRegistrationFailed = 103,
    EntryPointFailed = 104,
};

struct RemoteLoaderContext {
    std::uint8_t* image_base;
    LoadLibraryAFn load_library_a;
    GetProcAddressFn get_proc_address;
    RtlAddFunctionTableFn rtl_add_function_table;
    RtlDeleteFunctionTableFn rtl_delete_function_table;
    DWORD status;
    DWORD detail;
    BOOL dll_main_result;
    BOOL function_table_registered;
};

#pragma optimize("", off)
#pragma code_seg(push, ".pzmap$a")
__declspec(noinline) DWORD WINAPI RemoteLoader(RemoteLoaderContext* context) {
    if (context == nullptr || context->image_base == nullptr ||
        context->load_library_a == nullptr || context->get_proc_address == nullptr) {
        return static_cast<DWORD>(LoaderStatus::InvalidImage);
    }

    std::uint8_t* image = context->image_base;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        context->status = static_cast<DWORD>(LoaderStatus::InvalidImage);
        return context->status;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        context->status = static_cast<DWORD>(LoaderStatus::InvalidImage);
        return context->status;
    }

    context->status = static_cast<DWORD>(LoaderStatus::ResolvingImports);
    const IMAGE_DATA_DIRECTORY import_directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_directory.VirtualAddress != 0) {
        auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            image + import_directory.VirtualAddress);
        DWORD descriptor_index = 0;
        while (descriptor->Name != 0) {
            HMODULE dependency = context->load_library_a(
                reinterpret_cast<LPCSTR>(image + descriptor->Name));
            if (dependency == nullptr) {
                context->detail = descriptor_index;
                context->status = static_cast<DWORD>(
                    LoaderStatus::ImportModuleFailed);
                return context->status;
            }

            auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                image + (descriptor->OriginalFirstThunk != 0
                    ? descriptor->OriginalFirstThunk
                    : descriptor->FirstThunk));
            auto* address = reinterpret_cast<IMAGE_THUNK_DATA64*>(
                image + descriptor->FirstThunk);
            DWORD thunk_index = 0;
            while (lookup->u1.AddressOfData != 0) {
                FARPROC function = nullptr;
                if (IMAGE_SNAP_BY_ORDINAL64(lookup->u1.Ordinal)) {
                    function = context->get_proc_address(
                        dependency,
                        reinterpret_cast<LPCSTR>(
                            IMAGE_ORDINAL64(lookup->u1.Ordinal)));
                } else {
                    auto* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                        image + lookup->u1.AddressOfData);
                    function = context->get_proc_address(
                        dependency, reinterpret_cast<LPCSTR>(name->Name));
                }
                if (function == nullptr) {
                    context->detail =
                        (descriptor_index << 16) | (thunk_index & 0xFFFF);
                    context->status = static_cast<DWORD>(
                        LoaderStatus::ImportFunctionFailed);
                    return context->status;
                }
                address->u1.Function = reinterpret_cast<ULONGLONG>(function);
                ++lookup;
                ++address;
                ++thunk_index;
            }
            ++descriptor;
            ++descriptor_index;
        }
    }

    const IMAGE_DATA_DIRECTORY exception_directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (exception_directory.VirtualAddress != 0 &&
        exception_directory.Size >= sizeof(RUNTIME_FUNCTION)) {
        context->status = static_cast<DWORD>(
            LoaderStatus::RegisteringExceptions);
        if (context->rtl_add_function_table == nullptr ||
            !context->rtl_add_function_table(
                reinterpret_cast<PRUNTIME_FUNCTION>(
                    image + exception_directory.VirtualAddress),
                exception_directory.Size / sizeof(RUNTIME_FUNCTION),
                reinterpret_cast<DWORD64>(image))) {
            context->status = static_cast<DWORD>(
                LoaderStatus::ExceptionRegistrationFailed);
            return context->status;
        }
        context->function_table_registered = TRUE;
    }

    const IMAGE_DATA_DIRECTORY tls_directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tls_directory.VirtualAddress != 0) {
        context->status = static_cast<DWORD>(LoaderStatus::RunningTls);
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(
            image + tls_directory.VirtualAddress);
        auto** callback = reinterpret_cast<TlsCallbackFn*>(
            tls->AddressOfCallBacks);
        if (callback != nullptr) {
            while (*callback != nullptr) {
                (*callback)(image, DLL_PROCESS_ATTACH, nullptr);
                ++callback;
            }
        }
    }

    context->status = static_cast<DWORD>(LoaderStatus::RunningEntryPoint);
    if (nt->OptionalHeader.AddressOfEntryPoint != 0) {
        const auto entry = reinterpret_cast<DllEntryPointFn>(
            image + nt->OptionalHeader.AddressOfEntryPoint);
        context->dll_main_result = entry(
            reinterpret_cast<HINSTANCE>(image), DLL_PROCESS_ATTACH, nullptr);
        if (!context->dll_main_result) {
            if (context->function_table_registered &&
                context->rtl_delete_function_table != nullptr) {
                context->rtl_delete_function_table(
                    reinterpret_cast<PRUNTIME_FUNCTION>(
                        image + exception_directory.VirtualAddress));
                context->function_table_registered = FALSE;
            }
            context->status = static_cast<DWORD>(
                LoaderStatus::EntryPointFailed);
            return context->status;
        }
    } else {
        context->dll_main_result = TRUE;
    }

    context->status = static_cast<DWORD>(LoaderStatus::Complete);
    return 0;
}
#pragma code_seg(pop)

#pragma code_seg(push, ".pzmap$z")
__declspec(noinline) void RemoteLoaderEnd() {}
#pragma code_seg(pop)
#pragma optimize("", on)

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    HANDLE Get() const { return value_; }
    explicit operator bool() const {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE value_;
};

class RemoteAllocation {
public:
    RemoteAllocation(HANDLE process, void* address)
        : process_(process), address_(address) {}
    ~RemoteAllocation() {
        if (address_ != nullptr) {
            VirtualFreeEx(process_, address_, 0, MEM_RELEASE);
        }
    }
    RemoteAllocation(const RemoteAllocation&) = delete;
    RemoteAllocation& operator=(const RemoteAllocation&) = delete;
    void* Get() const { return address_; }
    void Release() { address_ = nullptr; }

private:
    HANDLE process_;
    void* address_;
};

bool RangeInside(std::size_t offset, std::size_t size, std::size_t limit) {
    return offset <= limit && size <= limit - offset;
}

std::wstring Win32Error(const wchar_t* operation) {
    return std::wstring(operation) + L"，Win32 错误：" +
        std::to_wstring(GetLastError());
}

std::uintptr_t FindRemoteModuleBase(
    DWORD process_id, const wchar_t* module_name) {
    UniqueHandle snapshot(CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, process_id));
    if (!snapshot) return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Module32FirstW(snapshot.Get(), &entry)) return 0;
    do {
        if (_wcsicmp(entry.szModule, module_name) == 0) {
            return reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
        }
    } while (Module32NextW(snapshot.Get(), &entry));
    return 0;
}

void* ResolveRemoteFunction(DWORD process_id, FARPROC local_function) {
    if (local_function == nullptr) return nullptr;

    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(local_function), &owner)) {
        return nullptr;
    }
    wchar_t owner_path[MAX_PATH]{};
    if (GetModuleFileNameW(owner, owner_path, MAX_PATH) == 0) return nullptr;
    const wchar_t* owner_name = wcsrchr(owner_path, L'\\');
    owner_name = owner_name == nullptr ? owner_path : owner_name + 1;

    const std::uintptr_t remote_owner =
        FindRemoteModuleBase(process_id, owner_name);
    if (remote_owner == 0) return nullptr;
    const std::uintptr_t offset =
        reinterpret_cast<std::uintptr_t>(local_function) -
        reinterpret_cast<std::uintptr_t>(owner);
    return reinterpret_cast<void*>(remote_owner + offset);
}

DWORD ProtectionForSection(DWORD characteristics) {
    const bool execute = (characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    const bool read = (characteristics & IMAGE_SCN_MEM_READ) != 0;
    const bool write = (characteristics & IMAGE_SCN_MEM_WRITE) != 0;
    if (execute) {
        if (write) return PAGE_EXECUTE_READWRITE;
        return read ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    }
    if (write) return PAGE_READWRITE;
    return read ? PAGE_READONLY : PAGE_NOACCESS;
}

bool ApplyRelocations(
    std::vector<std::uint8_t>& mapped_image,
    const IMAGE_NT_HEADERS64& nt,
    std::uintptr_t remote_base,
    std::wstring& error) {
    const std::int64_t delta = static_cast<std::int64_t>(remote_base) -
        static_cast<std::int64_t>(nt.OptionalHeader.ImageBase);
    if (delta == 0) return true;

    const IMAGE_DATA_DIRECTORY directory =
        nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (directory.VirtualAddress == 0 || directory.Size == 0 ||
        !RangeInside(directory.VirtualAddress, directory.Size,
            mapped_image.size())) {
        error = L"DLL 无法重定位到目标地址。";
        return false;
    }

    std::size_t consumed = 0;
    while (consumed < directory.Size) {
        if (!RangeInside(
                directory.VirtualAddress + consumed,
                sizeof(IMAGE_BASE_RELOCATION), mapped_image.size())) {
            error = L"DLL 重定位表已损坏。";
            return false;
        }
        auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            mapped_image.data() + directory.VirtualAddress + consumed);
        if (block->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) ||
            block->SizeOfBlock > directory.Size - consumed) {
            error = L"DLL 重定位块大小无效。";
            return false;
        }

        const DWORD count =
            (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) /
            sizeof(WORD);
        const auto* entries = reinterpret_cast<const WORD*>(
            reinterpret_cast<const std::uint8_t*>(block) +
            sizeof(IMAGE_BASE_RELOCATION));
        for (DWORD index = 0; index < count; ++index) {
            const WORD type = entries[index] >> 12;
            const WORD offset = entries[index] & 0x0FFF;
            if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
            if (type != IMAGE_REL_BASED_DIR64) {
                error = L"DLL 包含不支持的重定位类型：" +
                    std::to_wstring(type);
                return false;
            }
            const std::size_t patch_rva =
                static_cast<std::size_t>(block->VirtualAddress) + offset;
            if (!RangeInside(patch_rva, sizeof(std::uint64_t),
                    mapped_image.size())) {
                error = L"DLL 重定位地址越界。";
                return false;
            }
            auto* value = reinterpret_cast<std::uint64_t*>(
                mapped_image.data() + patch_rva);
            *value = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(*value) + delta);
        }
        consumed += block->SizeOfBlock;
    }
    return true;
}

std::wstring LoaderFailureMessage(const RemoteLoaderContext& context) {
    switch (static_cast<LoaderStatus>(context.status)) {
        case LoaderStatus::ImportModuleFailed:
            return L"目标进程无法加载 DLL 依赖，导入项：" +
                std::to_wstring(context.detail);
        case LoaderStatus::ImportFunctionFailed:
            return L"目标进程无法解析 DLL 导入函数，位置：" +
                std::to_wstring(context.detail);
        case LoaderStatus::ExceptionRegistrationFailed:
            return L"无法注册 DLL 的 x64 异常表。";
        case LoaderStatus::EntryPointFailed:
            return L"DLL 入口点拒绝初始化。";
        case LoaderStatus::InvalidImage:
            return L"远程加载器检测到无效 DLL 映像。";
        default:
            return L"远程加载器未完成，阶段：" +
                std::to_wstring(context.status);
    }
}

}  // namespace

bool ManualMapImage(
    DWORD process_id,
    const std::vector<std::uint8_t>& file_image,
    std::wstring& error) {
    PZ_VMP_BEGIN_ULTRA("PZ.EXE.ManualMapImage");
    if (file_image.size() < sizeof(IMAGE_DOS_HEADER)) {
        error = L"内嵌 DLL 文件过小。";
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(
        file_image.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !RangeInside(static_cast<std::size_t>(dos->e_lfanew),
            sizeof(IMAGE_NT_HEADERS64), file_image.size())) {
        error = L"内嵌 DLL 的 DOS/NT 头无效。";
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        file_image.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage == 0 ||
        nt->OptionalHeader.SizeOfHeaders == 0 ||
        nt->OptionalHeader.SizeOfImage >
            static_cast<DWORD>(std::numeric_limits<int>::max())) {
        error = L"内嵌 DLL 不是有效的 64 位 PE 映像。";
        return false;
    }

    std::vector<std::uint8_t> mapped_image(
        nt->OptionalHeader.SizeOfImage, 0);
    const std::size_t header_size = std::min<std::size_t>(
        nt->OptionalHeader.SizeOfHeaders, file_image.size());
    if (header_size > mapped_image.size()) {
        error = L"内嵌 DLL 的头部大小无效。";
        return false;
    }
    std::memcpy(mapped_image.data(), file_image.data(), header_size);

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        if (section->SizeOfRawData == 0) continue;
        if (!RangeInside(section->PointerToRawData, section->SizeOfRawData,
                file_image.size()) ||
            !RangeInside(section->VirtualAddress, section->SizeOfRawData,
                mapped_image.size())) {
            error = L"内嵌 DLL 的节数据越界，节索引：" +
                std::to_wstring(index);
            return false;
        }
        std::memcpy(
            mapped_image.data() + section->VirtualAddress,
            file_image.data() + section->PointerToRawData,
            section->SizeOfRawData);
    }

    UniqueHandle process(OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, process_id));
    if (!process) {
        error = Win32Error(L"无法打开游戏进程");
        return false;
    }

    void* image_address = VirtualAllocEx(
        process.Get(),
        reinterpret_cast<void*>(nt->OptionalHeader.ImageBase),
        mapped_image.size(), MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if (image_address == nullptr) {
        image_address = VirtualAllocEx(
            process.Get(), nullptr, mapped_image.size(),
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
    if (image_address == nullptr) {
        error = Win32Error(L"无法为 DLL 映像分配目标内存");
        return false;
    }
    RemoteAllocation image_allocation(process.Get(), image_address);
    if (!ApplyRelocations(
            mapped_image, *nt,
            reinterpret_cast<std::uintptr_t>(image_address), error)) {
        return false;
    }
    if (!WriteProcessMemory(
            process.Get(), image_address, mapped_image.data(),
            mapped_image.size(), nullptr)) {
        error = Win32Error(L"无法写入 DLL 映像");
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RemoteLoaderContext context{};
    context.image_base = static_cast<std::uint8_t*>(image_address);
    context.load_library_a = reinterpret_cast<LoadLibraryAFn>(
        ResolveRemoteFunction(
            process_id, GetProcAddress(kernel32, "LoadLibraryA")));
    context.get_proc_address = reinterpret_cast<GetProcAddressFn>(
        ResolveRemoteFunction(
            process_id, GetProcAddress(kernel32, "GetProcAddress")));
    context.rtl_add_function_table = reinterpret_cast<RtlAddFunctionTableFn>(
        ResolveRemoteFunction(
            process_id, GetProcAddress(ntdll, "RtlAddFunctionTable")));
    context.rtl_delete_function_table =
        reinterpret_cast<RtlDeleteFunctionTableFn>(
            ResolveRemoteFunction(
                process_id, GetProcAddress(ntdll, "RtlDeleteFunctionTable")));
    if (context.load_library_a == nullptr ||
        context.get_proc_address == nullptr ||
        context.rtl_add_function_table == nullptr ||
        context.rtl_delete_function_table == nullptr) {
        error = L"无法解析目标进程的加载器函数地址。";
        return false;
    }

    void* remote_context = VirtualAllocEx(
        process.Get(), nullptr, sizeof(context), MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (remote_context == nullptr) {
        error = Win32Error(L"无法分配远程加载器状态");
        return false;
    }
    RemoteAllocation context_allocation(process.Get(), remote_context);
    if (!WriteProcessMemory(
            process.Get(), remote_context, &context, sizeof(context),
            nullptr)) {
        error = Win32Error(L"无法写入远程加载器状态");
        return false;
    }

    const auto loader_begin = reinterpret_cast<const std::uint8_t*>(
        &RemoteLoader);
    const auto loader_end = reinterpret_cast<const std::uint8_t*>(
        &RemoteLoaderEnd);
    if (loader_end <= loader_begin ||
        static_cast<std::size_t>(loader_end - loader_begin) > 65536) {
        error = L"远程加载器代码布局无效。";
        return false;
    }
    const SIZE_T loader_size =
        static_cast<SIZE_T>(loader_end - loader_begin);
    void* remote_loader = VirtualAllocEx(
        process.Get(), nullptr, loader_size, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);
    if (remote_loader == nullptr) {
        error = Win32Error(L"无法分配远程加载器代码");
        return false;
    }
    RemoteAllocation loader_allocation(process.Get(), remote_loader);
    if (!WriteProcessMemory(
            process.Get(), remote_loader, loader_begin, loader_size,
            nullptr)) {
        error = Win32Error(L"无法写入远程加载器代码");
        return false;
    }
    DWORD old_loader_protection = 0;
    if (!VirtualProtectEx(
            process.Get(), remote_loader, loader_size, PAGE_EXECUTE_READ,
            &old_loader_protection) ||
        !FlushInstructionCache(process.Get(), remote_loader, loader_size)) {
        error = Win32Error(L"无法启用远程加载器代码");
        return false;
    }

    UniqueHandle loader_thread(CreateRemoteThread(
        process.Get(), nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_loader),
        remote_context, 0, nullptr));
    if (!loader_thread) {
        error = Win32Error(L"无法启动远程加载器线程");
        return false;
    }
    const DWORD wait_result = WaitForSingleObject(loader_thread.Get(), 15000);
    if (wait_result != WAIT_OBJECT_0) {
        error = wait_result == WAIT_TIMEOUT
            ? L"远程加载器执行超时。"
            : Win32Error(L"等待远程加载器失败");
        // The thread may still be executing. Do not free code or image memory
        // underneath it; leaving the allocations is safer than corrupting the
        // target process.
        image_allocation.Release();
        context_allocation.Release();
        loader_allocation.Release();
        return false;
    }

    DWORD thread_result = 0;
    if (!GetExitCodeThread(loader_thread.Get(), &thread_result) ||
        !ReadProcessMemory(
            process.Get(), remote_context, &context, sizeof(context),
            nullptr)) {
        error = Win32Error(L"无法读取远程加载结果");
        // DllMain may already have started the trainer bootstrap thread.
        image_allocation.Release();
        return false;
    }
    if (thread_result != 0 ||
        context.status != static_cast<DWORD>(LoaderStatus::Complete) ||
        !context.dll_main_result) {
        error = LoaderFailureMessage(context);
        return false;
    }

    DWORD previous_protection = 0;
    if (!VirtualProtectEx(
            process.Get(), image_address,
            nt->OptionalHeader.SizeOfHeaders, PAGE_READONLY,
            &previous_protection)) {
        error = Win32Error(L"DLL 已初始化，但无法保护 PE 头部");
        image_allocation.Release();
        return false;
    }
    section = IMAGE_FIRST_SECTION(nt);
    for (WORD index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        const SIZE_T section_size = std::max<DWORD>(
            section->Misc.VirtualSize, section->SizeOfRawData);
        if (section_size == 0) continue;
        if (!VirtualProtectEx(
                process.Get(),
                static_cast<std::uint8_t*>(image_address) +
                    section->VirtualAddress,
                section_size,
                ProtectionForSection(section->Characteristics),
                &previous_protection)) {
            error = L"DLL 已初始化，但无法保护节，索引：" +
                std::to_wstring(index);
            image_allocation.Release();
            return false;
        }
    }
    FlushInstructionCache(process.Get(), image_address, mapped_image.size());

    image_allocation.Release();
    PZ_VMP_END();
    return true;
}

}  // namespace launcher
