#pragma once

#include "recompiler/x86_decoder.hpp"
#include "xbe/xbe_parser.hpp"
#include "runtime/core/memory.hpp"
#include "runtime/core/kernel.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace xbe::recompiler {

/// Guest x86-32 CPU register state
struct CpuContext {
    uint32_t eax{0};
    uint32_t ecx{0};
    uint32_t edx{0};
    uint32_t ebx{0};
    uint32_t esp{0x0008F000};
    uint32_t ebp{0x0008F000};
    uint32_t esi{0};
    uint32_t edi{0};
    uint32_t eip{0};
    uint32_t eflags{0};
    bool     halted{false};

    static constexpr uint32_t EFLAGS_ZF = 1u << 6;
    static constexpr uint32_t EFLAGS_SF = 1u << 7;

    [[nodiscard]] uint32_t& reg(X86Reg r) noexcept {
        switch (r) {
            case X86Reg::EAX: return eax;
            case X86Reg::ECX: return ecx;
            case X86Reg::EDX: return edx;
            case X86Reg::EBX: return ebx;
            case X86Reg::ESP: return esp;
            case X86Reg::EBP: return ebp;
            case X86Reg::ESI: return esi;
            case X86Reg::EDI: return edi;
            case X86Reg::NONE:
            default:          return eax;
        }
    }

    [[nodiscard]] uint32_t reg(X86Reg r) const noexcept {
        switch (r) {
            case X86Reg::EAX: return eax;
            case X86Reg::ECX: return ecx;
            case X86Reg::EDX: return edx;
            case X86Reg::EBX: return ebx;
            case X86Reg::ESP: return esp;
            case X86Reg::EBP: return ebp;
            case X86Reg::ESI: return esi;
            case X86Reg::EDI: return edi;
            case X86Reg::NONE:
            default:          return 0;
        }
    }
};

/// Translates decoded x86-32 instructions into C++20 source or executes them directly
class CppEmitter {
public:
    CppEmitter() = default;
    ~CppEmitter() = default;

    /// Emit a complete C++20 translation unit for the given decoded instructions
    [[nodiscard]] std::string emit_translation_unit(
        const xbe::XbeParser& parser,
        const std::vector<DecodedInstruction>& instructions) const;

    /// Execute decoded instructions directly against guest memory and HLE kernel
    bool execute_instructions(
        const std::vector<DecodedInstruction>& instructions,
        const xbe::XbeParser& parser,
        CpuContext& ctx,
        runtime::core::XboxMemory& mem,
        runtime::core::XboxKernel& kernel) const;
};

} // namespace xbe::recompiler
