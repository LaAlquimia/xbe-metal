#pragma once

#include "runtime/core/memory.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace runtime::core {

// ============================================================================
// NTSTATUS Return Codes
// ============================================================================

constexpr uint32_t STATUS_SUCCESS                = 0x00000000;
constexpr uint32_t STATUS_UNSUCCESSFUL           = 0xC0000001;
constexpr uint32_t STATUS_NOT_IMPLEMENTED        = 0xC0000002;
constexpr uint32_t STATUS_ACCESS_VIOLATION       = 0xC0000005;
constexpr uint32_t STATUS_INVALID_HANDLE         = 0xC0000008;
constexpr uint32_t STATUS_INVALID_PARAMETER      = 0xC000000D;
constexpr uint32_t STATUS_NO_MEMORY              = 0xC0000017;
constexpr uint32_t STATUS_OBJECT_NAME_NOT_FOUND  = 0xC0000034;

// ============================================================================
// Firmware Reboot/Halt Codes (HalReturnToFirmware)
// ============================================================================

enum HalFirmwareRoutine : uint32_t {
    HalHaltRoutine         = 0,
    HalRebootRoutine       = 1,
    HalQuickRebootRoutine  = 2,
    HalKdRebootRoutine     = 3,
    HalFatalErrorReboot    = 4,
};

// ============================================================================
// Kernel Function Handler Prototype
// ============================================================================

using KernelHandler = std::function<uint32_t(XboxMemory& memory, const std::vector<uint32_t>& args)>;

struct KernelExport {
    uint32_t ordinal{0};
    std::string name;
    KernelHandler handler;
};

// ============================================================================
// Xbox Kernel HLE (High-Level Emulation) Engine
// ============================================================================

class XboxKernel {
public:
    XboxKernel();
    explicit XboxKernel(XboxMemory& memory);
    ~XboxKernel() = default;

    /// Register a kernel export with its handler
    void register_export(uint32_t ordinal, std::string name, KernelHandler handler);

    /// Query export metadata
    [[nodiscard]] bool has_export(uint32_t ordinal) const noexcept;
    [[nodiscard]] const std::string& get_export_name(uint32_t ordinal) const;

    /// Dispatch a kernel call by ordinal with given arguments
    uint32_t dispatch(uint32_t ordinal, XboxMemory& memory, const std::vector<uint32_t>& args);
    uint32_t dispatch(uint32_t ordinal, const std::vector<uint32_t>& args);

    /// Set an optional logging sink for kernel debug messages (DbgPrint)
    void set_debug_sink(std::function<void(const std::string&)> sink) {
        m_debug_sink = std::move(sink);
    }

    /// Retrieve total number of registered kernel exports
    [[nodiscard]] size_t get_export_count() const noexcept { return m_exports.size(); }

    /// Helper to install thunks into guest memory
    void install_thunks(XboxMemory& memory, uint32_t thunk_table_va);

private:
    void register_default_handlers();

    XboxMemory* m_memory_ref{nullptr};
    std::unordered_map<uint32_t, KernelExport> m_exports;
    std::string m_unknown_export_str{"UnknownKernelExport"};
    std::function<void(const std::string&)> m_debug_sink;
};

// ============================================================================
// Exported HLE Mock Function Prototypes
// ============================================================================

uint32_t hle_AvGetSavedDataAddress(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_AvSendTVEncoderOption(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_AvSetDisplayMode(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_DbgPrint(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_DbgPrompt(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_HalReturnToFirmware(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_HalWriteSMBusValue(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_HalReadSMBusValue(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeGetCurrentThread(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeGetCurrentProcess(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeInitializeEvent(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeSetEvent(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeResetEvent(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeDelayExecutionThread(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeQueryPerformanceCounter(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeQueryPerformanceFrequency(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeQuerySystemTime(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_KeTickCount(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_MmAllocateContiguousMemory(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_MmAllocateContiguousMemoryEx(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_MmFreeContiguousMemory(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_MmGetPhysicalAddress(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_MmIsAddressValid(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtAllocateVirtualMemory(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtFreeVirtualMemory(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtClose(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtCreateFile(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtOpenFile(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtReadFile(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtWriteFile(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_NtDeviceIoControlFile(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_RtlInitAnsiString(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_RtlZeroMemory(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_XboxKrnlVersion(XboxMemory& memory, const std::vector<uint32_t>& args);
uint32_t hle_XapiInitProcess(XboxMemory& memory, const std::vector<uint32_t>& args);

} // namespace runtime::core

namespace xbe::core {
    using Kernel = runtime::core::XboxKernel;
} // namespace xbe::core
