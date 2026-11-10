#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>

namespace xbe {

// ============================================================================
// Constants and Magic Numbers
// ============================================================================

/// Magic identifier for Xbox Executable ('XBEH' in little-endian: 0x48454258)
constexpr uint32_t XBE_MAGIC = 0x48454258;
constexpr uint32_t kXbeMagic = 0x48454258;

/// XOR Keys for Entry Point and Thunk decoding
constexpr uint32_t XBE_RETAIL_ENTRY_POINT_XOR_KEY = 0xA8FC57AB;
constexpr uint32_t XBE_RETAIL_THUNK_XOR_KEY       = 0x5B6D40B6;

constexpr uint32_t XBE_DEBUG_ENTRY_POINT_XOR_KEY  = 0x94859D4B;
constexpr uint32_t XBE_DEBUG_THUNK_XOR_KEY        = 0xEFB1F152;

// Compatibility aliases
constexpr uint32_t kRetailEntryPointKey = XBE_RETAIL_ENTRY_POINT_XOR_KEY;
constexpr uint32_t kDebugEntryPointKey  = XBE_DEBUG_ENTRY_POINT_XOR_KEY;

// ============================================================================
// Initialization Flags
// ============================================================================

enum XbeInitFlags : uint32_t {
    XBE_INIT_MOUNT_UTILITY_DRIVE  = 0x00000001,
    XBE_INIT_FORMAT_UTILITY_DRIVE = 0x00000002,
    XBE_INIT_LIMIT_64MB           = 0x00000004,
    XBE_INIT_DONT_SETUP_HARDDISK  = 0x00000008,
};

// ============================================================================
// Allowed Media Types
// ============================================================================

enum XbeAllowedMedia : uint32_t {
    XBE_MEDIA_HARD_DISK           = 0x00000001,
    XBE_MEDIA_DVD_X2              = 0x00000002,
    XBE_MEDIA_DVD_CD              = 0x00000004,
    XBE_MEDIA_CD                  = 0x00000008,
    XBE_MEDIA_DVD_5_RO            = 0x00000010,
    XBE_MEDIA_DVD_9_RO            = 0x00000020,
    XBE_MEDIA_DVD_5_RW            = 0x00000040,
    XBE_MEDIA_DVD_9_RW            = 0x00000080,
    XBE_MEDIA_DONGLE              = 0x00000100,
    XBE_MEDIA_MEDIA_BOARD         = 0x00000200,
    XBE_MEDIA_NONSECURE_HARD_DISK = 0x40000000,
    XBE_MEDIA_NONSECURE_MODE      = 0x80000000,
};

// ============================================================================
// Game Region Bitmask
// ============================================================================

enum XbeGameRegion : uint32_t {
    XBE_REGION_US_CANADA     = 0x00000001, // NTSC-U
    XBE_REGION_JAPAN         = 0x00000002, // NTSC-J
    XBE_REGION_REST_OF_WORLD = 0x00000004, // PAL / Europe / Australia
    XBE_REGION_MANUFACTURING = 0x80000000, // Debug / Devkit
};

// ============================================================================
// Section Flags
// ============================================================================

enum XbeSectionFlags : uint32_t {
    XBE_SECTION_WRITABLE             = 0x00000001,
    XBE_SECTION_PRELOAD              = 0x00000002,
    XBE_SECTION_EXECUTABLE           = 0x00000004,
    XBE_SECTION_INSERTED_FILE        = 0x00000008,
    XBE_SECTION_HEAD_PAGE_READ_ONLY  = 0x00000010,
    XBE_SECTION_TAIL_PAGE_READ_ONLY  = 0x00000020,
};

// ============================================================================
// Structure Definitions (Packed to 1 byte for exact binary alignment)
// ============================================================================

#pragma pack(push, 1)

/// XBE Image Header (Standard 376 bytes / 0x178)
struct XbeHeader {
    union { uint32_t magic; uint32_t Magic; };
    uint8_t  signature[256];
    union { uint32_t base_address; uint32_t baseAddress; uint32_t BaseAddress; };
    union { uint32_t size_of_headers; uint32_t sizeOfHeaders; uint32_t SizeOfHeaders; };
    union { uint32_t size_of_image; uint32_t sizeOfImage; uint32_t SizeOfImage; };
    union { uint32_t size_of_image_header; uint32_t sizeOfImageHeader; uint32_t SizeOfImageHeader; };
    union { uint32_t time_date_stamp; uint32_t timeDateStamp; uint32_t TimeDateStamp; };
    union { uint32_t certificate_address; uint32_t certificateAddress; uint32_t CertificateAddress; };
    union { uint32_t number_of_sections; uint32_t numberOfSections; uint32_t NumberOfSections; };
    union { uint32_t section_headers_address; uint32_t sectionHeadersAddress; uint32_t SectionHeadersAddress; };
    union { uint32_t init_flags; uint32_t initFlags; uint32_t InitFlags; };
    union { uint32_t entry_point; uint32_t entryPoint; uint32_t EntryPoint; };
    union { uint32_t tls_address; uint32_t tlsAddress; uint32_t TLSAddress; };
    union { uint32_t stack_commit; uint32_t stackCommit; uint32_t StackCommit; };
    union { uint32_t heap_reserve; uint32_t heapReserve; uint32_t HeapReserve; };
    union { uint32_t heap_commit; uint32_t heapCommit; uint32_t HeapCommit; };
    union { uint32_t pe_base_address; uint32_t peBaseAddress; uint32_t PeBaseAddress; };
    union { uint32_t pe_size_of_image; uint32_t peSizeOfImage; uint32_t PeSizeOfImage; };
    union { uint32_t pe_checksum; uint32_t peChecksum; uint32_t PeChecksum; };
    union { uint32_t pe_time_date_stamp; uint32_t peTimeDateStamp; uint32_t PeTimeDateStamp; };
    union { uint32_t debug_path_name_address; uint32_t debugPathNameAddress; uint32_t DebugPathNameAddress; };
    union { uint32_t debug_file_name_address; uint32_t debugFileNameAddress; uint32_t DebugFileNameAddress; };
    union { uint32_t debug_unicode_file_name_address; uint32_t debugUnicodeFileNameAddress; uint32_t DebugUnicodeFileNameAddress; };
    union { uint32_t kernel_image_thunk_address; uint32_t kernelImageThunkAddress; uint32_t KernelImageThunkAddress; };
    union { uint32_t non_kernel_import_directory_address; uint32_t nonKernelImportDirectoryAddress; uint32_t NonKernelImportDirectoryAddress; };
    union { uint32_t number_of_library_versions; uint32_t numberOfLibraryVersions; uint32_t NumberOfLibraryVersions; };
    union { uint32_t library_versions_address; uint32_t libraryVersionsAddress; uint32_t LibraryVersionsAddress; };
    union { uint32_t kernel_library_version_address; uint32_t kernelLibraryVersionAddress; uint32_t KernelLibraryVersionAddress; };
    union { uint32_t xapi_library_version_address; uint32_t xapiLibraryVersionAddress; uint32_t XapiLibraryVersionAddress; };
    union { uint32_t microsoft_logo_address; uint32_t microsoftLogoAddress; uint32_t MicrosoftLogoAddress; };
    union { uint32_t microsoft_logo_size; uint32_t microsoftLogoSize; uint32_t MicrosoftLogoSize; };
};
static_assert(sizeof(XbeHeader) == 0x0178, "XbeHeader size must be 0x178 (376 bytes)");

/// XBE Certificate (Standard 464 bytes / 0x1D0)
struct XbeCertificate {
    union { uint32_t size; uint32_t Size; };
    union { uint32_t time_date_stamp; uint32_t timeDateStamp; uint32_t TimeDateStamp; };
    union { uint32_t title_id; uint32_t titleId; uint32_t TitleID; };
    union { char16_t title_name[40]; char16_t titleName[40]; char16_t TitleName[40]; };
    union { uint32_t alt_title_ids[16]; uint32_t altTitleIds[16]; uint32_t AlternativeTitleIDs[16]; };
    union { uint32_t allowed_media; uint32_t allowedMedia; uint32_t AllowedMedia; };
    union { uint32_t game_region; uint32_t gameRegion; uint32_t GameRegion; };
    union { uint32_t game_ratings; uint32_t gameRatings; uint32_t GameRatings; };
    union { uint32_t disk_number; uint32_t diskNumber; uint32_t DiskNumber; };
    union { uint32_t version; uint32_t Version; };
    union { uint8_t  lan_key[16]; uint8_t lanKey[16]; uint8_t LanKey[16]; };
    union { uint8_t  signature_key[16]; uint8_t sigKey[16]; uint8_t SignatureKey[16]; };
    union { uint8_t  title_alt_signature_keys[16][16]; uint8_t altSigKeys[16][16]; uint8_t TitleAltSignatureKeys[16][16]; };

