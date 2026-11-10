#include "runtime/core/kernel.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>

namespace runtime::core {

// ============================================================================
// XboxKernel Implementation
// ============================================================================

XboxKernel::XboxKernel() {
    register_default_handlers();
}

XboxKernel::XboxKernel(XboxMemory& memory)
    : m_memory_ref(&memory)
{
    register_default_handlers();
}

void XboxKernel::register_export(uint32_t ordinal, std::string name, KernelHandler handler) {
    m_exports[ordinal] = KernelExport{
        .ordinal = ordinal,
        .name = std::move(name),
        .handler = std::move(handler)
    };
}

bool XboxKernel::has_export(uint32_t ordinal) const noexcept {
    return m_exports.find(ordinal) != m_exports.end();
}

const std::string& XboxKernel::get_export_name(uint32_t ordinal) const {
    auto it = m_exports.find(ordinal);
    if (it != m_exports.end()) {
        return it->second.name;
    }
    return m_unknown_export_str;
}

uint32_t XboxKernel::dispatch(uint32_t ordinal, XboxMemory& memory, const std::vector<uint32_t>& args) {
    auto it = m_exports.find(ordinal);
    if (it != m_exports.end() && it->second.handler) {
        return it->second.handler(memory, args);
    }

    std::cerr << "[Kernel] Unimplemented kernel export called: ordinal " << ordinal
              << " (" << get_export_name(ordinal) << ")\n";
    return STATUS_NOT_IMPLEMENTED;
}

uint32_t XboxKernel::dispatch(uint32_t ordinal, const std::vector<uint32_t>& args) {
    if (m_memory_ref) {
        return dispatch(ordinal, *m_memory_ref, args);
    }
    std::cerr << "[Kernel] Dispatch called without memory instance!\n";
    return STATUS_UNSUCCESSFUL;
}

void XboxKernel::install_thunks(XboxMemory& memory, uint32_t thunk_table_va) {
    if (thunk_table_va == 0) return;

    uint32_t curr_va = thunk_table_va;
    while (true) {
        uint32_t entry = memory.read<uint32_t>(curr_va);
        if (entry == 0) break;

        curr_va += sizeof(uint32_t);
    }
}

void XboxKernel::register_default_handlers() {
    register_export(1,   "AvGetSavedDataAddress",          hle_AvGetSavedDataAddress);
    register_export(2,   "AvSendTVEncoderOption",          hle_AvSendTVEncoderOption);
    register_export(8,   "AvSetDisplayMode",               hle_AvSetDisplayMode);
    register_export(12,  "DbgPrint",                       hle_DbgPrint);
    register_export(13,  "DbgPrompt",                      hle_DbgPrompt);
    register_export(49,  "HalReturnToFirmware",            hle_HalReturnToFirmware);
    register_export(55,  "HalWriteSMBusValue",             hle_HalWriteSMBusValue);
    register_export(56,  "HalReadSMBusValue",              hle_HalReadSMBusValue);
    register_export(104, "KeDelayExecutionThread",         hle_KeDelayExecutionThread);
    register_export(106, "KeGetCurrentThread",             hle_KeGetCurrentThread);
    register_export(107, "KeGetCurrentProcess",            hle_KeGetCurrentProcess);
    register_export(119, "KeInitializeEvent",              hle_KeInitializeEvent);
    register_export(133, "KeQueryPerformanceCounter",      hle_KeQueryPerformanceCounter);
    register_export(134, "KeQueryPerformanceFrequency",    hle_KeQueryPerformanceFrequency);
    register_export(135, "KeQuerySystemTime",              hle_KeQuerySystemTime);
    register_export(139, "KeResetEvent",                   hle_KeResetEvent);
    register_export(148, "KeSetEvent",                     hle_KeSetEvent);
    register_export(153, "KeTickCount",                    hle_KeTickCount);
    register_export(165, "MmAllocateContiguousMemory",     hle_MmAllocateContiguousMemory);
    register_export(166, "MmAllocateContiguousMemoryEx",   hle_MmAllocateContiguousMemoryEx);
    register_export(168, "MmFreeContiguousMemory",         hle_MmFreeContiguousMemory);
    register_export(170, "MmGetPhysicalAddress",           hle_MmGetPhysicalAddress);
    register_export(172, "MmIsAddressValid",               hle_MmIsAddressValid);
    register_export(184, "NtAllocateVirtualMemory",        hle_NtAllocateVirtualMemory);
    register_export(187, "NtClose",                        hle_NtClose);
    register_export(190, "NtCreateFile",                   hle_NtCreateFile);
    register_export(194, "NtDeviceIoControlFile",          hle_NtDeviceIoControlFile);
    register_export(199, "NtFreeVirtualMemory",            hle_NtFreeVirtualMemory);
    register_export(203, "NtOpenFile",                     hle_NtOpenFile);
    register_export(219, "NtReadFile",                     hle_NtReadFile);
    register_export(237, "NtWriteFile",                    hle_NtWriteFile);
    register_export(247, "RtlInitAnsiString",              hle_RtlInitAnsiString);
    register_export(290, "RtlZeroMemory",                  hle_RtlZeroMemory);
    register_export(325, "XboxKrnlVersion",                hle_XboxKrnlVersion);
    register_export(350, "XapiInitProcess",                hle_XapiInitProcess);
}

// ============================================================================
// Kernel HLE Mock Implementations
// ============================================================================

uint32_t hle_AvGetSavedDataAddress(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)args;
    static constexpr uint32_t AV_SAVED_DATA_ADDR = 0x80001000;
    if (memory.read<uint32_t>(AV_SAVED_DATA_ADDR) == 0) {
        memory.write<uint32_t>(AV_SAVED_DATA_ADDR, 0x00400001);     // Standard AV pack + NTSC
        memory.write<uint32_t>(AV_SAVED_DATA_ADDR + 4, 0x00000001); // Region 1 (North America)
    }
    return AV_SAVED_DATA_ADDR;
}

