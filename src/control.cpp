#include "risc201/control.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace risc201 {

std::string ControlSignals::to_string() const {
    std::ostringstream oss;
    bool first = true;
    auto add = [&](const std::string& str) {
        if (!first) oss << " ";
        oss << str;
        first = false;
    };
    if (regWrite) add("RegW");
    if (memRead)  add("MemR");
    if (memWrite) add("MemW");
    if (aluSrc)   add("AluSrc");
    if (branch)   add("Branch");
    if (jump)     add("Jump");
    if (call)     add("Call");
    if (ret)      add("Ret");
    if (halt)     add("Halt");
    
    if (aluOp != AluOp::PASS || (first)) {
        std::string opStr;
        switch(aluOp) {
            case AluOp::ADD: opStr = "ADD"; break;
            case AluOp::SUB: opStr = "SUB"; break;
            case AluOp::MUL: opStr = "MUL"; break;
            case AluOp::DIV: opStr = "DIV"; break;
            case AluOp::MOD: opStr = "MOD"; break;
            case AluOp::AND: opStr = "AND"; break;
            case AluOp::OR:  opStr = "OR"; break;
            case AluOp::NOT: opStr = "NOT"; break;
            case AluOp::LSL: opStr = "LSL"; break;
            case AluOp::LSR: opStr = "LSR"; break;
            case AluOp::ASR: opStr = "ASR"; break;
            case AluOp::CMP: opStr = "CMP"; break;
            case AluOp::PASS: opStr = "PASS"; break;
        }
        add("ALU=" + opStr);
    }
    
    if (first) {
        return "None";
    }
    
    return oss.str();
}

ControlSignals ControlUnit::hardwired_decode(Opcode op, bool imm) const {
    ControlSignals cs;
    switch (op) {
        case Opcode::ADD: cs.regWrite = true; cs.aluOp = AluOp::ADD; cs.aluSrc = imm; break;
        case Opcode::SUB: cs.regWrite = true; cs.aluOp = AluOp::SUB; cs.aluSrc = imm; break;
        case Opcode::MUL: cs.regWrite = true; cs.aluOp = AluOp::MUL; cs.aluSrc = imm; break;
        case Opcode::DIV: cs.regWrite = true; cs.aluOp = AluOp::DIV; cs.aluSrc = imm; break;
        case Opcode::MOD: cs.regWrite = true; cs.aluOp = AluOp::MOD; cs.aluSrc = imm; break;
        case Opcode::CMP: cs.aluOp = AluOp::CMP; cs.aluSrc = imm; break;
        case Opcode::AND: cs.regWrite = true; cs.aluOp = AluOp::AND; cs.aluSrc = imm; break;
        case Opcode::OR:  cs.regWrite = true; cs.aluOp = AluOp::OR;  cs.aluSrc = imm; break;
        case Opcode::NOT: cs.regWrite = true; cs.aluOp = AluOp::NOT; cs.aluSrc = imm; break;
        case Opcode::MOV: cs.regWrite = true; cs.aluOp = AluOp::PASS;cs.aluSrc = imm; break;
        case Opcode::LSL: cs.regWrite = true; cs.aluOp = AluOp::LSL; cs.aluSrc = imm; break;
        case Opcode::LSR: cs.regWrite = true; cs.aluOp = AluOp::LSR; cs.aluSrc = imm; break;
        case Opcode::ASR: cs.regWrite = true; cs.aluOp = AluOp::ASR; cs.aluSrc = imm; break;
        case Opcode::LD:  cs.regWrite = true; cs.memRead = true; cs.aluOp = AluOp::ADD; cs.aluSrc = true; break;
        case Opcode::ST:  cs.memWrite = true; cs.aluOp = AluOp::ADD; cs.aluSrc = true; break;
        case Opcode::BEQ:
        case Opcode::BGT: cs.branch = true; break;
        case Opcode::B:   cs.jump = true; break;
        case Opcode::CALL:cs.call = true; cs.regWrite = true; break;
        case Opcode::RET: cs.ret = true; break;
        case Opcode::NOP: break;
        case Opcode::HALT:cs.halt = true; break;
    }
    return cs;
}

uint32_t ControlUnit::encode_horizontal(const ControlSignals& cs, uint32_t next, bool end) {
    uint32_t raw = 0;
    if (cs.regWrite) raw |= (1 << 23);
    if (cs.memRead)  raw |= (1 << 22);
    if (cs.memWrite) raw |= (1 << 21);
    if (cs.aluSrc)   raw |= (1 << 20);
    raw |= ((static_cast<uint32_t>(cs.aluOp) & 0xF) << 16);
    if (cs.branch)   raw |= (1 << 15);
    if (cs.jump)     raw |= (1 << 14);
    if (cs.call)     raw |= (1 << 13);
    if (cs.ret)      raw |= (1 << 12);
    if (cs.halt)     raw |= (1 << 11);
    raw |= ((next & 0xFF) << 3);
    if (end)         raw |= (1 << 2);
    return raw;
}

