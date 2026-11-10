#pragma once

#include "xbe/xbe_header.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <string>
#include <memory>
#include <cstring>
#include <stdexcept>

namespace runtime::core {

// ============================================================================
// Memory Constants & Page Flags
// ============================================================================

constexpr size_t XBOX_PAGE_SIZE           = 4096; // 4KB page
constexpr size_t XBOX_RAM_SIZE_64MB       = 64 * 1024 * 1024;  // 64 MB Retail
constexpr size_t XBOX_RAM_SIZE_128MB      = 128 * 1024 * 1024; // 128 MB Devkit

// Physical memory mapping aliases in Xbox 32-bit address space
constexpr uint32_t XBOX_CACHED_PHYS_BASE   = 0x80000000;
constexpr uint32_t XBOX_UNCACHED_PHYS_BASE = 0xA0000000;
constexpr uint32_t XBOX_PHYS_MASK_64MB     = 0x03FFFFFF;
constexpr uint32_t XBOX_PHYS_MASK_128MB    = 0x07FFFFFF;

enum PageProtection : uint8_t {
    PAGE_PROT_NONE    = 0,
    PAGE_PROT_READ    = 1 << 0,
    PAGE_PROT_WRITE   = 1 << 1,
    PAGE_PROT_EXECUTE = 1 << 2,
};

struct PageTableEntry {
    uint32_t physical_page{0xFFFFFFFF}; // Index of physical 4KB frame (0xFFFFFFFF = unmapped)
    uint8_t  flags{0};                   // Bitmask of PageProtection
    bool     allocated{false};           // Whether page is backed by physical RAM
};

// ============================================================================
// Xbox Unified Physical & Virtual Memory Manager
// ============================================================================

class XboxMemory {
public:
    static constexpr size_t kDefaultXboxRAM = XBOX_RAM_SIZE_64MB;
    static constexpr size_t kDevXboxRAM     = XBOX_RAM_SIZE_128MB;
    static constexpr uint32_t kPageSize     = static_cast<uint32_t>(XBOX_PAGE_SIZE);

    explicit XboxMemory(size_t ram_size = XBOX_RAM_SIZE_64MB);
    ~XboxMemory() = default;

    // Non-copyable, movable
    XboxMemory(const XboxMemory&) = delete;
    XboxMemory& operator=(const XboxMemory&) = delete;
    XboxMemory(XboxMemory&&) noexcept = default;
    XboxMemory& operator=(XboxMemory&&) noexcept = default;

    /// Compatibility initialization
    bool Initialize(size_t ramSize = kDefaultXboxRAM);
    void Shutdown();

    /// Reset all physical memory and page mappings
    void reset();

    /// Physical RAM size (64MB or 128MB)
    [[nodiscard]] size_t get_ram_size() const noexcept { return m_ram_size; }
    [[nodiscard]] size_t GetTotalSize() const noexcept { return m_ram_size; }
    [[nodiscard]] size_t get_allocated_ram_bytes() const noexcept;

    // Direct access to base physical RAM buffer
    [[nodiscard]] uint8_t* GetBasePointer() noexcept { return m_physical_ram.data(); }
    [[nodiscard]] const uint8_t* GetBasePointer() const noexcept { return m_physical_ram.data(); }

    // Virtual memory mapping
    bool allocate_virtual(uint32_t va, size_t size, uint8_t protection = PAGE_PROT_READ | PAGE_PROT_WRITE);
    bool free_virtual(uint32_t va, size_t size);
    bool protect_virtual(uint32_t va, size_t size, uint8_t protection);

    // Contiguous physical allocation (used by MmAllocateContiguousMemory)
    uint32_t allocate_contiguous(size_t size, uint32_t alignment = XBOX_PAGE_SIZE, uint8_t protection = PAGE_PROT_READ | PAGE_PROT_WRITE);
    bool free_contiguous(uint32_t va, size_t size);

    // Dynamic allocator compatibility
    uint32_t Allocate(size_t size, uint32_t alignment = kPageSize);
    void Free(uint32_t address);

