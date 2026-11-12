#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace risc201 {

enum class Opcode : uint8_t {
    ADD=0, SUB=1, MUL=2, DIV=3, MOD=4, CMP=5,
    AND=6, OR=7, NOT=8, MOV=9,
    LSL=10, LSR=11, ASR=12, NOP=13,
    LD=14, ST=15, BEQ=16, BGT=17, B=18,
    CALL=19, RET=20, HALT=21
};

constexpr int NUM_OPCODES = 22;

struct Instruction {
    Opcode op{Opcode::NOP};
    bool immediate{false};
    uint8_t rd{0}, rs1{0}, rs2{0};
    int32_t imm{0};
    int32_t offset{0};
    uint32_t raw{0};
};

const char* mnemonic(Opcode op);
bool parse_opcode(const std::string& s, Opcode& op);
uint32_t encode(const Instruction& i);
Instruction decode(uint32_t word);
std::string disassemble(const Instruction& i);
std::string hex32(uint32_t v);

// Classification helpers
bool is_branch(Opcode op);   // BEQ, BGT, B, CALL
bool is_memory(Opcode op);   // LD, ST
bool is_alu(Opcode op);      // ADD..ASR, CMP, MOV

} // namespace risc201