ControlSignals ControlUnit::decode_horizontal(uint32_t raw) {
    ControlSignals cs;
    cs.regWrite = (raw & (1 << 23)) != 0;
    cs.memRead  = (raw & (1 << 22)) != 0;
    cs.memWrite = (raw & (1 << 21)) != 0;
    cs.aluSrc   = (raw & (1 << 20)) != 0;
    cs.aluOp    = static_cast<AluOp>((raw >> 16) & 0xF);
    cs.branch   = (raw & (1 << 15)) != 0;
    cs.jump     = (raw & (1 << 14)) != 0;
    cs.call     = (raw & (1 << 13)) != 0;
    cs.ret      = (raw & (1 << 12)) != 0;
    cs.halt     = (raw & (1 << 11)) != 0;
    return cs;
}

uint32_t ControlUnit::encode_vertical(const ControlSignals& cs, uint32_t next, bool end) {
    uint32_t opField = 0;
    uint32_t dataField = 0;
    
    if (cs.halt) {
        opField = 7;
    } else if (cs.jump || cs.call || cs.ret) {
        opField = 6;
        if (cs.jump) dataField = 0;
        else if (cs.call) dataField = 1;
        else if (cs.ret) dataField = 2;
    } else if (cs.branch) {
        opField = 5;
    } else if (cs.memWrite) {
        opField = 3;
    } else if (cs.memRead) {
        opField = 2;
    } else if (cs.regWrite) {
        if (cs.aluOp != AluOp::PASS) {
            opField = 4;
            dataField = static_cast<uint32_t>(cs.aluOp);
        } else {
            opField = 4;
            dataField = static_cast<uint32_t>(AluOp::PASS);
        }
    } else if (cs.aluOp != AluOp::PASS) {
        opField = 1;
        dataField = static_cast<uint32_t>(cs.aluOp);
    } else {
        opField = 0;
    }

    uint32_t raw = 0;
    raw |= ((opField & 0x7) << 13);
    raw |= ((dataField & 0xF) << 9);
    raw |= ((next & 0xFF) << 1);
    if (end) raw |= 1;
    return raw;
}

ControlSignals ControlUnit::decode_vertical(uint32_t raw) {
    ControlSignals cs;
    uint32_t opField = (raw >> 13) & 0x7;
    uint32_t dataField = (raw >> 9) & 0xF;
    
    switch (opField) {
        case 0: break;
        case 1: cs.aluOp = static_cast<AluOp>(dataField); break;
        case 2: cs.regWrite = true; cs.memRead = true; break;
        case 3: cs.memWrite = true; break;
        case 4: cs.regWrite = true; cs.aluOp = static_cast<AluOp>(dataField); break;
        case 5: cs.branch = true; break;
        case 6:
            if (dataField == 0) cs.jump = true;
            else if (dataField == 1) cs.call = true;
            else if (dataField == 2) cs.ret = true;
            break;
        case 7: cs.halt = true; break;
    }
    return cs;
}

ControlUnit::ControlUnit(ControlStyle s) : style_(s) {
    load_microcode();
}

ControlSignals ControlUnit::decode(Opcode op, bool imm) const {
    if (style_ == ControlStyle::HARDWIRED) {
        return hardwired_decode(op, imm);
    } else {
        uint32_t micro_pc = dispatch(op);
        ControlSignals aggregated;
        while (true) {
            const auto& mi = fetch_micro(micro_pc);
            if (mi.signals.regWrite) aggregated.regWrite = true;
            if (mi.signals.memRead)  aggregated.memRead = true;
            if (mi.signals.memWrite) aggregated.memWrite = true;
            if (mi.signals.aluOp != AluOp::PASS) aggregated.aluOp = mi.signals.aluOp;
            if (mi.signals.branch)   aggregated.branch = true;
            if (mi.signals.jump)     aggregated.jump = true;
            if (mi.signals.call)     aggregated.call = true;
            if (mi.signals.ret)      aggregated.ret = true;
            if (mi.signals.halt)     aggregated.halt = true;
            if (mi.end_of_sequence) break;
            micro_pc = mi.next_address;
        }
        aggregated.aluSrc = imm;
        return aggregated;
    }
}

const MicroInstruction& ControlUnit::fetch_micro(uint32_t micro_pc) const {
    if (micro_pc >= control_memory_.size()) {
        throw std::out_of_range("MicroPC out of bounds");
    }
    return control_memory_[micro_pc];
}

uint32_t ControlUnit::dispatch(Opcode op) const {
    int idx = static_cast<int>(op);
    if (idx >= 0 && idx < NUM_OPCODES) {
        return dispatch_table_[idx];
    }
    return 0;
}

