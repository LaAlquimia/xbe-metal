#include "xbe/xbe_parser.hpp"
#include "recompiler/x86_decoder.hpp"
#include "recompiler/cpp_emitter.hpp"
#include "runtime/core/memory.hpp"
#include "runtime/core/kernel.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// Builds a valid synthetic Retail-encrypted Xbox executable (.xbe) in memory
std::vector<uint8_t> build_synthetic_xbe() {
    constexpr size_t TOTAL_SIZE     = 0x2000;
    constexpr uint32_t BASE_VA      = 0x00010000;
    constexpr uint32_t SEC_VA       = 0x00011000;
    constexpr uint32_t THUNK_VA     = 0x00011200;

    std::vector<uint8_t> image(TOTAL_SIZE, 0);

    // 1. Populate XbeHeader at file offset 0x0000 (VA 0x00010000)
    xbe::XbeHeader hdr{};
    hdr.magic                        = xbe::XBE_MAGIC;
    hdr.base_address                 = BASE_VA;
    hdr.size_of_headers              = 0x00001000;
    hdr.size_of_image                = static_cast<uint32_t>(TOTAL_SIZE);
    hdr.size_of_image_header         = sizeof(xbe::XbeHeader);
    hdr.time_date_stamp              = 0x65432100;
    hdr.certificate_address          = BASE_VA + 0x0180;
    hdr.number_of_sections           = 1;
    hdr.section_headers_address      = BASE_VA + 0x0380;
    hdr.init_flags                   = xbe::XBE_INIT_MOUNT_UTILITY_DRIVE;
    hdr.entry_point                  = SEC_VA ^ xbe::XOR_EP_RETAIL;
    hdr.kernel_image_thunk_address   = THUNK_VA ^ xbe::XOR_KT_RETAIL;
    std::memcpy(image.data(), &hdr, sizeof(hdr));

    // 2. Populate XbeCertificate at file offset 0x0180 (VA 0x00010180)
    xbe::XbeCertificate cert{};
    cert.size          = sizeof(xbe::XbeCertificate);
    cert.time_date_stamp = 0x65432100;
    cert.title_id      = 0x48420001; // "HB-001"
    const char title_ascii[] = "Homebrew Hello";
    for (size_t i = 0; i < sizeof(title_ascii) - 1; ++i) {
        cert.title_name[i] = static_cast<char16_t>(title_ascii[i]);
    }
    cert.allowed_media = xbe::XBE_MEDIA_HARD_DISK | xbe::XBE_MEDIA_DVD_X2;
    cert.game_region   = xbe::XBE_REGION_US_CANADA | xbe::XBE_REGION_REST_OF_WORLD;
    cert.version       = 1;
    std::memcpy(image.data() + 0x0180, &cert, sizeof(cert));

    // 3. Populate XbeSectionHeader at file offset 0x0380 (VA 0x00010380)
    xbe::XbeSectionHeader sec{};
    sec.flags                = xbe::XBE_SECTION_WRITABLE | xbe::XBE_SECTION_PRELOAD | xbe::XBE_SECTION_EXECUTABLE;
    sec.virtual_address      = SEC_VA;
    sec.virtual_size         = 0x00001000;
    sec.raw_address          = 0x00001000;
    sec.raw_size             = 0x00001000;
    sec.section_name_address = BASE_VA + 0x0400;
    std::memcpy(image.data() + 0x0380, &sec, sizeof(sec));

    // Section name ".text" at file offset 0x0400 (VA 0x00010400)
    const char sec_name[] = ".text";
    std::memcpy(image.data() + 0x0400, sec_name, sizeof(sec_name));

    // 4. Populate x86-32 machine code at file offset 0x1000 (VA 0x00011000)
    const uint8_t machine_code[] = {
        0x55,                               // push ebp
        0x89, 0xE5,                         // mov ebp, esp
        0x68, 0x00, 0x11, 0x01, 0x00,       // push 0x00011100
        0xFF, 0x15, 0x00, 0x12, 0x01, 0x00, // call dword ptr [0x00011200] (DbgPrint)
        0x83, 0xC4, 0x04,                   // add esp, 4
        0xB8, 0xDE, 0xC0, 0x37, 0x13,       // mov eax, 0x1337C0DE
        0x5D,                               // pop ebp
        0xC3                                // ret
    };
    std::memcpy(image.data() + 0x1000, machine_code, sizeof(machine_code));

    // 5. Embedded ASCII string at file offset 0x1100 (VA 0x00011100)
    const char hello_str[] = "Hello from xbe-metal static recompiler!";
    std::memcpy(image.data() + 0x1100, hello_str, sizeof(hello_str));

    // 6. Kernel Thunk Table at file offset 0x1200 (VA 0x00011200)
    // Entry 0: Ordinal 14 (0x8000000E -> DbgPrint), Entry 1: 0x00000000 (Terminator)
    const uint32_t thunk_entry = 0x8000000Eu;
    const uint32_t thunk_term  = 0x00000000u;
    std::memcpy(image.data() + 0x1200, &thunk_entry, sizeof(thunk_entry));
    std::memcpy(image.data() + 0x1204, &thunk_term, sizeof(thunk_term));

    return image;
}

