#pragma once
#include "isa.hpp"
#include "alu.hpp"
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>

namespace risc201 {

struct ControlSignals {
    bool regWrite = false;
    bool memRead = false;
    bool memWrite = false;
    bool aluSrc = false;     // false=register, true=immediate
    AluOp aluOp = AluOp::PASS;
    bool branch = false;
    bool jump = false;       // unconditional jump (B instruction)
    bool call = false;
    bool ret = false;
    bool halt = false;
    std::string to_string() const;  // for debug display
};

enum class ControlStyle { HARDWIRED, HORIZONTAL, VERTICAL };

struct MicroInstruction {
    uint32_t address = 0;
    ControlSignals signals;
    uint32_t next_address = 0;
    uint32_t raw_bits = 0;
    bool end_of_sequence = false;
};

class ControlUnit {
    ControlStyle style_;
    std::vector<MicroInstruction> control_memory_;
    std::array<uint32_t, NUM_OPCODES> dispatch_table_{};

    // Decoding methods
    ControlSignals hardwired_decode(Opcode op, bool imm) const;

public:
    explicit ControlUnit(ControlStyle s = ControlStyle::HARDWIRED);

    // Microcode word encoding / decoding (public for verification and testing)
    static uint32_t encode_horizontal(const ControlSignals& cs, uint32_t next, bool end);
    static ControlSignals decode_horizontal(uint32_t raw);
    static uint32_t encode_vertical(const ControlSignals& cs, uint32_t next, bool end);
    static ControlSignals decode_vertical(uint32_t raw);

    // Instruction-level decode (for hardwired mode)
    ControlSignals decode(Opcode op, bool imm) const;

    // Microprogrammed interface
    const MicroInstruction& fetch_micro(uint32_t micro_pc) const;
    uint32_t dispatch(Opcode op) const;
    void load_microcode();

    // Info
    ControlStyle style() const { return style_; }
    size_t control_memory_size() const { return control_memory_.size(); }
    size_t word_width() const;  // bits per micro-instruction
    std::string style_name() const;
    std::string dump_microcode() const;  // formatted dump of control memory
};

} // namespace risc201