    /// Converts UTF-16LE title_name into a standard UTF-8 std::string
    std::string get_title_name() const {
        std::string result;
        result.reserve(40);
        for (int i = 0; i < 40; ++i) {
            char16_t c = title_name[i];
            if (c == 0) break;
            if (c < 0x80) {
                result.push_back(static_cast<char>(c));
            } else if (c < 0x800) {
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            } else {
                result.push_back(static_cast<char>(0xE0 | (c >> 12)));
                result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
        return result;
    }

    std::string GetTitleName() const { return get_title_name(); }

    /// Returns Title ID formatted as hex and ASCII publisher code if readable
    std::string get_formatted_title_id() const {
        std::ostringstream ss;
        char publisher[3] = {
            static_cast<char>((title_id >> 24) & 0xFF),
            static_cast<char>((title_id >> 16) & 0xFF),
            '\0'
        };
        uint16_t title_num = static_cast<uint16_t>(title_id & 0xFFFF);

        bool printable = ((publisher[0] >= 32 && publisher[0] <= 126) &&
                          (publisher[1] >= 32 && publisher[1] <= 126));

        ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << title_id;
        if (printable) {
            ss << " [" << publisher[0] << publisher[1] << "-" << std::dec << std::setw(3) << std::setfill('0') << title_num << "]";
        }
        return ss.str();
    }

    /// Decodes allowed media bitmask into human-readable flags
    std::vector<std::string> get_allowed_media_list() const {
        std::vector<std::string> media;
        if (allowed_media & XBE_MEDIA_HARD_DISK)           media.push_back("HardDisk");
        if (allowed_media & XBE_MEDIA_DVD_X2)              media.push_back("DVD-X2");
        if (allowed_media & XBE_MEDIA_DVD_CD)              media.push_back("DVD-CD");
        if (allowed_media & XBE_MEDIA_CD)                  media.push_back("CD");
        if (allowed_media & XBE_MEDIA_DVD_5_RO)            media.push_back("DVD-5-RO");
        if (allowed_media & XBE_MEDIA_DVD_9_RO)            media.push_back("DVD-9-RO");
        if (allowed_media & XBE_MEDIA_DVD_5_RW)            media.push_back("DVD-5-RW");
        if (allowed_media & XBE_MEDIA_DVD_9_RW)            media.push_back("DVD-9-RW");
        if (allowed_media & XBE_MEDIA_DONGLE)              media.push_back("Dongle");
        if (allowed_media & XBE_MEDIA_MEDIA_BOARD)         media.push_back("MediaBoard");
        if (allowed_media & XBE_MEDIA_NONSECURE_HARD_DISK) media.push_back("NonsecureHardDisk");
        if (allowed_media & XBE_MEDIA_NONSECURE_MODE)      media.push_back("NonsecureMode");
        return media;
    }

    /// Decodes game region bitmask into human-readable flags
    std::vector<std::string> get_game_regions_list() const {
        std::vector<std::string> regions;
        if (game_region & XBE_REGION_US_CANADA)     regions.push_back("North America (NTSC-U)");
        if (game_region & XBE_REGION_JAPAN)         regions.push_back("Japan (NTSC-J)");
        if (game_region & XBE_REGION_REST_OF_WORLD) regions.push_back("Europe / Rest of World (PAL)");
        if (game_region & XBE_REGION_MANUFACTURING) regions.push_back("Manufacturing / Debug");
        return regions;
    }
};
static_assert(sizeof(XbeCertificate) == 0x1D0, "XbeCertificate size must be 0x1D0 (464 bytes)");

/// XBE Section Header (56 bytes / 0x38)
struct XbeSectionHeader {
    union { uint32_t flags; uint32_t Flags; };
    union { uint32_t virtual_address; uint32_t virtualAddress; uint32_t VirtualAddress; };
    union { uint32_t virtual_size; uint32_t virtualSize; uint32_t VirtualSize; };
    union { uint32_t raw_address; uint32_t rawAddress; uint32_t RawAddress; };
    union { uint32_t raw_size; uint32_t rawSize; uint32_t RawSize; };
    union { uint32_t section_name_address; uint32_t sectionNameAddress; uint32_t SectionNameAddress; };
    union { uint32_t section_reference_count; uint32_t sectionNameRefCount; uint32_t SectionReferenceCount; };
    union { uint32_t head_shared_page_ref_address; uint32_t HeadSharedPageRefAddress; };
    union { uint32_t tail_shared_page_ref_address; uint32_t TailSharedPageRefAddress; };
    union { uint8_t  section_digest[20]; uint8_t digest[20]; uint8_t SectionDigest[20]; };

    [[nodiscard]] constexpr bool is_writable() const noexcept {
        return (flags & XBE_SECTION_WRITABLE) != 0;
    }
    [[nodiscard]] constexpr bool is_preload() const noexcept {
        return (flags & XBE_SECTION_PRELOAD) != 0;
    }
    [[nodiscard]] constexpr bool is_executable() const noexcept {
        return (flags & XBE_SECTION_EXECUTABLE) != 0;
    }
    [[nodiscard]] constexpr bool is_inserted_file() const noexcept {
        return (flags & XBE_SECTION_INSERTED_FILE) != 0;
    }
    [[nodiscard]] constexpr bool is_head_page_read_only() const noexcept {
        return (flags & XBE_SECTION_HEAD_PAGE_READ_ONLY) != 0;
    }
    [[nodiscard]] constexpr bool is_tail_page_read_only() const noexcept {
        return (flags & XBE_SECTION_TAIL_PAGE_READ_ONLY) != 0;
    }

    /// Returns a short string summarizing the permissions and attributes (e.g. "RWX Preload")
    std::string get_flags_string() const {
        std::string s;
        s += "R";
        s += is_writable() ? "W" : "-";
        s += is_executable() ? "X" : "-";
        if (is_preload()) s += " [Preload]";
        if (is_inserted_file()) s += " [Inserted]";
        if (is_head_page_read_only()) s += " [HeadRO]";
        if (is_tail_page_read_only()) s += " [TailRO]";
        return s;
    }
};
static_assert(sizeof(XbeSectionHeader) == 0x38, "XbeSectionHeader size must be 0x38 (56 bytes)");

/// Thread Local Storage Directory
struct XbeTls {
    uint32_t data_start_address;
    uint32_t data_end_address;
    uint32_t tls_index_address;
    uint32_t tls_callback_address;
    uint32_t zero_fill_size;
    uint32_t characteristics;
};
static_assert(sizeof(XbeTls) == 24, "XbeTls size must be 24 bytes");

/// Library Version Entry (16 bytes)
struct XbeLibraryVersion {
    char     name[8];
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t build_version;
    uint16_t flags;

    std::string get_name() const {
        std::string s;
        for (int i = 0; i < 8 && name[i] != '\0'; ++i) {
            s.push_back(name[i]);
        }
        return s;
    }

    std::string get_formatted_version() const {
        std::ostringstream ss;
        ss << major_version << "." << minor_version << "." << build_version;
        if (is_debug_build()) {
            ss << " (Debug)";
        }
        return ss.str();
    }

    [[nodiscard]] constexpr bool is_debug_build() const noexcept {
        return (flags & 0x8000) != 0;
    }
};
static_assert(sizeof(XbeLibraryVersion) == 16, "XbeLibraryVersion size must be 16 bytes");

#pragma pack(pop)

} // namespace xbe
