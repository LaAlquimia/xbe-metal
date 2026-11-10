#include "xbe/xbe_parser.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <unistd.h>

namespace fs = std::filesystem;

// ============================================================================
// ANSI Color Formatting Helpers
// ============================================================================

struct Colors {
    bool enabled{true};

    const char* reset()   const noexcept { return enabled ? "\033[0m" : ""; }
    const char* bold()    const noexcept { return enabled ? "\033[1m" : ""; }
    const char* dim()     const noexcept { return enabled ? "\033[2m" : ""; }
    const char* red()     const noexcept { return enabled ? "\033[31m" : ""; }
    const char* green()   const noexcept { return enabled ? "\033[32m" : ""; }
    const char* yellow()  const noexcept { return enabled ? "\033[33m" : ""; }
    const char* blue()    const noexcept { return enabled ? "\033[34m" : ""; }
    const char* magenta() const noexcept { return enabled ? "\033[35m" : ""; }
    const char* cyan()    const noexcept { return enabled ? "\033[36m" : ""; }
    const char* white()   const noexcept { return enabled ? "\033[37m" : ""; }
};

static std::string format_hex32(uint32_t val) {
    std::ostringstream ss;
    ss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << val;
    return ss.str();
}

static std::string format_size(uint64_t bytes) {
    std::ostringstream ss;
    if (bytes >= 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (double)bytes / (1024.0 * 1024.0) << " MB";
    } else if (bytes >= 1024) {
        ss << std::fixed << std::setprecision(2) << (double)bytes / 1024.0 << " KB";
    } else {
        ss << bytes << " B";
    }
    return ss.str();
}

static std::string format_hex_digest(const uint8_t* digest, size_t len) {
    std::ostringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

static std::string format_timestamp(uint32_t timestamp) {
    if (timestamp == 0) return "N/A";
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm_buf);
    return std::string(buf);
}

