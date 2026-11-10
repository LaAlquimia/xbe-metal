#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace xbe::recompiler {

/// 32-bit general-purpose x86 registers
enum class X86Reg : uint8_t {
    EAX  = 0,
    ECX  = 1,
    EDX  = 2,
    EBX  = 3,
    ESP  = 4,
    EBP  = 5,
    ESI  = 6,
    EDI  = 7,
    NONE = 0xFF
};

/// Returns lowercase Intel register name ("eax", "ecx", ...)
[[nodiscard]] const char* reg_name(X86Reg reg) noexcept;

/// Supported x86-32 instruction mnemonics for Xbox static recompilation
enum class X86Mnemonic {
    NOP,
    PUSH_REG,
    POP_REG,
    PUSH_IMM32,
    PUSH_IMM8,
    MOV_REG_IMM32,
    MOV_REG_REG,
    MOV_REG_MEM,
    MOV_MEM_REG,
    LEA_REG_MEM,
    ADD_REG_IMM,
    ADD_REG_REG,
    SUB_REG_IMM,
    SUB_REG_REG,
    XOR_REG_REG,
    CMP_REG_IMM,
    CMP_REG_REG,
    TEST_REG_REG,
    CALL_REL32,
    CALL_INDIRECT_MEM,
    JMP_REL32,
    JMP_REL8,
    JE_REL8,
    JNE_REL8,
    RET,
    RET_IMM16,
    INT3,
    UNKNOWN
};

/// Decoded x86-32 instruction representation
struct DecodedInstruction {
    uint32_t    address{0};
    uint8_t     size{0};
    X86Mnemonic mnemonic{X86Mnemonic::UNKNOWN};
    X86Reg      dst_reg{X86Reg::NONE};
    X86Reg      src_reg{X86Reg::NONE};
    int32_t     disp{0};
    uint32_t    imm{0};
    uint32_t    mem_address{0};
    std::string text;
};

/// Lightweight x86-32 instruction decoder for Xbox executables
class X86Decoder {
public:
    X86Decoder() = default;
    ~X86Decoder() = default;

    /// Decode a single instruction at the beginning of `code` mapped at `va`
    [[nodiscard]] DecodedInstruction decode_one(std::span<const uint8_t> code, uint32_t va) const;

    /// Decode a contiguous basic block / function starting at `base_va`
    [[nodiscard]] std::vector<DecodedInstruction> decode_block(
        std::span<const uint8_t> code,
        uint32_t base_va,
        size_t max_instructions = 256) const;
};

} // namespace xbe::recompiler
