#include "recompiler/x86_decoder.hpp"
#include <cstdio>
#include <cstring>

namespace xbe::recompiler {

namespace {

uint16_t read_u16_le(const uint8_t* p) noexcept {
    uint16_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

uint32_t read_u32_le(const uint8_t* p) noexcept {
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

int32_t read_i32_le(const uint8_t* p) noexcept {
    int32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

std::string hex32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

std::string hex_compact(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%X", v);
    return buf;
}

struct ModRmDecoded {
    uint8_t  mod{0};
    X86Reg   reg{X86Reg::NONE};
    uint8_t  reg_idx{0};
    X86Reg   rm_reg{X86Reg::NONE};
    int32_t  disp{0};
    uint32_t mem_address{0};
    uint8_t  extra_bytes{0}; // Includes ModR/M byte + optional SIB + displacement
    bool     valid{false};
};

ModRmDecoded decode_modrm(std::span<const uint8_t> bytes) noexcept {
    ModRmDecoded out{};
    if (bytes.empty()) {
        return out;
    }

    uint8_t modrm = bytes[0];
    out.mod     = static_cast<uint8_t>((modrm >> 6) & 0x03);
    out.reg_idx = static_cast<uint8_t>((modrm >> 3) & 0x07);
    uint8_t rm  = static_cast<uint8_t>(modrm & 0x07);

    out.reg    = static_cast<X86Reg>(out.reg_idx);
    out.rm_reg = static_cast<X86Reg>(rm);

    size_t offset = 1; // Past ModR/M

    if (out.mod == 3) {
        out.extra_bytes = 1;
        out.valid = true;
        return out;
    }

    // Memory addressing (mod == 0, 1, 2)
    if (rm == 4) {
        // SIB byte present
        if (offset >= bytes.size()) return out;
        uint8_t sib = bytes[offset++];
        uint8_t base = static_cast<uint8_t>(sib & 0x07);
        out.rm_reg = static_cast<X86Reg>(base);

        if (out.mod == 0 && base == 5) {
            if (offset + 4 > bytes.size()) return out;
            out.mem_address = read_u32_le(bytes.data() + offset);
            out.disp = static_cast<int32_t>(out.mem_address);
            out.rm_reg = X86Reg::NONE;
            offset += 4;
        }
    } else if (out.mod == 0 && rm == 5) {
        // Direct [disp32]
        if (offset + 4 > bytes.size()) return out;
        out.mem_address = read_u32_le(bytes.data() + offset);
        out.disp = static_cast<int32_t>(out.mem_address);
        out.rm_reg = X86Reg::NONE;
        offset += 4;
    }

    if (out.mod == 1) {
        if (offset + 1 > bytes.size()) return out;
        out.disp = static_cast<int32_t>(static_cast<int8_t>(bytes[offset]));
        offset += 1;
    } else if (out.mod == 2) {
        if (offset + 4 > bytes.size()) return out;
        out.disp = read_i32_le(bytes.data() + offset);
        offset += 4;
    }

    out.extra_bytes = static_cast<uint8_t>(offset);
    out.valid = true;
    return out;
}

std::string format_mem_operand(const ModRmDecoded& m) {
    if (m.rm_reg == X86Reg::NONE) {
        return "dword ptr [" + hex32(m.mem_address) + "]";
    }
    if (m.disp == 0) {
        return std::string("dword ptr [") + reg_name(m.rm_reg) + "]";
    }
    if (m.disp > 0) {
        return std::string("dword ptr [") + reg_name(m.rm_reg) + " + " + hex_compact(static_cast<uint32_t>(m.disp)) + "]";
    }
    uint32_t abs_disp = static_cast<uint32_t>(-static_cast<int64_t>(m.disp));
    return std::string("dword ptr [") + reg_name(m.rm_reg) + " - " + hex_compact(abs_disp) + "]";
}

} // namespace

const char* reg_name(X86Reg reg) noexcept {
    switch (reg) {
        case X86Reg::EAX:  return "eax";
        case X86Reg::ECX:  return "ecx";
        case X86Reg::EDX:  return "edx";
        case X86Reg::EBX:  return "ebx";
        case X86Reg::ESP:  return "esp";
        case X86Reg::EBP:  return "ebp";
        case X86Reg::ESI:  return "esi";
        case X86Reg::EDI:  return "edi";
        case X86Reg::NONE:
        default:           return "none";
    }
}

DecodedInstruction X86Decoder::decode_one(std::span<const uint8_t> code, uint32_t va) const {
    DecodedInstruction inst{};
    inst.address = va;

    if (code.empty()) {
        inst.size = 0;
        inst.mnemonic = X86Mnemonic::UNKNOWN;
        inst.text = "db <empty>";
        return inst;
    }

    const uint8_t op = code[0];

    // 0x90: NOP
    if (op == 0x90) {
        inst.size = 1;
        inst.mnemonic = X86Mnemonic::NOP;
        inst.text = "nop";
        return inst;
    }

    // 0x50..0x57: PUSH r32
    if (op >= 0x50 && op <= 0x57) {
        X86Reg r = static_cast<X86Reg>(op - 0x50);
        inst.size = 1;
        inst.mnemonic = X86Mnemonic::PUSH_REG;
        inst.dst_reg = r;
        inst.src_reg = r;
        inst.text = std::string("push ") + reg_name(r);
        return inst;
    }

    // 0x58..0x5F: POP r32
    if (op >= 0x58 && op <= 0x5F) {
        X86Reg r = static_cast<X86Reg>(op - 0x58);
        inst.size = 1;
        inst.mnemonic = X86Mnemonic::POP_REG;
        inst.dst_reg = r;
        inst.src_reg = r;
        inst.text = std::string("pop ") + reg_name(r);
        return inst;
    }

    // 0x68 imm32: PUSH imm32
    if (op == 0x68 && code.size() >= 5) {
        uint32_t imm = read_u32_le(code.data() + 1);
        inst.size = 5;
        inst.mnemonic = X86Mnemonic::PUSH_IMM32;
        inst.imm = imm;
        inst.text = "push " + hex32(imm);
        return inst;
    }

    // 0x6A imm8: PUSH imm8 (sign-extended to 32 bits)
    if (op == 0x6A && code.size() >= 2) {
        int32_t simm = static_cast<int32_t>(static_cast<int8_t>(code[1]));
        inst.size = 2;
        inst.mnemonic = X86Mnemonic::PUSH_IMM8;
        inst.imm = static_cast<uint32_t>(simm);
        inst.disp = simm;
        inst.text = "push " + hex_compact(inst.imm);
        return inst;
    }

    // 0xB8..0xBF imm32: MOV r32, imm32
    if (op >= 0xB8 && op <= 0xBF && code.size() >= 5) {
        X86Reg r = static_cast<X86Reg>(op - 0xB8);
        uint32_t imm = read_u32_le(code.data() + 1);
        inst.size = 5;
        inst.mnemonic = X86Mnemonic::MOV_REG_IMM32;
        inst.dst_reg = r;
        inst.src_reg = r;
        inst.imm = imm;
        inst.text = std::string("mov ") + reg_name(r) + ", " + hex32(imm);
        return inst;
    }

    // 0x89 /r: MOV r/m32, r32
    if (op == 0x89 && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid) {
            inst.size = static_cast<uint8_t>(1 + m.extra_bytes);
            if (m.mod == 3) {
                inst.mnemonic = X86Mnemonic::MOV_REG_REG;
                inst.dst_reg = m.rm_reg;
                inst.src_reg = m.reg;
                inst.text = std::string("mov ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            } else {
                inst.mnemonic = X86Mnemonic::MOV_MEM_REG;
                inst.dst_reg = m.rm_reg;
                inst.src_reg = m.reg;
                inst.disp = m.disp;
                inst.mem_address = m.mem_address;
                inst.text = "mov " + format_mem_operand(m) + ", " + reg_name(m.reg);
            }
            return inst;
        }
    }

    // 0x8B /r: MOV r32, r/m32
    if (op == 0x8B && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid) {
            inst.size = static_cast<uint8_t>(1 + m.extra_bytes);
            if (m.mod == 3) {
                inst.mnemonic = X86Mnemonic::MOV_REG_REG;
                inst.dst_reg = m.reg;
                inst.src_reg = m.rm_reg;
                inst.text = std::string("mov ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            } else {
                inst.mnemonic = X86Mnemonic::MOV_REG_MEM;
                inst.dst_reg = m.reg;
                inst.src_reg = m.rm_reg;
                inst.disp = m.disp;
                inst.mem_address = m.mem_address;
                inst.text = std::string("mov ") + reg_name(m.reg) + ", " + format_mem_operand(m);
            }
            return inst;
        }
    }

    // 0x8D /r: LEA r32, m
    if (op == 0x8D && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod != 3) {
            inst.size = static_cast<uint8_t>(1 + m.extra_bytes);
            inst.mnemonic = X86Mnemonic::LEA_REG_MEM;
            inst.dst_reg = m.reg;
            inst.src_reg = m.rm_reg;
            inst.disp = m.disp;
            inst.mem_address = m.mem_address;
            inst.text = std::string("lea ") + reg_name(m.reg) + ", " + format_mem_operand(m);
            return inst;
        }
    }

    // 0x01 /r (ADD r/m32, r32) or 0x03 /r (ADD r32, r/m32)
    if ((op == 0x01 || op == 0x03) && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod == 3) {
            inst.size = 2;
            inst.mnemonic = X86Mnemonic::ADD_REG_REG;
            inst.dst_reg = (op == 0x01) ? m.rm_reg : m.reg;
            inst.src_reg = (op == 0x01) ? m.reg : m.rm_reg;
            inst.text = std::string("add ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            return inst;
        }
    }

    // 0x29 /r (SUB r/m32, r32) or 0x2B /r (SUB r32, r/m32)
    if ((op == 0x29 || op == 0x2B) && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod == 3) {
            inst.size = 2;
            inst.mnemonic = X86Mnemonic::SUB_REG_REG;
            inst.dst_reg = (op == 0x29) ? m.rm_reg : m.reg;
            inst.src_reg = (op == 0x29) ? m.reg : m.rm_reg;
            inst.text = std::string("sub ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            return inst;
        }
    }

    // 0x31 /r (XOR r/m32, r32) or 0x33 /r (XOR r32, r/m32)
    if ((op == 0x31 || op == 0x33) && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod == 3) {
            inst.size = 2;
            inst.mnemonic = X86Mnemonic::XOR_REG_REG;
            inst.dst_reg = (op == 0x31) ? m.rm_reg : m.reg;
            inst.src_reg = (op == 0x31) ? m.reg : m.rm_reg;
            inst.text = std::string("xor ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            return inst;
        }
    }

    // 0x39 /r (CMP r/m32, r32) or 0x3B /r (CMP r32, r/m32)
    if ((op == 0x39 || op == 0x3B) && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod == 3) {
            inst.size = 2;
            inst.mnemonic = X86Mnemonic::CMP_REG_REG;
            inst.dst_reg = (op == 0x39) ? m.rm_reg : m.reg;
            inst.src_reg = (op == 0x39) ? m.reg : m.rm_reg;
            inst.text = std::string("cmp ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            return inst;
        }
    }

    // 0x85 /r: TEST r/m32, r32
    if (op == 0x85 && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        if (m.valid && m.mod == 3) {
            inst.size = 2;
            inst.mnemonic = X86Mnemonic::TEST_REG_REG;
            inst.dst_reg = m.rm_reg;
            inst.src_reg = m.reg;
            inst.text = std::string("test ") + reg_name(inst.dst_reg) + ", " + reg_name(inst.src_reg);
            return inst;
        }
    }

    // 0x81 /0..7 imm32 or 0x83 /0..7 imm8 (ADD / SUB / CMP r32, imm)
    if ((op == 0x81 || op == 0x83) && code.size() >= 2) {
        auto m = decode_modrm(code.subspan(1));
        size_t imm_bytes = (op == 0x81) ? 4 : 1;
        size_t total_len = 1 + m.extra_bytes + imm_bytes;
        if (m.valid && m.mod == 3 && code.size() >= total_len) {
            uint32_t imm_val = 0;
            if (op == 0x81) {
                imm_val = read_u32_le(code.data() + 1 + m.extra_bytes);
            } else {
                int32_t simm8 = static_cast<int32_t>(static_cast<int8_t>(code[1 + m.extra_bytes]));
                imm_val = static_cast<uint32_t>(simm8);
            }

            inst.size = static_cast<uint8_t>(total_len);
            inst.dst_reg = m.rm_reg;
            inst.src_reg = m.rm_reg;
            inst.imm = imm_val;

            if (m.reg_idx == 0) {
                inst.mnemonic = X86Mnemonic::ADD_REG_IMM;
                inst.text = std::string("add ") + reg_name(m.rm_reg) + ", " + hex_compact(imm_val);
                return inst;
            } else if (m.reg_idx == 5) {
                inst.mnemonic = X86Mnemonic::SUB_REG_IMM;
                inst.text = std::string("sub ") + reg_name(m.rm_reg) + ", " + hex_compact(imm_val);
                return inst;
            } else if (m.reg_idx == 7) {
                inst.mnemonic = X86Mnemonic::CMP_REG_IMM;
                inst.text = std::string("cmp ") + reg_name(m.rm_reg) + ", " + hex_compact(imm_val);
                return inst;
            }
        }
    }

    // 0xE8 rel32: CALL rel32
    if (op == 0xE8 && code.size() >= 5) {
        int32_t rel = read_i32_le(code.data() + 1);
        uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(va) + 5 + rel);
        inst.size = 5;
        inst.mnemonic = X86Mnemonic::CALL_REL32;
        inst.disp = rel;
        inst.imm = target;
        inst.mem_address = target;
        inst.text = "call " + hex32(target);
        return inst;
    }

    // 0xFF 0x15 addr32: CALL dword ptr [addr32] (Xbox kernel thunk call)
    if (op == 0xFF && code.size() >= 6 && code[1] == 0x15) {
        uint32_t addr = read_u32_le(code.data() + 2);
        inst.size = 6;
        inst.mnemonic = X86Mnemonic::CALL_INDIRECT_MEM;
        inst.mem_address = addr;
        inst.imm = addr;
        inst.text = "call dword ptr [" + hex32(addr) + "]";
        return inst;
    }

    // 0xE9 rel32: JMP rel32
    if (op == 0xE9 && code.size() >= 5) {
        int32_t rel = read_i32_le(code.data() + 1);
        uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(va) + 5 + rel);
        inst.size = 5;
        inst.mnemonic = X86Mnemonic::JMP_REL32;
        inst.disp = rel;
        inst.imm = target;
        inst.mem_address = target;
        inst.text = "jmp " + hex32(target);
        return inst;
    }

