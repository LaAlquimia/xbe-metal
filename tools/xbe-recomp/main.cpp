#include "xbe/xbe_parser.hpp"
#include "recompiler/x86_decoder.hpp"
#include "recompiler/cpp_emitter.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " <input.xbe> [-o <output.cpp>] [--disasm]\n"
              << "Options:\n"
              << "  -o <path>   Write generated C++20 translation unit to <path>\n"
              << "  --disasm    Print decoded x86-32 disassembly from entry point\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_path;
    std::string output_path;
    bool show_disasm = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--disasm") {
            show_disasm = true;
        } else if (arg == "-o" && (i + 1) < argc) {
            output_path = argv[++i];
        } else if (input_path.empty()) {
            input_path = arg;
        } else {
            std::cerr << "[xbe-recomp] Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_path.empty()) {
        std::cerr << "[xbe-recomp] Error: No input .xbe file specified.\n";
        return 1;
    }

    xbe::XbeParser parser;
    if (!parser.load_from_file(input_path) || !parser.is_valid()) {
        std::cerr << "[xbe-recomp] Failed to parse XBE '" << input_path
                  << "': " << parser.get_error() << "\n";
        return 1;
    }

    const uint32_t entry_va = parser.get_entry_point();
    const xbe::ParsedSection* entry_sec = parser.find_section_by_va(entry_va);
    if (!entry_sec) {
        std::cerr << "[xbe-recomp] Error: Could not find section containing entry point 0x"
                  << std::hex << entry_va << std::dec << "\n";
        return 1;
    }

    const uint32_t offset_in_sec = entry_va - entry_sec->virtualAddress;
    if (offset_in_sec >= entry_sec->data.size()) {
        std::cerr << "[xbe-recomp] Error: Entry point offset exceeds section raw data.\n";
        return 1;
    }

    std::span<const uint8_t> code_span(
        entry_sec->data.data() + offset_in_sec,
        entry_sec->data.size() - offset_in_sec
    );

    xbe::recompiler::X86Decoder decoder;
    auto instructions = decoder.decode_block(code_span, entry_va);

    if (show_disasm) {
        std::cout << "; Disassembly for '" << parser.get_title_name() << "' (Entry: 0x"
                  << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << entry_va << std::dec << ")\n";
        for (const auto& inst : instructions) {
            std::cout << "  0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
                      << inst.address << ":  " << inst.text << std::dec << "\n";
        }
        std::cout << "\n";
    }

    xbe::recompiler::CppEmitter emitter;
    std::string cpp_source = emitter.emit_translation_unit(parser, instructions);

    if (!output_path.empty()) {
        std::ofstream out(output_path, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "[xbe-recomp] Error: Could not open output file: " << output_path << "\n";
            return 1;
        }
        out << cpp_source;
        std::cout << "[xbe-recomp] Wrote recompiled C++20 translation unit ("
                  << instructions.size() << " instructions) to " << output_path << "\n";
    } else {
        std::cout << cpp_source;
    }

    return 0;
}
