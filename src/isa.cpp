#include "risc201/isa.hpp"
#include <cctype>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace risc201 {

const char* mnemonic(Opcode op) {
    switch (op) {
        case Opcode::ADD: return "add";
        case Opcode::SUB: return "sub";
        case Opcode::MUL: return "mul";
        case Opcode::DIV: return "div";
        case Opcode::MOD: return "mod";
        case Opcode::CMP: return "cmp";
        case Opcode::AND: return "and";
        case Opcode::OR:  return "or";
        case Opcode::NOT: return "not";
        case Opcode::MOV: return "mov";
        case Opcode::LSL: return "lsl";
        case Opcode::LSR: return "lsr";
        case Opcode::ASR: return "asr";
        case Opcode::NOP: return "nop";
        case Opcode::LD:  return "ld";
        case Opcode::ST:  return "st";
        case Opcode::BEQ: return "beq";
        case Opcode::BGT: return "bgt";
        case Opcode::B:   return "b";
        case Opcode::CALL:return "call";
        case Opcode::RET: return "ret";
        case Opcode::HALT:return "halt";
        default: return "unknown";
    }
}

bool parse_opcode(const std::string& s, Opcode& op) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    for (int i = 0; i < NUM_OPCODES; ++i) {
        if (lower == mnemonic(static_cast<Opcode>(i))) {
            op = static_cast<Opcode>(i);
            return true;
        }
    }
    return false;
}

uint32_t encode(const Instruction& i) {
    uint32_t raw = (static_cast<uint32_t>(i.op) & 0x1F) << 27;
    
    if (i.op == Opcode::NOP || i.op == Opcode::RET || i.op == Opcode::HALT) {
        return raw;
    }
    
    if (is_branch(i.op)) {
        uint32_t off27 = i.offset & 0x7FFFFFF;
        raw |= off27;
        return raw;
    }
    
    raw |= (i.immediate ? 1 : 0) << 26;
    raw |= (i.rd & 0xF) << 22;
    raw |= (i.rs1 & 0xF) << 18;
    
    if (i.immediate) {
        uint32_t imm18 = i.imm & 0x3FFFF;
        raw |= imm18;
    } else {
        raw |= (i.rs2 & 0xF) << 14;
    }
    
    return raw;
}

Instruction decode(uint32_t word) {
    Instruction i;
    i.raw = word;
    i.op = static_cast<Opcode>((word >> 27) & 0x1F);
    
    if (i.op == Opcode::NOP || i.op == Opcode::RET || i.op == Opcode::HALT) {
        return i;
    }
    
    if (is_branch(i.op)) {
        uint32_t off27 = word & 0x7FFFFFF;
        if (off27 & 0x4000000) {
            off27 |= 0xF8000000; // Sign extend to 32 bits
        }
        i.offset = static_cast<int32_t>(off27);
        return i;
    }
    
    i.immediate = (word >> 26) & 1;
    i.rd = (word >> 22) & 0xF;
    i.rs1 = (word >> 18) & 0xF;
    
    if (i.immediate) {
        uint32_t imm18 = word & 0x3FFFF;
        if (imm18 & 0x20000) {
            imm18 |= 0xFFFC0000;
        }
        i.imm = static_cast<int32_t>(imm18);
    } else {
        i.rs2 = (word >> 14) & 0xF;
    }
    
    return i;
}

std::string disassemble(const Instruction& i) {
    std::ostringstream oss;
    oss << mnemonic(i.op);
    
    if (i.op == Opcode::NOP || i.op == Opcode::RET || i.op == Opcode::HALT) {
        return oss.str();
    }
    
    if (is_branch(i.op)) {
        oss << " " << i.offset;
        return oss.str();
    }
    
    if (is_memory(i.op)) {
        oss << " r" << (int)i.rd << ", " << i.imm << ", [r" << (int)i.rs1 << "]";
        return oss.str();
    }
    
    if (i.op == Opcode::CMP) {
        oss << " r" << (int)i.rs1 << ", ";
        if (i.immediate) {
            oss << i.imm;
        } else {
            oss << "r" << (int)i.rs2;
        }
        return oss.str();
    }

    if (i.op == Opcode::MOV || i.op == Opcode::NOT) {
        oss << " r" << (int)i.rd << ", ";
        if (i.immediate) {
            oss << i.imm;
        } else {
            oss << "r" << (int)i.rs2;
        }
        return oss.str();
    }
    
    oss << " r" << (int)i.rd << ", r" << (int)i.rs1 << ", ";
    if (i.immediate) {
        oss << i.imm;
    } else {
        oss << "r" << (int)i.rs2;
    }
    
    return oss.str();
}

std::string hex32(uint32_t v) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << v;
    return oss.str();
}

bool is_branch(Opcode op) {
    return op == Opcode::BEQ || op == Opcode::BGT || op == Opcode::B || op == Opcode::CALL;
}

bool is_memory(Opcode op) {
    return op == Opcode::LD || op == Opcode::ST;
}

bool is_alu(Opcode op) {
    return (op >= Opcode::ADD && op <= Opcode::ASR) || op == Opcode::CMP || op == Opcode::MOV;
}

} // namespace risc201
