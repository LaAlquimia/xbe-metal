#include "runtime/core/memory.hpp"
#include <algorithm>
#include <cmath>

namespace runtime::core {

XboxMemory::XboxMemory(size_t ram_size)
    : m_ram_size(ram_size)
    , m_phys_mask((ram_size == XBOX_RAM_SIZE_128MB) ? XBOX_PHYS_MASK_128MB : XBOX_PHYS_MASK_64MB)
    , m_physical_ram(m_ram_size, 0)
    , m_physical_frames_used(m_ram_size / XBOX_PAGE_SIZE, false)
    , m_page_table(NUM_VIRTUAL_PAGES)
    , m_initialized(true)
{
    // Reserve physical frame 0 (0x00000000 - 0x00000FFF) to safeguard against null pointers
    if (!m_physical_frames_used.empty()) {
        m_physical_frames_used[0] = true;
    }
}

bool XboxMemory::Initialize(size_t ramSize) {
    m_ram_size = ramSize;
    m_phys_mask = (ramSize == XBOX_RAM_SIZE_128MB) ? XBOX_PHYS_MASK_128MB : XBOX_PHYS_MASK_64MB;
    m_physical_ram.assign(m_ram_size, 0);
    m_physical_frames_used.assign(m_ram_size / XBOX_PAGE_SIZE, false);
    m_page_table.assign(NUM_VIRTUAL_PAGES, PageTableEntry{});
    m_current_heap_offset = 0x01000000;

    if (!m_physical_frames_used.empty()) {
        m_physical_frames_used[0] = true;
    }
    m_initialized = true;
    return true;
}

void XboxMemory::Shutdown() {
    reset();
    m_initialized = false;
}

void XboxMemory::reset() {
    std::fill(m_physical_ram.begin(), m_physical_ram.end(), 0);
    std::fill(m_physical_frames_used.begin(), m_physical_frames_used.end(), false);
    if (!m_physical_frames_used.empty()) {
        m_physical_frames_used[0] = true; // Frame 0 reserved
    }
    std::fill(m_page_table.begin(), m_page_table.end(), PageTableEntry{});
    m_current_heap_offset = 0x01000000;
}

size_t XboxMemory::get_allocated_ram_bytes() const noexcept {
    size_t count = 0;
    for (bool used : m_physical_frames_used) {
        if (used) count++;
    }
    return count * XBOX_PAGE_SIZE;
}

uint32_t XboxMemory::allocate_physical_frame() {
    for (size_t i = 1; i < m_physical_frames_used.size(); ++i) {
        if (!m_physical_frames_used[i]) {
            m_physical_frames_used[i] = true;
            std::memset(m_physical_ram.data() + (i * XBOX_PAGE_SIZE), 0, XBOX_PAGE_SIZE);
            return static_cast<uint32_t>(i);
        }
    }
    return 0xFFFFFFFF; // Out of memory
}

void XboxMemory::free_physical_frame(uint32_t frame_index) {
    if (frame_index > 0 && frame_index < m_physical_frames_used.size()) {
        m_physical_frames_used[frame_index] = false;
    }
}

bool XboxMemory::allocate_virtual(uint32_t va, size_t size, uint8_t protection) {
    if (size == 0) return true;

    uint32_t start_page = va / XBOX_PAGE_SIZE;
    uint32_t end_page = (va + size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;

    if (end_page > NUM_VIRTUAL_PAGES) {
        return false;
    }

    for (uint32_t p = start_page; p < end_page; ++p) {
        if (!m_page_table[p].allocated) {
            uint32_t frame = allocate_physical_frame();
            if (frame == 0xFFFFFFFF) {
                // Out of physical memory, rollback previously allocated in this call
                for (uint32_t rollback = start_page; rollback < p; ++rollback) {
                    free_physical_frame(m_page_table[rollback].physical_page);
                    m_page_table[rollback] = PageTableEntry{};
                }
                return false;
            }
            m_page_table[p].physical_page = frame;
            m_page_table[p].allocated = true;
        }
        m_page_table[p].flags = protection;
    }

    return true;
}

bool XboxMemory::free_virtual(uint32_t va, size_t size) {
    if (size == 0) return true;

    uint32_t start_page = va / XBOX_PAGE_SIZE;
    uint32_t end_page = (va + size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;

    if (end_page > NUM_VIRTUAL_PAGES) {
        end_page = static_cast<uint32_t>(NUM_VIRTUAL_PAGES);
    }

    for (uint32_t p = start_page; p < end_page; ++p) {
        if (m_page_table[p].allocated) {
            free_physical_frame(m_page_table[p].physical_page);
            m_page_table[p] = PageTableEntry{};
        }
    }

    return true;
}

bool XboxMemory::protect_virtual(uint32_t va, size_t size, uint8_t protection) {
    if (size == 0) return true;

    uint32_t start_page = va / XBOX_PAGE_SIZE;
    uint32_t end_page = (va + size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;

    if (end_page > NUM_VIRTUAL_PAGES) return false;

    for (uint32_t p = start_page; p < end_page; ++p) {
        if (!m_page_table[p].allocated) {
            return false;
        }
        m_page_table[p].flags = protection;
    }

    return true;
}

uint32_t XboxMemory::allocate_contiguous(size_t size, uint32_t alignment, uint8_t protection) {
    if (size == 0) return 0;

    size_t num_pages = (size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;
    size_t align_pages = std::max<size_t>(1, alignment / XBOX_PAGE_SIZE);

    size_t total_frames = m_ram_size / XBOX_PAGE_SIZE;
    size_t candidate_start = 0xFFFFFFFF;

    for (size_t i = align_pages; i + num_pages <= total_frames; i += align_pages) {
        bool contiguous_free = true;
        for (size_t j = 0; j < num_pages; ++j) {
            if (m_physical_frames_used[i + j]) {
                contiguous_free = false;
                break;
            }
        }
        if (contiguous_free) {
            candidate_start = i;
            break;
        }
    }

    if (candidate_start == 0xFFFFFFFF) {
        return 0;
    }

    for (size_t j = 0; j < num_pages; ++j) {
        m_physical_frames_used[candidate_start + j] = true;
        std::memset(m_physical_ram.data() + ((candidate_start + j) * XBOX_PAGE_SIZE), 0, XBOX_PAGE_SIZE);
    }

    uint32_t phys_addr = static_cast<uint32_t>(candidate_start * XBOX_PAGE_SIZE);
    uint32_t va = XBOX_CACHED_PHYS_BASE + phys_addr;
    return va;
}

bool XboxMemory::free_contiguous(uint32_t va, size_t size) {
    if (size == 0) return true;

    uint32_t phys_addr = virtual_to_physical(va);
    size_t start_frame = phys_addr / XBOX_PAGE_SIZE;
    size_t num_pages = (size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;

    for (size_t j = 0; j < num_pages; ++j) {
        free_physical_frame(static_cast<uint32_t>(start_frame + j));
    }

    return true;
}

uint32_t XboxMemory::Allocate(size_t size, uint32_t alignment) {
    uint32_t aligned_addr = (m_current_heap_offset + alignment - 1) & ~(alignment - 1);
    if (!allocate_virtual(aligned_addr, size, PAGE_PROT_READ | PAGE_PROT_WRITE)) {
        return 0;
    }
    m_current_heap_offset = aligned_addr + static_cast<uint32_t>(size);
    return aligned_addr;
}

void XboxMemory::Free(uint32_t address) {
    (void)address;
}

bool XboxMemory::map_headers(const xbe::XbeHeader& header, std::span<const uint8_t> raw_headers) {
    if (raw_headers.empty()) return false;

    uint32_t va = header.base_address;
    size_t size = header.size_of_headers;

    if (!allocate_virtual(va, size, PAGE_PROT_READ | PAGE_PROT_WRITE)) {
        return false;
    }

    size_t copy_bytes = std::min<size_t>(size, raw_headers.size());
    return write_bytes(va, raw_headers.data(), copy_bytes);
}

bool XboxMemory::map_section(const xbe::XbeSectionHeader& section, std::span<const uint8_t> raw_data) {
    uint32_t va = section.virtual_address;
    uint32_t vsize = section.virtual_size;
    if (vsize == 0) return true;

    uint8_t prot = PAGE_PROT_READ;
    if (section.is_writable())   prot |= PAGE_PROT_WRITE;
    if (section.is_executable()) prot |= PAGE_PROT_EXECUTE;

    if (!allocate_virtual(va, vsize, prot)) {
        return false;
    }

    if (!raw_data.empty()) {
        size_t copy_bytes = std::min<size_t>(vsize, raw_data.size());
        if (!write_bytes(va, raw_data.data(), copy_bytes)) {
            return false;
        }
    }

    return true;
}

bool XboxMemory::is_valid_va(uint32_t va) const noexcept {
    if (va >= XBOX_CACHED_PHYS_BASE && va < (XBOX_CACHED_PHYS_BASE + m_ram_size)) {
        return true;
    }
    if (va >= XBOX_UNCACHED_PHYS_BASE && va < (XBOX_UNCACHED_PHYS_BASE + m_ram_size)) {
        return true;
    }

    uint32_t page_idx = va / XBOX_PAGE_SIZE;
    if (page_idx >= NUM_VIRTUAL_PAGES) return false;

    return m_page_table[page_idx].allocated;
}

bool XboxMemory::is_valid_range(uint32_t va, size_t size, uint8_t required_prot) const noexcept {
    if (size == 0) return true;

    if (va >= XBOX_CACHED_PHYS_BASE && (va + size) <= (XBOX_CACHED_PHYS_BASE + m_ram_size)) {
        return true;
    }
    if (va >= XBOX_UNCACHED_PHYS_BASE && (va + size) <= (XBOX_UNCACHED_PHYS_BASE + m_ram_size)) {
        return true;
    }

    uint32_t start_page = va / XBOX_PAGE_SIZE;
    uint32_t end_page = (va + size + XBOX_PAGE_SIZE - 1) / XBOX_PAGE_SIZE;

    if (end_page > NUM_VIRTUAL_PAGES) return false;

    for (uint32_t p = start_page; p < end_page; ++p) {
        if (!m_page_table[p].allocated) {
            return false;
        }
        if (required_prot != 0 && ((m_page_table[p].flags & required_prot) != required_prot)) {
            return false;
        }
    }

    return true;
}

uint32_t XboxMemory::virtual_to_physical(uint32_t va) const noexcept {
    if (va >= XBOX_CACHED_PHYS_BASE && va < (XBOX_CACHED_PHYS_BASE + m_ram_size)) {
        return (va - XBOX_CACHED_PHYS_BASE) & m_phys_mask;
    }
    if (va >= XBOX_UNCACHED_PHYS_BASE && va < (XBOX_UNCACHED_PHYS_BASE + m_ram_size)) {
        return (va - XBOX_UNCACHED_PHYS_BASE) & m_phys_mask;
    }

    uint32_t page_idx = va / XBOX_PAGE_SIZE;
    if (page_idx < NUM_VIRTUAL_PAGES && m_page_table[page_idx].allocated) {
        uint32_t offset = va % XBOX_PAGE_SIZE;
        return (m_page_table[page_idx].physical_page * XBOX_PAGE_SIZE) + offset;
    }

    return 0xFFFFFFFF;
}

uint8_t* XboxMemory::get_host_pointer(uint32_t va) noexcept {
    if (va >= XBOX_CACHED_PHYS_BASE && va < (XBOX_CACHED_PHYS_BASE + m_ram_size)) {
        uint32_t phys_offset = (va - XBOX_CACHED_PHYS_BASE) & m_phys_mask;
        return m_physical_ram.data() + phys_offset;
    }

    if (va >= XBOX_UNCACHED_PHYS_BASE && va < (XBOX_UNCACHED_PHYS_BASE + m_ram_size)) {
        uint32_t phys_offset = (va - XBOX_UNCACHED_PHYS_BASE) & m_phys_mask;
        return m_physical_ram.data() + phys_offset;
    }

    uint32_t page_idx = va / XBOX_PAGE_SIZE;
    if (page_idx >= NUM_VIRTUAL_PAGES) return nullptr;

    const auto& pte = m_page_table[page_idx];
    if (!pte.allocated || pte.physical_page >= (m_ram_size / XBOX_PAGE_SIZE)) {
        return nullptr;
    }

    uint32_t offset = va % XBOX_PAGE_SIZE;
    return m_physical_ram.data() + (pte.physical_page * XBOX_PAGE_SIZE) + offset;
}

const uint8_t* XboxMemory::get_host_pointer(uint32_t va) const noexcept {
    if (va >= XBOX_CACHED_PHYS_BASE && va < (XBOX_CACHED_PHYS_BASE + m_ram_size)) {
        uint32_t phys_offset = (va - XBOX_CACHED_PHYS_BASE) & m_phys_mask;
        return m_physical_ram.data() + phys_offset;
    }

    if (va >= XBOX_UNCACHED_PHYS_BASE && va < (XBOX_UNCACHED_PHYS_BASE + m_ram_size)) {
        uint32_t phys_offset = (va - XBOX_UNCACHED_PHYS_BASE) & m_phys_mask;
        return m_physical_ram.data() + phys_offset;
    }

    uint32_t page_idx = va / XBOX_PAGE_SIZE;
    if (page_idx >= NUM_VIRTUAL_PAGES) return nullptr;

    const auto& pte = m_page_table[page_idx];
    if (!pte.allocated || pte.physical_page >= (m_ram_size / XBOX_PAGE_SIZE)) {
        return nullptr;
    }

    uint32_t offset = va % XBOX_PAGE_SIZE;
    return m_physical_ram.data() + (pte.physical_page * XBOX_PAGE_SIZE) + offset;
}

bool XboxMemory::read_bytes(uint32_t va, void* dst, size_t size) const noexcept {
    if (size == 0) return true;
    if (!dst) return false;

    uint8_t* out = reinterpret_cast<uint8_t*>(dst);
    size_t bytes_left = size;
    uint32_t current_va = va;

    while (bytes_left > 0) {
        const uint8_t* host_ptr = get_host_pointer(current_va);
        if (!host_ptr) return false;

        size_t page_offset = current_va % XBOX_PAGE_SIZE;
        size_t chunk_size = std::min<size_t>(bytes_left, XBOX_PAGE_SIZE - page_offset);

        std::memcpy(out, host_ptr, chunk_size);

        out += chunk_size;
        current_va += static_cast<uint32_t>(chunk_size);
        bytes_left -= chunk_size;
    }

    return true;
}

bool XboxMemory::write_bytes(uint32_t va, const void* src, size_t size) noexcept {
    if (size == 0) return true;
    if (!src) return false;

    const uint8_t* in = reinterpret_cast<const uint8_t*>(src);
    size_t bytes_left = size;
    uint32_t current_va = va;

    while (bytes_left > 0) {
        uint8_t* host_ptr = get_host_pointer(current_va);
        if (!host_ptr) return false;

        size_t page_offset = current_va % XBOX_PAGE_SIZE;
        size_t chunk_size = std::min<size_t>(bytes_left, XBOX_PAGE_SIZE - page_offset);

        std::memcpy(host_ptr, in, chunk_size);

        in += chunk_size;
        current_va += static_cast<uint32_t>(chunk_size);
        bytes_left -= chunk_size;
    }

    return true;
}

std::string XboxMemory::read_string(uint32_t va, size_t max_len) const {
    std::string result;
    result.reserve(std::min<size_t>(max_len, 64));

    for (size_t i = 0; i < max_len; ++i) {
        uint8_t ch = 0;
        if (!read_bytes(va + static_cast<uint32_t>(i), &ch, 1)) {
            break;
        }
        if (ch == '\0') {
            break;
        }
        result.push_back(static_cast<char>(ch));
    }

    return result;
}

std::u16string XboxMemory::read_u16string(uint32_t va, size_t max_chars) const {
    std::u16string result;
    result.reserve(std::min<size_t>(max_chars, 64));

    for (size_t i = 0; i < max_chars; ++i) {
        char16_t ch = 0;
        if (!read_bytes(va + static_cast<uint32_t>(i * 2), &ch, 2)) {
            break;
        }
        if (ch == 0) {
            break;
        }
        result.push_back(ch);
    }

    return result;
}

} // namespace runtime::core