uint32_t hle_AvSendTVEncoderOption(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_AvSetDisplayMode(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_DbgPrint(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;

    uint32_t fmt_ptr = args[0];
    std::string fmt_str = memory.read_string(fmt_ptr, 1024);

    std::string output;
    size_t arg_idx = 1;

    for (size_t i = 0; i < fmt_str.size(); ++i) {
        if (fmt_str[i] == '%' && (i + 1) < fmt_str.size()) {
            char specifier = fmt_str[i + 1];
            if (specifier == 's' && arg_idx < args.size()) {
                uint32_t str_va = args[arg_idx++];
                output += memory.read_string(str_va, 512);
                i++;
                continue;
            } else if ((specifier == 'd' || specifier == 'i') && arg_idx < args.size()) {
                output += std::to_string(static_cast<int32_t>(args[arg_idx++]));
                i++;
                continue;
            } else if ((specifier == 'x' || specifier == 'X' || specifier == 'p') && arg_idx < args.size()) {
                char hex_buf[32];
                std::snprintf(hex_buf, sizeof(hex_buf), "0x%X", args[arg_idx++]);
                output += hex_buf;
                i++;
                continue;
            } else if (specifier == '%') {
                output += '%';
                i++;
                continue;
            }
        }
        output += fmt_str[i];
    }

    std::cout << "[DbgPrint] " << output;
    if (output.empty() || output.back() != '\n') {
        std::cout << "\n";
    }
    return STATUS_SUCCESS;
}

uint32_t hle_DbgPrompt(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_HalReturnToFirmware(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    uint32_t routine = !args.empty() ? args[0] : 0;
    const char* name = "Unknown";
    switch (routine) {
        case HalHaltRoutine:        name = "HalHaltRoutine"; break;
        case HalRebootRoutine:      name = "HalRebootRoutine"; break;
        case HalQuickRebootRoutine: name = "HalQuickRebootRoutine"; break;
        case HalKdRebootRoutine:    name = "HalKdRebootRoutine"; break;
        case HalFatalErrorReboot:   name = "HalFatalErrorReboot"; break;
    }
    std::cout << "[Kernel] HalReturnToFirmware invoked: " << name << " (" << routine << ")\n";
    return STATUS_SUCCESS;
}

uint32_t hle_HalWriteSMBusValue(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_HalReadSMBusValue(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    if (args.size() >= 4) {
        uint32_t data_ptr = args[3];
        if (data_ptr != 0) {
            memory.write<uint8_t>(data_ptr, 0);
        }
    }
    return STATUS_SUCCESS;
}

uint32_t hle_KeGetCurrentThread(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)args;
    static constexpr uint32_t THREAD_VA = 0x80002000;
    return THREAD_VA;
}

uint32_t hle_KeGetCurrentProcess(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)args;
    static constexpr uint32_t PROCESS_VA = 0x80002200;
    return PROCESS_VA;
}

uint32_t hle_KeInitializeEvent(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;
    uint32_t event_va = args[0];
    memory.write<uint32_t>(event_va, 0x00000000);
    return STATUS_SUCCESS;
}

uint32_t hle_KeSetEvent(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;
    uint32_t event_va = args[0];
    memory.write<uint8_t>(event_va + 1, 1);
    return 0;
}

uint32_t hle_KeResetEvent(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;
    uint32_t event_va = args[0];
    uint8_t prev = memory.read<uint8_t>(event_va + 1);
    memory.write<uint8_t>(event_va + 1, 0);
    return prev;
}

uint32_t hle_KeDelayExecutionThread(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_KeQueryPerformanceCounter(XboxMemory& memory, const std::vector<uint32_t>& args) {
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    int64_t count = duration_cast<nanoseconds>(now).count();

    if (!args.empty() && args[0] != 0) {
        memory.write<int64_t>(args[0], count);
    }
    return static_cast<uint32_t>(count & 0xFFFFFFFF);
}

uint32_t hle_KeQueryPerformanceFrequency(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return 1000000000;
}

uint32_t hle_KeQuerySystemTime(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    int64_t intervals = duration_cast<nanoseconds>(now).count() / 100;
    intervals += 116444736000000000LL;
    memory.write<int64_t>(args[0], intervals);
    return STATUS_SUCCESS;
}

uint32_t hle_KeTickCount(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(duration_cast<milliseconds>(now).count());
}

uint32_t hle_MmAllocateContiguousMemory(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return 0;
    uint32_t num_bytes = args[0];
    return memory.allocate_contiguous(num_bytes, XBOX_PAGE_SIZE);
}

uint32_t hle_MmAllocateContiguousMemoryEx(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.size() < 4) return 0;
    uint32_t num_bytes = args[0];
    uint32_t alignment = args[3] != 0 ? args[3] : XBOX_PAGE_SIZE;
    return memory.allocate_contiguous(num_bytes, alignment);
}

uint32_t hle_MmFreeContiguousMemory(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return STATUS_INVALID_PARAMETER;
    uint32_t va = args[0];
    memory.free_contiguous(va, XBOX_PAGE_SIZE);
    return STATUS_SUCCESS;
}

uint32_t hle_MmGetPhysicalAddress(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return 0xFFFFFFFF;
    return memory.virtual_to_physical(args[0]);
}

uint32_t hle_MmIsAddressValid(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.empty()) return 0;
    return memory.is_valid_va(args[0]) ? 1 : 0;
}

uint32_t hle_NtAllocateVirtualMemory(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.size() < 3) return STATUS_INVALID_PARAMETER;

    uint32_t base_addr_ptr = args[0];
    uint32_t region_size_ptr = args[2];

    uint32_t req_addr = memory.read<uint32_t>(base_addr_ptr);
    uint32_t req_size = memory.read<uint32_t>(region_size_ptr);

    if (req_addr == 0) {
        static uint32_t s_next_dynamic_va = 0x01000000;
        req_addr = s_next_dynamic_va;
        s_next_dynamic_va += (req_size + XBOX_PAGE_SIZE - 1) & ~(XBOX_PAGE_SIZE - 1);
    }

    if (!memory.allocate_virtual(req_addr, req_size, PAGE_PROT_READ | PAGE_PROT_WRITE)) {
        return STATUS_NO_MEMORY;
    }

    memory.write<uint32_t>(base_addr_ptr, req_addr);
    return STATUS_SUCCESS;
}

