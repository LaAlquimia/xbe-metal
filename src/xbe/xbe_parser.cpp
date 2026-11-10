#include "xbe/xbe_parser.hpp"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace xbe {

void XbeParser::reset() {
    m_raw_data.clear();
    m_header = {};
    m_certificate = {};
    m_tls.reset();
    m_sections.clear();
    m_libraries.clear();
    m_kernel_imports.clear();
    m_key_type = XbeKeyType::Unknown;
    m_entry_point = 0;
    m_kernel_thunk_address = 0;
    m_is_valid = false;
    m_error_message.clear();
}

bool XbeParser::load_from_file(const std::filesystem::path& path) {
    reset();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_error_message = "Could not open file: " + path.string();
        return false;
    }

    std::streamsize size = file.tellg();
    if (size < static_cast<std::streamsize>(sizeof(XbeHeader))) {
        m_error_message = "File is smaller than XBE header: " + path.string();
        return false;
    }

    file.seekg(0, std::ios::beg);
    m_raw_data.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_raw_data.data()), size)) {
        m_error_message = "Failed reading binary data from: " + path.string();
        return false;
    }

    return parse_internal();
}

bool XbeParser::load_from_memory(std::span<const uint8_t> buffer) {
    reset();

    if (buffer.size() < sizeof(XbeHeader)) {
        m_error_message = "Buffer size is smaller than XBE header";
        return false;
    }

    m_raw_data.assign(buffer.begin(), buffer.end());
    return parse_internal();
}

std::span<const uint8_t> XbeParser::get_raw_headers() const noexcept {
    if (m_raw_data.size() >= m_header.size_of_headers) {
        return std::span<const uint8_t>(m_raw_data.data(), m_header.size_of_headers);
    }
    return {};
}

const char* XbeParser::get_key_type_name() const noexcept {
    switch (m_key_type) {
        case XbeKeyType::Retail:  return "Retail";
        case XbeKeyType::Debug:   return "Debug";
        case XbeKeyType::Plain:   return "Plain / Unencrypted";
        case XbeKeyType::Unknown:
        default:                  return "Unknown";
    }
}

const ParsedSection* XbeParser::find_section(std::string_view name) const noexcept {
    for (const auto& sec : m_sections) {
        if (sec.name == name) return &sec;
    }
    return nullptr;
}

const ParsedSection* XbeParser::find_section_by_va(uint32_t va) const noexcept {
    for (const auto& sec : m_sections) {
        if (va >= sec.header.virtual_address && va < (sec.header.virtual_address + sec.header.virtual_size)) {
            return &sec;
        }
    }
    return nullptr;
}

std::optional<size_t> XbeParser::virtual_to_file_offset(uint32_t va) const noexcept {
    // Check if within header region
    if (va >= m_header.base_address && va < (m_header.base_address + m_header.size_of_headers)) {
        size_t offset = va - m_header.base_address;
        if (offset < m_raw_data.size()) return offset;
    }

    // Check across mapped sections
    for (const auto& sec : m_sections) {
        if (va >= sec.header.virtual_address && va < (sec.header.virtual_address + sec.header.virtual_size)) {
            uint32_t offset_in_sec = va - sec.header.virtual_address;
            if (offset_in_sec < sec.header.raw_size) {
                size_t file_offset = sec.header.raw_address + offset_in_sec;
                if (file_offset < m_raw_data.size()) return file_offset;
            }
        }
    }

    // Direct fallback mapping
    if (va >= m_header.base_address) {
        size_t off = va - m_header.base_address;
        if (off < m_raw_data.size()) return off;
    }

    return std::nullopt;
}

bool XbeParser::parse_internal() {
    if (!validate_header()) return false;
    detect_keys_and_decode();
    if (!parse_certificate()) return false;
    if (!parse_sections()) return false;
    parse_libraries();
    parse_tls();
    parse_kernel_thunks();

    m_is_valid = true;
    return true;
}