    // 0xEB rel8: JMP short rel8
    if (op == 0xEB && code.size() >= 2) {
        int32_t rel = static_cast<int32_t>(static_cast<int8_t>(code[1]));
        uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(va) + 2 + rel);
        inst.size = 2;
        inst.mnemonic = X86Mnemonic::JMP_REL8;
        inst.disp = rel;
        inst.imm = target;
        inst.mem_address = target;
        inst.text = "jmp short " + hex32(target);
        return inst;
    }

    // 0x74 rel8: JE rel8
    if (op == 0x74 && code.size() >= 2) {
        int32_t rel = static_cast<int32_t>(static_cast<int8_t>(code[1]));
        uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(va) + 2 + rel);
        inst.size = 2;
        inst.mnemonic = X86Mnemonic::JE_REL8;
        inst.disp = rel;
        inst.imm = target;
        inst.mem_address = target;
        inst.text = "je " + hex32(target);
        return inst;
    }

    // 0x75 rel8: JNE rel8
    if (op == 0x75 && code.size() >= 2) {
        int32_t rel = static_cast<int32_t>(static_cast<int8_t>(code[1]));
        uint32_t target = static_cast<uint32_t>(static_cast<int64_t>(va) + 2 + rel);
        inst.size = 2;
        inst.mnemonic = X86Mnemonic::JNE_REL8;
        inst.disp = rel;
        inst.imm = target;
        inst.mem_address = target;
        inst.text = "jne " + hex32(target);
        return inst;
    }

    // 0xC3: RET
    if (op == 0xC3) {
        inst.size = 1;
        inst.mnemonic = X86Mnemonic::RET;
        inst.text = "ret";
        return inst;
    }

    // 0xC2 imm16: RET imm16
    if (op == 0xC2 && code.size() >= 3) {
        uint16_t imm16 = read_u16_le(code.data() + 1);
        inst.size = 3;
        inst.mnemonic = X86Mnemonic::RET_IMM16;
        inst.imm = imm16;
        inst.text = "ret " + hex_compact(imm16);
        return inst;
    }

    // 0xCC: INT3
    if (op == 0xCC) {
        inst.size = 1;
        inst.mnemonic = X86Mnemonic::INT3;
        inst.text = "int3";
        return inst;
    }

    // Fallback: unrecognized 1-byte opcode
    inst.size = 1;
    inst.mnemonic = X86Mnemonic::UNKNOWN;
    inst.imm = op;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "db 0x%02X", op);
    inst.text = buf;
    return inst;
}

std::vector<DecodedInstruction> X86Decoder::decode_block(
    std::span<const uint8_t> code,
    uint32_t base_va,
    size_t max_instructions) const
{
    std::vector<DecodedInstruction> instructions;
    size_t offset = 0;

    while (offset < code.size() && instructions.size() < max_instructions) {
        uint32_t current_va = base_va + static_cast<uint32_t>(offset);
        DecodedInstruction inst = decode_one(code.subspan(offset), current_va);
        if (inst.size == 0) {
            break;
        }

        instructions.push_back(inst);
        offset += inst.size;

        // Stop at function return or breakpoint
        if (inst.mnemonic == X86Mnemonic::RET ||
            inst.mnemonic == X86Mnemonic::RET_IMM16 ||
            inst.mnemonic == X86Mnemonic::INT3) {
            break;
        }
    }

    return instructions;
}

} // namespace xbe::recompiler
