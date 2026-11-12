#include "risc201/assembler.hpp"
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <regex>

namespace risc201 {

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool is_reg(const std::string& s) {
    std::string reg = s;
    std::transform(reg.begin(), reg.end(), reg.begin(), ::tolower);
    if (reg == "sp" || reg == "ra") return true;
    if (reg.size() > 1 && reg[0] == 'r') {
        for (size_t i = 1; i < reg.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(reg[i]))) return false;
        }
        return true;
    }
    return false;
}

static uint8_t parse_reg(const std::string& s, int line_no) {
    std::string reg = s;
    std::transform(reg.begin(), reg.end(), reg.begin(), ::tolower);
    if (reg == "sp") return 14;
    if (reg == "ra") return 15;
    if (reg.size() > 1 && reg[0] == 'r') {
        try {
            int val = std::stoi(reg.substr(1));
            if (val < 0 || val > 15) {
                throw std::out_of_range("Register index out of range");
            }
            return static_cast<uint8_t>(val);
        } catch (...) {
            throw std::runtime_error("Line " + std::to_string(line_no) + ": Invalid register '" + s + "' (must be r0-r15, sp, or ra)");
        }
    }
    throw std::runtime_error("Line " + std::to_string(line_no) + ": Invalid register '" + s + "' (expected register name)");
}

static int32_t parse_imm(const std::string& s, int line_no, int bits = 18) {
    int64_t val = 0;
    try {
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            val = std::stoll(s, nullptr, 16);
        } else {
            val = std::stoll(s, nullptr, 10);
        }
    } catch (...) {
        throw std::runtime_error("Line " + std::to_string(line_no) + ": Invalid immediate value '" + s + "'");
    }

    int64_t min_val = -(1LL << (bits - 1));
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t max_unsigned = (1LL << bits) - 1;

    if (val < min_val || (val > max_val && val > max_unsigned)) {
        throw std::runtime_error("Line " + std::to_string(line_no) + ": Immediate value " + std::to_string(val) +
                                 " out of range for " + std::to_string(bits) + "-bit field [" +
                                 std::to_string(min_val) + ", " + std::to_string(max_val) + "]");
    }
    return static_cast<int32_t>(val);
}