void ControlUnit::load_microcode() {
    control_memory_.clear();
    uint32_t address = 0;

    auto add_micro = [&](Opcode op, const std::vector<ControlSignals>& seq) {
        dispatch_table_[static_cast<int>(op)] = address;
        for (size_t i = 0; i < seq.size(); ++i) {
            MicroInstruction mi;
            mi.address = address;
            mi.signals = seq[i];
            mi.end_of_sequence = (i == seq.size() - 1);
            mi.next_address = mi.end_of_sequence ? 0 : address + 1;
            
            if (style_ == ControlStyle::HORIZONTAL) {
                mi.raw_bits = encode_horizontal(mi.signals, mi.next_address, mi.end_of_sequence);
            } else if (style_ == ControlStyle::VERTICAL) {
                mi.raw_bits = encode_vertical(mi.signals, mi.next_address, mi.end_of_sequence);
            } else {
                mi.raw_bits = 0; 
            }
            control_memory_.push_back(mi);
            address++;
        }
    };

    auto alu_seq = [](AluOp aop, bool is_cmp = false) {
        std::vector<ControlSignals> seq(3);
        seq[0].aluOp = AluOp::PASS;
        seq[1].aluOp = aop;
        seq[2].aluOp = AluOp::PASS;
        if (!is_cmp) {
            seq[2].regWrite = true;
        }
        return seq;
    };

    add_micro(Opcode::ADD, alu_seq(AluOp::ADD));
    add_micro(Opcode::SUB, alu_seq(AluOp::SUB));
    add_micro(Opcode::MUL, alu_seq(AluOp::MUL));
    add_micro(Opcode::DIV, alu_seq(AluOp::DIV));
    add_micro(Opcode::MOD, alu_seq(AluOp::MOD));
    add_micro(Opcode::CMP, alu_seq(AluOp::CMP, true));
    add_micro(Opcode::AND, alu_seq(AluOp::AND));
    add_micro(Opcode::OR,  alu_seq(AluOp::OR));
    add_micro(Opcode::NOT, alu_seq(AluOp::NOT));
    add_micro(Opcode::MOV, alu_seq(AluOp::PASS));
    add_micro(Opcode::LSL, alu_seq(AluOp::LSL));
    add_micro(Opcode::LSR, alu_seq(AluOp::LSR));
    add_micro(Opcode::ASR, alu_seq(AluOp::ASR));

    {
        std::vector<ControlSignals> seq(3);
        seq[0].aluOp = AluOp::ADD; seq[0].aluSrc = true;
        seq[1].memRead = true;
        seq[2].regWrite = true;
        add_micro(Opcode::LD, seq);
    }
    {
        std::vector<ControlSignals> seq(2);
        seq[0].aluOp = AluOp::ADD; seq[0].aluSrc = true;
        seq[1].memWrite = true;
        add_micro(Opcode::ST, seq);
    }
    {
        std::vector<ControlSignals> seq(1);
        seq[0].branch = true;
        add_micro(Opcode::BEQ, seq);
        add_micro(Opcode::BGT, seq);
    }
    {
        std::vector<ControlSignals> seq(1);
        seq[0].jump = true;
        add_micro(Opcode::B, seq);
    }
    {
        std::vector<ControlSignals> seq(2);
        seq[0].call = true; seq[0].regWrite = true;
        seq[1].jump = true;
        add_micro(Opcode::CALL, seq);
    }
    {
        std::vector<ControlSignals> seq(1);
        seq[0].ret = true;
        add_micro(Opcode::RET, seq);
    }
    {
        std::vector<ControlSignals> seq(1);
        add_micro(Opcode::NOP, seq);
    }
    {
        std::vector<ControlSignals> seq(1);
        seq[0].halt = true;
        add_micro(Opcode::HALT, seq);
    }
}

size_t ControlUnit::word_width() const {
    if (style_ == ControlStyle::HORIZONTAL) return 24;
    if (style_ == ControlStyle::VERTICAL) return 16;
    return 0;
}

std::string ControlUnit::style_name() const {
    if (style_ == ControlStyle::HORIZONTAL) return "Horizontal Microprogrammed";
    if (style_ == ControlStyle::VERTICAL) return "Vertical Microprogrammed";
    return "Hardwired";
}

std::string ControlUnit::dump_microcode() const {
    std::ostringstream oss;
    oss << "Control Memory (" << style_name();
    if (word_width() > 0) {
        oss << ", " << word_width() << "-bit words";
    }
    oss << ")\n";
    oss << "Addr  Raw       Signals                          Next  End\n";
    
    for (const auto& mi : control_memory_) {
        oss << std::setfill('0') << std::setw(4) << mi.address << "  ";
        if (word_width() > 0) {
            oss << "0x" << std::hex << std::uppercase << std::setw(6) << mi.raw_bits << std::dec << "  ";
        } else {
            oss << "--------  ";
        }
        
        std::string sigs = mi.signals.to_string();
        oss << std::setfill(' ') << std::left << std::setw(33) << sigs;
        
        oss << std::right << std::setfill('0') << std::setw(4) << mi.next_address << "  ";
        oss << (mi.end_of_sequence ? "yes" : "no") << "\n";
    }
    return oss.str();
}

} // namespace risc201
