#pragma once
#include "isa.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace risc201 {

class Assembler {
public:
    // Two-pass assembly from assembly source code to machine code words
    std::vector<uint32_t> assemble(const std::string& source) const;

    // Formatting machine code to text representations
    std::string to_hex_string(const std::vector<uint32_t>& machine_code) const;
    std::string to_binary_string(const std::vector<uint32_t>& machine_code) const;

    // Parsing text representations to machine code words
    std::vector<uint32_t> parse_hex(const std::string& hex_input) const;
    std::vector<uint32_t> parse_binary(const std::string& bin_input) const;

    // Disassembly from representations back to assembly instructions
    std::string disassemble_hex(const std::string& hex_input) const;
    std::string disassemble_binary(const std::string& bin_input) const;
    std::string disassemble_program(const std::vector<uint32_t>& program) const;

    // Static Load-Use Delay Slot Optimizer (Instruction Scheduling)
    std::string optimize_load_delay_slots(const std::string& source) const;
};

} // namespace risc201