static std::vector<std::string> split_tokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : line) {
        if (c == ',' || c == '[' || c == ']' || std::isspace(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

struct SourceItem {
    int line_no;
    std::string text;
};

std::vector<uint32_t> Assembler::assemble(const std::string& source) const {
    std::istringstream iss(source);
    std::string line;
    std::unordered_map<std::string, uint32_t> labels;
    std::vector<SourceItem> instructions;
    
    uint32_t pc = 0;
    int line_no = 0;
    
    // Pass 1: find labels and compute PCs
    while (std::getline(iss, line)) {
        line_no++;
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty()) continue;
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string label = trim(line.substr(0, colon_pos));
            if (label.empty()) {
                throw std::runtime_error("Line " + std::to_string(line_no) + ": Empty label declaration");
            }
            if (labels.find(label) != labels.end()) {
                throw std::runtime_error("Line " + std::to_string(line_no) + ": Duplicate label definition '" + label + "'");
            }
            labels[label] = pc;
            line = trim(line.substr(colon_pos + 1));
            if (line.empty()) continue;
        }
        
        instructions.push_back({line_no, line});
        std::string lower_line = line;
        std::transform(lower_line.begin(), lower_line.end(), lower_line.begin(), ::tolower);
        
        if (lower_line.find("push ") == 0 || lower_line.find("pop ") == 0) {
            pc += 8;
        } else {
            pc += 4;
        }
    }
    
    // Pass 2: encode
    std::vector<uint32_t> machine_code;
    pc = 0;
    
    for (const auto& item : instructions) {
        int cur_line = item.line_no;
        std::vector<std::string> tokens = split_tokens(item.text);
        if (tokens.empty()) continue;
        
        std::string mnemonic_str = tokens[0];
        std::transform(mnemonic_str.begin(), mnemonic_str.end(), mnemonic_str.begin(), ::tolower);
        
        if (mnemonic_str == "push") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand for 'push'");
            }
            // sub sp, sp, 4
            Instruction i1; i1.op = Opcode::SUB; i1.rd = 14; i1.rs1 = 14; i1.immediate = true; i1.imm = 4;
            machine_code.push_back(encode(i1));
            // st rX, 0, [sp]
            Instruction i2; i2.op = Opcode::ST; i2.rd = parse_reg(tokens[1], cur_line); i2.rs1 = 14; i2.immediate = true; i2.imm = 0;
            machine_code.push_back(encode(i2));
            pc += 8;
            continue;
        }
        if (mnemonic_str == "pop") {
            if (tokens.size() < 2) {
                throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand for 'pop'");
            }
            // ld rX, 0, [sp]
            Instruction i1; i1.op = Opcode::LD; i1.rd = parse_reg(tokens[1], cur_line); i1.rs1 = 14; i1.immediate = true; i1.imm = 0;
            machine_code.push_back(encode(i1));
            // add sp, sp, 4
            Instruction i2; i2.op = Opcode::ADD; i2.rd = 14; i2.rs1 = 14; i2.immediate = true; i2.imm = 4;
            machine_code.push_back(encode(i2));
            pc += 8;
            continue;
        }
        
        Opcode op;
        if (!parse_opcode(mnemonic_str, op)) {
            throw std::runtime_error("Line " + std::to_string(cur_line) + ": Unknown opcode '" + tokens[0] + "'");
        }
        
        Instruction i;
        i.op = op;
        
        if (op == Opcode::NOP || op == Opcode::RET || op == Opcode::HALT) {
            // no operands
        } else if (is_branch(op)) {
            if (tokens.size() < 2) {
                throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing target operand for '" + tokens[0] + "'");
            }
            std::string target = tokens[1];
            if (labels.count(target)) {
                int32_t diff = labels[target] - (pc + 4);
                i.offset = diff / 4;
            } else {
                bool is_num = false;
                if (!target.empty()) {
                    if (std::isdigit(static_cast<unsigned char>(target[0])) || target[0] == '-' || target[0] == '+') {
                        is_num = true;
                    }
                }
                if (is_num) {
                    i.offset = parse_imm(target, cur_line, 27);
                } else {
                    throw std::runtime_error("Line " + std::to_string(cur_line) + ": Undefined label '" + target + "'");
                }
            }
        } else if (is_memory(op)) {
            // ld rD, imm, [rS] or ld rD, [rS]
            if (tokens.size() < 3) {
                throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand(s) for '" + tokens[0] + "'");
            }
            i.rd = parse_reg(tokens[1], cur_line);
            i.immediate = true;
            if (tokens.size() >= 4) {
                i.imm = parse_imm(tokens[2], cur_line, 18);
                i.rs1 = parse_reg(tokens[3], cur_line);
            } else {
                i.imm = 0;
                i.rs1 = parse_reg(tokens[2], cur_line);
            }
        } else {
            // ALU
            if (op == Opcode::CMP) {
                if (tokens.size() < 3) {
                    throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand(s) for 'cmp'");
                }
                i.rs1 = parse_reg(tokens[1], cur_line);
                if (is_reg(tokens[2])) {
                    i.rs2 = parse_reg(tokens[2], cur_line);
                    i.immediate = false;
                } else {
                    i.imm = parse_imm(tokens[2], cur_line, 18);
                    i.immediate = true;
                }
            } else if (op == Opcode::MOV || op == Opcode::NOT) {
                if (tokens.size() < 3) {
                    throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand(s) for '" + tokens[0] + "'");
                }
                i.rd = parse_reg(tokens[1], cur_line);
                if (is_reg(tokens[2])) {
                    i.rs2 = parse_reg(tokens[2], cur_line);
                    i.immediate = false;
                } else {
                    i.imm = parse_imm(tokens[2], cur_line, 18);
                    i.immediate = true;
                }
            } else {
                if (tokens.size() < 4) {
                    throw std::runtime_error("Line " + std::to_string(cur_line) + ": Missing operand(s) for '" + tokens[0] + "' (expected 3 operands)");
                }
                i.rd = parse_reg(tokens[1], cur_line);
                i.rs1 = parse_reg(tokens[2], cur_line);
                if (is_reg(tokens[3])) {
                    i.rs2 = parse_reg(tokens[3], cur_line);
                    i.immediate = false;
                } else {
                    i.imm = parse_imm(tokens[3], cur_line, 18);
                    i.immediate = true;
                }
            }
        }
        
        machine_code.push_back(encode(i));
        pc += 4;
    }
    
    return machine_code;
}

std::string Assembler::to_hex_string(const std::vector<uint32_t>& machine_code) const {
    std::ostringstream oss;
    for (uint32_t word : machine_code) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%08X\n", word);
        oss << buf;
    }
    return oss.str();
}