uint32_t hle_NtFreeVirtualMemory(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.size() < 2) return STATUS_INVALID_PARAMETER;

    uint32_t base_addr_ptr = args[0];
    uint32_t region_size_ptr = args[1];

    uint32_t free_addr = memory.read<uint32_t>(base_addr_ptr);
    uint32_t free_size = memory.read<uint32_t>(region_size_ptr);

    memory.free_virtual(free_addr, free_size);
    return STATUS_SUCCESS;
}

uint32_t hle_NtClose(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_NtCreateFile(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    if (!args.empty() && args[0] != 0) {
        static uint32_t s_dummy_handle = 0x100;
        memory.write<uint32_t>(args[0], ++s_dummy_handle);
    }
    return STATUS_SUCCESS;
}

uint32_t hle_NtOpenFile(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    if (!args.empty() && args[0] != 0) {
        static uint32_t s_dummy_handle = 0x200;
        memory.write<uint32_t>(args[0], ++s_dummy_handle);
    }
    return STATUS_SUCCESS;
}

uint32_t hle_NtReadFile(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_NtWriteFile(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_NtDeviceIoControlFile(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

uint32_t hle_RtlInitAnsiString(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.size() < 2) return STATUS_INVALID_PARAMETER;

    uint32_t dest_struct_va = args[0];
    uint32_t src_str_va = args[1];

    if (dest_struct_va == 0) return STATUS_INVALID_PARAMETER;

    uint16_t len = 0;
    if (src_str_va != 0) {
        std::string s = memory.read_string(src_str_va, 2048);
        len = static_cast<uint16_t>(s.size());
    }

    memory.write<uint16_t>(dest_struct_va, len);
    memory.write<uint16_t>(dest_struct_va + 2, len + 1);
    memory.write<uint32_t>(dest_struct_va + 4, src_str_va);

    return STATUS_SUCCESS;
}

uint32_t hle_RtlZeroMemory(XboxMemory& memory, const std::vector<uint32_t>& args) {
    if (args.size() < 2) return STATUS_INVALID_PARAMETER;
    uint32_t dst = args[0];
    uint32_t size = args[1];
    std::vector<uint8_t> zeros(size, 0);
    memory.write_bytes(dst, zeros.data(), size);
    return STATUS_SUCCESS;
}

uint32_t hle_XboxKrnlVersion(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)args;
    static constexpr uint32_t KRNL_VER_VA = 0x80000100;
    memory.write<uint16_t>(KRNL_VER_VA, 1);
    memory.write<uint16_t>(KRNL_VER_VA + 2, 0);
    memory.write<uint16_t>(KRNL_VER_VA + 4, 5849);
    memory.write<uint16_t>(KRNL_VER_VA + 6, 1);
    return KRNL_VER_VA;
}

uint32_t hle_XapiInitProcess(XboxMemory& memory, const std::vector<uint32_t>& args) {
    (void)memory;
    (void)args;
    return STATUS_SUCCESS;
}

} // namespace runtime::core
