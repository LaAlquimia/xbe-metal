#pragma once

#include "xbe/xbe_header.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <optional>
#include <memory>

namespace xbe {

/// Encryption key type detected in XBE
enum class XbeKeyType {
    Unknown,
    Retail,
    Debug,
    Plain
};

/// Fully extracted section containing metadata and raw bytes
struct ParsedSection {
    XbeSectionHeader header{};
    std::string name;
    uint32_t virtualAddress{0};
    uint32_t virtualSize{0};
    uint32_t rawAddress{0};
    uint32_t rawSize{0};
    uint32_t flags{0};
    std::vector<uint8_t> data;
};
using XbeSection = ParsedSection;

/// Represents an imported kernel symbol resolved from the thunk table
struct KernelImport {
    uint32_t thunk_address{0}; // Virtual address where pointer/ordinal is stored
    uint32_t ordinal{0};       // Kernel ordinal number (1..N)
    std::string name;          // Human-readable kernel export name
};

class XbeParser {
public:
    XbeParser() = default;
    ~XbeParser() = default;

    /// Load and parse an XBE from disk
    bool load_from_file(const std::filesystem::path& path);
    bool LoadFromFile(const std::string& path) { return load_from_file(path); }

    /// Load and parse an XBE from an in-memory buffer
    bool load_from_memory(std::span<const uint8_t> buffer);
    bool LoadFromMemory(const uint8_t* data, size_t size) {
        if (!data || size == 0) return false;
        return load_from_memory(std::span<const uint8_t>(data, size));
    }

    /// Reset parser state
    void reset();

    // Status queries
    [[nodiscard]] bool is_valid() const noexcept { return m_is_valid; }
    [[nodiscard]] bool IsValid() const noexcept { return m_is_valid; }
    [[nodiscard]] const std::string& get_error() const noexcept { return m_error_message; }

    // Core structures
    [[nodiscard]] const XbeHeader& get_header() const noexcept { return m_header; }
    [[nodiscard]] const XbeHeader& GetHeader() const noexcept { return m_header; }
    [[nodiscard]] const XbeCertificate& get_certificate() const noexcept { return m_certificate; }
    [[nodiscard]] const XbeCertificate& GetCertificate() const noexcept { return m_certificate; }
    [[nodiscard]] std::span<const uint8_t> get_raw_headers() const noexcept;
    [[nodiscard]] std::span<const uint8_t> get_raw_data() const noexcept { return m_raw_data; }

    // Convenience getters
    [[nodiscard]] std::string get_title_name() const { return m_certificate.get_title_name(); }
    [[nodiscard]] std::string GetTitleName() const { return get_title_name(); }
    [[nodiscard]] uint32_t get_title_id() const noexcept { return m_certificate.title_id; }
    [[nodiscard]] uint32_t GetTitleId() const noexcept { return get_title_id(); }

    // Decrypted addresses & key type
    [[nodiscard]] XbeKeyType get_key_type() const noexcept { return m_key_type; }
    [[nodiscard]] const char* get_key_type_name() const noexcept;
    [[nodiscard]] uint32_t get_entry_point() const noexcept { return m_entry_point; }
    [[nodiscard]] uint32_t GetDecodedEntryPoint() const noexcept { return m_entry_point; }
    [[nodiscard]] uint32_t get_kernel_thunk_address() const noexcept { return m_kernel_thunk_address; }

    // Sections
    [[nodiscard]] const std::vector<ParsedSection>& get_sections() const noexcept { return m_sections; }
    [[nodiscard]] const std::vector<ParsedSection>& GetSections() const noexcept { return m_sections; }
    [[nodiscard]] const ParsedSection* find_section(std::string_view name) const noexcept;
    [[nodiscard]] const ParsedSection* find_section_by_va(uint32_t va) const noexcept;

    // Libraries & Imports
    [[nodiscard]] const std::vector<XbeLibraryVersion>& get_libraries() const noexcept { return m_libraries; }
    [[nodiscard]] const std::vector<KernelImport>& get_kernel_imports() const noexcept { return m_kernel_imports; }

    // TLS
    [[nodiscard]] const std::optional<XbeTls>& get_tls() const noexcept { return m_tls; }

    // Address translation: converts a guest virtual address to an offset within the XBE file
    [[nodiscard]] std::optional<size_t> virtual_to_file_offset(uint32_t va) const noexcept;

private:
    bool parse_internal();
    bool validate_header();
    void detect_keys_and_decode();
    bool parse_certificate();
    bool parse_sections();
    bool parse_libraries();
    bool parse_tls();
    bool parse_kernel_thunks();

    std::vector<uint8_t> m_raw_data;
    XbeHeader m_header{};
    XbeCertificate m_certificate{};
    std::optional<XbeTls> m_tls;
    std::vector<ParsedSection> m_sections;
    std::vector<XbeLibraryVersion> m_libraries;
    std::vector<KernelImport> m_kernel_imports;

    XbeKeyType m_key_type{XbeKeyType::Unknown};
    uint32_t m_entry_point{0};
    uint32_t m_kernel_thunk_address{0};

    bool m_is_valid{false};
    std::string m_error_message;
};

// Backwards-compatibility alias
using XbeFile = XbeParser;

} // namespace xbe