bool XbeParser::validate_header() {
    if (m_raw_data.size() < sizeof(XbeHeader)) {
        m_error_message = "File too small for XbeHeader";
        return false;
    }

    std::memcpy(&m_header, m_raw_data.data(), sizeof(XbeHeader));
    if (m_header.magic != XBE_MAGIC) {
        m_error_message = "Invalid magic bytes (expected 'XBEH')";
        return false;
    }

    return true;
}

void XbeParser::detect_keys_and_decode() {
    uint32_t retail_ep = m_header.entry_point ^ XBE_RETAIL_ENTRY_POINT_XOR_KEY;
    uint32_t debug_ep  = m_header.entry_point ^ XBE_DEBUG_ENTRY_POINT_XOR_KEY;

    uint32_t img_start = m_header.base_address;
    uint32_t img_end   = m_header.base_address + m_header.size_of_image;

    if (retail_ep >= img_start && retail_ep < img_end) {
        m_key_type = XbeKeyType::Retail;
        m_entry_point = retail_ep;
        m_kernel_thunk_address = m_header.kernel_image_thunk_address ^ XBE_RETAIL_THUNK_XOR_KEY;
    } else if (debug_ep >= img_start && debug_ep < img_end) {
        m_key_type = XbeKeyType::Debug;
        m_entry_point = debug_ep;
        m_kernel_thunk_address = m_header.kernel_image_thunk_address ^ XBE_DEBUG_THUNK_XOR_KEY;
    } else {
        m_key_type = XbeKeyType::Plain;
        m_entry_point = m_header.entry_point;
        m_kernel_thunk_address = m_header.kernel_image_thunk_address;
    }
}

bool XbeParser::parse_certificate() {
    auto off = virtual_to_file_offset(m_header.certificate_address);
    if (!off || *off + sizeof(XbeCertificate) > m_raw_data.size()) {
        m_error_message = "Certificate address out of bounds";
        return false;
    }

    std::memcpy(&m_certificate, m_raw_data.data() + *off, sizeof(XbeCertificate));
    return true;
}

bool XbeParser::parse_sections() {
    m_sections.clear();
    auto sec_offset = virtual_to_file_offset(m_header.section_headers_address);
    if (!sec_offset) {
        m_error_message = "Section headers address out of bounds";
        return false;
    }

    for (uint32_t i = 0; i < m_header.number_of_sections; ++i) {
        size_t entry_offset = *sec_offset + (i * sizeof(XbeSectionHeader));
        if (entry_offset + sizeof(XbeSectionHeader) > m_raw_data.size()) break;

        ParsedSection sec{};
        std::memcpy(&sec.header, m_raw_data.data() + entry_offset, sizeof(XbeSectionHeader));
        sec.virtualAddress = sec.header.virtual_address;
        sec.virtualSize = sec.header.virtual_size;
        sec.rawAddress = sec.header.raw_address;
        sec.rawSize = sec.header.raw_size;
        sec.flags = sec.header.flags;

        // Resolve name
        auto name_off = virtual_to_file_offset(sec.header.section_name_address);
        if (name_off && *name_off < m_raw_data.size()) {
            sec.name = reinterpret_cast<const char*>(m_raw_data.data() + *name_off);
        } else {
            sec.name = "section_" + std::to_string(i);
        }

        // Section payload
        if (sec.header.raw_address + sec.header.raw_size <= m_raw_data.size()) {
            sec.data.assign(
                m_raw_data.data() + sec.header.raw_address,
                m_raw_data.data() + sec.header.raw_address + sec.header.raw_size
            );
        }

        m_sections.push_back(std::move(sec));
    }

    return true;
}