std::string Assembler::to_binary_string(const std::vector<uint32_t>& machine_code) const {
    std::ostringstream oss;
    for (uint32_t word : machine_code) {
        for (int b = 31; b >= 0; --b) {
            oss << ((word >> b) & 1);
        }
        oss << "\n";
    }
    return oss.str();
}

std::vector<uint32_t> Assembler::parse_hex(const std::string& hex_input) const {
    std::vector<uint32_t> words;
    std::istringstream iss(hex_input);
    std::string line;
    while (std::getline(iss, line)) {
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        comment_pos = line.find("//");
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        line = trim(line);
        if (line.empty()) continue;
        uint32_t word = std::stoul(line, nullptr, 16);
        words.push_back(word);
    }
    return words;
}

std::vector<uint32_t> Assembler::parse_binary(const std::string& bin_input) const {
    std::vector<uint32_t> words;
    std::istringstream iss(bin_input);
    std::string line;
    while (std::getline(iss, line)) {
        size_t comment_pos = line.find(';');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        comment_pos = line.find("//");
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        
        // Remove spaces
        std::string bits;
        for (char c : line) {
            if (c == '0' || c == '1') bits += c;
        }
        if (bits.empty()) continue;
        if (bits.size() != 32) {
            throw std::runtime_error("Invalid binary instruction length: expected 32 bits, got " + std::to_string(bits.size()));
        }
        uint32_t word = 0;
        for (size_t i = 0; i < 32; ++i) {
            if (bits[i] == '1') {
                word |= (1u << (31 - i));
            }
        }
        words.push_back(word);
    }
    return words;
}

std::string Assembler::disassemble_program(const std::vector<uint32_t>& program) const {
    std::ostringstream oss;
    for (uint32_t word : program) {
        Instruction i = decode(word);
        oss << disassemble(i) << "\n";
    }
    return oss.str();
}

std::string Assembler::disassemble_hex(const std::string& hex_input) const {
    return disassemble_program(parse_hex(hex_input));
}

std::string Assembler::disassemble_binary(const std::string& bin_input) const {
    return disassemble_program(parse_binary(bin_input));
}