bool check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "[FAIL] " << msg << "\n";
        return false;
    }
    std::cout << "[PASS] " << msg << "\n";
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<uint8_t> xbe_bytes = build_synthetic_xbe();

    // Optional CLI flag: --write-sample <path>
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--write-sample") {
            std::filesystem::path sample_path(argv[i + 1]);
            if (sample_path.has_parent_path()) {
                std::filesystem::create_directories(sample_path.parent_path());
            }
            std::ofstream out(sample_path, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                std::cerr << "Failed to write sample XBE to " << sample_path << "\n";
                return 1;
            }
            out.write(reinterpret_cast<const char*>(xbe_bytes.data()),
                      static_cast<std::streamsize>(xbe_bytes.size()));
            std::cout << "[INFO] Wrote synthetic XBE sample to " << sample_path << "\n";
        }
    }

    // ========================================================================
    // Assertion 1: XbeParser loads synthetic .xbe, detects Retail key,
    //              decodes entry point 0x00011000, and resolves DbgPrint import
    // ========================================================================
    xbe::XbeParser parser;
    if (!check(parser.load_from_memory(xbe_bytes) && parser.is_valid(),
               "1a. XbeParser loads synthetic .xbe from memory")) {
        return 1;
    }
    if (!check(parser.get_key_type() == xbe::XbeKeyType::Retail,
               "1b. XbeParser detects Retail encryption key")) {
        return 1;
    }
    if (!check(parser.get_entry_point() == 0x00011000u,
               "1c. XbeParser decodes entry point 0x00011000")) {
        return 1;
    }
    const auto& imports = parser.get_kernel_imports();
    if (!check(imports.size() == 1 && imports[0].ordinal == 14 && imports[0].name == "DbgPrint",
               "1d. XbeParser resolves kernel thunk ordinal 14 (DbgPrint)")) {
        return 1;
    }

    // ========================================================================
    // Assertion 2: X86Decoder decodes the 8 instructions accurately
    // ========================================================================
    const auto* text_sec = parser.find_section_by_va(parser.get_entry_point());
    if (!check(text_sec != nullptr, "2a. Locate .text section at entry point")) {
        return 1;
    }

    xbe::recompiler::X86Decoder decoder;
    auto instructions = decoder.decode_block(text_sec->data, parser.get_entry_point());
    if (!check(instructions.size() == 8, "2b. X86Decoder decodes exactly 8 instructions")) {
        std::cerr << "Decoded count: " << instructions.size() << "\n";
        return 1;
    }
    if (!check(instructions[0].mnemonic == xbe::recompiler::X86Mnemonic::PUSH_REG &&
               instructions[1].mnemonic == xbe::recompiler::X86Mnemonic::MOV_REG_REG &&
               instructions[2].mnemonic == xbe::recompiler::X86Mnemonic::PUSH_IMM32 &&
               instructions[2].imm == 0x00011100u &&
               instructions[3].mnemonic == xbe::recompiler::X86Mnemonic::CALL_INDIRECT_MEM &&
               instructions[3].mem_address == 0x00011200u &&
               instructions[4].mnemonic == xbe::recompiler::X86Mnemonic::ADD_REG_IMM &&
               instructions[5].mnemonic == xbe::recompiler::X86Mnemonic::MOV_REG_IMM32 &&
               instructions[5].imm == 0x1337C0DEu &&
               instructions[6].mnemonic == xbe::recompiler::X86Mnemonic::POP_REG &&
               instructions[7].mnemonic == xbe::recompiler::X86Mnemonic::RET,
               "2c. X86Decoder matches all 8 expected mnemonics and operands")) {
        return 1;
    }

    // ========================================================================
    // Assertion 3: CppEmitter::emit_translation_unit produces C++20 code
    //              containing kernel.dispatch(14 and 0x1337C0DE
    // ========================================================================
    xbe::recompiler::CppEmitter emitter;
    std::string cpp_code = emitter.emit_translation_unit(parser, instructions);
    if (!check(cpp_code.find("kernel.dispatch(14") != std::string::npos &&
               cpp_code.find("0x1337C0DE") != std::string::npos,
               "3. CppEmitter emits C++20 translation unit with kernel.dispatch(14 and 0x1337C0DE")) {
        std::cerr << "Emitted C++:\n" << cpp_code << "\n";
        return 1;
    }

    // ========================================================================
    // Assertion 4: CppEmitter::execute_instructions maps XBE into XboxMemory,
    //              captures DbgPrint output, and verifies ctx.eax == 0x1337C0DE
    // ========================================================================
    runtime::core::XboxMemory memory;
    runtime::core::XboxKernel kernel(memory);

    std::string captured_debug;
    kernel.set_debug_sink([&](const std::string& msg) {
        captured_debug += msg;
    });

    xbe::recompiler::CpuContext ctx{};
    bool exec_ok = emitter.execute_instructions(instructions, parser, ctx, memory, kernel);
    if (!check(exec_ok, "4a. CppEmitter::execute_instructions runs without fault")) {
        return 1;
    }
    if (!check(captured_debug.find("Hello from xbe-metal static recompiler!") != std::string::npos,
               "4b. Kernel DbgPrint sink captured 'Hello from xbe-metal static recompiler!'")) {
        std::cerr << "Captured debug string: '" << captured_debug << "'\n";
        return 1;
    }
    if (!check(ctx.eax == 0x1337C0DEu,
               "4c. Guest execution returned EAX == 0x1337C0DE")) {
        std::cerr << "Actual EAX: 0x" << std::hex << ctx.eax << std::dec << "\n";
        return 1;
    }

    std::cout << "\nAll 4 end-to-end xbe-metal recompiler pipeline assertions passed!\n";
    return 0;
}