bool XbeParser::parse_libraries() {
    m_libraries.clear();
    if (m_header.number_of_library_versions == 0 || m_header.library_versions_address == 0) {
        return true;
    }

    auto lib_off = virtual_to_file_offset(m_header.library_versions_address);
    if (!lib_off) return true;

    for (uint32_t i = 0; i < m_header.number_of_library_versions; ++i) {
        size_t entry_off = *lib_off + (i * sizeof(XbeLibraryVersion));
        if (entry_off + sizeof(XbeLibraryVersion) > m_raw_data.size()) break;

        XbeLibraryVersion lib{};
        std::memcpy(&lib, m_raw_data.data() + entry_off, sizeof(XbeLibraryVersion));
        m_libraries.push_back(lib);
    }

    return true;
}

bool XbeParser::parse_tls() {
    m_tls.reset();
    if (m_header.tls_address == 0) return true;

    auto tls_off = virtual_to_file_offset(m_header.tls_address);
    if (!tls_off || *tls_off + sizeof(XbeTls) > m_raw_data.size()) return true;

    XbeTls tls{};
    std::memcpy(&tls, m_raw_data.data() + *tls_off, sizeof(XbeTls));
    m_tls = tls;
    return true;
}

static std::string lookup_kernel_ordinal_name(uint32_t ordinal) {
    switch (ordinal) {
        case 1:   return "AvGetSavedDataAddress";
        case 2:   return "AvSendTVEncoderOption";
        case 8:   return "AvSetDisplayMode";
        case 9:   return "AvSetSavedDataAddress";
        case 12:  return "DbgPrint";
        case 13:  return "DbgPrompt";
        case 14:  return "DbgPrint";
        case 49:  return "HalReturnToFirmware";
        case 55:  return "HalWriteSMBusValue";
        case 56:  return "HalReadSMBusValue";
        case 104: return "KeDelayExecutionThread";
        case 106: return "KeGetCurrentThread";
        case 107: return "KeGetCurrentProcess";
        case 119: return "KeInitializeEvent";
        case 133: return "KeQueryPerformanceCounter";
        case 134: return "KeQueryPerformanceFrequency";
        case 135: return "KeQuerySystemTime";
        case 139: return "KeResetEvent";
        case 148: return "KeSetEvent";
        case 153: return "KeTickCount";
        case 165: return "MmAllocateContiguousMemory";
        case 166: return "MmAllocateContiguousMemoryEx";
        case 168: return "MmFreeContiguousMemory";
        case 170: return "MmGetPhysicalAddress";
        case 172: return "MmIsAddressValid";
        case 184: return "NtAllocateVirtualMemory";
        case 187: return "NtClose";
        case 190: return "NtCreateFile";
        case 194: return "NtDeviceIoControlFile";
        case 199: return "NtFreeVirtualMemory";
        case 203: return "NtOpenFile";
        case 219: return "NtReadFile";
        case 237: return "NtWriteFile";
        case 247: return "RtlInitAnsiString";
        case 290: return "RtlZeroMemory";
        case 325: return "XboxKrnlVersion";
        case 350: return "XapiInitProcess";
        default:  return "Ordinal_" + std::to_string(ordinal);
    }
}

bool XbeParser::parse_kernel_thunks() {
    m_kernel_imports.clear();
    if (m_kernel_thunk_address == 0) return true;

    auto thunk_off = virtual_to_file_offset(m_kernel_thunk_address);
    if (!thunk_off) return true;

    size_t cur = *thunk_off;
    while (cur + 4 <= m_raw_data.size()) {
        uint32_t raw_thunk = 0;
        std::memcpy(&raw_thunk, m_raw_data.data() + cur, 4);
        if (raw_thunk == 0) break;

        // Xbox kernel import thunks have MSB set: 0x80000000 | ordinal
        if (raw_thunk & 0x80000000) {
            uint32_t ordinal = raw_thunk & 0x7FFFFFFF;
            m_kernel_imports.push_back(KernelImport{
                .thunk_address = static_cast<uint32_t>(m_kernel_thunk_address + (m_kernel_imports.size() * 4)),
                .ordinal = ordinal,
                .name = lookup_kernel_ordinal_name(ordinal)
            });
        }
        cur += 4;
    }

    return true;
}

} // namespace xbe