std::string Assembler::optimize_load_delay_slots(const std::string& source) const {
    std::istringstream iss(source);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(iss, l)) {
        lines.push_back(l);
    }

    auto extract_tokens = [](const std::string& line) -> std::vector<std::string> {
        std::vector<std::string> toks;
        std::string cur;
        for (char c : line) {
            if (c == ';' || c == '#') break;
            if (c == ',' || c == '[' || c == ']' || std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
            } else {
                cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        if (!cur.empty()) toks.push_back(cur);
        return toks;
    };

    auto is_label = [](const std::string& s) {
        size_t non_ws = s.find_first_not_of(" \t\r\n");
        if (non_ws == std::string::npos) return false;
        return s.find(':') != std::string::npos;
    };

    auto is_control = [](const std::string& op) {
        return (op == "b" || op == "beq" || op == "bgt" || op == "call" || op == "ret" || op == "halt");
    };

    // Helper to determine write register of an instruction (empty if none or cmp/branches/st)
    auto get_written_reg = [](const std::vector<std::string>& toks) -> std::string {
        if (toks.size() < 2) return "";
        const std::string& op = toks[0];
        if (op == "cmp" || op == "st" || op == "b" || op == "beq" || op == "bgt" || op == "ret" || op == "nop" || op == "halt") {
            return "";
        }
        if (op == "call") return "ra";
        if (is_reg(toks[1])) return toks[1];
        return "";
    };

    // Helper to collect all source registers read by an instruction
    auto get_read_regs = [](const std::vector<std::string>& toks) -> std::vector<std::string> {
        std::vector<std::string> reads;
        if (toks.empty()) return reads;
        const std::string& op = toks[0];
        if (op == "nop" || op == "halt" || op == "b" || op == "call") return reads;
        if (op == "ret") { reads.push_back("ra"); return reads; }
        if (op == "cmp") {
            if (toks.size() > 1 && is_reg(toks[1])) reads.push_back(toks[1]);
            if (toks.size() > 2 && is_reg(toks[2])) reads.push_back(toks[2]);
            return reads;
        }
        if (op == "st") {
            // st rX, imm, [rS] -> reads rX (value) and rS (base address)
            if (toks.size() > 1 && is_reg(toks[1])) reads.push_back(toks[1]);
            for (size_t k = 2; k < toks.size(); ++k) {
                if (is_reg(toks[k])) reads.push_back(toks[k]);
            }
            return reads;
        }
        if (op == "ld") {
            // ld rD, imm, [rS] -> reads rS (base address)
            for (size_t k = 2; k < toks.size(); ++k) {
                if (is_reg(toks[k])) reads.push_back(toks[k]);
            }
            return reads;
        }
        if (op == "mov" || op == "not") {
            // mov/not rD, rS -> reads rS (operand 2)
            if (toks.size() > 2 && is_reg(toks[2])) reads.push_back(toks[2]);
            return reads;
        }
        // General 3-address ALU ops: op rD, rS1, rS2 -> reads rS1 and possibly rS2
        if (toks.size() > 2 && is_reg(toks[2])) reads.push_back(toks[2]);
        if (toks.size() > 3 && is_reg(toks[3])) reads.push_back(toks[3]);
        return reads;
    };

    for (size_t i = 0; i + 1 < lines.size(); ++i) {
        if (is_label(lines[i])) continue;
        auto toks1 = extract_tokens(lines[i]);
        if (toks1.empty() || toks1[0] != "ld") continue;

        // Destination of load is toks1[1] (e.g. "r1")
        std::string load_dest = get_written_reg(toks1);
        if (load_dest.empty()) continue;

        // Check if next instruction lines[i+1] actually reads load_dest (Load-Use hazard)
        auto toks2 = extract_tokens(lines[i + 1]);
        if (toks2.empty() || is_label(lines[i + 1]) || is_control(toks2[0])) continue;
        
        auto reads2 = get_read_regs(toks2);
        bool uses_load = false;
        for (const auto& r : reads2) {
            if (r == load_dest) { uses_load = true; break; }
        }

        if (!uses_load) continue; // No load-use stall between i and i+1

        // Lookahead to find an independent candidate instruction at j > i + 1
        for (size_t j = i + 2; j < lines.size(); ++j) {
            if (is_label(lines[j])) break; // Basic block boundary: do not cross
            auto toks_cand = extract_tokens(lines[j]);
            if (toks_cand.empty() || is_control(toks_cand[0])) break;

            // 1. Candidate must NOT be a memory instruction (load or store) to preserve memory ordering
            if (toks_cand[0] == "ld" || toks_cand[0] == "st") continue;

            // 2. Candidate must NOT be CMP (or instructions affecting flags read by branches)
            if (toks_cand[0] == "cmp") continue;

            // 3. Candidate registers
            std::string cand_write = get_written_reg(toks_cand);
            auto cand_reads = get_read_regs(toks_cand);

            // Candidate must not read or write load_dest (RAW / WAW hazard with load)
            if (cand_write == load_dest) continue;
            bool reads_load_dest = false;
            for (const auto& r : cand_reads) {
                if (r == load_dest) { reads_load_dest = true; break; }
            }
            if (reads_load_dest) continue;

            // 4. Check dependencies across all intervening instructions from i+1 to j-1
            bool conflict = false;
            for (size_t m = i + 1; m < j; ++m) {
                auto toks_mid = extract_tokens(lines[m]);
                if (toks_mid.empty()) continue;
                if (is_control(toks_mid[0])) { conflict = true; break; }

                std::string mid_write = get_written_reg(toks_mid);
                auto mid_reads = get_read_regs(toks_mid);

                // RAW hazard: intermediate instruction reads what candidate writes
                if (!cand_write.empty()) {
                    for (const auto& mr : mid_reads) {
                        if (mr == cand_write) { conflict = true; break; }
                    }
                    if (conflict) break;
                }

                // WAR hazard: intermediate instruction writes what candidate reads
                if (!mid_write.empty()) {
                    for (const auto& cr : cand_reads) {
                        if (cr == mid_write) { conflict = true; break; }
                    }
                    if (conflict) break;
                }

                // WAW hazard: candidate and intermediate instruction write to same register
                if (!cand_write.empty() && cand_write == mid_write) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;

            // Proven safe: Hoist candidate lines[j] into lines[i + 1] (the load delay slot)
            std::string hoisted = lines[j];
            lines.erase(lines.begin() + j);
            lines.insert(lines.begin() + i + 1, hoisted);
            break;
        }
    }

    std::ostringstream oss;
    for (const auto& line : lines) {
        oss << line << "\n";
    }
    return oss.str();
}

} // namespace risc201