    // XBE Image Loader mapping
    bool map_headers(const xbe::XbeHeader& header, std::span<const uint8_t> raw_headers);
    bool map_section(const xbe::XbeSectionHeader& section, std::span<const uint8_t> raw_data);

    // Address translation & host pointer resolution
    [[nodiscard]] bool is_valid_va(uint32_t va) const noexcept;
    [[nodiscard]] bool is_valid_range(uint32_t va, size_t size, uint8_t required_prot = 0) const noexcept;
    [[nodiscard]] uint32_t virtual_to_physical(uint32_t va) const noexcept;

    [[nodiscard]] uint8_t* get_host_pointer(uint32_t va) noexcept;
    [[nodiscard]] const uint8_t* get_host_pointer(uint32_t va) const noexcept;

    [[nodiscard]] void* TranslateAddress(uint32_t va) noexcept {
        return static_cast<void*>(get_host_pointer(va));
    }
    [[nodiscard]] const void* TranslateAddress(uint32_t va) const noexcept {
        return static_cast<const void*>(get_host_pointer(va));
    }

    // Typed reads/writes
    template <typename T>
    [[nodiscard]] T read(uint32_t va) const {
        static_assert(std::is_trivially_copyable_v<T>, "Read type must be trivially copyable");
        T val{};
        if (!read_bytes(va, &val, sizeof(T))) {
            throw std::runtime_error("XboxMemory: read violation at 0x" + std::to_string(va));
        }
        return val;
    }

    template <typename T>
    void write(uint32_t va, const T& val) {
        static_assert(std::is_trivially_copyable_v<T>, "Write type must be trivially copyable");
        if (!write_bytes(va, &val, sizeof(T))) {
            throw std::runtime_error("XboxMemory: write violation at 0x" + std::to_string(va));
        }
    }

    uint8_t  Read8(uint32_t address) const  { return read<uint8_t>(address); }
    uint16_t Read16(uint32_t address) const { return read<uint16_t>(address); }
    uint32_t Read32(uint32_t address) const { return read<uint32_t>(address); }
    uint64_t Read64(uint32_t address) const { return read<uint64_t>(address); }

    void Write8(uint32_t address, uint8_t value)   { write<uint8_t>(address, value); }
    void Write16(uint32_t address, uint16_t value) { write<uint16_t>(address, value); }
    void Write32(uint32_t address, uint32_t value) { write<uint32_t>(address, value); }
    void Write64(uint32_t address, uint64_t value) { write<uint64_t>(address, value); }

    // Block operations
    bool read_bytes(uint32_t va, void* dst, size_t size) const noexcept;
    bool write_bytes(uint32_t va, const void* src, size_t size) noexcept;

    bool ReadBlock(uint32_t address, void* dst, size_t size) const noexcept {
        return read_bytes(address, dst, size);
    }
    bool WriteBlock(uint32_t address, const void* src, size_t size) noexcept {
        return write_bytes(address, src, size);
    }

    // String helpers
    [[nodiscard]] std::string read_string(uint32_t va, size_t max_len = 1024) const;
    [[nodiscard]] std::u16string read_u16string(uint32_t va, size_t max_chars = 512) const;

private:
    uint32_t allocate_physical_frame();
    void free_physical_frame(uint32_t frame_index);

    size_t m_ram_size;
    uint32_t m_phys_mask;
    std::vector<uint8_t> m_physical_ram;
    std::vector<bool> m_physical_frames_used;
    uint32_t m_current_heap_offset{0x01000000};

    // 4GB Virtual address space mapped via 4KB pages (1,048,576 pages)
    static constexpr size_t NUM_VIRTUAL_PAGES = 1024 * 1024;
    std::vector<PageTableEntry> m_page_table;
    bool m_initialized{false};
};

} // namespace runtime::core

namespace xbe::core {
    using Memory = runtime::core::XboxMemory;
} // namespace xbe::core