static void print_banner(const Colors& c) {
    std::cout << c.cyan() << c.bold()
              << "╔═══════════════════════════════════════════════════════════════════════╗\n"
              << "║                   xbe-dump - Xbox Executable Inspector                ║\n"
              << "║                       Part of the xbe-metal Project                   ║\n"
              << "╚═══════════════════════════════════════════════════════════════════════╝\n"
              << c.reset() << "\n";
}

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [options] <path-to-xbe>\n\n"
              << "Options:\n"
              << "  -h, --help      Display this help menu\n"
              << "  -v, --verbose   Show full details (libraries, thunks, digests)\n"
              << "  --no-color      Disable ANSI color codes\n"
              << "  --imports       Display the imported kernel thunk table\n"
              << "  --libs          Display linked Xbox SDK library versions\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    Colors col;
    if (!isatty(fileno(stdout))) {
        col.enabled = false;
    }

    bool verbose = false;
    bool show_imports = false;
    bool show_libs = false;
    std::string xbe_path_str;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
            show_imports = true;
            show_libs = true;
        } else if (arg == "--no-color") {
            col.enabled = false;
        } else if (arg == "--imports") {
            show_imports = true;
        } else if (arg == "--libs") {
            show_libs = true;
        } else if (arg[0] != '-') {
            xbe_path_str = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (xbe_path_str.empty()) {
        std::cerr << "Error: No .xbe file path provided.\n";
        print_usage(argv[0]);
        return 1;
    }

    fs::path xbe_path(xbe_path_str);
    if (!fs::exists(xbe_path)) {
        std::cerr << col.red() << "Error: File not found: " << xbe_path_str << col.reset() << "\n";
        return 1;
    }

    xbe::XbeParser parser;
    if (!parser.load_from_file(xbe_path)) {
        std::cerr << col.red() << "Error parsing XBE: " << parser.get_error() << col.reset() << "\n";
        return 1;
    }

    print_banner(col);

    const auto& hdr = parser.get_header();
    const auto& cert = parser.get_certificate();

    // 1. Basic File & Image Header Summary
    std::cout << col.bold() << col.yellow() << "▶ EXECUTABLE HEADER" << col.reset() << "\n";
    std::cout << "  File:               " << col.white() << xbe_path.filename().string() << col.reset()
              << " (" << format_size(fs::file_size(xbe_path)) << ")\n";
    std::cout << "  Timestamp:          " << format_timestamp(hdr.time_date_stamp) << "\n";
    std::cout << "  Base Address:       " << col.magenta() << format_hex32(hdr.base_address) << col.reset() << "\n";
    std::cout << "  Size of Headers:    " << format_hex32(hdr.size_of_headers) << " (" << format_size(hdr.size_of_headers) << ")\n";
    std::cout << "  Size of Image:      " << format_hex32(hdr.size_of_image) << " (" << format_size(hdr.size_of_image) << ")\n";
    std::cout << "  Encryption Key:     " << col.green() << col.bold() << parser.get_key_type_name() << col.reset() << "\n";
    std::cout << "  Entry Point:        " << col.magenta() << format_hex32(parser.get_entry_point()) << col.reset()
              << " (Encoded: " << format_hex32(hdr.entry_point) << ")\n";
    std::cout << "  Kernel Thunks:      " << col.magenta() << format_hex32(parser.get_kernel_thunk_address()) << col.reset()
              << " (Encoded: " << format_hex32(hdr.kernel_image_thunk_address) << ")\n";
    std::cout << "  Init Flags:         " << format_hex32(hdr.init_flags) << "\n\n";

    // 2. Certificate Details
    std::cout << col.bold() << col.yellow() << "▶ CERTIFICATE INFORMATION" << col.reset() << "\n";
    std::cout << "  Title Name:         " << col.green() << col.bold() << cert.get_title_name() << col.reset() << "\n";
    std::cout << "  Title ID:           " << col.cyan() << cert.get_formatted_title_id() << col.reset() << "\n";
    std::cout << "  Version:            " << cert.version << " (Disk #" << cert.disk_number << ")\n";

    auto regions = cert.get_game_regions_list();
    std::cout << "  Region:             ";
    if (regions.empty()) {
        std::cout << "None / Region-Free\n";
    } else {
        for (size_t i = 0; i < regions.size(); ++i) {
            std::cout << regions[i] << (i + 1 < regions.size() ? ", " : "\n");
        }
    }

    auto media = cert.get_allowed_media_list();
    std::cout << "  Allowed Media:      ";
    if (media.empty()) {
        std::cout << "None\n";
    } else {
        for (size_t i = 0; i < media.size(); ++i) {
            std::cout << media[i] << (i + 1 < media.size() ? " | " : "\n");
        }
    }

    if (verbose) {
        std::cout << "  LAN Key:            " << format_hex_digest(cert.lan_key, 16) << "\n";
        std::cout << "  Signature Key:      " << format_hex_digest(cert.signature_key, 16) << "\n";
    }
    std::cout << "\n";

    // 3. Sections Table
    const auto& sections = parser.get_sections();
    std::cout << col.bold() << col.yellow() << "▶ SECTION HEADERS (" << sections.size() << ")" << col.reset() << "\n";
    std::cout << col.dim()
              << "┌────┬────────────────┬────────────┬────────────┬────────────┬────────────┬─────────────────┐\n"
              << col.reset();
    std::cout << col.bold()
              << "│  # │ Section Name   │ Virt Addr  │ Virt Size  │ Raw Addr   │ Raw Size   │ Flags           │\n"
              << col.reset();
    std::cout << col.dim()
              << "├────┼────────────────┼────────────┼────────────┼────────────┼────────────┼─────────────────┤\n"
              << col.reset();

    for (size_t i = 0; i < sections.size(); ++i) {
        const auto& sec = sections[i];

        std::ostringstream row_ss;
        row_ss << "│ " << std::setw(2) << std::right << std::setfill(' ') << i << " │ ";
        std::cout << row_ss.str();

        std::ostringstream name_ss;
        name_ss << std::setw(14) << std::left << std::setfill(' ') << sec.name;
        std::cout << col.bold() << name_ss.str() << col.reset() << " │ ";

        std::cout << col.magenta() << format_hex32(sec.header.virtual_address) << col.reset() << " │ ";

        std::ostringstream vsize_ss;
        vsize_ss << std::setw(10) << std::right << std::setfill(' ') << format_size(sec.header.virtual_size);
        std::cout << vsize_ss.str() << " │ ";

        std::cout << format_hex32(sec.header.raw_address) << " │ ";

        std::ostringstream rsize_ss;
        rsize_ss << std::setw(10) << std::right << std::setfill(' ') << format_size(sec.header.raw_size);
        std::cout << rsize_ss.str() << " │ ";

        std::ostringstream flags_ss;
        flags_ss << std::setw(15) << std::left << std::setfill(' ') << sec.header.get_flags_string();
        std::cout << col.cyan() << flags_ss.str() << col.reset() << " │\n";

        if (verbose) {
            std::cout << "│    │   SHA-1 Digest: " << col.dim() << format_hex_digest(sec.header.section_digest, 20)
                      << col.reset() << std::string(24, ' ') << "│\n";
        }
    }

    std::cout << col.dim()
              << "└────┴────────────────┴────────────┴────────────┴────────────┴────────────┴─────────────────┘\n"
              << col.reset() << "\n";

    // 4. Linked Libraries
    const auto& libs = parser.get_libraries();
    if (show_libs && !libs.empty()) {
        std::cout << col.bold() << col.yellow() << "▶ LINKED LIBRARIES (" << libs.size() << ")" << col.reset() << "\n";
        for (const auto& lib : libs) {
            std::ostringstream lib_name_ss;
            lib_name_ss << std::setw(10) << std::left << std::setfill(' ') << lib.get_name();
            std::cout << "  • " << col.bold() << lib_name_ss.str() << col.reset()
                      << " Version " << lib.get_formatted_version() << "\n";
        }
        std::cout << "\n";
    }

    // 5. Kernel Imports
    const auto& imports = parser.get_kernel_imports();
    if (show_imports && !imports.empty()) {
        std::cout << col.bold() << col.yellow() << "▶ KERNEL IMPORTS (" << imports.size() << ")" << col.reset() << "\n";
        for (const auto& imp : imports) {
            std::ostringstream imp_ss;
            imp_ss << "  [" << std::setw(3) << std::right << std::setfill(' ') << imp.ordinal << "] ";
            std::cout << imp_ss.str();

            std::ostringstream name_ss;
            name_ss << std::setw(30) << std::left << std::setfill(' ') << imp.name;
            std::cout << col.cyan() << name_ss.str() << col.reset();

            std::cout << " (Thunk VA: " << col.magenta() << format_hex32(imp.thunk_address) << col.reset() << ")\n";
        }
        std::cout << "\n";
    }

    return 0;
}
